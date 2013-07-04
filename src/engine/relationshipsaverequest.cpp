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

#include "relationshipsaverequest.h"

#include <engine/engine.h>
#include <lib/constants.h>

#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerRelationshipSaveRequest::QTrackerRelationshipSaveRequest(QContactAbstractRequest *request,
                                                                 QContactTrackerEngine *engine,
                                                                 QObject *parent)
    : QTrackerBaseRequest<QContactRelationshipSaveRequest>(engine, parent)
    , m_relationships(staticCast(request)->relationships())
{
}

void
QTrackerRelationshipSaveRequest::run()
{
    if (not turnIrreversible()) {
        return;
    }

    if (m_relationships.isEmpty()) {
        setLastError(QContactManager::BadArgumentError);
        return;
    }

    static const QString sparqlTemplate = QLatin1String
            ("INSERT {\n"
             "  GRAPH <%3> {\n"
             "    ?member nco:belongsToGroup ?group\n"
             "  }\n"
             "}\n"
             "WHERE {\n"
             "  ?member a nco:Contact . ?group a nco:Contact .\n"
             "  FILTER(%1=tracker:id(?member) && %2=tracker:id(?group))\n"
             "}\n");

    QString queryString;

    static const int digitsPerUInt = log(UINT_MAX) / log(10);
    queryString.reserve((sparqlTemplate.length() + 2 * digitsPerUInt) * m_relationships.length());

    // TODO: support saving of foreign relationships
    for (int i = 0; i < m_relationships.length(); ++i) {
        QContactRelationship &relationship = m_relationships[i];

        if (relationship.relationshipType() != QContactRelationship::HasMember) {
            qctWarn(QString::fromLatin1("Only HasMember relationships supported, got %1").
                    arg(relationship.relationshipType()));
            m_errorMap.insert(i, QContactManager::NotSupportedError);
            continue;
        }

        const QContactId firstContactId = relationship.first();

        if (firstContactId.managerUri() != engine()->managerUri()) {
            qctWarn(QString::fromLatin1("First contact in relationship must be local, got contact from %1").
                    arg(firstContactId.managerUri()));
            m_errorMap.insert(i, QContactManager::InvalidRelationshipError);
            continue;
        }

        QContactId secondContactId = relationship.second();

        if (secondContactId.managerUri().isEmpty()) {
            secondContactId.setManagerUri(engine()->managerUri());
            relationship.setSecond(secondContactId);
        } else if (secondContactId.managerUri() != engine()->managerUri()) {
            qctWarn(QString::fromLatin1("Foreign contact in group currently not supported, got contact from %1").
                    arg(secondContactId.managerUri()));
            m_errorMap.insert(i, QContactManager::NotSupportedError);
            continue;
        }

        if (firstContactId.localId() == secondContactId.localId()) {
            qctWarn(QString::fromLatin1("First and second contact in relationship must be different, got 2x %1").
                    arg(firstContactId.localId()));
            m_errorMap.insert(i, QContactManager::InvalidRelationshipError);
            continue;
        }

        queryString += sparqlTemplate.arg(QString::number(secondContactId.localId()),
                                          QString::number(firstContactId.localId()),
                                          QtContactsTrackerDefaultGraphIri);
    }

    if (not queryString.isEmpty()) {
        delete runQuery(QSparqlQuery(queryString, QSparqlQuery::InsertStatement), SyncQueryOptions);
    } else if (not m_errorMap.empty()) {
        setLastError((m_errorMap.constEnd() - 1).value());
    }
}

void QTrackerRelationshipSaveRequest::updateRequest(QContactManager::Error error)

{
    engine()->updateRelationshipSaveRequest(staticCast(engine()->request(this).data()),
                                            m_relationships, error, m_errorMap,
                                            QContactAbstractRequest::FinishedState);
}
