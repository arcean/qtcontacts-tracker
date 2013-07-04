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

#include "tasks.h"

#include "engine.h"
#include "guidalgorithm.h"

#include <dao/contactdetailschema.h>
#include <dao/support.h>
#include <lib/logger.h>
#include <lib/resourcecache.h>
#include <lib/sparqlresolver.h>
#include <ontologies.h>

CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////

static QSet<QString>
collectIris(const QMap<QString, QTrackerContactDetailSchema> &schemas)
{
    QSet<QString> iris;

    foreach(const QTrackerContactDetailSchema &schema, schemas) {
        iris += schema.requiredResourceIris();
    }

    return (iris += nco::default_contact_me::iri());
}

QctResourceCacheTask::QctResourceCacheTask(const QMap<QString, QTrackerContactDetailSchema> &schemas,
                                           QObject *parent)
    : QctTask(parent)
    , m_iris(collectIris(schemas).toList())
{
}

QctResourceCacheTask::~QctResourceCacheTask()
{
}

void
QctResourceCacheTask::run()
{
    if (not QctResourceCache::instance().prefill(m_iris)) {
        qctWarn("Cannot prefill resource cache with resources required by detail schemas");
    }

    emit finished(this);
}

////////////////////////////////////////////////////////////////////////////////

QctGuidAlgorithmTask::QctGuidAlgorithmTask(QContactTrackerEngine *engine,
                                           QObject *parent)
    : QctTask(parent)
    , m_engine(engine)
{
}

QctGuidAlgorithmTask::~QctGuidAlgorithmTask()
{
}

void
QctGuidAlgorithmTask::run()
{
    m_engine->guidAlgorithm().callWhenReady(this, SLOT(resolverReady()));
}

void
QctGuidAlgorithmTask::resolverReady()
{
    emit finished(this);
}

////////////////////////////////////////////////////////////////////////////////

QctRequestTask::QctRequestTask(QContactAbstractRequest *request,
                               QTrackerAbstractRequest *worker,
                               QObject *parent)
    : QctTask(parent)
    , m_worker(worker)
{
    Q_ASSERT(m_worker != 0);

    // change the worker's parent for memory management, but even more important to drag
    // the worker into the proper thread when the task is run by the task queue.
    m_worker->setParent(this);

    connect(request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(onStateChanged(QContactAbstractRequest::State)), Qt::DirectConnection);
    connect(request, SIGNAL(destroyed()),
            this, SLOT(onRequestDestroyed()), Qt::DirectConnection);
}

QctRequestTask::~QctRequestTask()
{
}

QTrackerAbstractRequest::Dependencies
QctRequestTask::dependencies() const
{
    return m_worker->dependencies();
}

void
QctRequestTask::run()
{
    Q_ASSERT(QThread::currentThread() == thread());

    m_worker->exec();
}

void
QctRequestTask::onStateChanged(QContactAbstractRequest::State state)
{
    const QctRequestLocker request = m_worker->engine()->request(m_worker);

    // Don't expect anything from Qt memory management.
    if (request.isNull()) {
        return;
    }

    switch(state) {
    case QContactAbstractRequest::FinishedState:
    case QContactAbstractRequest::CanceledState:
        // Avoid that we do cleanup and signal emission twice.
        disconnect(request.data(), SIGNAL(destroyed()),
                   this, SLOT(onRequestDestroyed()));

        // We call requestDestroyed now, so that we remove the worker<->request
        // associations. We must do that, because the worker will be free'd when
        // we emit finished (two lines below), which means we can potentially
        // create a new worker with the same memory address.
        m_worker->engine()->dropRequest(request);
        emit finished(this);
        break;

    default:
        break;
    }
}

void
QctRequestTask::onRequestDestroyed()
{
    emit finished(this);
}
