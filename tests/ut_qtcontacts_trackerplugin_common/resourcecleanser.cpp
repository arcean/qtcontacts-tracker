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

#include "resourcecleanser.h"

#include <lib/logger.h>
#include <lib/sparqlconnectionmanager.h>

#include <QtSparql>

ResourceCleanser::ResourceCleanser(const QSet<QString> &subjects, QObject *parent)
    : QObject(parent), m_subjects(subjects)
{
}

ResourceCleanser::~ResourceCleanser()
{
}

void
ResourceCleanser::run()
{
    QString queryString = QLatin1String("DELETE {");

    foreach(const QString &iri, m_subjects) {
        queryString += QString::fromLatin1("<%1> a rdfs:Resource .").arg(iri);
    }

    queryString += QLatin1String("}");

    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn("No QtSparql connection available.");
        return;
    }

    const QSparqlQuery query(queryString, QSparqlQuery::DeleteStatement);
    QScopedPointer<QSparqlResult> result(connection.syncExec(query));

    if (result->hasError()) {
        qctWarn(result->lastError().message());
    }
}

#include "moc_resourcecleanser.cpp"
