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

#include "contactidfetchrequest.h"

#include "dao/contactdetailschema.h"
#include "dao/scalarquerybuilder.h"
#include "engine/abstractcontactfetchrequest.h"
#include "engine/engine.h"
#include "lib/contactlocalidfetchrequest.h"
#include "lib/sparqlconnectionmanager.h"

#include <ontologies/rdf.h>
#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactIdFetchRequest::QTrackerContactIdFetchRequest(QContactAbstractRequest *request,
                                                             QContactTrackerEngine *engine,
                                                             QObject *parent)
    : QTrackerBaseRequest<QContactLocalIdFetchRequest>(engine, parent)
    , m_filter(staticCast(request)->filter())
    , m_sorting(staticCast(request)->sorting())
    , m_limit(-1)
    , m_forceNative(false)
{
    QctContactLocalIdFetchRequest *qctRequest = qobject_cast<QctContactLocalIdFetchRequest*>(request);

    if (qctRequest != 0) {
        m_limit = qctRequest->limit();
        m_forceNative = qctRequest->forceNative();
    }
}

QTrackerContactIdFetchRequest::~QTrackerContactIdFetchRequest()
{
}

QString
QTrackerContactIdFetchRequest::buildQuery(QContactManager::Error &error, bool &sortable)
{
    typedef QList<OrderComparator> OrderComparatorList;

    Select select;
    PatternBase base;

    Variable contact = Variable(QLatin1String("contact"));
    select.addProjection(Functions::trackerId.apply(contact));

    // List of OrderComparators for each contact type
    QHash<QString, OrderComparatorList> orderComparators;

    // We need canSort because QContactManager::Error is not precise enough: canSort specifically
    // tells if the sorting step failed
    sortable = true;

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        PatternGroup group;
        Filter filter;
        const QTrackerContactDetailSchema &schema = engine()->schema(contactType);

        // add restrictions from request filters
        QTrackerScalarContactQueryBuilder queryBuilder(schema, engine()->managerUri());

        error = queryBuilder.bindFilter(m_filter, filter);

        if (error != QContactManager::NoError) {
            return QString();
        }

        foreach(const QString &classIri, schema.contactClassIris()) {
            ResourceValue iri = ResourceValue(classIri);
            group.addPattern(contact, Resources::rdf::type::resource(), iri);
        }

        group.setFilter(filter);

        if (not base.isValid()) {
            base = group;
        } else {
            base = Union(base, group);
        }

        if (not m_sorting.isEmpty()) {
            error = queryBuilder.bindSortOrders(m_sorting, orderComparators[contactType]);

            if (error != QContactManager::NoError) {
                sortable = false;
                return QString();
            }
        }
    }

    // Assemble the ORDER BY directives together
    if (not orderComparators.isEmpty()) {
        const QStringList contactTypes = engine()->supportedContactTypes();

        // FIXME: Sorting of than two schemas is not implemented.
        // The reason is that we use something like the following SPARQL statement:
        //
        //   ORDER BY(IF(schema1, properties for schema1, properties for schema 2))
        //
        // We could indeed nest the IF to support more than two schemas, but so far we
        // only have contacts and groups and therefore keep things simple.
        if (contactTypes.isEmpty() || contactTypes.size() > 2) {
            qctWarn("Can't generate SPARQL for sorting if there are more than two schemas. "
                    "Falling back to in-memory sorting.");
            error = QContactManager::NotSupportedError;
            sortable = false;
            return QString();
        }

        if (contactTypes.size() == 1) {
            select.setOrderBy(orderComparators.value(contactTypes.first()));
        } else {
            // contactTypes.size() == 2

            const QString firstType = contactTypes.first();
            const QString secondType = contactTypes.at(1);
            const OrderComparatorList firstOrders = orderComparators.value(firstType);
            const OrderComparatorList secondOrders = orderComparators.value(secondType);

            // Check that the same number of comparators was generated for each schema
            // This should always be the case, since even if detail X is present in schema
            // A but not natively in schema B, it'll be binded as a custom detail for B
            if (firstOrders.size() != secondOrders.size()) {
                qctWarn("INTERNAL ERROR: Not all schemas have the same number of "
                        "comparators. Falling back to in-memory sorting.");
                error = QContactManager::NotSupportedError;
                sortable = false;
                return QString();
            }

            Exists ifCondition;

            foreach (const QString &classIri, engine()->schema(firstType).contactClassIris()) {
                ifCondition.addPattern(contact, Resources::rdf::type::resource(), ResourceValue(classIri));
            }

            // The final comparator list, which combines the comparators from
            // firstOrders and secondOrders
            OrderComparatorList orders;

            for (int i = 0; i < firstOrders.size(); ++i) {
                Select s;

                s.addProjection(Functions::if_.apply(Filter(ifCondition),
                                                     firstOrders.at(i).expression(),
                                                     secondOrders.at(i).expression()));

                orders.append(OrderComparator(Filter(s), firstOrders.at(i).modifier()));
            }

            select.setOrderBy(orders);
        }
    }

    error = QContactManager::NoError;
    select.addRestriction(base);

    if (m_limit >= 0) {
        select.setLimit(m_limit);
    }

    return select.sparql(engine()->selectQueryOptions());
}

void
QTrackerContactIdFetchRequest::run()
{
    if (isCanceled()) {
        return;
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    // canSort tells if native sorting can be achieved
    bool sorted = false;
    const QString query = buildQuery(error, sorted);

    if (sorted && error == QContactManager::NoError) {
        // Native sorting can be done, and query was built successfully
        runNative(query);
    } else if (not sorted && error == QContactManager::NotSupportedError) {
        // Native sorting cannot be achieved, emulate using a normal fetch request
        runEmulated();
    } else {
        // An error happened somewhere not in sorting
        setLastError(error);
    }
}

void
QTrackerContactIdFetchRequest::runNative(const QString &queryString)
{
    QScopedPointer<QSparqlResult> result(runQuery(QSparqlQuery(queryString), SyncQueryOptions));

    if (result.isNull()) {
        // runQuery() called reportError()
        return;
    }

    // We use a QSet since we want to ensure there are no duplicates
    QSet<QContactLocalId> localIds;

    // We use a QSet because we want to eliminate duplicates.
    // In case of unioned QContactDetailFilter (for instance), the same localId
    // may be included multiple times; this is undesirable.
    while(not isCanceled() && result->next()) {
        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowModels)) {
            qDebug() << result->current();
        }

        const uint id = result->value(0).toUInt();

        if (not localIds.contains(id)) {
            localIds.insert(id);
            m_localIds.append(id);
        }
    }
}

void
QTrackerContactIdFetchRequest::runEmulated()
{
    if (m_forceNative) {
        setLastError(QContactManager::NotSupportedError);
        return;
    }

    // We only use emulated version for sorting
    Q_ASSERT(not m_sorting.isEmpty());

    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
        qctWarn(QString::fromLatin1("Warning, using emulated localId fetch request"));
    }


    QContactFetchHint hint;
    // An empty hint means "all details", therefore we pass at least one detail name here
    hint.setDetailDefinitionsHint(QStringList() << m_sorting.first().detailDefinitionName());

    if (m_limit >= 0) {
        hint.setMaxCountHint(m_limit);
    }

    QContactFetchRequest request;
    request.setFetchHint(hint);
    request.setSorting(m_sorting);
    request.setFilter(m_filter);

    QScopedPointer<QTrackerAbstractRequest>(engine()->createRequestWorker(&request))->exec();

    if (request.error() != QContactManager::NoError) {
        setLastError(request.error());
        return;
    }

    int nRows = 0;

    foreach(const QContact &contact, request.contacts()) {
        if ((m_limit >= 0) && (++nRows > m_limit)) {
            break;
        }

        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowModels)) {
            qDebug() << contact.localId();
        }

        // Here we do not need to check for duplicate ids, since the fetch request
        // already does that
        m_localIds.append(contact.localId());
    }
}

void
QTrackerContactIdFetchRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateContactLocalIdFetchRequest(staticCast(engine()->request(this).data()),
                                               m_localIds, error,
                                               QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactidfetchrequest.cpp"
