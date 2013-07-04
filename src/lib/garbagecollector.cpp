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

#include "garbagecollector.h"

#include "logger.h"

bool
QctGarbageCollector::registerQuery(const QString &id, const QString &query)
{
    static const QString registerMethod = QString::fromLatin1("Register");

    QDBusMessage message = createCall(registerMethod);

    QVariantList arguments;
    arguments.append(id);
    arguments.append(query);

    message.setArguments(arguments);

    const QDBusConnection connection = QDBusConnection::sessionBus();

    bool success = connection.send(message);

    if (not success) {
        qctWarn(QString::fromLatin1("Could not register GC query: %1").
                arg(connection.lastError().message()));
    }

    return success;
}

bool
QctGarbageCollector::trigger(const QString &id, double load)
{
    static const QString triggerMethod = QString::fromLatin1("Trigger");

    QDBusMessage message = createCall(triggerMethod);

    QVariantList arguments;
    arguments.append(id);
    arguments.append(load);

    message.setArguments(arguments);

    const QDBusConnection connection = QDBusConnection::sessionBus();

    bool success = connection.send(message);

    if (not success) {
        qctWarn(QString::fromLatin1("Could not trigger GC: %1").
                arg(connection.lastError().message()));
    }

    return success;
}

QDBusMessage
QctGarbageCollector::createCall(const QString &method)
{
    static const QString service = QString::fromLatin1("com.nokia.contactsd");
    static const QString objectPath = QString::fromLatin1("/com/nokia/contacts/GarbageCollector1");
    static const QString interface = QString::fromLatin1("com.nokia.contacts.GarbageCollector1");

    return QDBusMessage::createMethodCall(service, objectPath, interface, method);
}
