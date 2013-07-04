/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 *********************************************************************************/

#include "guidalgorithm.h"

#include <dao/support.h>

#include <lib/logger.h>
#include <lib/settings.h>
#include <lib/threadlocaldata.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_CELLULAR

#include <CellularQt/SIM>
#include <CellularQt/SIMPhonebook>
#include <sysinfo.h>

using Cellular::SIM::SIMStatus;
using Cellular::SIMPhonebook;

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////

const QString QctGuidAlgorithm::Default = QLatin1String("default");
const QString QctGuidAlgorithm::Cellular = QLatin1String("cellular");

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctDefaultGuidAlgorithm : public QctGuidAlgorithm
{
public: // QctGuidAlgorithm methods
    QContactGuid makeGuid(const QContact &contact);
    void callWhenReady(QObject *receiver, const char *member);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QContactGuid
QctDefaultGuidAlgorithm::makeGuid(const QContact &/*contact*/)
{
    QContactGuid guidDetail;
    guidDetail.setGuid(qctUuidString());
    return guidDetail;
}

void
QctDefaultGuidAlgorithm::callWhenReady(QObject *receiver, const char *member)
{
    if (0 == receiver || 0 == member) {
        qctWarn("Invalid arguments passed");
        return;
    }

    // This algorithm doesn't need any initialization, therefore signal its readyness
    // immediatly. Still we want to make sure the slot always is called from event loop.
    // Unfortunatly QMetaObject::invoke() has no clue about signatures as returned by
    // the SLOT() macro (WTF!!?). Therefore we abuse QTimer::singleShot().
    QTimer::singleShot(0, receiver, member);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#if ENABLE_CELLULAR

class QctClosure
{
public: // constructors
    QctClosure(QObject *receiver, const char *member);

public: // methods
    void invoke();

private: // fields
    QPointer<QObject> m_receiver;
    QByteArray m_member;
};

class QctCellularGuidAlgorithm : public QObject, public QctGuidAlgorithm
{
    Q_OBJECT

public: // constructors
    QctCellularGuidAlgorithm();
    ~QctCellularGuidAlgorithm();

public: // QctGuidAlgorithm methods
    QContactGuid makeGuid(const QContact &contact);
    void callWhenReady(QObject *receiver, const char *member);

private: // methods
    static QString getSystemInfo(const QString &key);
    static QString makeHash(QString text, int length);

    void updateHardwareHash(const QString &msisdn);
    void notifyReady();

private slots:
    void onSimStatusChanged(SIMStatus::Status);

    void onReadExactComplete(int index, const QString &name, const QString &number,
                             const QString &secondName, const QString &additionalNumber,
                             const QString &email, SIMPhonebook::SIMPhonebookError error);

private: // fields
    bool m_hashReady;
    SIMStatus m_status;
    SIMPhonebook m_phonebook;

    QString m_hardwareHash;
    QQueue<QctClosure> m_closures;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctClosure::QctClosure(QObject *receiver, const char *member)
    : m_receiver(receiver)
    , m_member(member)
{
}

void
QctClosure::invoke()
{
    if (not m_receiver.isNull()) {
        QTimer::singleShot(0, m_receiver.data(), m_member.data());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QctCellularGuidAlgorithm::QctCellularGuidAlgorithm()
{
    updateHardwareHash(QctThreadLocalData::instance()->settings()->lastMSISDN());
    m_hashReady = false;

    // connect signals
    connect(&m_status, SIGNAL(statusChanged(SIMStatus::Status)),
            SLOT(onSimStatusChanged(SIMStatus::Status)));
    connect(&m_status, SIGNAL(statusComplete(SIMStatus::Status,SIMError)),
            SLOT(onSimStatusChanged(SIMStatus::Status)));
    connect(&m_phonebook, SIGNAL(readExactComplete(int,QString,QString,QString,QString,QString,
                                                   SIMPhonebook::SIMPhonebookError)),
            SLOT(onReadExactComplete(int,QString,QString,QString,QString,QString,
                                     SIMPhonebook::SIMPhonebookError)));

    // query current SIM status
    m_status.status();
}

QctCellularGuidAlgorithm::~QctCellularGuidAlgorithm()
{
}

QString
QctCellularGuidAlgorithm::getSystemInfo(const QString &key)
{
    struct system_config *sc = 0;
    QString result;

    if (0 != sysinfo_init(&sc)) {
        const QByteArray keyArray = key.toLatin1();
        unsigned long size = 0;
        uint8_t *data = 0;

        if (0 == sysinfo_get_value(sc, keyArray.data(), &data, &size)) {
            for(unsigned long i = 0; i < size; ++i) {
                result.append(QLatin1Char(data[i]));
            }
        }

        sysinfo_finish(sc);
    }

    return result;
}

QString
QctCellularGuidAlgorithm::makeHash(QString text, int length)
{
    text = text.leftJustified(length);
    length /= 2;

    const int stride = text.length() / length;
    QString result;

    for(int i = 0; i < length; ++i) {
        char c = '\0';

        for(int j = 0; j < stride; ++j) {
            c += text.at(j + i * stride).toLatin1();
        }

        QString number = QString::number(abs(c), 16);

        if (number.length() < 2) {
            result += QLatin1Char('0');
        }

        result += number;
    }

    return result;
}

void
QctCellularGuidAlgorithm::updateHardwareHash(const QString &msisdn)
{
    const QString model = getSystemInfo(QLatin1String("/component/product-name"));
    const QString imei = getSystemInfo(QLatin1String("/certs/npc/esn/gsm"));
    m_hardwareHash = makeHash(imei + msisdn + model, 16);
}

void
QctCellularGuidAlgorithm::notifyReady()
{
    while(not m_closures.isEmpty()) {
        m_closures.dequeue().invoke();
    }
}

void
QctCellularGuidAlgorithm::onSimStatusChanged(SIMStatus::Status status)
{
    if (SIMStatus::Ok == status) {
        m_phonebook.readExact(SIMPhonebook::MSISDN, 0);
    } else {
        static const QString empty;
        onReadExactComplete(0, empty, empty, empty, empty, empty, SIMPhonebook::NoSIMService);
    }
}

void
QctCellularGuidAlgorithm::onReadExactComplete(int /*index*/,
                                              const QString &/*name*/,
                                              const QString &number,
                                              const QString &/*secondName*/,
                                              const QString &/*additionalNumber*/,
                                              const QString &/*email*/,
                                              SIMPhonebook::SIMPhonebookError error)
{
    QctSettings *const settings = QctThreadLocalData::instance()->settings();

    if (SIMPhonebook::NoError == error && not number.isEmpty()) {
        settings->setLastMSISDN(number);
    }

    updateHardwareHash(settings->lastMSISDN());
    m_hashReady = true;
    notifyReady();
}

QContactGuid
QctCellularGuidAlgorithm::makeGuid(const QContact &contact)
{
    const QContactName nameDetail = contact.detail<QContactName>();
    const QDateTime now = QDateTime::currentDateTime().toUTC();

    const QString contactSeed =
            nameDetail.firstName() + nameDetail.lastName() +
            contact.detail<QContactPhoneNumber>().number();

    QContactGuid guidDetail;

    guidDetail.setGuid(makeHash(contactSeed, 10) + QLatin1String("-") +
                       now.toString(QLatin1String("yyyyMMdd")) + QLatin1String("-") +
                       now.toString(QLatin1String("hhmmss")) + QLatin1String("-") +
                       m_hardwareHash);

    return guidDetail;
}

void
QctCellularGuidAlgorithm::callWhenReady(QObject *receiver, const char *member)
{
    if (m_hashReady) {
        QTimer::singleShot(0, receiver, member);
    } else {
        m_closures.enqueue(QctClosure(receiver, member));
    }
}

#endif // ENABLE_CELLULAR

////////////////////////////////////////////////////////////////////////////////////////////////////

QctGuidAlgorithm *
QctGuidAlgorithmFactory::algorithm(const QString &name)
{
    if (name.isEmpty()) {
        return algorithm(QctGuidAlgorithm::Default);
    }

    typedef QMap<QString, QctGuidAlgorithm *> GeneratorMap;
    static GeneratorMap algorithmMap;

    GeneratorMap::Iterator algorithm = algorithmMap.find(name);

    if (algorithm != algorithmMap.end()) {
        return algorithm.value();
    }

    if (name == QctGuidAlgorithm::Default) {
        static QctDefaultGuidAlgorithm defaultAlgorithm;
        algorithmMap.insert(QctGuidAlgorithm::Default, &defaultAlgorithm);
        return &defaultAlgorithm;
    }

    if (name == QctGuidAlgorithm::Cellular) {
#ifdef ENABLE_CELLULAR
        static QctCellularGuidAlgorithm cellularAlgorithm;
        algorithmMap.insert(QctGuidAlgorithm::Cellular, &cellularAlgorithm);
        return &cellularAlgorithm;
#else // ENABLE_CELLULAR
        qctWarn("Cellular GUID algorithm not available.");
#endif // ENABLE_CELLULAR
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#include "guidalgorithm.moc"
