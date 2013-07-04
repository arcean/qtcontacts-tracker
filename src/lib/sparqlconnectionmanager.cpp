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

#include "sparqlconnectionmanager.h"

#include "logger.h"
#include "settings.h"
#include "threadlocaldata.h"

#include <QtCore/QStringList>
#include <QtSparql/QSparqlConnection>

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_GLOBAL_STATIC(QctSparqlConnectionManager, defaultSparqlManager)

////////////////////////////////////////////////////////////////////////////////////////////////////

QctSparqlConnectionManager::QctSparqlConnectionManager()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QSparqlConnection &
QctSparqlConnectionManager::defaultConnection()
{
    return defaultSparqlManager()->connection();
}

QSparqlConnection &
QctSparqlConnectionManager::connection()
{
    QctThreadLocalData *const threadLocalData = QctThreadLocalData::instance();
    QSparqlConnection *connection = threadLocalData->sparqlConnection();

    if (0 == connection) {
        static const QStringList permittedDriverNames = threadLocalData->settings()->sparqlBackends();
        bool validConnectionMissing = true;

        foreach(const QString &driverName, permittedDriverNames) {
            connection = new QSparqlConnection(driverName);

            if (connection->isValid()) {
                validConnectionMissing = false;
                break;
            }
        }

        if (validConnectionMissing) {
            QString permittedDrivers = permittedDriverNames.join(QLatin1String(", "));
            QString availableDrivers = QSparqlConnection::drivers().join(QLatin1String(", "));

            if (permittedDrivers.isEmpty()) {
                permittedDrivers = QLatin1String("none");
            }

            if (availableDrivers.isEmpty()) {
                availableDrivers = QLatin1String("none");
            }

            qctWarn(QString::fromLatin1("Could not find any usable QtSparql backend driver. "
                                        "Please install one of the configured drivers, or "
                                        "correct the setting. Configured backends: %1. "
                                        "Available backends: %2.").
                    arg(permittedDrivers, availableDrivers));

            connection = new QSparqlConnection(QLatin1String("invalid"));
        }

        threadLocalData->setSparqlConnection(connection);
    }

    return *connection;
}
