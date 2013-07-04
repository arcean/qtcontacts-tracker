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

// Needs to go on top due to clash between GIO and "signals" keyword.
#include <tracker-sparql.h>

#include "abstractrequest.h"

#include <engine/engine.h>
#include <lib/threadutils.h>

#include <ontologies/nco.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

static QString
makePrefix(QContactTrackerEngine *engine)
{
    QString prefix;

    if (0 != engine) {
        prefix = engine->managerUri();
    }

    // strip common URI prefix
    static const QString commonPrefix = QLatin1String("qtcontacts:tracker:");

    if (prefix.startsWith(commonPrefix)) {
        prefix = prefix.mid(commonPrefix.length());
    }

    return prefix;
}

static QContactTrackerEngine::DebugFlag
queryDebugFlag(const QSparqlQuery &query)
{
    switch(query.type()) {
    case QSparqlQuery::InsertStatement:
    case QSparqlQuery::DeleteStatement:
        return QContactTrackerEngine::ShowUpdates;

    default:
        break;
    }

    return QContactTrackerEngine::ShowSelects;
}

QContactManager::Error
QTrackerAbstractRequest::translateError(const QSparqlError& error)
{
    switch (error.number()) {
    case TRACKER_SPARQL_ERROR_NO_SPACE:
            return QContactManager::OutOfMemoryError;

    case TRACKER_SPARQL_ERROR_UNSUPPORTED:
        return QContactManager::NotSupportedError;

    default:
        return QContactManager::UnspecifiedError;
    }
}

static QSparqlQueryOptions
makeQueryOptions(QSparqlQueryOptions::ExecutionMethod executionMethod,
                 QSparqlQueryOptions::Priority priority)
{
    QSparqlQueryOptions options;
    options.setExecutionMethod(executionMethod);
    options.setPriority(priority);
    return options;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const QSparqlQueryOptions QTrackerAbstractRequest::AsyncQueryOptions =
makeQueryOptions(QSparqlQueryOptions::AsyncExec, QSparqlQueryOptions::NormalPriority);

const QSparqlQueryOptions QTrackerAbstractRequest::SyncQueryOptions =
makeQueryOptions(QSparqlQueryOptions::SyncExec, QSparqlQueryOptions::NormalPriority);

const QSparqlQueryOptions QTrackerAbstractRequest::SyncBatchQueryOptions =
makeQueryOptions(QSparqlQueryOptions::SyncExec, QSparqlQueryOptions::LowPriority);

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerAbstractRequest::QTrackerAbstractRequest(QContactTrackerEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_logger(makePrefix(engine))
    , m_lastError(QContactManager::NoError)
    , m_canceled(false)
    , m_cancelable(true)
{
    if (0 == m_engine) {
        qctFail("No engine passed to request worker");
    }

    m_logger.setShowLocation(::qctLogger().showLocation());
}

QTrackerAbstractRequest::~QTrackerAbstractRequest()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
QTrackerAbstractRequest::exec()
{
    run();

    if (isCanceled()) {
        QContactManagerEngine::updateRequestState(engine()->request(this).data(),
                                                  QContactAbstractRequest::CanceledState);
    } else {
        updateRequest(m_lastError);
    }
}

bool
QTrackerAbstractRequest::cancel()
{
    QCT_SYNCHRONIZED_READ(&m_cancelableLock);

    if (isCancelable()) {
        m_canceled = true;
    }

    return m_canceled;
}

bool
QTrackerAbstractRequest::isCanceled() const
{
    return m_canceled;
}

bool
QTrackerAbstractRequest::turnIrreversible()
{
    QCT_SYNCHRONIZED_WRITE(&m_cancelableLock);

    if (isCanceled()) {
        return false;
    }

    m_cancelable = false;

    return true;
}

bool
QTrackerAbstractRequest::isCancelable() const
{
    return m_cancelable;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QSparqlResult *
QTrackerAbstractRequest::runQuery(const QSparqlQuery &query,
                                  const QSparqlQueryOptions &options,
                                  QSparqlConnection &connection)
{
    if (not connection.isValid()) {
        reportError(QLatin1String("No valid QtSparql connection."));
        return 0;
    }

    if (engine()->hasDebugFlag(queryDebugFlag(query))) {
        qDebug() << query.query();
    }

    QScopedPointer<QSparqlResult> result(connection.exec(query, options));

    if (result->hasError()) {
        reportError(result->lastError());
        return 0;
    }

    if (options.executionMethod() == QSparqlQueryOptions::SyncExec) {
        static bool warningNotShownYet = true;

        if (result->hasFeature(QSparqlResult::Sync)) {
            warningNotShownYet = true;
        } else if (warningNotShownYet) {
            qctWarn(QString::fromLatin1("QtSparql driver %1 doesn't support synchronous data access. "
                                        "Expect significantly increased memory consumption from fallback "
                                        "implementation. Consider using a different QtSparql driver.").
                    arg(connection.driverName()));

            warningNotShownYet = false;
        }

        result->setParent(this);
    } else {
        connect(result.data(), SIGNAL(finished()),
                result.data(), SLOT(deleteLater()));
    }

    return result.take();
}

void QTrackerAbstractRequest::reportError(const char *message, QContactManager::Error error)
{
    reportError(QLatin1String(message), error);
}

void
QTrackerAbstractRequest::reportError(const QString &message, QContactManager::Error error)
{
    qctWarn(QString::fromLatin1("%1 failed: %2").
            arg(QLatin1String(metaObject()->className()),
                qctTruncate(message)));

    setLastError(error);
}

void
QTrackerAbstractRequest::reportError(const QList<QSparqlError> &errors, const QString &details)
{
    if (not details.isEmpty()) {
        qctWarn(QString::fromLatin1("%1 failed: %2").
                arg(QLatin1String(metaObject()->className()), details));
    }

    setLastError(QContactManager::UnspecifiedError);

    foreach(const QSparqlError &e, errors) {
        qctWarn(QString::fromLatin1("%1 failed: %2").
                arg(QLatin1String(metaObject()->className()),
                    qctTruncate(e.message())));

        const QContactManager::Error error = translateError(e);

        if (QContactManager::OutOfMemoryError == error) {
            setLastError(error);
        } else if (lastError() != error && lastError() == QContactManager::UnspecifiedError) {
            setLastError(error);
        }
    }
}

void
QTrackerAbstractRequest::reportError(const QSparqlError &error, const QString &details)
{
    const QString message = details % QLatin1String(details.isEmpty() ? "": ": ") % error.message();
    reportError(message, translateError(error));
}
