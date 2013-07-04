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

#include "subject.h"
#include "conversion.h"
#include "support.h"

#include <lib/logger.h>
#include <lib/phoneutils.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

static QVariant
toVariant(const QUuid &uuid)
{
    if (not uuid.isNull()) {
        return uuid.toString();
    }

    return QVariant();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
QTrackerContactSubject::isContentScheme(Scheme scheme)
{
    switch(scheme) {
    case None:
    case Anonymous:
        break;

    case PhoneNumber:
    case EmailAddress:
    case Telepathy:
    case Presence:
        return true;
    }

    return false;
}


QVariant
QTrackerContactSubject::parseIri(Scheme scheme, const QString &iri, bool *ok)
{
    switch(scheme) {
    case None:
    case Anonymous:
        return toVariant(parseAnonymousIri(iri, ok));

    case PhoneNumber:
        break; // we don't support parsing of it

    case EmailAddress:
        return parseEmailAddressIri(iri, ok);

    case Telepathy:
        return QVariant::fromValue(parseTelepathyIri(iri, ok));

    case Presence:
        return QVariant::fromValue(parsePresenceIri(iri, ok));
    }

    return qctPropagate(false, ok), QVariant();
}

QString
QTrackerContactSubject::makeIri(Scheme scheme, const QVariantList &values)
{
    switch(scheme) {
    case None:
    case Anonymous:
        if (1 != values.count()) {
#ifndef QT_NO_DEBUG
            qWarning() << Q_FUNC_INFO << "invalid arguments for anonymous subject:" << values;
#endif // QT_NO_DEBUG
            break;
        }

        return makeAnonymousIri(values.first().toString());

    case PhoneNumber:
        if (2 != values.count()) {
#ifndef QT_NO_DEBUG
            qWarning() << Q_FUNC_INFO << "invalid arguments for phone number subject:" << values;
#endif // QT_NO_DEBUG
            break;
        }

        return qctMakePhoneNumberIri(values.at(0).toString(), values.at(1).toStringList());

    case EmailAddress:
        if (1 != values.count()) {
#ifndef QT_NO_DEBUG
            qWarning() << Q_FUNC_INFO << "invalid arguments for email address subject:" << values;
#endif // QT_NO_DEBUG
            break;
        }

        return makeEmailAddressIri(values.first().toString());

    case Telepathy:
        if (2 == values.count()) {
            return makeTelepathyIri(values[0].toString(), values[1].toString());
        }

        if (1 == values.count()) {
            return makeTelepathyIri(values.first().toString());
        }

#ifndef QT_NO_DEBUG
        qWarning() << Q_FUNC_INFO << "invalid arguments for online account subject:" << values;
#endif // QT_NO_DEBUG
        break;

    case Presence:
        if (2 == values.count()) {
            return makePresenceIri(values[0].toString(), values[1].toString());
        }

        if (1 == values.count()) {
            return makePresenceIri(values.first().toString());
        }

#ifndef QT_NO_DEBUG
        qWarning() << Q_FUNC_INFO << "invalid arguments for presence subject:" << values;
#endif // QT_NO_DEBUG
        break;
    }

    return QString();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QUuid
parseAnonymousIri(const QString &iri, bool *ok)
{
    static const QString prefix = QLatin1String("urn:uuid:");
    QUuid uuid;

    if (iri.startsWith(prefix)) {
        const QString suffix = iri.mid(prefix.length());
        uuid = QUuid(QLatin1Char('{') + suffix + QLatin1Char('}'));
    }

    return qctPropagate(not uuid.isNull(), ok), uuid;
}

static QString
parseTextIri(const QString &prefix, const QString &iri, bool *ok)
{
    QString value;

    if (iri.startsWith(prefix)) {
        value = iri.mid(prefix.length());
    }

    return qctPropagate(not value.isEmpty(), ok), value;
}

QString
parseEmailAddressIri(const QString &iri, bool *ok)
{
    static const QString prefix = QLatin1String("mailto:");
    return parseTextIri(prefix, iri, ok);
}

QString
parseTelepathyIri(const QString &iri, bool *ok)
{
    static const QString prefix = QLatin1String("telepathy:");
    return parseTextIri(prefix, iri, ok);
}

QString
parsePresenceIri(const QString &iri, bool *ok)
{
    static const QString prefix = QLatin1String("presence:");
    return parseTextIri(prefix, iri, ok);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QString
makeAnonymousIri(const QUuid &uuid)
{
    if (uuid.isNull()) {
        return QString();
    }

    static const QString pattern = QLatin1String("urn:uuid:%1");
    return pattern.arg(qctUuidString(uuid));
}

QString
makeEmailAddressIri(const QString &emailAddress)
{
    static const QString pattern = QLatin1String("mailto:%1");
    return pattern.arg(emailAddress);
}

QString
makeTelepathyIri(const QString &accountPath, const QString &imAddress)
{
    if (not accountPath.startsWith(QLatin1Char('/'))) {
#ifndef QT_NO_DEBUG
        qctWarn(QString::fromLatin1("Invalid account path: %1").arg(accountPath));
#endif // QT_NO_DEBUG
        return QString();
    }

    if (imAddress.isEmpty()) {
        static const QString pattern = QLatin1String("telepathy:%1");
        return pattern.arg(accountPath);
    }

    static const QString pattern = QLatin1String("telepathy:%1!%2");
    return pattern.arg(accountPath, imAddress);
}

QString
makeTelepathyIri(const QString &connectionPath)
{
    const int separatorIndex = connectionPath.indexOf(QLatin1Char('!'));

    if (separatorIndex >= 0) {
        return makeTelepathyIri(connectionPath.left(separatorIndex),
                                connectionPath.mid(separatorIndex + 1));
    }

    return makeTelepathyIri(connectionPath, QString());
}

QString
makePresenceIri(const QString &accountPath, const QString &imAddress)
{
    // Notice to asymmetry with makeTelepathyIri():
    // - "telepathy:" IRIs can point to accounts and to connections
    // - "presence:" IRIs will always point to connections

    if (not accountPath.startsWith(QLatin1Char('/'))) {
#ifndef QT_NO_DEBUG
        qctWarn(QString::fromLatin1("Invalid account path: %1").arg(accountPath));
#endif // QT_NO_DEBUG
        return QString();
    }

    if (imAddress.isEmpty()) {
#ifndef QT_NO_DEBUG
        qctWarn(QString::fromLatin1("IM address cannot be empty: %1").arg(accountPath));
#endif // QT_NO_DEBUG
        return QString();
    }


    static const QString pattern = QLatin1String("presence:%1!%2");
    return pattern.arg(accountPath, imAddress);
}

QString
makePresenceIri(const QString &connectionPath)
{
    const int separatorIndex = connectionPath.indexOf(QLatin1Char('!'));

    if (-1 == separatorIndex) {
#ifndef QT_NO_DEBUG
        qctWarn(QString::fromLatin1("Invalid connection path").arg(connectionPath));
#endif // QT_NO_DEBUG
        return QString();
    }

    return makePresenceIri(connectionPath.left(separatorIndex),
                           connectionPath.mid(separatorIndex + 1));
}
