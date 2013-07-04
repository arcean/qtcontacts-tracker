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

#include "settings.h"
#include "settings_p.h"

#include "customdetails.h"
#include "logger.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

const QString QctSettings::DefaultNameOrder = QContactDisplayLabel__FieldOrderFirstName;

const QStringList QctSettings::DefaultSparqlBackends = QStringList() << QLatin1String("QTRACKER_DIRECT")
                                                                     << QLatin1String("QTRACKER");

const QString QctSettings::NumberMatchLengthKey = QLatin1String("numberMatchLength");
const QString QctSettings::ConcurrencyLevelKey  = QLatin1String("concurrencyLevel");
const QString QctSettings::AvatarSizeKey        = QLatin1String("avatarSize");
const QString QctSettings::NameOrderKey         = QLatin1String("nameOrder");
const QString QctSettings::GuidAlgorithmNameKey = QLatin1String("guidAlgorithmName");
const QString QctSettings::LastMSISDNKey        = QLatin1String("lastMSISDN");
const QString QctSettings::SparqlBackendsKey    = QLatin1String("sparqlBackends");
const QString QctSettings::PreferNicknameKey    = QLatin1String("preferNickname");

///////////////////////////////////////////////////////////////////////////////////////////////////

Q_GLOBAL_STATIC(QctSettingsSingleton, settingsSingleton)

void QctSettingsSingleton::onStoredSettingsChanged()
{
    // make sure the storage proxy is up-to-date
    m_storedSettings.sync();

    // find changed settings and update working copy
    QHash<QString,QVariant> changedSettings;

    for (QHash<QString,QVariant>::Iterator it = m_localSettings.begin();
         it != m_localSettings.end(); ++it) {
        const QString &key = it.key();
        QVariant &localValue = it.value();
        const QVariant storedValue = m_storedSettings.value(key, localValue);

        if (storedValue != localValue) {
            // collect changed settings
            changedSettings.insert(key, storedValue);
            // update working copy
            localValue = storedValue;
        }
    }

    // notify about changed settings
    if (not changedSettings.isEmpty()) {
        emit valuesChanged(changedSettings);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class QctSettingsData : public QSharedData
{
    // Stub to prevent potential ABI issues with QctSettings in the future. Not used yet.
};

static inline int
defaultConcurrencyLevel()
{
    const static int idealThreadCount = QThread::idealThreadCount() + 1;
    return idealThreadCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QctSettingsSingleton::QctSettingsSingleton()
    : m_storedSettings(QSettings::IniFormat, QSettings::UserScope,
                       QLatin1String("Nokia"), QLatin1String("Contacts"))
{
    if (qApp) {
        // Ensure the singleton and its child objects have consistent thread affinity.
        // Without this step the singleton would belong to the random, potentially short
        // living thread accessing the singleton first.
        moveToThread(qApp->thread());
    } else {
        qctWarn("QctSettings: Cannot be used without QCoreApplication");
    }

    QFileInfo settingsFileInfo = m_storedSettings.fileName();
    QDir settingsDir = settingsFileInfo.absoluteDir();

    // create settings folder if it doesn't exist yet, so that it can be watched
    if (not settingsDir.exists()) {
        settingsDir.mkpath(QLatin1String("."));
    }

    // create settings file if it doesn't exist yet, so that it can be watched
    if (not settingsFileInfo.exists()) {
        QFile settingsFile(settingsFileInfo.absoluteFilePath());
        settingsFile.open(QFile::WriteOnly);
        settingsFile.close();
    }

    // watch settings file to send notifications about changes
    const QStringList paths = QStringList() << settingsFileInfo.absoluteFilePath();
    QFileSystemWatcher *settingsStoreWatcher = new QFileSystemWatcher(paths, this);
    connect(settingsStoreWatcher, SIGNAL(fileChanged(QString)), SLOT(onStoredSettingsChanged()));

    if (qApp) {
        // Ensure the QFileSystemWatcher is destroyed before the event loop quits.
        // Otherwise a dead-lock will occur on application shutdown. QTBUG-15255.
        // Now the file watcher will get deleted when the singleton gets destroyed,
        // or right before the application object tears down. Whatever occurs first.
        connect(qApp, SIGNAL(aboutToQuit()), settingsStoreWatcher, SLOT(deleteLater()));
    }
}

void
QctSettingsSingleton::registerSetting(const QString& key, const QVariant &defaultValue)
{
    if (not m_localSettings.contains(key)) {
        const QVariant storedValue = m_storedSettings.value(key, defaultValue);
        m_localSettings.insert(key, storedValue);
    }
}

void
QctSettingsSingleton::sync()
{
    m_storedSettings.sync();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QctSettings::QctSettings(QObject *parent)
    : QObject(parent)
    , d(0)
{
    static bool isSettingsRegistered = false;

    if (not isSettingsRegistered) {
        registerSetting(NumberMatchLengthKey, DefaultPhoneNumberLength);
        registerSetting(ConcurrencyLevelKey, defaultConcurrencyLevel());
        registerSetting(AvatarSizeKey, DefaultAvatarSize);
        registerSetting(NameOrderKey, DefaultNameOrder);
        registerSetting(GuidAlgorithmNameKey);
        registerSetting(LastMSISDNKey);
        registerSetting(SparqlBackendsKey, DefaultSparqlBackends);
        registerSetting(PreferNicknameKey, DefaultPreferNickname);

        isSettingsRegistered = true;
    }

    connect(settingsSingleton(),
            SIGNAL(valuesChanged(QHash<QString,QVariant>)),
            SIGNAL(valuesChanged(QHash<QString,QVariant>)));
}

QctSettings::~QctSettings()
{
    sync();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
QctSettings::setValue(const QString &key, const QVariant &value)
{
    settingsSingleton()->setValue(key, value);
}

QVariant
QctSettings::value(const QString& key, const QVariant& defaultValue) const
{
    settingsSingleton()->registerSetting(key, defaultValue);
    return settingsSingleton()->value(key);
}

QVariant
QctSettings::value(const QString &key) const
{
    return settingsSingleton()->value(key);
}

void QctSettings::registerSetting(const QString& key, const QVariant &defaultValue)
{
    settingsSingleton()->registerSetting(key, defaultValue);
}

void QctSettings::sync()
{
    settingsSingleton()->sync();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
QctSettings::setLocalPhoneNumberLength(int length)
{
    setValue(NumberMatchLengthKey, length);
}

int
QctSettings::localPhoneNumberLength() const
{
    bool valid;
    int length = value(NumberMatchLengthKey).toInt(&valid);

    if (not valid) {
        length = DefaultPhoneNumberLength;
    }

    return length;
}

void
QctSettings::setConcurrencyLevel(int concurrencyLevel)
{
    setValue(ConcurrencyLevelKey, concurrencyLevel);
}

int
QctSettings::concurrencyLevel() const
{
    bool valid;
    int concurrencyLevel = value(ConcurrencyLevelKey).toInt(&valid);

    if (not valid || concurrencyLevel < 1) {
        concurrencyLevel = qMax(1, defaultConcurrencyLevel());
    }

    return concurrencyLevel;
}

void
QctSettings::setAvatarSize(int avatarSize)
{
    setValue(AvatarSizeKey, avatarSize);
}

QSize
QctSettings::avatarSize() const
{
    bool valid;
    int avatarSize = value(AvatarSizeKey).toInt(&valid);

    if (not valid) {
        avatarSize = DefaultAvatarSize;
    }

    return QSize(avatarSize, avatarSize);
}

void
QctSettings::setNameOrder(const QString &nameOrder)
{
    setValue(NameOrderKey, nameOrder);
}

QString
QctSettings::nameOrder() const
{
    return value(NameOrderKey).toString();
}

void
QctSettings::setGuidAlgorithmName(const QString &algorithmName)
{
    setValue(GuidAlgorithmNameKey, algorithmName);
}

QString
QctSettings::guidAlgorithmName() const
{
    return value(GuidAlgorithmNameKey).toString();
}

void
QctSettings::setLastMSISDN(const QString &msisdn)
{
    setValue(LastMSISDNKey, msisdn);
}

QString
QctSettings::lastMSISDN() const
{
    return value(LastMSISDNKey).toString();
}

void
QctSettings::setSparqlBackends(const QStringList &backends)
{
    setValue(SparqlBackendsKey, backends);
}

QStringList
QctSettings::sparqlBackends() const
{
    return value(SparqlBackendsKey).toStringList();
}

void
QctSettings::setPreferNickname(bool preferNickname)
{
    setValue(PreferNicknameKey, preferNickname);
}

bool
QctSettings::preferNickname() const
{
    return value(PreferNicknameKey).toBool();
}
