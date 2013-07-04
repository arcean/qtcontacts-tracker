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

#ifndef QTRACKERCONTACTENGINE_P_H
#define QTRACKERCONTACTENGINE_P_H

#include "abstractrequest.h"
#include "guidalgorithm.h"
#include "engine.h"

#include <dao/contactdetailschema.h>

#include <lib/queue.h>

#include <QtCore>

typedef QMap<QString, QContactDetailDefinitionMap> CustomContactDetailMap;

class QctTrackerChangeListener;
class QctTrackerIdResolver;

class QContactTrackerEngineParameters
{
public:
    QContactTrackerEngineParameters(const QMap<QString, QString> &parameters,
                                    const QString &engineName, int engineVersion);

public: // fields
    QString m_engineName;
    int m_engineVersion;
    int m_requestTimeout;
    int m_trackerTimeout;
    int m_coalescingDelay;
    int m_concurrencyLevel;
    int m_batchSize;
    int m_gcLimit;

    QString m_syncTarget;
    QStringList m_weakSyncTargets;

    QctGuidAlgorithm *m_guidAlgorithm;

    QMap<QString, QString> m_parameters;
    QMap<QString, QString> m_managerParameters;

    QContactTrackerEngine::DebugFlags m_debugFlags;

    Cubi::Options::SparqlOptions m_selectQueryOptions;
    Cubi::Options::SparqlOptions m_updateQueryOptions;

    QMap<QString, QTrackerContactDetailSchema> m_detailSchemas;

    bool m_omitPresenceChanges : 1;
    bool m_mangleAllSyncTargets : 1;
    bool m_isRestrictive : 1;
};

class QContactTrackerEngineData : public QSharedData
{
    // Using void pointer for the key since it might point onto an already destructed object.
    typedef QList<QTrackerAbstractRequest*> RequestList;
    typedef QContactTrackerEngine::TaskQueue TaskQueue;

public: // constructor/destructor
    explicit QContactTrackerEngineData(const QMap<QString, QString> &parameters,
                                       const QString &engineName, int engineVersion);
    virtual ~QContactTrackerEngineData();

public: // operators
    QContactTrackerEngineData(const QContactTrackerEngineData &other);

public: // parameters
    const QContactTrackerEngineParameters m_parameters;

public: // caches
    QList<QVariant::Type> m_supportedDataTypes;
    uint m_selfContactId;

public: // state
    QctTrackerChangeListener *m_changeListener;

    QHash<const QContactAbstractRequest*, QTrackerAbstractRequest*> m_workersByRequest;
    QHash<const QTrackerAbstractRequest*, QContactAbstractRequest*> m_requestsByWorker;
    QReadWriteLock m_tableLock;
    QMutex m_requestLifeGuard;

    void enqueueTask(QctTask *task, TaskQueue queue);

    CustomContactDetailMap m_customDetails;

    QTrackerAbstractRequest::Dependencies m_satisfiedDependencies;

    QString m_gcQueryId;

    bool m_mandatoryTokensFound : 1;

private: // fields
    QctQueue *m_asyncQueue;
    QctQueue *m_syncQueue;
};

/*!
 * A helper used to exit request event loops for the the synchronous API
 * and for the misguided users of waitForFinished facility.
 */
class RequestEventLoop : public QEventLoop
{
    Q_OBJECT

public: // constructors/destructors
    explicit RequestEventLoop(QContactAbstractRequest *request, int timeout);

public: // attributes
    bool isFinished() const { return m_finished; }

public slots:
    /// Quits the event loop when called.
    void stateChanged(QContactAbstractRequest::State newState);
    void requestDone();

private: // fields
    bool m_finished;
};

///////////////////////////////////////////////////////////////////////////////

class QctTaskWaiter : public QObject
{
    Q_OBJECT

public: // constructors/destructors
    explicit QctTaskWaiter(QctTask *task, QObject *parent = 0);

public: // methods
    bool wait(int timeout);

private slots:
    void onTaskFinished();
    void onTaskDestroyed();

private: // members
    QWaitCondition m_waitCondition;
    QMutex m_mutex;

    QctTask *m_task;

    bool m_finished : 1;
};

#endif // QTRACKERCONTACTENGINE_P_H
