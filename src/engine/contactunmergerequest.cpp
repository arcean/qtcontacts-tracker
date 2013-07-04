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

#include "contactunmergerequest.h"

#include <dao/scalarquerybuilder.h>
#include <dao/subject.h>
#include <dao/support.h>
#include <engine/engine.h>
#include <lib/constants.h>
#include <lib/customdetails.h>
#include <lib/sparqlresolver.h>
#include <lib/unmergeimcontactsrequest.h>

#include <QtSparql>

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

QTrackerContactSaveOrUnmergeRequest::QTrackerContactSaveOrUnmergeRequest(QContactAbstractRequest *request,
                                                                         QContactTrackerEngine *engine,
                                                                         QObject *parent)
    : QTrackerBaseRequest<QctUnmergeIMContactsRequest>(engine, parent)
    , m_unmergeOnlineAccounts(staticCast(request)->unmergeOnlineAccounts())
    , m_sourceContact(staticCast(request)->sourceContact())
{
}

QTrackerContactSaveOrUnmergeRequest::~QTrackerContactSaveOrUnmergeRequest()
{
}

QString
QTrackerContactSaveOrUnmergeRequest::buildQuery()
{
    const Options::SparqlOptions sparqlOptions = engine()->updateQueryOptions();

    QStringList queries;

    foreach(const QContactOnlineAccount &account, m_unmergeOnlineAccounts) {
        const QString contactIri(makeAnonymousIri(QUuid::createUuid()));

        queries.append(insertContactQuery(contactIri, account).sparql(sparqlOptions));

        // When unmerging contacts we add telepathy as a fallback generator, so
        // that they get deleted properly when their hour comes
        queries.append(insertTelepathyGeneratorFallbackQuery(contactIri).sparql(sparqlOptions));

        foreach (const Delete &d, cleanupQueries(account)) {
            queries.append(d.sparql(sparqlOptions));
        }

        m_unmergedContactIris += contactIri;
    }

    return queries.join(QLatin1String("\n"));
}

void
QTrackerContactSaveOrUnmergeRequest::run()
{
    if (not turnIrreversible()) {
        return;
    }

    if (resolveSourceContact() && fetchPredicates() &&
        unmergeContacts() && resolveUnmergedContactIds()) {
    }
}

bool
QTrackerContactSaveOrUnmergeRequest::resolveSourceContact()
{
    if (m_sourceContact.id().managerUri() != engine()->managerUri() ||
        m_sourceContact.localId() == 0) {
        setLastError(QContactManager::BadArgumentError);
        return false;
    }

    QctResourceIriResolver resolver(QList<QContactLocalId>() << m_sourceContact.localId());

    if (not resolver.lookupAndWait()) {
        reportError(resolver.errors(), QLatin1String("Cannot resolve resource IRI for source contact"));
        return false;
    }

    if (resolver.resourceIris().count() != 1) {
        setLastError(QContactManager::DoesNotExistError);
        return false;
    }

    m_sourceContactIri = resolver.resourceIris().first();

    return true;
}

bool
QTrackerContactSaveOrUnmergeRequest::resolveUnmergedContactIds()
{
    QctTrackerIdResolver resolver(m_unmergedContactIris);

    if (not resolver.lookupAndWait()) {
        reportError(resolver.errors(), QLatin1String("Cannot resolve local ids of unmerged contacts"));
        return false;
    }

    if (resolver.trackerIds().count() != m_unmergedContactIris.count()) {
        reportError("Failed to resolve local ids for all merged contacts");
        return false;
    }

    m_unmergedContactIds = resolver.trackerIds();

    return true;
}

// can't make results a QVector<QVector<QString>> since QString::split returns
// QStringList
static QVector<QStringList>
parseResults(const QString &data)
{
    const QStringList rows = data.split(QTrackerScalarContactQueryBuilder::detailSeparator());

    QVector<QStringList> results;
    results.reserve(rows.count());

    foreach (const QString &row, rows) {
        const QStringList tokens = row.split(QTrackerScalarContactQueryBuilder::fieldSeparator());

        results.append(tokens);
    }

    return results;
}

bool
QTrackerContactSaveOrUnmergeRequest::fetchPredicates()
{
    static const LiteralValue detailSeparator(QTrackerScalarContactQueryBuilder::detailSeparator());
    static const LiteralValue fieldSeparator(QTrackerScalarContactQueryBuilder::fieldSeparator());

    // We run two selects (nested in one)
    // 1st one is to get the graph of all ?contact ?p ?o statements
    // graph is either qct, or the telepathy iri, so we can use the graph to
    // know which properties to detach from the original contact
    //
    // 2nd one is to enumerate the online accounts, because the graph for
    // ?contact nco:hasAffiliation ?aff is either qct, telepathy iri or
    // contactsd graph (if the affiliation has one IMAddress on it), so we need
    // to check the actual IMAddress to know which affiliations we have to move

    const ResourceValue contact(m_sourceContactIri);

    // 1st select
    const Variable predicate;
    const Variable predicateGraph;

    const ValueChain predicateProjections =
            ValueChain() << predicateGraph << fieldSeparator
                         << predicate;

    Graph g(predicateGraph);
    g.addPattern(contact, predicate, Variable());

    Select predicateSelect;
    predicateSelect.addProjection(Functions::groupConcat.
                                  apply(Functions::concat.apply(predicateProjections),
                                        detailSeparator));
    predicateSelect.addRestriction(g);

    // 2nd select

    const Variable affiliationGraph;
    const Variable affiliation;
    const Variable imAddress;

    const ValueChain accountProjections =
            ValueChain() << affiliationGraph << fieldSeparator
                         << affiliation << fieldSeparator
                         << imAddress;

    g = Graph(affiliationGraph);
    g.addPattern(contact, nco::hasAffiliation::resource(), affiliation);
    g.addPattern(affiliation, nco::hasIMAddress::resource(), imAddress);

    Select accountSelect;
    accountSelect.addProjection(Functions::groupConcat.
                                apply(Functions::concat.apply(accountProjections),
                                      detailSeparator));
    accountSelect.addRestriction(g);

    // Final step, combine the two selects in one
    Select select;
    select.addProjection(predicateSelect);
    select.addProjection(accountSelect);

    const QSparqlQuery query(select.sparql(engine()->selectQueryOptions()));
    QScopedPointer<QSparqlResult> result(runQuery(query, SyncQueryOptions));

    if (result.isNull()) {
        return false; // runQuery() calls reportError()
    }

    while (result->next()) {
        const QString predicateData = result->stringValue(0);
        const QString accountData = result->stringValue(1);

        // We first get the list of predicate/graph
        foreach (const QStringList &row, parseResults(predicateData)) {
            if (row.size() != 2) {
                qctWarn("Skipping invalid result row");
                continue;
            }

            // Skip predicates that might blow up the source contact
            if (row.at(1) == rdf::type::iri()) {
                continue;
            }

            m_predicates.insert(row.at(0), row.at(1));
        }

        // And then the list of affiliation graph/affiliation iri/imaddress iri
        foreach (const QStringList &row, parseResults(accountData)) {
            if (row.size() != 3) {
                qctWarn("Skipping invalid result row");
                continue;
            }

            m_onlineAccounts.insert(row.at(2), qMakePair(row.at(0), row.at(1)));
        }
    }

    return true;
}

bool
QTrackerContactSaveOrUnmergeRequest::unmergeContacts()
{
    const QSparqlQuery unmergeQuery(buildQuery(), QSparqlQuery::InsertStatement);
    return not QScopedPointer<QSparqlResult>(runQuery(unmergeQuery, SyncQueryOptions)).isNull();
}

Insert
QTrackerContactSaveOrUnmergeRequest::insertContactQuery(const QString &contactIri,
                                                        const QContactOnlineAccount &account)
{
    static const QString affiliationNamePattern = QLatin1String("affiliation%1");

    int affiliationIndex = 0;

    const ResourceValue contact(contactIri);
    const QString accountPath = account.value(QContactOnlineAccount__FieldAccountPath);
    const QString accountUri = account.accountUri();
    const QString telepathyIri = makeTelepathyIri(accountPath, accountUri);

    Insert insert;

    // We first reparent all the hasAffiliation statements that were in the telepathy graph (if any)
    const QSet<QString> telepathyPredicates = m_predicates.values(telepathyIri).toSet();

    Graph commonGraph = Graph(ResourceValue(QtContactsTrackerDefaultGraphIri));

    commonGraph.addPattern(contact, rdf::type::resource(), nco::PersonContact::resource());
    commonGraph.addPattern(contact, nie::contentLastModified::resource(), LiteralValue(QDateTime::currentDateTimeUtc()));
    insert.addData(commonGraph);

    if (not telepathyPredicates.isEmpty()) {
        Graph imGraph = Graph(ResourceValue(telepathyIri));
        Graph restrictionGraph = Graph(ResourceValue(telepathyIri));
        const ResourceValue source(m_sourceContactIri);

        foreach (const QString &predicate, telepathyPredicates) {
            const ResourceValue predicateResource(predicate);
            Variable value;

            imGraph.addPattern(contact, predicateResource, value);
            restrictionGraph.addPattern(source, predicateResource, value);
        }

        insert.addData(imGraph);
        insert.addRestriction(restrictionGraph);
    }

    // Then reparent the IMAddress
    // Because there might be several IMAddress on a single affiliation (if the affiliation
    // statement is in qct's graph, when adding a new contact with OnlineAccounts), we
    // go "down" to IMAddress precision.
    if (m_onlineAccounts.contains(telepathyIri)) {
        const QPair<QString, QString> accountData = m_onlineAccounts.value(telepathyIri);
        Graph imAffiliationGraph = Graph(ResourceValue(accountData.first));
        const BlankValue affiliation(affiliationNamePattern.arg(affiliationIndex++));
        ResourceValue imAddress = ResourceValue(telepathyIri);

        imAffiliationGraph.addPattern(affiliation, rdf::type::resource(), nco::Affiliation::resource());
        imAffiliationGraph.addPattern(contact, nco::hasAffiliation::resource(), affiliation);
        imAffiliationGraph.addPattern(affiliation, nco::hasIMAddress::resource(), imAddress);

        insert.addData(imAffiliationGraph);
    }

    return insert;
}

QList<Delete>
QTrackerContactSaveOrUnmergeRequest::cleanupQueries(const QContactOnlineAccount &account)
{
    const ResourceValue source(m_sourceContactIri);
    const QString accountPath = account.value(QContactOnlineAccount__FieldAccountPath);
    const QString accountUri = account.accountUri();
    const QString telepathyIri = makeTelepathyIri(accountPath, accountUri);

    QList<Cubi::Delete> results;

    Delete d;

    // Delete all properties when statement is in the graph of unmerged IMAddress
    if (m_predicates.contains(telepathyIri)) {
        Graph deleteGraph = Graph(ResourceValue(telepathyIri));
        Graph whereGraph = Graph(ResourceValue(telepathyIri));

        foreach (const QString &predicate, m_predicates.values(telepathyIri).toSet()) {
            ResourceValue predicateResource = ResourceValue(predicate);

            // Avoid predicates that might blow everything up
            if (predicateResource == rdf::type::resource()) {
                continue;
            }

            Variable value;

            deleteGraph.addPattern(source, predicateResource, value);
            whereGraph.addPattern(source, predicateResource, value);
        }

        d.addData(deleteGraph);
        d.addRestriction(whereGraph);
    }

    results.append(d);

    // We do the delete in two parts, because the restrictions are different (if
    // the where for the first one does not match, we still want to try to delete
    // a potential IMAddress

    d = Delete();

    // And also the IMAddress itself
    if (m_onlineAccounts.contains(telepathyIri)) {
        const QPair<QString, QString> accountData = m_onlineAccounts.value(telepathyIri);
        const ResourceValue affiliation = ResourceValue(accountData.second);
        const ResourceValue imAddress = ResourceValue(telepathyIri);

        d.addData(source, nco::hasAffiliation::resource(), affiliation);
        d.addData(affiliation, nco::hasIMAddress::resource(), imAddress);
    }

    results.append(d);

    return results;
}

Insert
QTrackerContactSaveOrUnmergeRequest::insertTelepathyGeneratorFallbackQuery(const QString &contactIri)
{
    static const LiteralValue telepathyGenerator = LiteralValue(QString::fromLatin1("telepathy"));

    Insert insert;
    Graph graph = Graph(ResourceValue(QtContactsTrackerDefaultGraphIri));
    const ResourceValue subject(contactIri);
    PatternGroup restriction;
    PatternGroup optionalGenerator;
    Variable generator;

    graph.addPattern(subject, nie::generator::resource(), telepathyGenerator);
    insert.addData(graph);

    restriction.addPattern(subject, rdf::type::resource(), nco::PersonContact::resource());
    optionalGenerator.setOptional(true);
    optionalGenerator.addPattern(subject, nie::generator::resource(), generator);
    restriction.addPattern(optionalGenerator);
    restriction.setFilter(Functions::not_.apply(Functions::bound.apply(generator)));
    insert.addRestriction(restriction);

    return insert;
}

void
QTrackerContactSaveOrUnmergeRequest::updateRequest(QContactManager::Error error)
{
    const QctRequestLocker &request = engine()->request(this);

    if (not request.isNull()) {
        staticCast(request.data())->setUnmergedContactIds(m_unmergedContactIds);

        engine()->updateContactSaveRequest(staticCast(request.data()),
                                           QList<QContact>(), error, ErrorMap(),
                                           QContactAbstractRequest::FinishedState);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactunmergerequest.cpp"
