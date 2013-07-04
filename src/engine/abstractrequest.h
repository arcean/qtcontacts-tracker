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

#ifndef QTRACKERABSTRACTREQUEST_H_
#define QTRACKERABSTRACTREQUEST_H_

#include <qtcontacts.h>

#define QTC_NO_GLOBAL_LOGGER 1
#include <lib/logger.h>
#undef QTC_NO_GLOBAL_LOGGER

#include <lib/sparqlconnectionmanager.h>
#include <QtSparql>
#include <QReadWriteLock>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QContactTrackerEngine;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerAbstractRequest : public QObject
{
    Q_DISABLE_COPY(QTrackerAbstractRequest)
    Q_OBJECT

public: // constants
    static const QSparqlQueryOptions AsyncQueryOptions;
    static const QSparqlQueryOptions SyncQueryOptions;
    static const QSparqlQueryOptions SyncBatchQueryOptions;

public: // typedefs
    enum Dependency {
        NoDependencies    = 0,
        ResourceCache     = 1 << 0,
        GuidAlgorithm     = 1 << 1
    };

    Q_DECLARE_FLAGS(Dependencies, Dependency)

protected: // typedefs
    typedef QMap<int, QContactManager::Error> ErrorMap;

protected: // constructors
    QTrackerAbstractRequest(QContactTrackerEngine *engine, QObject *parent = 0);

public: // destructor
    virtual ~QTrackerAbstractRequest();

public: // attributes
    virtual Dependencies dependencies() const { return ResourceCache; }
    const QContactTrackerEngine * engine() const { return m_engine; }
    QContactTrackerEngine * engine() { return m_engine; }

protected: // abstract API which must be implemented
    virtual void updateRequest(QContactManager::Error error) = 0;
    virtual void run() = 0;

public: // overriddable methods
    /**
     * Runs the request and emits result on end.
     */
    virtual void exec();

    /**
     * Returns \c true if cancel was successful, \c false if not.
     * Needs to support being called from another thread than the one of this object.
     */
    virtual bool cancel();

protected: // internal attributes
    const QctLogger & qctLogger() const { return m_logger; }

    QContactManager::Error lastError() const { return m_lastError; }
    void setLastError(QContactManager::Error error) { m_lastError = error; }

    bool isCanceled() const;
    bool isCancelable() const;

protected: // internal methods
    /**
     * Run a QSparqlQuery
     */
    QSparqlResult * runQuery(const QSparqlQuery &query,
                             const QSparqlQueryOptions &options = AsyncQueryOptions,
                             QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection());

    /**
      * Logs error message and reports requested error code from event loop
      */
    void reportError(const char *message, QContactManager::Error error = QContactManager::UnspecifiedError);
    void reportError(const QString &message, QContactManager::Error error = QContactManager::UnspecifiedError);
    void reportError(const QList<QSparqlError> &error, const QString &details = QString());
    void reportError(const QSparqlError &error, const QString &details = QString());

    bool turnIrreversible();

    static QContactManager::Error translateError(const QSparqlError &error);

private: // fields
    QContactTrackerEngine *const m_engine;
    QctLogger m_logger;

    QReadWriteLock m_cancelableLock;

    QContactManager::Error m_lastError;

    bool m_canceled : 1;
    bool m_cancelable : 1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QTrackerAbstractRequest::Dependencies)

#endif /* QTRACKERABSTRACTREQUEST_H_ */
