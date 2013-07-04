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

#include "contactcopyandremoverequest.h"

#include <dao/scalarquerybuilder.h>
#include <dao/support.h>
#include <engine/engine.h>
#include <lib/constants.h>
#include <lib/contactmergerequest.h>
#include <lib/customdetails.h>
#include <lib/sparqlresolver.h>

#include <QtSparql>

#include <cubi.h>

#include <ontologies/nco.h>
#include <ontologies/rdf.h>
#include <ontologies/rdfs.h>

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

QTrackerContactCopyAndRemoveRequest::QTrackerContactCopyAndRemoveRequest(QContactAbstractRequest *request,
                                                                         QContactTrackerEngine *engine,
                                                                         QObject *parent)
    : QTrackerBaseRequest<QctContactMergeRequest>(engine, parent)
    , m_mergeIds(staticCast(request)->mergeIds())
{
}

QTrackerContactCopyAndRemoveRequest::~QTrackerContactCopyAndRemoveRequest()
{
}

bool
QTrackerContactCopyAndRemoveRequest::verifyRequest()
{
    MergeIdMap::ConstIterator it = m_mergeIds.constBegin();

    for(int i = 0; it != m_mergeIds.constEnd(); ++it, ++i) {
        if (0 == it.key() || 0 == it.value()) {
            m_errorMap.insert(i, QContactManager::BadArgumentError);
            setLastError(QContactManager::BadArgumentError);
        }
    }

    if (lastError() != QContactManager::NoError) {
        return false;
    }

    return true;
}

bool
QTrackerContactCopyAndRemoveRequest::resolveContactIds()
    {

    // Resolve needed contact IRIs.
    const QList<uint> idsToLookup = QList<uint>()
            << m_mergeIds.uniqueKeys()
            << m_mergeIds.values();

    QctResourceIriResolver resolver(idsToLookup);

    if (not resolver.lookupAndWait()) {
        reportError(resolver.errors(), QLatin1String("Cannot resolve local ids"));
        return false;
    }

    for (int i = 0; i < resolver.trackerIds().count(); ++i) {
        m_contactIris.insert(resolver.trackerIds().at(i),
                             resolver.resourceIris().at(i));
    }

    return true;
}

bool
QTrackerContactCopyAndRemoveRequest::fetchContactDetails()
{
    enum {SyncTargetColumn = 0, GraphedPropertiesColumn, UngraphedPropertiesColumn};
    enum {PropertyField = 0, CardinalityField, NFields};

    static const QString queryTemplate = QLatin1String
            ("SELECT\n"
             "  nie:generator(<%1>)\n"
             "  (SELECT\n"
             "        GROUP_CONCAT(fn:concat(?p, \"%2\", tracker:coalesce(nrl:maxCardinality(?p),\"\")), \"%3\")\n"
             "   WHERE\n"
             "   {\n"
             "        GRAPH ?g\n"
             "        {\n"
             "                <%1> ?p ?o FILTER(?p NOT IN(dc:date,nie:informationElementDate,nie:generator,nco:contactLocalUID))\n"
             "        }\n"
             "   })\n"
             "  (SELECT\n"
             "        GROUP_CONCAT(fn:concat(?p, \"%2\", tracker:coalesce(nrl:maxCardinality(?p),\"\")), \"%3\")\n"
             "   WHERE\n"
             "   {\n"
             "        <%1> ?p ?o FILTER(?p NOT IN(dc:date,nie:informationElementDate,nie:generator,nco:contactLocalUID))\n"
             "   })\n"
             "{}\n");

    QMap<QString, bool> singleWithGraph;

    foreach(const QString &contactIri, m_contactIris) {
        const QString queryString = queryTemplate.arg(contactIri,
                                                      QTrackerScalarContactQueryBuilder::fieldSeparator(),
                                                      QTrackerScalarContactQueryBuilder::listSeparator());
        const QSparqlQuery query(queryString, QSparqlQuery::SelectStatement);

        QScopedPointer<QSparqlResult> result(runQuery(query, SyncQueryOptions));

        if (result.isNull()) {
            return false; // runQuery() called reportError()
        }

        // parse result
        QString syncTarget;

        while (result->next()) {
            QStringList graphedProperties = result->stringValue(GraphedPropertiesColumn).split(QTrackerScalarContactQueryBuilder::listSeparator());

            foreach (const QString property, graphedProperties) {
                QStringList values = property.split(QTrackerScalarContactQueryBuilder::fieldSeparator());

                if (values.size() != NFields) {
                    qctWarn(QString::fromLatin1("Invalid field data: %1").arg(property));
                    return false;
                }

                if (values.at(CardinalityField).toInt() != 1) {
                    m_contactPredicatesMultiWithGraph.insertMulti(contactIri, values.at(PropertyField));
                } else {
                    singleWithGraph.insert(values.at(PropertyField), true);
                    m_contactPredicatesSingleWithGraph.insertMulti(contactIri, values.at(PropertyField));
                }
            }

            QStringList ungraphedProperties = result->stringValue(UngraphedPropertiesColumn).split(QTrackerScalarContactQueryBuilder::listSeparator());

            foreach (const QString property, ungraphedProperties) {
                QStringList values = property.split(QTrackerScalarContactQueryBuilder::fieldSeparator());

                if (values.size() != NFields) {
                    qctWarn(QString::fromLatin1("Invalid field data: %1").arg(property));
                    return false;
                }

                if (values.at(CardinalityField).toInt() != 1) {
                    m_contactPredicatesMulti.insertMulti(contactIri, values.at(PropertyField));
                } else if (not singleWithGraph.contains(values.at(PropertyField))) {
                    m_contactPredicatesSingle.insertMulti(contactIri, values.at(PropertyField));
                }

                if (syncTarget.isNull()) {
                    syncTarget = result->stringValue(SyncTargetColumn);
                }
            }

            if (not syncTarget.isEmpty()) {
                m_contactSyncTargets.insert(contactIri, syncTarget);
            }
        }
    }

    return true;
}

bool
QTrackerContactCopyAndRemoveRequest::doMerge()
{
    QString queryString;

    foreach(QContactLocalId id, m_mergeIds.uniqueKeys()) {
        queryString += buildMergeQuery(id);

        if (lastError() != QContactManager::NoError) {
            return false;
        }
    }

    const QSparqlQuery query(queryString, QSparqlQuery::InsertStatement);
    return not QScopedPointer<QSparqlResult>(runQuery(query, SyncQueryOptions)).isNull();
}

QString
QTrackerContactCopyAndRemoveRequest::pickPreferredSyncTarget(const QSet<QString> &syncTargets,
                                                             const QString &destinationSyncTarget)
{
    // Compute the sync target for the resulting contact.
    //
    // The choice of the sync target follows those rules:
    //
    //      "mfe"       + *             → "mfe"
    //      "telepathy" + "addressbook" → "addressbook"
    //      "telepathy" + "telepathy"   → "telepathy"
    //      "telepathy" + "other"       → "other"
    //
    // First walk all the sync targets looking for an MFE sync target.
    // If we have more than one MFE sync target, it's an error (would delete some
    // contacts from the server)
    // The MFE sync targets are of the form mfe#accountId
    {
        const QLatin1String mfePrefix = QContactSyncTarget__SyncTargetMfe;
        QString finalSyncTarget;

        foreach (const QString &syncTarget, syncTargets) {
            if (not syncTarget.startsWith(mfePrefix, Qt::CaseInsensitive)) {
                continue;
            }

            if (finalSyncTarget.isEmpty()) {
                finalSyncTarget = syncTarget;
                continue;
            }

            // If we found more than one MFE sync target in the list
            reportError("Trying to merge two contacts from different MFE accounts. "
                        "This is not supported, since one of the source contacts "
                        "would get deleted from the MFE server.",
                        QContactManager::BadArgumentError);
            return QString();
        }

        if (not finalSyncTarget.isEmpty()) {
            return finalSyncTarget;
        }
    }

    const QString engineSyncTarget = engine()->syncTarget();

    foreach (const QString &syncTarget, syncTargets) {
        if (syncTarget.compare(engineSyncTarget, Qt::CaseInsensitive) == 0) {
            return syncTarget;
        }
    }

    const QLatin1String telepathySyncTarget = QContactSyncTarget__SyncTargetTelepathy.operator QLatin1String();
    const bool isDestinationSyncTargetTelepathy = (destinationSyncTarget.compare(telepathySyncTarget, Qt::CaseInsensitive) == 0);

    // If we have only telepathy contacts in the merge request
    if (syncTargets.size() == 1 && isDestinationSyncTargetTelepathy) {
        return QContactSyncTarget__SyncTargetTelepathy;
    }

    // None of the rules above matched, we pick the destination sync target as a
    // fallback except if it's telepathy (telepathy never "dominates" any other
    // sync target) or if it's empty
    if (not isDestinationSyncTargetTelepathy && not destinationSyncTarget.isEmpty()) {
        return destinationSyncTarget;
    }

    // The destination sync target is telepathy or empty, but we have some other
    // sync targets in the list too, so we pick one of them (not much else we can do)
    foreach (const QString &syncTarget, syncTargets) {
        if (syncTarget.compare(telepathySyncTarget, Qt::CaseInsensitive) != 0) {
            return syncTarget;
        }
    }

    // No valid sync target could be found - we fall back to the engine sync target
    return engineSyncTarget;
}

QString
QTrackerContactCopyAndRemoveRequest::buildMergeQuery(QContactLocalId id)
{
    const Options::SparqlOptions sparqlOptions = engine()->updateQueryOptions();
    const QString targetUrn = m_contactIris.value(id);
    QStringList sourceUrns;
    QStringList inserts;

    foreach (QContactLocalId id, m_mergeIds.values(id)) {
        sourceUrns.append(m_contactIris.value(id));
    }

    foreach (const QString &sourceUrn, sourceUrns) {
        inserts.append(mergeContacts(targetUrn, sourceUrn));
    }

    Delete sourceDelete;
    Variable deleteSubject;
    PatternGroup deleteRestriction;
    ValueList idsToDelete;

    foreach (QContactLocalId id, m_mergeIds.values(id)) {
       idsToDelete.addValue(LiteralValue(QVariant(id)));
    }

    sourceDelete.addData(deleteSubject, rdf::type::resource(), rdfs::Resource::resource());
    deleteRestriction.addPattern(deleteSubject, rdf::type::resource(), rdfs::Resource::resource());
    deleteRestriction.setFilter(Functions::in.apply(Functions::trackerId.apply(deleteSubject), idsToDelete));
    sourceDelete.addRestriction(deleteRestriction);


    static const QString timestampUpdateTemplate = QString::fromLatin1(
                "DELETE {"
                "  <%1> nie:contentLastModified ?v"
                "} WHERE {"
                "  <%1> nie:contentLastModified ?v"
                "}"
                "INSERT {"
                "  GRAPH <%3> {"
                "    <%1> nie:contentLastModified \"%2\""
                "  }"
                "}"
                );

    const QString currentDateTime = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString queryString;

    foreach (const QString &insert, inserts) {
        queryString.append(insert);
    }

    queryString.append(timestampUpdateTemplate.arg(targetUrn,
                                                   currentDateTime,
                                                   QtContactsTrackerDefaultGraphIri));

    queryString.append(sourceDelete.sparql(sparqlOptions));

    // Compute the sync target of the resulting contact
    const QString destinationSyncTarget = m_contactSyncTargets.value(targetUrn);
    QSet<QString> syncTargets;

    syncTargets.insert(destinationSyncTarget);

    foreach(const QString &iri, sourceUrns) {
        syncTargets.insert(m_contactSyncTargets.value(iri));
    }

    const QString preferredSyncTarget = pickPreferredSyncTarget(syncTargets, destinationSyncTarget);

    // Abort early if pickPreferredSyncTarget() reported an error
    if (lastError() != QContactManager::NoError) {
        return QString();
    }

    static const QString syncTargetQueryTemplate = QString::fromLatin1(
                "DELETE {GRAPH <%3> {<%1> nie:generator ?generator}}"
                "WHERE {GRAPH <%3> {<%1> nie:generator ?generator}}"
                "INSERT {GRAPH <%3> {<%1> nie:generator %2}}"
                );
    queryString.append(syncTargetQueryTemplate.arg(targetUrn,
                                                  LiteralValue(preferredSyncTarget).sparql(),
                                                  QtContactsTrackerDefaultGraphIri));

    return queryString;
}

QString
QTrackerContactCopyAndRemoveRequest::mergeContacts(const QString &targetUrn,
                                                   const QString &sourceUrn)
{
    const Options::SparqlOptions sparqlOptions = engine()->updateQueryOptions();

    QStringList queryStrings;
    ResourceValue target(targetUrn);
    ResourceValue source(sourceUrn);

    foreach (const QString &predicateUrn, m_contactPredicatesSingleWithGraph.values(sourceUrn)) {
        if (m_contactPredicatesSingle.contains(targetUrn, predicateUrn) ||
            m_contactPredicatesSingleWithGraph.contains(targetUrn, predicateUrn)) {
            continue;
        }

        Variable graphVar;
        Graph dataGraph(graphVar);
        Graph whereGraph(graphVar);
        const ResourceValue predicate(predicateUrn);
        Variable value;

        dataGraph.addPattern(target, predicate, value);
        whereGraph.addPattern(source, predicate, value);

        Insert insert;
        insert.addData(dataGraph);
        insert.addRestriction(whereGraph);
        queryStrings += insert.sparql(sparqlOptions);

        // Also fill the hash so that another "merge slave" will not try to write to this predicate
        m_contactPredicatesSingle.insertMulti(targetUrn, predicateUrn);
    }

    foreach (const QString &predicateUrn, m_contactPredicatesSingle.values(sourceUrn)) {
        if (m_contactPredicatesSingle.contains(targetUrn, predicateUrn) ||
            m_contactPredicatesSingleWithGraph.contains(targetUrn, predicateUrn)) {
            continue;
        }

        const ResourceValue predicate(predicateUrn);
        Variable value;

        Insert insert;
        insert.addData(target, predicate, value);
        insert.addRestriction(source, predicate, value);
        queryStrings += insert.sparql(sparqlOptions);

        // Also fill the hash so that another "merge slave" will not try to write
        // to this predicate
        m_contactPredicatesSingle.insertMulti(targetUrn, predicateUrn);
    }


    foreach (const QString &predicateUrn, m_contactPredicatesMultiWithGraph.values(sourceUrn).toSet()) {
        if (predicateUrn == rdf::type::iri()) {
            continue;
        }

        Variable graphVar;
        Graph dataGraph(graphVar);
        Graph whereGraph(graphVar);
        ResourceValue predicate(predicateUrn);
        Variable value;

        dataGraph.addPattern(target, predicate, value);
        whereGraph.addPattern(source, predicate, value);

        Insert insert;
        insert.addData(dataGraph);
        insert.addRestriction(whereGraph);
        queryStrings += insert.sparql(sparqlOptions);
    }

    foreach (const QString &predicateUrn, m_contactPredicatesMulti.values(sourceUrn).toSet()) {
        if (predicateUrn == rdf::type::iri()) {
            continue;
        }

        ResourceValue predicate(predicateUrn);
        Variable value;

        Insert insert;
        insert.addData(target, predicate, value);
        insert.addRestriction(source, predicate, value);
        queryStrings += insert.sparql(sparqlOptions);
    }

    return queryStrings.join(QLatin1String("\n"));
}

void
QTrackerContactCopyAndRemoveRequest::run()
{
    if (not turnIrreversible()
            || not verifyRequest()
            || not resolveContactIds()
            || not fetchContactDetails()
            || not doMerge()) {
        return; // request failed (http://en.wikipedia.org/wiki/Lazy_evaluation)
    }
}

void
QTrackerContactCopyAndRemoveRequest::updateRequest(QContactManager::Error error)
{

    engine()->updateContactRemoveRequest(staticCast(engine()->request(this).data()),
                                         error, m_errorMap, QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactcopyandremoverequest.cpp"
