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

#ifndef QTRACKERCONTACTENGINE_H
#define QTRACKERCONTACTENGINE_H

#include <cubi.h>
#include <qtcontacts.h>
#include <QMutexLocker>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerAbstractRequest;
class QTrackerContactDetailSchema;
class QctGuidAlgorithm;
class QctTask;

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef QMap<QString, QTrackerContactDetailSchema> QTrackerContactDetailSchemaMap;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctRequestLocker
{
public:
    QctRequestLocker(QMutexLocker *locker, QContactAbstractRequest *data)
        : m_locker(locker)
        , m_data(data)
    {
    }

public: // operators
    QctRequestLocker(const QctRequestLocker &other)
        : m_locker(other.m_locker.take())
        , m_data(other.m_data)
    {
        // this actually shall be a transfer constructor
        // too lazy to do the full idiom with proxy struct and operator voodoo
    }

public: // attributes
    QContactAbstractRequest * data() const { return m_data; }
    bool isNull() const { return 0 == data(); }

public: // operators
    QContactAbstractRequest * operator->() const { return data(); }

private:
    mutable QScopedPointer<QMutexLocker> m_locker;
    QContactAbstractRequest *const m_data;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QContactTrackerEngineData;
class QContactTrackerEngine : public QContactManagerEngineV2
{
    Q_OBJECT
    Q_INTERFACES(QtMobility::QContactManagerEngineV2)

protected:
    typedef QMap<int, QContactManager::Error> ErrorMap;

public:
    enum DebugFlag {
        ShowSelects = (1 << 0),
        ShowUpdates = (1 << 1),
        ShowModels = (1 << 2),
        ShowNotes = (1 << 3),
        ShowTiming = (1 << 4),
        ShowSignals = (1 << 5),

        SkipSecurityChecks = (1 << 20),
        SkipNagging = (1 << 21)
    };

    Q_DECLARE_FLAGS(DebugFlags, DebugFlag)

    enum TaskQueue {
        AsyncTaskQueue,
        SyncTaskQueue
    };

    static const int DefaultRequestTimeout = 0; // infinite
    static const int DefaultTrackerTimeout = 30 * 1000; // 30 seconds
    static const int DefaultCoalescingDelay = 10; // 10 milliseconds
    static const int DefaultBatchSize = 25;
    static const int DefaultGCLimit = 100;
    static const QString DefaultSyncTarget;
    static const QStringList DefaultWeakSyncTargets;

    QContactTrackerEngine(const QMap<QString, QString>& parameters,
                          const QString& managerName = QLatin1String("tracker"),
                          int interfaceVersion = -1, QObject *parent = 0);
    virtual ~QContactTrackerEngine();

    QContactTrackerEngine(const QContactTrackerEngine& other);
    QContactTrackerEngine& operator=(const QContactTrackerEngine& other);

    /* sync methods, wrapping async methods & waitForFinished */
    QList<QContactLocalId> contactIds(const QList<QContactSortOrder>& sortOrders, QContactManager::Error* error) const; // XXX FIXME: no longer part of engine API.
    QList<QContactLocalId> contactIds(const QContactFilter& filter, const QList<QContactSortOrder>& sortOrders, QContactManager::Error* error) const;
    QList<QContact> contacts(const QContactFilter& filter, const QList<QContactSortOrder>& sortOrders, const QContactFetchHint& fetchHint, QContactManager::Error* error) const;
    QList<QContact> contacts(const QList<QContactLocalId> &localIds, const QContactFetchHint &fetchHint, ErrorMap *errorMap, QContactManager::Error *error) const;
    QContact contact(const QContactLocalId& contactId, const QContactFetchHint& fetchHint, QContactManager::Error* error) const;
    QContact contactImpl(const QContactLocalId& contactId, const QContactFetchHint& fetchHint, QContactManager::Error* error) const;
    QList<QContactRelationship> relationships(const QString&, const QContactId&, QContactRelationship::Role, QContactManager::Error* error) const;

    /* Save contacts - single and in batch */
    bool saveContact( QContact* contact, QContactManager::Error* error);
    bool saveContacts(QList<QContact>* contacts, ErrorMap* errorMap, QContactManager::Error* error);
    bool saveContacts(QList<QContact> *contacts, const QStringList &definitionMask, ErrorMap *errorMap, QContactManager::Error *error);

    bool removeContact(const QContactLocalId& contactId, QContactManager::Error* error);
    bool removeContacts(const QList<QContactLocalId>& contactIds, ErrorMap* errorMap, QContactManager::Error* error) ;

    /* Save relationships - single and in batch */
    bool saveRelationship(QContactRelationship *relationship, QContactManager::Error *error);
    bool saveRelationships(QList<QContactRelationship> *relationships, ErrorMap *errorMap, QContactManager::Error* error);

    bool removeRelationship(const QContactRelationship &relationship, QContactManager::Error *error);
    bool removeRelationships(const QList<QContactRelationship> &relationships, ErrorMap *errorMap, QContactManager::Error *error);

    /* Definitions - Accessors and Mutators */
    QMap<QString, QContactDetailDefinition> detailDefinitions(const QString& contactType, QContactManager::Error* error) const;
    QContactDetailDefinition detailDefinition(const QString&, const QString&, QContactManager::Error* error) const;
    bool saveDetailDefinition(const QContactDetailDefinition &definition,  const QString &contactType, QContactManager::Error *error);
    bool removeDetailDefinition(const QString &definitionName, const QString &contactType, QContactManager::Error *error);

    /* Self contact */
    bool setSelfContactId(const QContactLocalId&, QContactManager::Error* error);
    QContactLocalId selfContactId(QContactManager::Error* error) const;

    /* Asynchronous Request Support */
    void requestDestroyed(QContactAbstractRequest* req);
    bool startRequest(QContactAbstractRequest* req);
    bool waitForRequestFinished(QContactAbstractRequest* req, int msecs);
    bool waitForRequestFinishedImpl(QContactAbstractRequest* req, int msecs);
    bool cancelRequest(QContactAbstractRequest *request);

    /* Capabilities reporting */
    QMap<QString, QString> managerParameters() const;
    QString managerName() const;
    int managerVersion() const;

    bool hasFeature(QContactManager::ManagerFeature feature, const QString& contactType) const;
    bool isFilterSupported(const QContactFilter& filter) const;
    QList<QVariant::Type> supportedDataTypes() const;
    QStringList supportedContactTypes() const;
    bool isRelationshipTypeSupported(const QString &relationshipType, const QString &contactType) const;

    /* Synthesise the display label of a contact */
    QString synthesizedDisplayLabel(const QContact& contact, QContactManager::Error* error) const;

    QContact compatibleContact(const QContact&, QContactManager::Error* error) const;

    /* XXX TODO: pure virtual unimplemented functions! */
    bool validateContact(const QContact&, QContactManager::Error* error) const;
    bool validateDefinition(const QContactDetailDefinition&, QContactManager::Error* error) const;

public: // custom attributes
    /// does not do any checks, e.g. on the thread the request belongs to,
    /// so can be used for internal requests (where requests belong to worker thread)
    QTrackerAbstractRequest * createRequestWorkerImpl(QContactAbstractRequest *request);
    const QTrackerContactDetailSchemaMap & schemas() const;
    const QTrackerContactDetailSchema & schema(const QString& contactType) const;

    const DebugFlags & debugFlags() const;
    bool hasDebugFlag(DebugFlag flag) const;
    int concurrencyLevel() const;
    int batchSize() const;
    int gcLimit() const;
    int requestTimeout() const;
    int trackerTimeout() const;
    int coalescingDelay() const;
    QctGuidAlgorithm & guidAlgorithm() const;
    const QString & syncTarget() const;
    const QStringList & weakSyncTargets() const;
    bool mangleAllSyncTargets() const;
    bool isRestrictive() const;

    Cubi::Options::SparqlOptions selectQueryOptions() const;
    Cubi::Options::SparqlOptions updateQueryOptions() const;

public: // custom methods
    QTrackerAbstractRequest * createRequestWorker(QContactAbstractRequest *request);
    QctRequestLocker request(const QTrackerAbstractRequest *worker) const;
    void dropRequest(const QctRequestLocker &req);

    QContactFetchHint normalizedFetchHint(QContactFetchHint fetchHint, const QString &nameOrder);
    void updateDisplayLabel(QContact &contact, const QString &nameOrder) const;
    /// creates display label for contact, using the generator-list given by @param nameOrder,
    /// which is the default one if @param nameOrder is an empty string
    QString createDisplayLabel(const QContact &contact, const QString &nameOrder = QString()) const;
    void updateAvatar(QContact &contact);
    bool isWeakSyncTarget(const QString &syncTarget) const;
    QString gcQueryId() const;
    QString cleanupQueryString() const;

protected:
    void connectNotify(const char *signal);

private slots:
    void onRequestDestroyed(QObject *obj = 0);

private:
    /// Checks if the process requested all required security tokens.
    bool checkSecurityTokens(QContactAbstractRequest *request);

    bool checkThreadOfRequest(QContactAbstractRequest *request) const;

    /// Creates and queues a new task for the \p request.
    /// Note that the return value of this method doesn't indicate success or failure of this
    /// operation. For instance this method will return 0 when the client managed to delete
    /// the request from some slot or some other thread.
    QctTask *startRequestImpl(QContactAbstractRequest *request, TaskQueue queue);

    /// Starts the request and blocks until the request is completed in a worker thread.
    bool runSyncRequest(QContactAbstractRequest *request, QContactManager::Error *error) const;

    void connectSignals();
    void disconnectSignals();

    void registerGcQuery();

private:
    QExplicitlySharedDataPointer<QContactTrackerEngineData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_DECLARE_OPERATORS_FOR_FLAGS(QContactTrackerEngine::DebugFlags)

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTENGINE_H
