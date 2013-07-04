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

#include "relationshipremoverequest.h"

#include <engine/engine.h>

#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerRelationshipRemoveRequest::QTrackerRelationshipRemoveRequest(QContactAbstractRequest *request,
                                                                     QContactTrackerEngine *engine,
                                                                     QObject *parent)
    : QTrackerBaseRequest<QContactRelationshipRemoveRequest>(engine, parent)
    , m_relationships(staticCast(request)->relationships())
{
}

QTrackerRelationshipRemoveRequest::~QTrackerRelationshipRemoveRequest()
{
}

QString
QTrackerRelationshipRemoveRequest::buildQuery()
{
    static const QString queryTemplate = QLatin1String
            ("DELETE {\n"
             "  ?c nco:belongsToGroup ?g\n"
             "} WHERE {\n"
             "  ?g a nco:Contact FILTER(tracker:id(?g) = %1).\n"
             "  ?c a nco:Contact FILTER(tracker:id(?c) = %2).\n"
             "}\n");

    QStringList queries;

    const QString managerUri = engine()->managerUri();

    for(int i = 0; i < m_relationships.count(); ++i) {
        const QContactRelationship &relationship = m_relationships.at(i);
        const QContactId firstContactId = relationship.first();
        const QContactId secondContactId = relationship.second();

        // Only HasMember currently supported
        if (QContactRelationship::HasMember != relationship.relationshipType()) {
            m_errorMap.insert(i, QContactManager::NotSupportedError);
            continue;
        }

        // First contact in relationship must be local
        if (firstContactId.managerUri() != managerUri) {
            m_errorMap.insert(i, QContactManager::InvalidRelationshipError);
            continue;
        }

        // Foreign contact in group currently not supported
        if (secondContactId.managerUri() != managerUri) {
            m_errorMap.insert(i, QContactManager::NotSupportedError);
            continue;
        }

        // First and second contact in relationship must be different
        if (firstContactId.localId() == secondContactId.localId()) {
            m_errorMap.insert(i, QContactManager::InvalidRelationshipError);
            continue;
        }

        queries.append(queryTemplate.arg(QString::number(firstContactId.localId()),
                                         QString::number(secondContactId.localId())));
    }

    if (not m_errorMap.isEmpty()) {
        setLastError((m_errorMap.constEnd() - 1).value());
    }

    return queries.join(QLatin1String("\n\n"));
}


void
QTrackerRelationshipRemoveRequest::run()
{
    if (not turnIrreversible()) {
        return;
    }

    // We explicitly don't check for contact existance (DoesNotExistError error) because:
    //
    // 1) Checking for contact existance costs time.
    // 2) More importantly checking for contact existance would be entirely racy.
    //    Therefore such a feature would be entirely random and useless.
    //
    // If some customer explicitly asks for that feature we can spend time on how to implement
    // an atomic contact existance check. Still this check would have to be explicitely requested
    // per contact manager parameter for first reason.

    const QString queryString = buildQuery();
    if (not queryString.isEmpty()) {
        delete runQuery(QSparqlQuery(buildQuery(), QSparqlQuery::DeleteStatement), SyncQueryOptions);
    }
}

void
QTrackerRelationshipRemoveRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateRelationshipRemoveRequest(staticCast(engine()->request(this).data()),
                                              error, m_errorMap, QContactAbstractRequest::FinishedState);
}
