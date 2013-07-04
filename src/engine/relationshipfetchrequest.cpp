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

#include "relationshipfetchrequest.h"

#include "engine/engine.h"

#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

static bool isSupportedRelationshipType(const QString &relationshipType)
{
    if (relationshipType.isEmpty() // wild card
        || (relationshipType == QContactRelationship::HasMember)) {
        return true;
    }

    // unsupported type
    return false;
}


QTrackerRelationshipFetchRequest::QTrackerRelationshipFetchRequest(QContactAbstractRequest *request,
                                                                   QContactTrackerEngine *engine,
                                                                   QObject *parent)
    : QTrackerBaseRequest<QContactRelationshipFetchRequest>(engine, parent)
    , m_firstContactId(staticCast(request)->first())
    , m_secondContactId(staticCast(request)->second())
    , m_relationshipType(staticCast(request)->relationshipType())
{
}

QTrackerRelationshipFetchRequest::~QTrackerRelationshipFetchRequest()
{
}

void
QTrackerRelationshipFetchRequest::run()
{
    if (isCanceled()) {
        return;
    }

    const static QString requestTemplatePrefix = QLatin1String
            ("SELECT tracker:id(?g) tracker:id(?c)\n"
             "WHERE\n"
             "{\n"
             "  ?c a nco:Contact ; nco:belongsToGroup ?g .\n"
             "  ?g a nco:Contact ; a nco:ContactGroup .\n");

    const static QString requestTemplateSuffix = QLatin1String("}");
    const static QString idTemplate = QLatin1String("  FILTER(tracker:id(%1) = %2) .\n");

    if (not isSupportedRelationshipType(m_relationshipType)) {
        qctWarn(QString::fromLatin1("Only HasMember relationships supported, got %1").
                arg(m_relationshipType));
        setLastError(QContactManager::NotSupportedError);
    }

    QString queryString = requestTemplatePrefix;

    // define group by id if given
    if (m_firstContactId != QContactId()) {
        queryString += idTemplate.arg(QLatin1String("?g"),
                                      QString::number(m_firstContactId.localId()));
    }

    // define membercontact by id if given
    if (m_secondContactId != QContactId()) {
        queryString += idTemplate.arg(QLatin1String("?c"),
                                      QString::number(m_secondContactId.localId()));
    }

    queryString += requestTemplateSuffix;

    // run query
    QScopedPointer<QSparqlResult> result(runQuery(QSparqlQuery(queryString), SyncQueryOptions));

    if (result.isNull()) {
        return; // runQuery() called reportError()
    }

    // fetch result
    QContactId contactId;
    contactId.setManagerUri(engine()->managerUri());

    while (not isCanceled() && result->next()) {
        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowModels)) {
            qDebug() << result->current();
        }

        bool ok0, ok1;

        QContactLocalId localIdFirst = result->value(0).toUInt(&ok0);
        QContactLocalId localIdSecond = result->value(1).toUInt(&ok1);

        if (ok0 && ok1 && (localIdFirst!=localIdSecond)) {
            QContactRelationship relationship;
            contactId.setLocalId(localIdFirst);
            relationship.setFirst(contactId);

            contactId.setLocalId(localIdSecond);
            relationship.setSecond(contactId);

            relationship.setRelationshipType(QContactRelationship::HasMember);

            m_relationships.append(relationship);
        }
    }
}


void
QTrackerRelationshipFetchRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateRelationshipFetchRequest(staticCast(engine()->request(this).data()),
                                             m_relationships, error, QContactAbstractRequest::FinishedState);
}
