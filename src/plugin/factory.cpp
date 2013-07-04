/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2009-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "factory.h"

#include <dao/support.h>
#include <engine/engine.h>
#include <lib/logger.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

QContactTrackerEngineV1::QContactTrackerEngineV1(const QMap<QString, QString>& parameters,
                                                 const QString& managerName, int interfaceVersion,
                                                 QObject *parent)
    : m_impl(new QContactTrackerEngine(parameters, managerName, interfaceVersion))
{
    setParent(parent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ContactTrackerFactory::ContactTrackerFactory()
{
    const QString banner =
            QString::fromLatin1(PACKAGE ": initializing " PACKAGE " " VERSION " for %2 [%1]").
            arg(QString::number(QCoreApplication::applicationPid()),
                QCoreApplication::applicationFilePath());

    qDebug(qPrintable(banner));
}

ContactTrackerFactory::~ContactTrackerFactory()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static int findInterfaceVersion(const QMap<QString, QString> &parameters)
{
    typedef QMap<QString, QString>::ConstIterator Iterator;
    const Iterator it = parameters.find(QLatin1String(QTCONTACTS_IMPLEMENTATION_VERSION_NAME));
    int interfaceVersion = -1;

    if (it != parameters.end()) {
        bool ok = false;
        interfaceVersion = it->toInt(&ok);

        if (not ok) {
            qctWarn("Malformed contact manager interface version");
            return 0;
        }
    }

    return interfaceVersion;
}

QContactManagerEngine *
ContactTrackerFactory::engine(const QMap<QString, QString> &parameters,
                              QContactManager::Error *error)
{
    const int interfaceVersion = findInterfaceVersion(parameters);

    switch(interfaceVersion) {
    case 1:
        qctPropagate(QContactManager::NoError, error);
        return new QContactTrackerEngineV1(parameters, managerName(), interfaceVersion);

    case 2: case -1:
        qctPropagate(QContactManager::NoError, error);
        return new QContactTrackerEngine(parameters, managerName(), interfaceVersion);

    default:
        qctPropagate(QContactManager::NotSupportedError, error);
        qctWarn(QString::fromLatin1("Engine interface version %1 is not supported").
                arg(interfaceVersion));
        return 0;
    }
}

QString
ContactTrackerFactory::managerName() const
{
    return QLatin1String("tracker");
}

QList<int>
ContactTrackerFactory::supportedImplementationVersions() const
{
    return QList<int>() << 1 << 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_EXPORT_PLUGIN2(qtcontacts_tracker, ContactTrackerFactory)
