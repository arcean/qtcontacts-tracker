/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "sparqlresolver.h"

#include "logger.h"
#include "resourcecache.h"
#include "sparqlconnectionmanager.h"

#include <ontologies.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////////////////////////

const int QctSparqlResolver::ColumnLimit = 250;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctSparqlResolverData : public QSharedData
{
    friend class QctSparqlResolver;

public:
    virtual ~QctSparqlResolverData() {}

protected: // fields
    QList<QSparqlError> m_errors;

private: // fields
    QList<QSparqlResult *> m_sparqlResults;
    QStringList m_classIris;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctTrackerIdResolverData : public QctSparqlResolverData
{
    friend class QctTrackerIdResolver;

private: // constructors
    explicit QctTrackerIdResolverData(const QStringList &resourceIris);

private: // fields
    const QStringList m_resourceIris;
    QList<uint> m_trackerIds;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctTrackerIriResolverData : public QctSparqlResolverData
{
    friend class QctResourceIriResolver;

private: // constructors
    explicit QctTrackerIriResolverData(const QList<uint> &trackerIds);

private: // fields
    const QList<uint> m_trackerIds;
    QStringList m_resourceIris;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctSparqlResolver::QctSparqlResolver(QctSparqlResolverData *data, QObject *parent)
    : QObject(parent)
    , d(data)
{
}

QctSparqlResolver::~QctSparqlResolver()
{
    qDeleteAll(d->m_sparqlResults);
    d->m_sparqlResults.clear();
}

QList<QSparqlQuery>
QctSparqlResolver::makeQueries()
{
    QList<QSparqlQuery> queries;

    if (classIris().isEmpty()) {
        // Build highly efficient query that only touches tracker caches
        // if no resource type restrictions are given.
        const QList<Projection> projections = makeProjections();

        for(int i = 0, c = projections.count(); i < c; ) {
            Select select;

            for(const int l = qMin(i + ColumnLimit, c); i < l; ++i) {
                select.addProjection(projections.at(i));
            }

            queries += QSparqlQuery(select.sparql());
        }
    } else {
        // Build generic less efficient query with resource type checks.
        queries = makeRestrictedQueries();
    }

    return queries;
}

bool
QctSparqlResolver::lookupAndWait()
{
    // get and check sparql connection
    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn("Cannot run resolver: No valid QtSparql connection.");
        return false;
    }

    // build queries
    const QList<QSparqlQuery> queries = makeQueries();

    // fast track if no queries are needed
    if (queries.isEmpty()) {
        if (not classIris().isEmpty()) {
            buildRestrictedResult(0);
        }

        return true;
    }

    // process query results
    int offset = 0;

    foreach(const QSparqlQuery &query, queries) {
        QScopedPointer<QSparqlResult> result(connection.syncExec(query));

        if (result->hasError()) {
            qctWarn(qPrintable(qctTruncate(result->lastError().message())));
            return false;
        }

        if (classIris().isEmpty()) {
            while(result->next()) {
                buildResult(offset, result.data());
                offset += result->current().count();
            }
        } else {
            buildRestrictedResult(result.data());
        }
    }

    return true;
}

bool
QctSparqlResolver::lookup()
{
    if (QThread::currentThread() != thread()) {
        qctWarn(QString::fromLatin1("lookup() must be called from same thread which created the %1").
                arg(QLatin1String(metaObject()->className())));
        return false;
    }

    const QList<QSparqlQuery> resolverQueries = makeQueries();

    // nothing to do
    if (resolverQueries.isEmpty()) {
        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
        return true;
    }

    // get and check sparql connection
    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn("Cannot run resolver: No valid QtSparql connection.");
        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
        return false;
    }

    // build lookup queries
    foreach(const QSparqlQuery &query, resolverQueries) {
        QScopedPointer<QSparqlResult> result(connection.exec(query));
        connect(result.data(), SIGNAL(finished()), SLOT(onResultFinished()));

        if (result->hasError()) {
            qctWarn(qPrintable(qctTruncate(result->lastError().message())));
            return false;
        }

        result->setParent(this);
        d->m_sparqlResults += result.take();
    }

    if (isFinished()) {
        parseResults();
        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

    return true;
}

void
QctSparqlResolver::setClassIris(const QStringList &classIris)
{
    d->m_classIris = classIris;
}

const QStringList &
QctSparqlResolver::classIris() const
{
    return d->m_classIris;
}

bool
QctSparqlResolver::isFinished() const
{
    // Check that all results are ready.
    // Check from back because last results usually shall finish last.
    for(int i = d->m_sparqlResults.count() - 1; i >= 0; --i) {
        if (not d->m_sparqlResults.at(i)->isFinished()) {
            return false;
        }
    }

    return true;
}

const QList<QSparqlError> &
QctSparqlResolver::errors() const
{
    return d->m_errors;
}

void
QctSparqlResolver::parseResults()
{
    // store the results
    if (classIris().isEmpty()) {
        int offset = 0;

        foreach(QSparqlResult *result, d->m_sparqlResults) {
            while(result->next()) {
                buildResult(offset, result);
                offset += result->current().count();
            }

            result->deleteLater();
        }
    } else {
        buildRestrictedResult(d->m_sparqlResults.first());
        d->m_sparqlResults.first()->deleteLater();
    }

    d->m_sparqlResults.clear();
}

void
QctSparqlResolver::onResultFinished()
{
    QSparqlResult *result = qobject_cast<QSparqlResult*>(sender());

    // result can be null if you call the function directly
    if (result != 0 && result->hasError()) {
        d->m_errors.append(result->lastError());
    }

    if (isFinished()) {
        parseResults();
        emit finished();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QctTrackerIdResolverData::QctTrackerIdResolverData(const QStringList &resourceIris)
    : m_resourceIris(resourceIris)
{
    m_trackerIds.reserve(resourceIris.count());
}

QctTrackerIdResolver::QctTrackerIdResolver(const QStringList &resourceIris, QObject *parent)
    : QctSparqlResolver(new QctTrackerIdResolverData(resourceIris), parent)
{
}

QctTrackerIdResolver::~QctTrackerIdResolver()
{
}

const QStringList &
QctTrackerIdResolver::resourceIris() const
{
    return data()->m_resourceIris;
}

const QList<uint> &
QctTrackerIdResolver::trackerIds() const
{
    return data()->m_trackerIds;
}

void
QctTrackerIdResolver::buildResult(int offset, const QSparqlResult *result)
{
    QctResourceCache &resourceCache = QctResourceCache::instance();
    const QStringList &resourceIris = data()->m_resourceIris;
    QList<uint> &trackerIds = data()->m_trackerIds;
    int columnCount = result->current().count();

    for(int column = 0, ri = offset; column < columnCount; ++column, ++ri) {
        // skip already known tracker ids
        while (0 != trackerIds.at(ri)) {
            ++ri;
        }

        const uint id = result->value(column).toUInt();
        resourceCache.insert(resourceIris.at(ri), id);
        trackerIds[ri] = id;
    }
}

QList<QSparqlQuery>
QctTrackerIdResolver::makeRestrictedQueries()
{
    // build restriction
    Variable resource(QLatin1String("r"));
    ValueList resourceIriValues;
    PatternGroup restriction;

    foreach(const QString &iri, classIris()) {
        restriction.addPattern(resource, rdf::type::resource(), ResourceValue(iri));
    }

    foreach(const QString &iri, resourceIris()) {
        if (not iri.isEmpty()) {
            resourceIriValues.addValue(LiteralValue(iri));
        }
    }

    QList<QSparqlQuery> result;

    if (not resourceIriValues.values().isEmpty()) {
        restriction.setFilter(Filter(Functions::in.apply(Functions::str.apply(resource),
                                                         resourceIriValues)));

        // compose the query
        Select query;

        query.addProjection(Functions::str.apply(resource));
        query.addProjection(Functions::trackerId.apply(resource));
        query.addRestriction(restriction);

        result += QSparqlQuery(query.sparql());
    }

    return result;
}

QList<Projection>
QctTrackerIdResolver::makeProjections()
{
    const QctResourceCache &resourceCache = QctResourceCache::instance();
    QList<uint> &trackerIds = data()->m_trackerIds;
    QList<Projection> projections;

    foreach(const QString &iri, data()->m_resourceIris) {
        const uint trackerId = resourceCache.trackerId(iri);

        if (0 == trackerId) {
            if (not iri.isEmpty()) {
                projections += Functions::trackerId.apply(ResourceValue(iri));
            } else {
                projections += LiteralValue(0);
            }
        }

        trackerIds += trackerId;
    }

    return projections;
}

void
QctTrackerIdResolver::buildRestrictedResult(QSparqlResult *result)
{
    QctResourceCache &resourceCache = QctResourceCache::instance();
    QHash<QString, uint> localCache;

    if (result) {
        while(result->next()) {
            bool validId = false;
            const int id = result->stringValue(1).toInt(&validId);
            const QString &iri = result->stringValue(0);

            if (validId) {
                resourceCache.insert(iri, id);
                localCache.insert(iri, id);
            }
        }
    }

    // Read results from local cache to avoid reporting IRIs for deleted resources,
    // but also don't remove IRIs of deleted resources from global cache to avoid trashing:
    // Tracker keeps IRI-id-mappings forever, so if we'd delete some value here, the next
    // unrestricted resolver run would reinsert the just deleted value.
    QList<uint> &trackerIds = data()->m_trackerIds;

    foreach(const QString &iri, resourceIris()) {
        trackerIds += localCache.value(iri);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QctTrackerIriResolverData::QctTrackerIriResolverData(const QList<uint> &trackerIds)
    : m_trackerIds(trackerIds)
{
    m_resourceIris.reserve(trackerIds.count());
}

QctResourceIriResolver::QctResourceIriResolver(const QList<uint> &trackerIds, QObject *parent)
    : QctSparqlResolver(new QctTrackerIriResolverData(trackerIds), parent)
{
}

QctResourceIriResolver::~QctResourceIriResolver()
{
}

const QStringList &
QctResourceIriResolver::resourceIris() const
{
    return data()->m_resourceIris;
}

const QList<uint> &
QctResourceIriResolver::trackerIds() const
{
    return data()->m_trackerIds;
}

QList<Projection>
QctResourceIriResolver::makeProjections()
{
    const QctResourceCache &resourceCache = QctResourceCache::instance();
    QStringList &resourceIris = data()->m_resourceIris;
    QList<Projection> projections;

    foreach(const uint &id, data()->m_trackerIds) {
        if (0 == id) {
            resourceIris += QString();
        } else {
            const QString &iri = resourceCache.resourceIri(id);

            if (iri.isEmpty()) {
                projections += Functions::trackerUri.apply(LiteralValue(id));
            }

            resourceIris += iri;
        }
    }

    return projections;
}

void
QctResourceIriResolver::buildResult(int offset, const QSparqlResult *result)
{
    QctResourceCache &resourceCache = QctResourceCache::instance();
    const QList<uint> &trackerIds = data()->m_trackerIds;
    QStringList &resourceIris = data()->m_resourceIris;
    const int columnCount = result->current().count();

    for(int column = 0, ri = offset; column < columnCount; ++column, ++ri) {
        // skip already known resource IRIs
        while(not resourceIris.at(ri).isEmpty()) {
            ++ri;
        }

        const QString iri = result->stringValue(column);
        resourceCache.insert(iri, trackerIds.at(ri));
        resourceIris[ri] = iri;
    }
}

QList<QSparqlQuery>
QctResourceIriResolver::makeRestrictedQueries()
{
    // build restriction
    Variable resource(QLatin1String("r"));
    ValueList trackerIdValues;
    PatternGroup restriction;

    foreach(const QString &iri, classIris()) {
        restriction.addPattern(resource, rdf::type::resource(), ResourceValue(iri));
    }

    foreach(uint id, trackerIds()) {
        if (id != 0) {
            trackerIdValues.addValue(LiteralValue(id));
        }
    }

    QList<QSparqlQuery> result;

    if (not trackerIdValues.values().isEmpty()) {
        // compose the query
        Select query;

        restriction.setFilter(Filter(Functions::in.apply(Functions::trackerId.apply(resource),
                                                         trackerIdValues)));

        query.addProjection(Functions::trackerId.apply(resource));
        query.addProjection(Functions::str.apply(resource));
        query.addRestriction(restriction);

        result += QSparqlQuery(query.sparql());
    }

    return result;
}

void
QctResourceIriResolver::buildRestrictedResult(QSparqlResult *result)
{
    QctResourceCache &resourceCache = QctResourceCache::instance();
    QHash<uint, QString> localCache;

    if (result) {
        while(result->next()) {
            bool validId = false;
            const int id = result->stringValue(0).toInt(&validId);
            const QString &iri = result->stringValue(1);

            if (validId) {
                resourceCache.insert(iri, id);
                localCache.insert(id, iri);
            }
        }
    }

    // Read results from local cache to avoid reporting IRIs for deleted resources,
    // but also don't remove IRIs of deleted resources from global cache to avoid trashing:
    // Tracker keeps IRI-id-mappings forever, so if we'd delete some value here, the next
    // unrestricted resolver run would reinsert the just deleted value.
    QStringList &resourceIris = data()->m_resourceIris;

    foreach(uint id, trackerIds()) {
        resourceIris += localCache.value(id);
    }
}
