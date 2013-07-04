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

#include "contactremoverequest.h"
#include "engine.h"

#include <dao/contactdetailschema.h>
#include <dao/subject.h>

#include <lib/garbagecollector.h>
#include <lib/sparqlconnectionmanager.h>
#include <lib/sparqlresolver.h>

#include <QtSparql>

QTrackerContactRemoveRequest::QTrackerContactRemoveRequest(QContactAbstractRequest *request,
                                                           QContactTrackerEngine *engine,
                                                           QObject *parent)
    : QTrackerBaseRequest<QContactRemoveRequest>(engine, parent)
    , m_contactIds(staticCast(request)->contactIds())
{
}

QTrackerContactRemoveRequest::~QTrackerContactRemoveRequest()
{
}

static QString
makeLocalUIDList(const QList<QContactLocalId> &localIdList)
{
    QStringList idStrings;

    foreach(QContactLocalId id, localIdList) {
        idStrings.append(QString::number(id));
    }

    return idStrings.join(QString::fromLatin1(", "));
}

QString
QTrackerContactRemoveRequest::buildQuery(const QList<QContactLocalId> &localIds) const
{
    static const QString queryTemplate = QLatin1String
            ("DELETE {\n"
             "  ?contact a rdfs:Resource\n"
             "} WHERE {\n"
             "  ?contact rdf:type nco:Contact .\n"
             "  FILTER(tracker:id(?contact) IN (%1))\n"
             "}\n");

    // Taking localIds from argument instead of taking member because remove requests
    // are split into small chunks to avoid "Too many variables" errors in tracker.
    return queryTemplate.arg(makeLocalUIDList(localIds));
}

QString
QTrackerContactRemoveRequest::buildQuery() const
{
    return buildQuery(m_contactIds);
}

void
QTrackerContactRemoveRequest::run()
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

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QContactLocalId selfId = engine()->selfContactId(&error);

    if (error != QContactManager::NoError) {
        setLastError(error);
        return;
    }

    const int nContacts = m_contactIds.count();

    for (int i = m_contactIds.size() - 1; i >= 0; --i) {
        if (selfId == m_contactIds[i]) {
            m_errorMap.insert(i, QContactManager::PermissionsError);
            m_contactIds.removeAt(i);
            // This error does not prevent us from removing the other contacts
            setLastError(QContactManager::PermissionsError);
        }
    }

    // check here for empty list to catch the case where the only id in the list
    // was the self-id.
    if (m_contactIds.isEmpty()) {
        return;
    }

    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn(QString::fromLatin1("Cannot remove contact: No valid QtSparql connection."));
        setLastError(QContactManager::UnspecifiedError);
        return;
    }

    while(not m_contactIds.isEmpty()) {
        // split the local id list to avoid "Too many SQL variables" warning
        const QList<QContactLocalId> nextLocalIds = m_contactIds.mid(0, QctSparqlResolver::ColumnLimit);
        m_contactIds = m_contactIds.mid(nextLocalIds.count());

        const QSparqlQuery query(buildQuery(nextLocalIds), QSparqlQuery::DeleteStatement);
        QScopedPointer<QSparqlResult> result(runQuery(query, SyncQueryOptions));

        if (result.isNull()) {
            // runQuery() called reportError()
            break;
        }
    }

    QctGarbageCollector::trigger(engine()->gcQueryId(), 1.0*nContacts/engine()->gcLimit());
}

void
QTrackerContactRemoveRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateContactRemoveRequest(staticCast(engine()->request(this).data()),
                                         error, m_errorMap, QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactremoverequest.cpp"
