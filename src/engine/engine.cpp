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

#include "engine.h"
#include "engine_p.h"

#include "contactfetchrequest.h"
#include "contactfetchbyidrequest.h"
#include "contactidfetchrequest.h"
#include "contactcopyandremoverequest.h"
#include "contactunmergerequest.h"
#include "displaylabelgenerator.h"
#include "relationshipfetchrequest.h"
#include "relationshipremoverequest.h"
#include "relationshipsaverequest.h"
#include "detaildefinitionfetchrequest.h"
#include "tasks.h"

#include <dao/contactdetail.h>
#include <dao/contactdetailschema.h>
#include <dao/scalarquerybuilder.h>
#include <dao/scalarquerybuilder.h>
#include <dao/support.h>

#include <lib/constants.h>
#include <lib/contactmergerequest.h>
#include <lib/customdetails.h>
#include <lib/garbagecollector.h>
#include <lib/presenceutils.h>
#include <lib/queue.h>
#include <lib/settings.h>
#include <lib/threadutils.h>
#include <lib/sparqlresolver.h>
#include <lib/threadlocaldata.h>
#include <lib/trackerchangelistener.h>
#include <lib/unmergeimcontactsrequest.h>

#include <qcontactdetails.h>

#include <ontologies/nco.h>
#include <QElapsedTimer>

#ifdef ENABLE_CREDENTIALS
#include <CredentialsManager>
#endif // ENABLE_CREDENTIALS

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE_RESOURCES
QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

#define REPORT_UNSUPPORTED_FUNCTION(error) do {\
    qctWarn(QString::fromLatin1("Method not implemented yet: %1"). \
            arg(QLatin1String(Q_FUNC_INFO))); \
    qctPropagate(QContactManager::NotSupportedError, error); \
} while(0)

////////////////////////////////////////////////////////////////////////////////////////////////////

static QContactManager::Error *const noErrorResult = 0;
const QString QContactTrackerEngine::DefaultSyncTarget = QContactSyncTarget__SyncTargetAddressBook;
const QStringList QContactTrackerEngine::DefaultWeakSyncTargets = QStringList() << QContactSyncTarget__SyncTargetTelepathy;

////////////////////////////////////////////////////////////////////////////////////////////////////

// added here as list was not working because of
// [warn ] QObject::connect: Cannot queue arguments of type 'QContactAbstractRequest::State'
// (Make sure 'QContactAbstractRequest::State' is registered using qRegisterMetaType().)
Q_DECLARE_METATYPE(QContactAbstractRequest::State)

////////////////////////////////////////////////////////////////////////////////////////////////////

static QMap<QString, QString>
readEnvironment()
{
    const QStringList tokenList(QProcessEnvironment::systemEnvironment().
                                value(QLatin1String("QT_CONTACTS_TRACKER")).
                                remove(QLatin1Char(' ')).
                                split(QLatin1String(";")));

    QMap<QString, QString> parameters;

    foreach(const QString &token, tokenList) {
        const int eq = token.indexOf(QLatin1Char('='));
        const QString key((eq >= 0 ? token.left(eq) : token).trimmed());
        const QString value((eq >= 0 ? token.mid(eq + 1).trimmed() : QString()));

        if (not key.isEmpty()) {
            parameters.insert(key, value);
        }
    }

    return parameters;
}

static void
parseParameter(int &result, const QString &key, const QString &text)
{
    bool ok = false;
    const int value = text.toInt(&ok);

    if (ok) {
        result = value;
    } else {
        qctWarn(QString::fromLatin1("Invalid value for %1 argument: %2").arg(key, text));
    }
}

/*!
 * \class QContactTrackerEngineParameters
 * \brief This class describes the various parameters you can pass at engine creation time
 * <table>
 * <tr>
 *  <td>timeout</td>
 *  <td>Timeout (in ms) after which sync requests should be canceled. 0 for no timeout.<br/>
 *      Default value 0.</td>
 * </tr>
 * <tr>
 *  <td>tracker-timeout</td>
 *  <td>Unused</td>
 * </tr>
 * <tr>
 *  <td>coalescing-delay</td>
 *  <td>Delay to wait (in ms) before emitting one of the contactsAdded/Changed/Removed (or
 *      relationshipsAdded/Removed) signals, to get a chance to coalesce them together.<br/>
 *      Default value: 0.</td>
 * </tr>
 * <tr>
 *  <td>writeback</td>
 *  <td>Controls which details are written to Tracker on saving.<br/>
 *      Valid values: "", a comma separated list of detail names, or "all"<br/>
 *      Default value: all details except QContactPresence</td>
 * </tr>
 * <tr>
 *  <td>debug</td>
 *  <td>Controls what debug messages are printed on the console<br/>
 *      Valid values (separated by ","):
 *      <ul>
 *       <li>queries: print all queries (SELECT and UPDATE)</li>
 *       <li>selects: print all SELECT queries</li>
 *       <li>updates: print all UPDATE queries</li>
 *       <li>models: print the computed result set after a SELECT</li>
 *       <li>timing: print timing information about requests</li>
 *       <li>signals: print debug information about change signals</li>
 *       <li>no-sec: disable security credentials checks</li>
 *       <li>no-nagging: don't print warning messages when using waitForFinished()</li>
 *      </ul>
 *      Default value: ""
 *  </td>
 * </tr>
 * <tr>
 *  <td>contact-types</td>
 *  <td>Filters the type of contacts fetched by the engine<br/>
 *      Valid values (separated by ","): any value of QContactType::type()<br/>
 *      Default values: "" (fetch all types)</td>
 * </tr>
 * <tr>
 *  <td>concurrency</td>
 *  <td>Unused</td>
 * </tr>
 * <tr>
 *  <td>batch-size</td>
 *  <td>Unused</td>
 * </tr>
 * <tr>
 *  <td>gc-limit</td>
 *  <td>Numbers of contacts saved/removed after which garbage collection should be triggered<br/>
 *      Default value: 100</td>
 * </tr>
 * <tr>
 *  <td>guid-algorithm</td>
 *  <td>Name of the GUID algorithm to use<br/>
 *      Valid values: "default", "cellular" (depends on CelullarQt)<br/>
 *      Default value: "default"</td>
 * </tr>
 * <tr>
 *  <td>default-sync-target</td>
 *  <td>Default value for the QContactSyncTarget set on saved contacts<br/>
 *      Default value: "addressbook"</td>
 * </tr>
 * <tr>
 *  <td>weak-sync-targets</td>
 *  <td>List of sync targets to replace with default-sync-target when saving a contact<br/>
 *      Valid values: a comma separated list of sync targets, or "all"<br/>
 *      Default value: "telepathy"</td>
 * </tr>
 * <tr>
 *  <td>native-numbers</td>
 *  <td>Whether to keep phone numbers in their native script, or convert them to Latin script<br/>
 *      Valid values: true to conserve the original script, false to convert to Latin<br/>
 *      Default value: false</td>
 * </tr>
 * <tr>
 *  <td>omit-presence-changes</td>
 *  <td>Whether the contactsChanged signals should be omitted if only the QContactPresence
 *      detail was changed.
 *      Valid values: true to omit the signals, false to emit all signals<br/>
 *      Default value: false</td>
 * </tr>
 * <tr>
 *  <td>compliance</td>
 *  <td>Selects API compliance level of the engine. Setting "restrictive" lets the engine follow
*       QtContacts API spec by the word. By default the engine is more forgiving.<br/>
 *      Default value: not set</td>
 * </tr>
 * </table>
 */
QContactTrackerEngineParameters::QContactTrackerEngineParameters(const QMap<QString, QString> &parameters,
                                                                 const QString& engineName, int engineVersion)
    : m_engineName(engineName)
    , m_engineVersion(engineVersion)
    , m_requestTimeout(QContactTrackerEngine::DefaultRequestTimeout)
    , m_trackerTimeout(QContactTrackerEngine::DefaultTrackerTimeout)
    , m_coalescingDelay(QContactTrackerEngine::DefaultCoalescingDelay)
    , m_concurrencyLevel(0) // Set in ctor
    , m_batchSize(QContactTrackerEngine::DefaultBatchSize)
    , m_gcLimit(QContactTrackerEngine::DefaultGCLimit)
    , m_syncTarget(QContactTrackerEngine::DefaultSyncTarget)
    , m_weakSyncTargets(QContactTrackerEngine::DefaultWeakSyncTargets)
    , m_guidAlgorithm(0)
    , m_parameters(parameters)
    , m_debugFlags(0)
    , m_selectQueryOptions(Cubi::Options::DefaultSparqlOptions)
    , m_updateQueryOptions(Cubi::Options::DefaultSparqlOptions)
    , m_omitPresenceChanges(false)
    , m_mangleAllSyncTargets(false)
    , m_isRestrictive(false)
{
    const QctSettings *const settings = QctThreadLocalData::instance()->settings();

    m_concurrencyLevel = settings->concurrencyLevel();

    // Names of parameters exposed in the manager URI. See managerParameters() for details.
    static const QStringList managerParameterNames = QStringList();

    // Permit overriding manager params from environment.
    static const QMap<QString, QString> environmentParameters = readEnvironment();
    m_parameters.unite(environmentParameters);

    // Parse engine parameters.
    typedef QTrackerContactDetailSchema::AvatarTypes AvatarTypes;

    AvatarTypes avatarTypes = (QTrackerContactDetailSchema::PersonalAvatar |
                               QTrackerContactDetailSchema::OnlineAvatar);

    bool convertNumersToLatin = false;
    bool writebackPresence = false;

    QString guidAlgorithmName = settings->guidAlgorithmName();
    QStringList contactTypes;

    for(QMap<QString, QString>::ConstIterator i = m_parameters.constBegin(); i != m_parameters.constEnd(); ++i) {
        if (managerParameterNames.contains(i.key())) {
            m_managerParameters.insert(i.key(), i.value());
        }

        if (QLatin1String("timeout") == i.key()) {
            parseParameter(m_requestTimeout, i.key(), i.value());
            continue;
        }

        if (QLatin1String("tracker-timeout") == i.key()) {
            parseParameter(m_trackerTimeout, i.key(), i.value());
            continue;
        }

        if (QLatin1String("coalescing-delay") == i.key()) {
            parseParameter(m_coalescingDelay, i.key(), i.value());
            continue;
        }

        if (QLatin1String("writeback") == i.key()) {
            const QSet<QString> values = i.value().split(QLatin1Char(',')).toSet();
            const bool all = values.contains(QLatin1String("all"));

            if (all || values.contains(QLatin1String("presence"))) {
                writebackPresence = true;
            }

            continue;
        }

        if (QLatin1String("debug") == i.key()) {
            const QSet<QString> values = i.value().split(QLatin1Char(',')).toSet();
            const bool all = values.contains(QLatin1String("all"));

            if (all || values.contains(QLatin1String("queries"))) {
                m_debugFlags |= QContactTrackerEngine::ShowSelects;
                m_debugFlags |= QContactTrackerEngine::ShowUpdates;
            }
            if (all || values.contains(QLatin1String("selects"))) {
                m_debugFlags |= QContactTrackerEngine::ShowSelects;
                m_selectQueryOptions |= Cubi::Options::PrettyPrint;
            }
            if (all || values.contains(QLatin1String("updates"))) {
                m_debugFlags |= QContactTrackerEngine::ShowUpdates;
                m_updateQueryOptions |= Cubi::Options::PrettyPrint;
            }
            if (all || values.contains(QLatin1String("models"))) {
                m_debugFlags |= QContactTrackerEngine::ShowModels;
            }
            if (all || values.contains(QLatin1String("notes"))) {
                m_debugFlags |= QContactTrackerEngine::ShowNotes;
            }
            if (all || values.contains(QLatin1String("timing"))) {
                m_debugFlags |= QContactTrackerEngine::ShowTiming;
            }
            if (all || values.contains(QLatin1String("signals"))) {
                m_debugFlags |= QContactTrackerEngine::ShowSignals;
            }
            if (all || values.contains(QLatin1String("no-sec"))) {
                m_debugFlags |= QContactTrackerEngine::SkipSecurityChecks;
            }
            if (all || values.contains(QLatin1String("no-nagging"))) {
                m_debugFlags |= QContactTrackerEngine::SkipNagging;
            }

            continue;
        }

        if (QLatin1String("contact-types") == i.key()) {
            contactTypes = i.value().split(QLatin1Char(','));
            continue;
        }

        if (QLatin1String("avatar-types") == i.key()) {
            const QSet<QString> values = i.value().split(QLatin1Char(',')).toSet();

            if (values.contains(QLatin1String("all"))) {
                avatarTypes = (QTrackerContactDetailSchema::PersonalAvatar |
                               QTrackerContactDetailSchema::OnlineAvatar |
                               QTrackerContactDetailSchema::SocialAvatar);
                continue;
            }

            avatarTypes = 0;

            if (values.contains(QLatin1String("personal"))) {
                avatarTypes |= QTrackerContactDetailSchema::PersonalAvatar;
            }
            if (values.contains(QLatin1String("online"))) {
                avatarTypes |= QTrackerContactDetailSchema::OnlineAvatar;
            }
            if (values.contains(QLatin1String("social"))) {
                avatarTypes |= QTrackerContactDetailSchema::SocialAvatar;
            }

            continue;
        }

        if (QLatin1String("concurrency") == i.key()) {
            if ((m_concurrencyLevel = i.value().toInt()) < 1) {
                m_concurrencyLevel = settings->concurrencyLevel();
            }

            continue;
        }

        if (QLatin1String("batch-size") == i.key()) {
            if ((m_batchSize = i.value().toInt()) < 1) {
                m_batchSize = QContactTrackerEngine::DefaultBatchSize;
            }

            continue;
        }

        if (QLatin1String("gc-limit") == i.key()) {
            if ((m_gcLimit = i.value().toInt()) < 1) {
                m_gcLimit = QContactTrackerEngine::DefaultGCLimit;
            }

            continue;
        }

        if (QLatin1String("guid-algorithm") == i.key()) {
            guidAlgorithmName = i.value();
            continue;
        }

        if (QLatin1String("default-sync-target") == i.key()) {
            m_syncTarget = i.value();
            continue;
        }

        if (QLatin1String("weak-sync-targets") == i.key()) {
            m_weakSyncTargets = i.value().split(QLatin1String(","), QString::SkipEmptyParts);

            if (m_weakSyncTargets.contains(QLatin1String("all"))) {
                m_mangleAllSyncTargets = true;
                m_weakSyncTargets.clear();
            }

            continue;
        }

        if (QLatin1String("compliance") == i.key()) {
            m_isRestrictive = (i.value() == QLatin1String("restrictive"));
            continue;
        }

        if (QLatin1String("native-numbers") == i.key()) {
            convertNumersToLatin = not(i.value().isEmpty() || QVariant(i.value()).toBool());
            continue;
        }

        if (QLatin1String("omit-presence-changes") == i.key()) {
            m_omitPresenceChanges = true;
            continue;
        }

        if (QLatin1String("com.nokia.qt.mobility.contacts.implementation.version") == i.key()) {
            continue;
        }

        qctWarn(QString::fromLatin1("Unknown parameter: %1").arg(i.key()));
    }

    // Setup detail schemas.
    if (contactTypes.isEmpty() || contactTypes.contains(QContactType::TypeContact, Qt::CaseInsensitive)) {
        QTrackerPersonContactDetailSchema personContactSchema(avatarTypes);
        m_detailSchemas.insert(QContactType::TypeContact, personContactSchema);
    }

    if (contactTypes.isEmpty() || contactTypes.contains(QContactType::TypeGroup, Qt::CaseInsensitive)) {
        QTrackerContactGroupDetailSchema contactGroupSchema(avatarTypes);
        m_detailSchemas.insert(QContactType::TypeGroup, contactGroupSchema);
    }

    foreach(QTrackerContactDetailSchema schema, m_detailSchemas) {
        schema.setConvertNumbersToLatin(convertNumersToLatin);
        schema.setWriteBackPresence(writebackPresence);
    }

    // Setup GUID algorithm.
    m_guidAlgorithm = QctGuidAlgorithmFactory::algorithm(guidAlgorithmName);

    if (0 == m_guidAlgorithm) {
        if (not guidAlgorithmName.isEmpty()) {
            qctWarn(QString::fromLatin1("Unknown GUID algorithm: %1. Using default algorithm.").
                    arg(guidAlgorithmName));
        }

        m_guidAlgorithm = QctGuidAlgorithmFactory::algorithm(QctGuidAlgorithm::Default);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QContactTrackerEngineData::QContactTrackerEngineData(const QMap<QString, QString> &parameters,
                                                     const QString &engineName, int engineVersion)
    : QSharedData()
    , m_parameters(parameters, engineName, engineVersion)
    , m_selfContactId(0)
    , m_changeListener(0) // create on demand
    , m_requestLifeGuard(QMutex::Recursive)
    , m_queue(new QctQueue)
    , m_satisfiedDependencies(QTrackerAbstractRequest::NoDependencies)
    , m_mandatoryTokensFound(false)
{
    // disable security token checks if requested
    if (m_parameters.m_debugFlags & QContactTrackerEngine::SkipSecurityChecks) {
        m_mandatoryTokensFound = true;
    }
}

QContactTrackerEngineData::QContactTrackerEngineData(const QContactTrackerEngineData& other)
    : QSharedData(other)
    , m_parameters(other.m_parameters)
    , m_supportedDataTypes(other.m_supportedDataTypes)
    , m_selfContactId(other.m_selfContactId)
    , m_changeListener(0) // must create our own when needed
    , m_requestLifeGuard(QMutex::Recursive)
    , m_queue(new QctQueue) // the queue is per engine
    , m_customDetails(other.m_customDetails)
    , m_satisfiedDependencies(other.m_satisfiedDependencies)
    , m_gcQueryId(other.m_gcQueryId)
    , m_mandatoryTokensFound(other.m_mandatoryTokensFound)
{
}

QContactTrackerEngineData::~QContactTrackerEngineData()
{
    // Delete the queue early to avoid that its tasks see a half destructed objects
    // like this engine or SPARQL resolvers.
    delete m_queue;
}

///////////////////////////////////////////////////////////////////////////////
// Synchronous API helpers
///////////////////////////////////////////////////////////////////////////////

RequestEventLoop::RequestEventLoop(QContactAbstractRequest *request, int timeout)
    : m_finished(request->isFinished() || request->isCanceled())
{
    // direct connection needed to ensure m_finished is toggled immediately
    connect(request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(stateChanged(QContactAbstractRequest::State)),
            Qt::DirectConnection);
    connect(request, SIGNAL(destroyed()),
            this, SLOT(requestDone()),
            Qt::DirectConnection);

    if (timeout > 0) {
        QTimer::singleShot(timeout, this, SLOT(quit()));
    }
}

void
RequestEventLoop::stateChanged(QContactAbstractRequest::State newState)
{
    switch (newState) {
    case QContactAbstractRequest::FinishedState:
    case QContactAbstractRequest::CanceledState:
        requestDone();
        break;
    default:
        break;
    }
}

void
RequestEventLoop::requestDone()
{
    m_finished = true;
    // Slot was attached with direct connection, therefore quit() must be invoked via auto
    // connection to ensure resultsAvailable() and stateChanged() reach the request thread's
    // event loop before this request loop is terminated and waitForFinished() leaves.
    metaObject()->invokeMethod(this, "quit", Qt::AutoConnection);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QctTaskWaiter::QctTaskWaiter(QctTask *task, QObject *parent)
    : QObject(parent)
    , m_task(task)
    , m_finished(false)
{
    // Use direct connection to keep the task thread from
    // deleting the task while we interact with it.
    connect(task, SIGNAL(finished(QctTask*)),
            this, SLOT(onTaskFinished()), Qt::DirectConnection);
    connect(task, SIGNAL(destroyed(QObject*)),
            this, SLOT(onTaskDestroyed()), Qt::DirectConnection);
}

bool
QctTaskWaiter::wait(int timeout)
{
    QCT_SYNCHRONIZED(&m_mutex);

    if (0 == m_task || m_finished) {
        return m_finished; // return proper result in case of onTaskDestroyed()
    }

    // while waiting the mutex will get released
    return m_waitCondition.wait(&m_mutex, (timeout != 0 ? timeout : ULONG_MAX));
}

void
QctTaskWaiter::onTaskFinished()
{
    {
        QCT_SYNCHRONIZED(&m_mutex);

        if (0 != m_task) {
            // We disconnect make sure that we don't get the signal twice with destroyed
            m_task->disconnect(this);
        }

        m_finished = true;
    }

    m_waitCondition.wakeAll();
}

void
QctTaskWaiter::onTaskDestroyed()
{
    {
        QCT_SYNCHRONIZED(&m_mutex);
        m_task = 0;
    }

    m_waitCondition.wakeAll();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Instances which manage contact engine instances
////////////////////////////////////////////////////////////////////////////////////////////////////

QContactTrackerEngine::QContactTrackerEngine(const QMap<QString, QString> &parameters,
                                             const QString &managerName, int interfaceVersion,
                                             QObject *parent)
    : d(new QContactTrackerEngineData(parameters, managerName, interfaceVersion))
{
    // workaround for Qt type system madness
    qRegisterMetaType<QContactAbstractRequest::State>();

    // workaround for QTMOBILITY-1526
    if (0 != parent) {
        setParent(parent);
    }

    connectSignals();
    registerGcQuery();
}

QContactTrackerEngine::QContactTrackerEngine(const QContactTrackerEngine& other)
    : d(other.d)
{
    d.detach();
    connectSignals();
}

QContactTrackerEngine::~QContactTrackerEngine()
{
    disconnectSignals();
}

QContactTrackerEngine&
QContactTrackerEngine::operator=(const QContactTrackerEngine& other)
{
    disconnectSignals();

    d = other.d;
    d.detach();

    connectSignals();

    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Methods which describe the contact engine's capabilities
////////////////////////////////////////////////////////////////////////////////////////////////////

QMap<QString, QString>
QContactTrackerEngine::managerParameters() const
{
    // Manager parameters appear within the contact manager's URI. The set of parameters exposed
    // in the manager URI must be minimal since the manager URI becomes part of each contact id.
    // Exposing too many parameters would cause the same contact having a different contact id
    // when read from different contact manager instances with slightly different settings like
    // different timeouts or change listener modes.
    return d->m_parameters.m_managerParameters;
}

QString
QContactTrackerEngine::managerName() const
{
    return d->m_parameters.m_engineName;
}

int
QContactTrackerEngine::managerVersion() const
{
    return d->m_parameters.m_engineVersion;
}

bool
QContactTrackerEngine::hasFeature(QContactManager::ManagerFeature feature,
                                  const QString& contactType) const
{
    if (not supportedContactTypes().contains(contactType)) {
        return false;
    }

    switch (feature) {
    case QContactManager::Anonymous:
    case QContactManager::ChangeLogs:
        return true;

    case QContactManager::Groups:
    case QContactManager::Relationships:
        return supportedContactTypes().contains(QContactType::TypeGroup);

    default:
        return false;
    }
}

bool
QContactTrackerEngine::isFilterSupported(const QContactFilter& filter) const
{
    return QTrackerScalarContactQueryBuilder::isFilterSupported(filter);
}

QList<QVariant::Type>
QContactTrackerEngine::supportedDataTypes() const
{
    if (d->m_supportedDataTypes.isEmpty()) {
        QSet<QVariant::Type> typeSet;

        foreach(const QTrackerContactDetailSchema& schema, schemas()) {
            typeSet += schema.supportedDataTypes();
        }

        d->m_supportedDataTypes = typeSet.toList();
    }

    return d->m_supportedDataTypes;
}

QStringList
QContactTrackerEngine::supportedContactTypes() const
{
    return schemas().keys();
}

QContactDetailDefinitionMap
QContactTrackerEngine::detailDefinitions(const QString& contactType,
                                         QContactManager::Error* error) const
{
    // Check if the requested contact type is known.
    const QTrackerContactDetailSchemaMap::ConstIterator schema = schemas().find(contactType);

    if (schema == schemas().constEnd()) {
        qctPropagate(QContactManager::InvalidContactTypeError, error);
        return QContactDetailDefinitionMap();
    }

    // List definitions of RDF detail schema.
    QContactDetailDefinitionMap definitions = schema->detailDefinitions();

    // Find the definition in custom detail schema.
    const CustomContactDetailMap::ConstIterator contactTypeDetails
            = d->m_customDetails.find(contactType);

    if (contactTypeDetails != d->m_customDetails.constEnd()) {
        for(QContactDetailDefinitionMap::ConstIterator it = contactTypeDetails->constBegin();
            it != contactTypeDetails->constEnd(); ++it) {
            definitions.insert(it.key(), it.value());
        }
    }

    // Report the mangled result.
    qctPropagate(QContactManager::NoError, error);
    return definitions;
}

QContactDetailDefinition
QContactTrackerEngine::detailDefinition(const QString& definitionName,
                                        const QString& contactType,
                                        QContactManager::Error* error) const
{
    // Check if the requested contact type is known.
    const QTrackerContactDetailSchemaMap::ConstIterator schema = schemas().find(contactType);

    if (schema == schemas().constEnd()) {
        qctPropagate(QContactManager::InvalidContactTypeError, error);
        return QContactDetailDefinition();
    }

    // Find the the definition in RDF detail schema.
    const QContactDetailDefinitionMap &definitions = schema->detailDefinitions();
    const QContactDetailDefinitionMap::ConstIterator detail = definitions.find(definitionName);

    if (definitions.constEnd() != detail) {
        qctPropagate(QContactManager::NoError, error);
        return detail.value();
    }

    // Find the definition in custom detail schema.
    const CustomContactDetailMap::ConstIterator contactTypeDetails
            = d->m_customDetails.find(contactType);

    if (contactTypeDetails != d->m_customDetails.constEnd()) {
        const QContactDetailDefinitionMap::ConstIterator customDetail
                = contactTypeDetails->find(definitionName);

        if (customDetail != contactTypeDetails->constEnd()) {
            qctPropagate(QContactManager::NoError, error);
            return customDetail.value();
        }
    }

    // Nothing found. Bad luck.
    qctPropagate(QContactManager::DoesNotExistError, error);
    return QContactDetailDefinition();
}

bool
QContactTrackerEngine::saveDetailDefinition(const QContactDetailDefinition &definition,
                                            const QString &contactType,
                                            QContactManager::Error *error)
{
    // Check if the requested contact type is known.
    const QTrackerContactDetailSchemaMap::ConstIterator schema = schemas().find(contactType);

    if (schema == schemas().constEnd()) {
        qctPropagate(QContactManager::InvalidContactTypeError, error);
        return false;
    }

    // Check if the definition would overwrite a schema definition.
    // They are fixed and cannot be overwritten since the ontology is fixed.
    const QContactDetailDefinitionMap &definitions = schema->detailDefinitions();
    const QContactDetailDefinitionMap::ConstIterator detail = definitions.find(definition.name());

    if (detail != definitions.constEnd()) {
        qctPropagate(QContactManager::PermissionsError, error);
        return false;
    }

    // Store the custom detail definition. Note: We don't persist custom details right now.
    d->m_customDetails[contactType].insert(definition.name(), definition);
    qctPropagate(QContactManager::NoError, error);
    return true;
}

bool
QContactTrackerEngine::removeDetailDefinition(const QString &definitionName,
                                              const QString &contactType,
                                              QContactManager::Error *error)
{
    // Check if the requested contact type is known.
    const QTrackerContactDetailSchemaMap::ConstIterator schema = schemas().find(contactType);

    if (schema == schemas().constEnd()) {
        qctPropagate(QContactManager::InvalidContactTypeError, error);
        return false;
    }

    // Check if the definition would overwrite a schema definition.
    // They are fixed and cannot be overwritten since the ontology is fixed.
    const QContactDetailDefinitionMap &definitions = schema->detailDefinitions();
    const QContactDetailDefinitionMap::ConstIterator detail = definitions.find(definitionName);

    if (detail != definitions.constEnd()) {
        qctPropagate(QContactManager::PermissionsError, error);
        return false;
    }

    // Remove the custom detail definition. Note: We don't persist custom details right now.
    const CustomContactDetailMap::Iterator customDetailMap = d->m_customDetails.find(contactType);

    if (customDetailMap == d->m_customDetails.constEnd() ||
        customDetailMap->remove(definitionName) == 0) {
        qctPropagate(QContactManager::DoesNotExistError, error);
        return true;
    }

    qctPropagate(QContactManager::NoError, error);
    return true;
}

const QTrackerContactDetailSchemaMap &
QContactTrackerEngine::schemas() const
{
    return d->m_parameters.m_detailSchemas;
}

const QTrackerContactDetailSchema &
QContactTrackerEngine::schema(const QString& contactType) const
{
    QTrackerContactDetailSchemaMap::ConstIterator schema = schemas().find(contactType);

    if (schema == schemas().constEnd()) {
        qctFail(QString::fromLatin1("Unexpected contact type %1. Aborting.").arg(contactType));
        return QTrackerContactDetailSchema::invalidSchema();
    }

    return schema.value();
}

bool
QContactTrackerEngine::setSelfContactId(const QContactLocalId&,
                                        QContactManager::Error* error)
{
    REPORT_UNSUPPORTED_FUNCTION(error);
    return false;
}

QContactLocalId
QContactTrackerEngine::selfContactId(QContactManager::Error* error) const
{
    if (0 == d->m_selfContactId) {
        QctTrackerIdResolver resolver(QStringList() << nco::default_contact_me::iri());

        if (resolver.lookupAndWait()) {
            d->m_selfContactId = resolver.trackerIds().first();
        }
    }

    qctPropagate(d->m_selfContactId ? QContactManager::NoError
                                    : QContactManager::DoesNotExistError, error);

    return d->m_selfContactId;
}

const QContactTrackerEngine::DebugFlags &
QContactTrackerEngine::debugFlags() const
{
    return d->m_parameters.m_debugFlags;
}

bool
QContactTrackerEngine::hasDebugFlag(DebugFlag flag) const
{
    return debugFlags().testFlag(flag);
}

int
QContactTrackerEngine::concurrencyLevel() const
{
    return d->m_parameters.m_concurrencyLevel;
}

int
QContactTrackerEngine::batchSize() const
{
    return d->m_parameters.m_batchSize;
}

int
QContactTrackerEngine::gcLimit() const
{
    return d->m_parameters.m_gcLimit;
}

int
QContactTrackerEngine::requestTimeout() const
{
    return d->m_parameters.m_requestTimeout;
}

int
QContactTrackerEngine::trackerTimeout() const
{
    return d->m_parameters.m_trackerTimeout;
}

int
QContactTrackerEngine::coalescingDelay() const
{
    return d->m_parameters.m_coalescingDelay;
}

QctGuidAlgorithm &
QContactTrackerEngine::guidAlgorithm() const
{
    return *d->m_parameters.m_guidAlgorithm;
}

const QString &
QContactTrackerEngine::syncTarget() const
{
    return d->m_parameters.m_syncTarget;
}

const QStringList &
QContactTrackerEngine::weakSyncTargets() const
{
    return d->m_parameters.m_weakSyncTargets;
}

bool
QContactTrackerEngine::mangleAllSyncTargets() const
{
    return d->m_parameters.m_mangleAllSyncTargets;
}

bool
QContactTrackerEngine::isRestrictive() const
{
    return d->m_parameters.m_isRestrictive;
}

Cubi::Options::SparqlOptions
QContactTrackerEngine::selectQueryOptions() const
{
    return d->m_parameters.m_selectQueryOptions;
}

Cubi::Options::SparqlOptions
QContactTrackerEngine::updateQueryOptions() const
{
    return d->m_parameters.m_updateQueryOptions;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Synchronous data access methods
////////////////////////////////////////////////////////////////////////////////////////////////////

QList<QContactLocalId>
QContactTrackerEngine::contactIds(const QList<QContactSortOrder>& sortOrders,
                                  QContactManager::Error* error) const
{
    return contactIds(QContactFilter(), sortOrders, error);
}

QList<QContactLocalId>
QContactTrackerEngine::contactIds(const QContactFilter& filter,
                                  const QList<QContactSortOrder>& sortOrders,
                                  QContactManager::Error* error) const
{
    QContactLocalIdFetchRequest request;

    request.setFilter(filter);
    request.setSorting(sortOrders);

    runSyncRequest(&request, error);

    return request.ids();
}

QList<QContact>
QContactTrackerEngine::contacts(const QContactFilter& filter,
                                const QList<QContactSortOrder>& sortOrders,
                                const QContactFetchHint& fetchHint,
                                QContactManager::Error* error) const
{
    QContactFetchRequest request;

    request.setFetchHint(fetchHint);
    request.setFilter(filter);
    request.setSorting(sortOrders);

    runSyncRequest(&request, error);

    return request.contacts();
}

QList<QContact>
QContactTrackerEngine::contacts(const QList<QContactLocalId> &localIds,
                                const QContactFetchHint &fetchHint,
                                ErrorMap *errorMap,
                                QContactManager::Error *error) const
{
    QContactFetchByIdRequest request;

    request.setLocalIds(localIds);
    request.setFetchHint(fetchHint);

    runSyncRequest(&request, error);

    qctPropagate(request.errorMap(), errorMap);

    return request.contacts();
}

QContact
QContactTrackerEngine::contact(const QContactLocalId& contactId,
                               const QContactFetchHint& fetchHint,
                               QContactManager::Error* error) const
{
    static bool warningNotShownYet = true;

    if (warningNotShownYet) {
        if (not hasDebugFlag(SkipNagging)) {
            qctWarn(QString::fromLatin1("\n"
                    "=============================================================================\n"
                    "WARNING /!\\ - AVOID CALLING THIS FUNCTION FROM PRODUCTION CODE!!!\n"
                    "=============================================================================\n"
                    "QContactManager::contact() is blocking on D-Bus roundtrip while accessing\n"
                    "tracker. Please consider using batch API (QContactManager::contacts()),\n"
                    "or even better use the asynchronous QContactFetchRequest API, instead of\n"
                    "fetching contacts one by one.\n"
                    "\n"
                    "Please note that reading 100 ids and 100 contact by ids is ~100 times\n"
                    "slower than reading 100 contacts at once with QContactFetchRequest.\n"
                    "\n"
                    "Offending application is %1 [%2].\n"
                    "=============================================================================").
                    arg(QCoreApplication::applicationFilePath(),
                        QString::number(QCoreApplication::applicationPid())));
        }

        warningNotShownYet = false;
    }

    return contactImpl(contactId, fetchHint, error);
}

// Used in tests, removed warning while decided if to provide sync api.
// Until then customers are advised to use async.
QContact
QContactTrackerEngine::contactImpl(const QContactLocalId& contactId,
                                   const QContactFetchHint& fetchHint,
                                   QContactManager::Error* error) const
{
    QContactLocalIdFilter idFilter;
    idFilter.setIds(QList<QContactLocalId>() << contactId);

    QContactFetchRequest request;
    request.setFetchHint(fetchHint);
    request.setFilter(idFilter);

    if (not runSyncRequest(&request, error)) {
        return QContact();
    }

    QList<QContact> contacts(request.contacts());

    if (contacts.isEmpty()) {
        qctPropagate(QContactManager::DoesNotExistError, error);
        return QContact();
    }

    if (contacts.count() > 1) {
        qctWarn(QString::fromLatin1("Expected only one contact, but got %1").arg(contacts.count()));
    }

    return contacts.first();
}

bool
QContactTrackerEngine::saveContact(QContact* contact, QContactManager::Error* error)
{
    if (0 == contact) {
        qctPropagate(QContactManager::BadArgumentError, error);
        return false;
    }

    QList<QContact> contactList = QList<QContact>() << *contact;
    const bool success = saveContacts(&contactList, 0, error);

    if (success) {
        setContactDisplayLabel(contact, contactList.first().displayLabel());
        contact->setId(contactList.first().id());
    }

    return success;
}

bool
QContactTrackerEngine::saveContacts(QList<QContact> *contacts,
                                    ErrorMap *errorMap, QContactManager::Error *error)
{
    return saveContacts(contacts, QStringList(), errorMap, error);
}

bool
QContactTrackerEngine::saveContacts(QList<QContact> *contacts,
                                    const QStringList &definitionMask,
                                    ErrorMap *errorMap, QContactManager::Error *error)
{
    if (0 == contacts) {
        qctPropagate(QContactManager::BadArgumentError, error);
        return false;
    }

    QContactSaveRequest request;
    request.setContacts(*contacts);
    request.setDefinitionMask(definitionMask);

    const bool hasFinished = runSyncRequest(&request, error);
    qctPropagate(request.errorMap(), errorMap);

    if (not hasFinished) {
        return false;
    }

    QList<QContact>::Iterator outputIter = contacts->begin();

    // only update the contact id and the display label, all other updates must be fetched
    foreach(const QContact &savedContact, request.contacts()) {
        setContactDisplayLabel(&(*outputIter), savedContact.displayLabel());
        outputIter->setId(savedContact.id());
        outputIter++;
    }

    return QContactManager::NoError == request.error();
}


bool
QContactTrackerEngine::removeContact(const QContactLocalId &contactId,
                                     QContactManager::Error *error)
{
    return removeContacts(QList<QContactLocalId>() << contactId, 0, error);
}

bool
QContactTrackerEngine::removeContacts(const QList<QContactLocalId>& contactIds,
                                      ErrorMap *errorMap, QContactManager::Error *error)
{
    QContactRemoveRequest request;

    request.setContactIds(contactIds);

    runSyncRequest(&request, error);

    qctPropagate(request.errorMap(), errorMap);

    return QContactManager::NoError == request.error();
}

QList<QContactRelationship>
QContactTrackerEngine::relationships(const QString &relationshipType,
                                     const QContactId &participantId,
                                     QContactRelationship::Role role,
                                     QContactManager::Error *error) const
{
    // Mapping of this call to QContactRelationshipFetchRequest is not directly possible
    // as QContactRelationshipFetchRequest gets set first and second contact but not
    // just one participantId and a role.
    //
    // So in the case that role is QContactRelationship::Either two separate requests need to be done.
    // As a contact is not allowed to be on the both side of any relationship this should not result in
    // duplicates.
    //
    // If participantId is the default-constructed QContactId, all relationships of the given type shall
    // be returned and the role ignored. The first request is used for that.


    if (role == QContactRelationship::Either && participantId != QContactId()) {
        QContactManager::Error internalError;
        QList<QContactRelationship> result;

        internalError = QContactManager::UnspecifiedError;
        result = relationships(relationshipType, participantId,
                               QContactRelationship::First,
                               &internalError);

        if (internalError != QContactManager::NoError) {
            qctPropagate(internalError, error);
            return QList<QContactRelationship>();
        }

        internalError = QContactManager::UnspecifiedError;
        result += relationships(relationshipType, participantId,
                                QContactRelationship::Second,
                                &internalError);

        if (internalError != QContactManager::NoError) {
            qctPropagate(internalError, error);
            return QList<QContactRelationship>();
        }

        return result;
    }

    QContactRelationshipFetchRequest request;

    request.setRelationshipType(relationshipType);

    switch(role) {
    case QContactRelationship::First:
        request.setFirst(participantId);
        break;
    case QContactRelationship::Second:
        request.setSecond(participantId);
        break;
    case QContactRelationship::Either:
        break;
    }

    runSyncRequest(&request, error);
    return request.relationships();
}

bool
QContactTrackerEngine::saveRelationship(QContactRelationship *relationship, QContactManager::Error *error)
{
    if (0 == relationship) {
        qctPropagate(QContactManager::BadArgumentError, error);
        return false;
    }

    QList<QContactRelationship> relationships =
            QList<QContactRelationship>() << *relationship;

    const bool success = saveRelationships(&relationships, 0, error);

    *relationship = relationships.first();

    return success;
}

bool
QContactTrackerEngine::saveRelationships(QList<QContactRelationship> *relationships,
                                         ErrorMap *errorMap, QContactManager::Error *error)
{
    if (0 == relationships) {
        qctPropagate(QContactManager::BadArgumentError, error);
        return false;
    }

    QContactRelationshipSaveRequest request;
    request.setRelationships(*relationships);

    runSyncRequest(&request, error);

    *relationships = request.relationships();
    qctPropagate(request.errorMap(), errorMap);

    return QContactManager::NoError == request.error();
}

bool
QContactTrackerEngine::removeRelationship(const QContactRelationship &relationship, QContactManager::Error *error)
{
    return removeRelationships(QList<QContactRelationship>() << relationship, 0, error);
}

bool
QContactTrackerEngine::removeRelationships(const QList<QContactRelationship> &relationships,
                                           ErrorMap *errorMap, QContactManager::Error *error)
{
    QContactRelationshipRemoveRequest request;
    request.setRelationships(relationships);

    runSyncRequest(&request, error);
    qctPropagate(request.errorMap(), errorMap);

    return QContactManager::NoError == request.error();
}

/// returns the DisplayNameDetailList for the name order given, if found.
/// Default is the one for QContactDisplayLabel__FirstNameLastNameOrder.
static const QList<QctDisplayLabelGenerator> &
findDisplayNameGenerators(const QString &nameOrder)
{
    const QctSettings *const settings = QctThreadLocalData::instance()->settings();

    QctDisplayLabelGenerator::ListOptions options = 0;

    if (nameOrder == QContactDisplayLabel__FieldOrderFirstName) {
        options |= QctDisplayLabelGenerator::FirstNameLastName;
    } else if (nameOrder == QContactDisplayLabel__FieldOrderLastName) {
        options |= QctDisplayLabelGenerator::LastNameFirstName;
    } else {
        if (nameOrder != settings->nameOrder()) {
            // use configured name-order setting when an unknown value was passed
            return findDisplayNameGenerators(settings->nameOrder());
        }

        // use a default value when the name-order setting itself is wrong
        return findDisplayNameGenerators(QctSettings::DefaultNameOrder);
    }

    if (settings->preferNickname()) {
        options |= QctDisplayLabelGenerator::PreferNickname;
    }

    return QctDisplayLabelGenerator::generators(options);
}

QContactFetchHint
QContactTrackerEngine::normalizedFetchHint(QContactFetchHint fetchHint, const QString &nameOrder)
{
    QStringList detailDefinitionHint = fetchHint.detailDefinitionsHint();

    // Make sure the display name can be synthesized when needed
    if (detailDefinitionHint.contains(QContactDisplayLabel::DefinitionName)) {
        foreach(const QctDisplayLabelGenerator &generator, findDisplayNameGenerators(nameOrder)) {
            if (not detailDefinitionHint.contains(generator.requiredDetailName())) {
                detailDefinitionHint += generator.requiredDetailName();
            }
        }

        fetchHint.setDetailDefinitionsHint(detailDefinitionHint);
    }

    return fetchHint;
}

void
QContactTrackerEngine::updateDisplayLabel(QContact &contact, const QString &nameOrder) const
{
    setContactDisplayLabel(&contact, createDisplayLabel(contact, nameOrder));
}

QString
QContactTrackerEngine::createDisplayLabel(const QContact &contact,
                                          const QString &nameOrder) const
{
    QString label;

    foreach(const QctDisplayLabelGenerator &generator, findDisplayNameGenerators(nameOrder)) {
        label = generator.createDisplayLabel(contact);

        if (not label.isEmpty()) {
            break;
        }
    }

    return label;
}

template <class T> static void
transfer(const T &key, const QContactDetail &source, QContactDetail &target)
{
    QVariant value(source.variantValue(key));

    if (not value.isNull()) {
        target.setValue(key, value);
    }
}

typedef QPair<QContactAvatar, int> AvatarWithAvailability;

inline bool
operator<(const AvatarWithAvailability &a, const AvatarWithAvailability &b)
{
    if (a.second != b.second) {
        // the lower the availability rank, the more significant the detail
        return a.second > b.second;
    }

    const QString accountA = a.first.linkedDetailUris().first();
    const QString accountB = b.first.linkedDetailUris().first();

    if (accountA != accountB) {
        return accountA < accountB;
    }

    const QString subTypeA = a.first.value(QContactAvatar__FieldSubType);
    const QString subTypeB = b.first.value(QContactAvatar__FieldSubType);

    if (subTypeA != subTypeB) {
        return subTypeA < subTypeB;
    }

    return a.first.imageUrl() < b.first.imageUrl();
}

void
QContactTrackerEngine::updateAvatar(QContact &contact)
{
    QContactPersonalAvatar personalAvatar = contact.detail<QContactPersonalAvatar>();

    const QList<QContactDetail> currentOnlineAvatars
            = contact.details(QContactOnlineAvatar::DefinitionName)
            + contact.details(QContactSocialAvatar::DefinitionName);

    // Function was triggered by an update of nco:photo
    if (not personalAvatar.isEmpty()) {
        QContactAvatar photoAvatar = personalAvatar.toAvatar();

        // If there was already a nco:photo (no linked uri), remove it
        contact.removeDetail(&personalAvatar);

        // nco:photo always goes first, so that QContact::detail<QContactAvatar> returns it
        contact.saveDetail(&photoAvatar);
    }

    // Function was triggered by an update of a IMAddress' imAvatar
    if (not currentOnlineAvatars.empty()) {
        // find all available presence details
        QMap<QString, QContactPresence> presenceDetails;

        foreach(const QContactPresence &detail, contact.details<QContactPresence>()) {
            const QString accountPath = parsePresenceIri(detail.detailUri());

            if (not accountPath.isEmpty()) {
                presenceDetails.insert(accountPath, detail);
            }
        }

        // store online avatars with their availablity
        QList<AvatarWithAvailability> avatarsWithAvailability;

        foreach(QContactDetail onlineAvatar, currentOnlineAvatars) {
            contact.removeDetail(&onlineAvatar);

            // FIXME: properly define schema for detailUri and linkedDetailUri
            QStringList linkedDetailUris = onlineAvatar.linkedDetailUris();

            if (linkedDetailUris.isEmpty()) {
                linkedDetailUris += QString();
            }

            foreach(const QString &uri, linkedDetailUris) {
                const QString accountPath = not uri.isEmpty()
                        ? parseTelepathyIri(uri)
                        : QString();

                int availability = qctAvailabilityRank(QContactPresence::PresenceUnknown);

                if (not accountPath.isEmpty()) {
                    const QContactPresence presence = presenceDetails.value(accountPath);
                    availability = qctAvailabilityRank(presence.presenceState());
                } else {
                    qctWarn("QTrackerContactOnlineAvatar should always be linked with an online account");
                }

                QContactAvatar avatarDetail;

                avatarDetail.setLinkedDetailUris(onlineAvatar.linkedDetailUris());
                avatarDetail.setImageUrl(onlineAvatar.value(QContactOnlineAvatar::FieldImageUrl));
                avatarDetail.setValue(QContactAvatar__FieldSubType, onlineAvatar.value(QContactOnlineAvatar::FieldSubType));

                avatarsWithAvailability += qMakePair(avatarDetail, availability);
            }
        }

        // sort avatars by their availablity
        qSort(avatarsWithAvailability);

        // add regular avatar details to contact
        for(int i = 0; i < avatarsWithAvailability.count(); ++i) {
            contact.saveDetail(&avatarsWithAvailability[i].first);
        }
    }
}

bool
QContactTrackerEngine::isWeakSyncTarget(const QString &syncTarget) const
{
    return mangleAllSyncTargets() || weakSyncTargets().contains(syncTarget);
}

QString
QContactTrackerEngine::gcQueryId() const
{
    return d->m_gcQueryId;
}

// TODO: we could cache the return value in this function
QString
QContactTrackerEngine::cleanupQueryString() const
{
    // Notice FILTER in WHERE clause - garbage collector needs to verify parenting outside this graph too.
    // reason - IMAccount hasIMAddress in contactsd private graph so no need to delete it. The same could apply
    // to any other property - it could have been added to parent in some other graph.
    // Camera geotagging creates addresses outside qct graph and have no "parent" contact pointing
    // to them through nco:hasAddress
    static const QString obsoleteResourcesPrefix = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  GRAPH <%1>\n"
             "  {\n"
             "    ?resource a <%2> .\n"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH <%1>\n"
             "  {\n"
             "    ?resource a <%3> .\n"
             "  }\n");
    static const QString imAccountStopPattern = QLatin1String
            ("  FILTER(NOT EXISTS { ?resource a nco:IMAccount }) .\n");
    static const QString obsoleteResourcesPattern = QLatin1String
            ("  FILTER(NOT EXISTS { ?parent <%1> ?resource }) .\n");
    static const QString obsoleteResourcesSuffix = QLatin1String
            ("}\n");

    QString queryString;

#ifndef QT_NO_DEBUG
    queryString += QLatin1String
            ("\n"
             "#--------------------------------------.\n"
             "# Drop drop obsolete custom properties |\n"
             "#--------------------------------------'\n");
#endif // QT_NO_DEBUG

    // collect obsolete resource types and their predicates
    typedef QMap<QString, bool> PredicateMap;
    typedef QMap<QString, PredicateMap> ResourceTypePredicateMap;
    ResourceTypePredicateMap obsoleteResources;

    foreach(const QTrackerContactDetailSchema &schema, schemas()) {
        foreach(const QTrackerContactDetail &detail, schema.details()) {
            // Note: Previously we only iterated possessed chains, but since the DELETE query
            // consideres graphs it should be save to consider all linked resources we create.
            foreach(const PropertyInfoList &chain, detail.predicateChains()) {
                const PropertyInfoBase &pi = chain.last();
                QString rangeIri = pi.rangeIri();

                if (rangeIri == rdfs::Resource::iri()) {
                    qctWarn(QString::fromLatin1("Not cleaning up obsolete resources for %1 property "
                                                "since the property's range is too generic (%2).").
                            arg(qctIriAlias(pi.iri()), qctIriAlias(pi.rangeIri())));
                    continue;
                }

                // Merge nfo:FileDataObject and nie:DataObject chains. Their resources are
                // overlapping since nfo:FileDataObject is a subclass of nie:DataObject.
                // FIXME: Find a more generic solution for this problem.
                if (rangeIri == nfo::FileDataObject::iri()) {
                    rangeIri = nie::DataObject::iri();
                }

                // FIXME: Hotfix for nco:photo and nco:video. If we'd consider the field chains,
                // we could check the field's RDF domain, and figure out this ourself. But doing
                // that would be way too invasive considering the project schedule.
                if (rangeIri == nie::InformationElement::iri()) {
                    rangeIri = nie::DataObject::iri();
                }

                obsoleteResources[rangeIri].insert(pi.iri(), true);
            }
        }
    }

    // glue everything together
    queryString += obsoleteResourcesPrefix.arg(QtContactsTrackerDefaultGraphIri,
                                               nao::Property::iri(),
                                               nao::Property::iri());
    queryString += obsoleteResourcesPattern.arg(nao::hasProperty::iri());
    queryString += obsoleteResourcesSuffix;

    for(ResourceTypePredicateMap::ConstIterator
        t = obsoleteResources.constBegin(); t != obsoleteResources.constEnd(); ++t) {
        queryString += obsoleteResourcesPrefix.arg(QtContactsTrackerDefaultGraphIri,
                                                   rdfs::Resource::iri(),
                                                   t.key());

        if (nco::IMAddress::iri() == t.key()) {
            // FIXME: Remove this workaround for NB#206404 - Saving a contact using
            // qtcontacts-tracker causes nco:IMAccount to be removed.
            queryString += imAccountStopPattern;
        }

        foreach(const QString &predicate, t.value().keys()) {
            queryString += obsoleteResourcesPattern.arg(predicate);
        }

        queryString += obsoleteResourcesSuffix;
    }

    return queryString;
}

void
QContactTrackerEngine::enqueueTask(QctTask *task)
{
    d->m_queue->enqueue(task);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Asynchronous data access methods
////////////////////////////////////////////////////////////////////////////////////////////////////

bool
QContactTrackerEngine::checkSecurityTokens(QContactAbstractRequest *request)
{
    // Plugin users often fail to provide all required security tokens.
    // Therefore we print warnings to educate them. If the security checks
    // should cause trouble they can be temporarly disabled by exporting
    // QT_CONTACTS_TRACKER="debug=no-sec".

    if (d->m_mandatoryTokensFound) {
        return true;
    }

    QStringList theGateKeepersBlameList;
    QStringList missingOptionalTokens;

#ifdef ENABLE_CREDENTIALS

    static const QStringList requiredSecurityTokens =
            QStringList() << QLatin1String("TrackerReadAccess")
                          << QLatin1String("TrackerWriteAccess");

    static const QStringList optionalSecurityTokens =
            QStringList() << QLatin1String("GRP::metadata-users");

    foreach(const QString &credential, requiredSecurityTokens) {
        QString errorMessage;

        if (not MssfQt::CredentialsManager::hasProcessCredential(0, credential, &errorMessage)) {
            theGateKeepersBlameList += credential;
            qctWarn(errorMessage);
        }
    }

    foreach(const QString &credential, optionalSecurityTokens) {
        QString errorMessage;

        if (not MssfQt::CredentialsManager::hasProcessCredential(0, credential, &errorMessage)) {
            missingOptionalTokens += credential;
            qctWarn(errorMessage);
        }
    }

#endif // ENABLE_CREDENTIALS

    if (not theGateKeepersBlameList.isEmpty()) {
        qctWarn(QString::fromLatin1("\n"
                 "=============================================================================\n"
                 "WARNING /!\\ - MANDATORY SECURITY TOKENS ARE MISSING\n"
                 "=============================================================================\n"
                 "Rejecting %2.\n"
                 "Please add an AEGIS manifest to your package requesting the following\n"
                 "security tokens: %1 for %3 [%4].\n"
                 "=============================================================================").
                arg(theGateKeepersBlameList.join(QLatin1String(", ")),
                    QLatin1String(request->metaObject()->className()),
                    QCoreApplication::applicationFilePath(),
                    QString::number(QCoreApplication::applicationPid())));

        return false;
    }

    if (not missingOptionalTokens.isEmpty()) {
        qctWarn(QString::fromLatin1("\n"
                 "=============================================================================\n"
                 "WARNING /!\\ - OPTIONAL SECURITY TOKENS ARE MISSING\n"
                 "=============================================================================\n"
                 "Full functionality like tracker direct access is not available.\n"
                 "Please add an AEGIS manifest to your package requesting the following\n"
                 "security tokens: %1 for %2 [%3].\n"
                 "=============================================================================").
                arg(missingOptionalTokens.join(QLatin1String(", ")),
                    QCoreApplication::applicationFilePath(),
                    QString::number(QCoreApplication::applicationPid())));
    }

    d->m_mandatoryTokensFound = true;

    return true;
}

static QString
addressStringFromPointer(const void *pointer)
{
    return QString::fromLatin1("0x") + QString::number(reinterpret_cast<quintptr>(pointer),16);
}

bool
QContactTrackerEngine::checkThreadOfRequest(QContactAbstractRequest *request) const
{
    const QThread * const requestThread = request->thread();

    // check that request and contactmanager are in same thread
    const QThread * const managerThread = thread();

    if (managerThread != requestThread) {
        qctWarn(QString::fromLatin1("\n"
                                    "================================================================================\n"
                                    "ERROR /!\\ - REQUEST IS NOT ASSIGNED TO THE SAME THREAD AS THE CONTACTMANAGER\n"
                                    "================================================================================\n"
                                    "A request has been started that is not assigned to the same thread as the\n"
                                    "contact manager it belongs to. This is not supported.\n"
                                    "See also http://doc.qt.nokia.com/stable/threads-qobject.html#qobject-reentrancy.\n"
                                    "\n"
                                    "Manager: \"%1\" (thread address: %2)\n"
                                    "Request: \"%3\" (object name: \"%4\", thread address: %5).\n"
                                    "\n"
                                    "Offending application is %6 [%7].\n"
                                    "=============================================================================").
                arg(managerUri(), addressStringFromPointer(managerThread),
                    QString::fromLatin1(request->metaObject()->className()),
                    request->objectName(), addressStringFromPointer(requestThread),
                    QCoreApplication::applicationFilePath(),
                    QString::number(QCoreApplication::applicationPid())));

        return false;
    }

    // check that start of the request is done in same thread
    const QThread * const currentThread = QThread::currentThread();

    if (currentThread != requestThread) {
        qctWarn(QString::fromLatin1("\n"
                                    "================================================================================\n"
                                    "ERROR /!\\ - REQUEST IS NOT STARTED IN THE THREAD IT BELONGS TO\n"
                                    "================================================================================\n"
                                    "QContactAbstractRequest::start() has been called on a request from a different\n"
                                    "thread than the one it is assigned to. This is not supported.\n"
                                    "See also http://doc.qt.nokia.com/stable/threads-qobject.html#qobject-reentrancy.\n"
                                    "\n"
                                    "Manager: \"%1\" (thread address: %2)\n"
                                    "Request: \"%3\" (object name: \"%4\", thread address: %5).\n"
                                    "\n"
                                    "Offending application is %6 [%7].\n"
                                    "=============================================================================").
                arg(managerUri(), addressStringFromPointer(managerThread),
                    QString::fromLatin1(request->metaObject()->className()),
                    request->objectName(), addressStringFromPointer(requestThread),
                    QCoreApplication::applicationFilePath(),
                    QString::number(QCoreApplication::applicationPid())));

        return false;
    }

    return true;
}

QctTask *
QContactTrackerEngine::startRequestImpl(QContactAbstractRequest *request)
{
    QCT_SYNCHRONIZED(&d->m_requestLifeGuard);

    QWeakPointer<QContactAbstractRequest> guardedRequest = request;

    // The worker puts the request under protection of the engine's request life-guard.
    QScopedPointer<QTrackerAbstractRequest> worker(createRequestWorker(guardedRequest.data()));

    if (worker.isNull()) {
        return 0;
    }

    QScopedPointer<QctRequestTask> task(new QctRequestTask(guardedRequest.data(), worker.take()));

    // Check if some evil client managed to delete the request from some other thread.
    if (guardedRequest.isNull()) {
        return 0;
    }

    // Make sure mandatory tracker:ids have been resolved.
    if (task->dependencies().testFlag(QTrackerAbstractRequest::ResourceCache) &&
        not d->m_satisfiedDependencies.testFlag(QTrackerAbstractRequest::ResourceCache)) {
        // Queue will take ownership of the task
        // We don't explicitly parent the taks when creating them, because this method
        // might be called from a thread which is not the one where the engine was created.
        // In that case, we would get the "QObject: Cannot create children for a parent that
        // is in a different thread" warning, and the parent would be NULL anyway...
        enqueueTask(new QctResourceCacheTask(schemas()));
        d->m_satisfiedDependencies |= QTrackerAbstractRequest::ResourceCache;
    }

    // Make sure GUID algorithms are available.
    if (task->dependencies().testFlag(QTrackerAbstractRequest::GuidAlgorithm) &&
        not d->m_satisfiedDependencies.testFlag(QTrackerAbstractRequest::GuidAlgorithm)) {
        // Queue will take ownership of the task
        // See above why we don't set the parent here
        enqueueTask(new QctGuidAlgorithmTask(this));
        d->m_satisfiedDependencies |= QTrackerAbstractRequest::GuidAlgorithm;
    }

    // The contacts engine must bring requests into activate state as soon as it is dealing
    // with them. We cannot queue the task first, since the queue might be empty right now,
    // causing the task to start work from inactive state. This would be ridicilous.
    updateRequestState(guardedRequest.data(), QContactAbstractRequest::ActiveState);

    // Check if some evil client managed to delete the request from some slot (or thread).
    if (guardedRequest.isNull()) {
        return 0;
    }

    // Transfer task ownership to the queue.
    QctRequestTask *const result = task.data();
    d->m_queue->enqueue(task.take());
    return result;
}

bool
QContactTrackerEngine::startRequest(QContactAbstractRequest *request)
{
    startRequestImpl(request);
    return true; // this always works
}

QTrackerAbstractRequest *
QContactTrackerEngine::createRequestWorker(QContactAbstractRequest *request)
{
    // about if mandatory security tokens are missing
    if (not checkSecurityTokens(request)) {
        return 0;
    }

    if (not checkThreadOfRequest(request)) {
        return 0;
    }

    // ensure old worker got destroyed when request gets reused
    requestDestroyed(request);

    return createRequestWorkerImpl(request);
}

QTrackerAbstractRequest *
QContactTrackerEngine::createRequestWorkerImpl(QContactAbstractRequest *request)
{

    QTrackerAbstractRequest *worker = 0;
    QElapsedTimer t; t.start();

    // choose proper request implementation
    switch(request->type())
    {
    case QContactAbstractRequest::ContactFetchRequest:
        worker = new QTrackerContactFetchRequest(request, this);
        break;

    case QContactAbstractRequest::ContactFetchByIdRequest:
        worker = new QTrackerContactFetchByIdRequest(request, this);
        break;

    case QContactAbstractRequest::ContactLocalIdFetchRequest:
        worker = new QTrackerContactIdFetchRequest(request, this);
        break;

    case QContactAbstractRequest::ContactRemoveRequest:
        if (0 == qobject_cast<QctContactMergeRequest *>(request)) {
            worker = new QTrackerContactRemoveRequest(request, this);
        } else {
            worker = new QTrackerContactCopyAndRemoveRequest(request, this);
        }
        break;

    case QContactAbstractRequest::ContactSaveRequest:
        if (0 == qobject_cast< QctUnmergeIMContactsRequest *> (request)) {
            worker = new QTrackerContactSaveRequest(request, this);
        } else {
            worker = new QTrackerContactSaveOrUnmergeRequest(request, this);
        }
        break;

    case QContactAbstractRequest::RelationshipFetchRequest:
        worker = new QTrackerRelationshipFetchRequest(request, this);
        break;

    case QContactAbstractRequest::RelationshipRemoveRequest:
        worker = new QTrackerRelationshipRemoveRequest(request, this);
        break;

    case QContactAbstractRequest::RelationshipSaveRequest:
        worker = new QTrackerRelationshipSaveRequest(request, this);
        break;

    case QContactAbstractRequest::DetailDefinitionFetchRequest:
        worker = new QTrackerDetailDefinitionFetchRequest(request, this);
        break;

    case QContactAbstractRequest::InvalidRequest:
    case QContactAbstractRequest::DetailDefinitionRemoveRequest:
    case QContactAbstractRequest::DetailDefinitionSaveRequest:
        break;
    }

    if (0 == worker) {
        qctWarn(QString::fromLatin1("Unsupported request type: %1").
                arg(QLatin1String(request->metaObject()->className())));
        return 0;
    }

    if (hasDebugFlag(ShowNotes)) {
        qDebug() << Q_FUNC_INFO << "time elapsed while constructing request workers:"<< request << t.elapsed();
    }

    if (hasDebugFlag(ShowNotes)) {
        qDebug() << Q_FUNC_INFO << "running" << worker->metaObject()->className();
    }

    // XXX The unit tests directly access the engine. Therefore requests created by our unit
    // tests don't have a manager attached. This prevents ~QContactAbstractRequest() to call our
    // engine's requestDestroyed(QContactAbstractRequest*) method, which results in memory leaks
    // within our our unit tests. To prevent those leaks we watch the request's destroyed()
    // signal and forward those signals to requestDestroyed(QContactAbstractRequest*) when
    // a request without contact manager is found.
    //
    // XXX This all of course is an ugly workaround. We probably should change the unit tests
    // to create use QContactManager accessing the engine via a static plugin.
    if (0 == request->manager()) {
        connect(request, SIGNAL(destroyed(QObject*)),
                this, SLOT(onRequestDestroyed(QObject*)), Qt::DirectConnection);
    }

    // track original request for this worker
    QCT_SYNCHRONIZED_WRITE(&d->m_tableLock);

    d->m_workersByRequest.insert(request, worker);
    d->m_requestsByWorker.insert(worker, request);

    return worker;
}

QctRequestLocker
QContactTrackerEngine::request(const QTrackerAbstractRequest *worker) const
{
    QScopedPointer<QMutexLocker> locker(new QMutexLocker(&d->m_requestLifeGuard));
    QCT_SYNCHRONIZED_READ(&d->m_tableLock);
    return QctRequestLocker(locker.take(), d->m_requestsByWorker.value(worker));
}

void
QContactTrackerEngine::connectSignals()
{
    if (d->m_changeListener) {
        connect(d->m_changeListener,
                SIGNAL(contactsAdded(QList<QContactLocalId>)),
                SIGNAL(contactsAdded(QList<QContactLocalId>)));
        connect(d->m_changeListener,
                SIGNAL(contactsChanged(QList<QContactLocalId>)),
                SIGNAL(contactsChanged(QList<QContactLocalId>)));
        connect(d->m_changeListener,
                SIGNAL(contactsRemoved(QList<QContactLocalId>)),
                SIGNAL(contactsRemoved(QList<QContactLocalId>)));
        connect(d->m_changeListener,
                SIGNAL(relationshipsAdded(QList<QContactLocalId>)),
                SIGNAL(relationshipsAdded(QList<QContactLocalId>)));
        connect(d->m_changeListener,
                SIGNAL(relationshipsRemoved(QList<QContactLocalId>)),
                SIGNAL(relationshipsRemoved(QList<QContactLocalId>)));
    }
}

void
QContactTrackerEngine::disconnectSignals()
{
    if (d->m_changeListener) {
        d->m_changeListener->disconnect(this);
        d->m_changeListener = 0;
    }
}

void
QContactTrackerEngine::registerGcQuery()
{
    d->m_gcQueryId = QString::fromLatin1("com.nokia.qtcontacts-tracker");
    QctGarbageCollector::registerQuery(d->m_gcQueryId, cleanupQueryString());
}

void
QContactTrackerEngine::connectNotify(const char *signal)
{
    if (0 == d->m_changeListener) {
        // Create the change listener on demand since:
        //
        // 1) Creating the listener is expensive as we must subscribe to some DBus signals.
        // 2) Watching DBus without any specific need wastes energy by wakeing up processes.
        //
        // Share the change listener for the reasons listed above.
        //
        // Still only share per thread (instead of process) to avoid nasty situations like the
        // random, listener owning thread terminating before other threads using the listener.

        // Must monitor individual contact classes instead of nco:Contact
        // to avoid bogus notifications for:
        //
        //  - QContactOrganization details, implemented via nco:OrganizationContact
        //  - incoming calls which cause call-ui to create temporary(?) nco:Contact instances
        //
        // Additionally, nco:Contact does not have the tracker:notify property set to true, whereas
        // nco:PersonContact and nco:ContactGroup do have the property, so only those classes will
        // receive notifications of changes.
        QSet<QString> contactClassIris;

        foreach (const QTrackerContactDetailSchema &schema, d->m_parameters.m_detailSchemas) {
            foreach(const QString &iri, schema.contactClassIris()) {
                if (iri != nco::Contact::iri()) {
                    contactClassIris += iri;
                }
            }
        }

        // Compute change filtering mode.
        QctTrackerChangeListener::ChangeFilterMode changeFilterMode = QctTrackerChangeListener::AllChanges;

        if (d->m_parameters.m_omitPresenceChanges) {
            changeFilterMode = QctTrackerChangeListener::IgnorePresenceChanges;
        }

        // Compute debug flags for the listener.
        QctTrackerChangeListener::DebugFlags debugFlags = QctTrackerChangeListener::NoDebug;

        if (d->m_parameters.m_debugFlags.testFlag(QContactTrackerEngine::ShowSignals)) {
            debugFlags |= QctTrackerChangeListener::PrintSignals;
        }

        // Compute cache id for the listener.
        // Cannot use the manager URI because it doesn't expose all relevant parameters.
        const QString listenerId = (QStringList(contactClassIris.toList()) <<
                                    QString::number(d->m_parameters.m_coalescingDelay) <<
                                    QString::number(changeFilterMode) <<
                                    QString::number(debugFlags)).join(QLatin1String(";"));

        // Check if there already exists a listener for that computed id.
        QctThreadLocalData *const threadLocalData = QctThreadLocalData::instance();
        d->m_changeListener = threadLocalData->trackerChangeListener(listenerId);

        if (0 == d->m_changeListener) {
            // Create new listener when needed.
            d->m_changeListener = new QctTrackerChangeListener(contactClassIris.toList(), QThread::currentThread());
            d->m_changeListener->setCoalescingDelay(d->m_parameters.m_coalescingDelay);
            d->m_changeListener->setChangeFilterMode(changeFilterMode);
            d->m_changeListener->setDebugFlags(debugFlags);

            threadLocalData->setTrackerChangeListener(listenerId, d->m_changeListener);
        }

        // Monitor the choosen listener's signals.
        connectSignals();
    }

    QContactManagerEngine::connectNotify(signal);
}

void
QContactTrackerEngine::requestDestroyed(QContactAbstractRequest *req)
{
    dropRequest(QctRequestLocker(new QMutexLocker(&d->m_requestLifeGuard), req));
}

void
QContactTrackerEngine::dropRequest(const QctRequestLocker &req)
{
    QCT_SYNCHRONIZED_WRITE(&d->m_tableLock);

    if (req.isNull()) {
        return;
    }

    QTrackerAbstractRequest *const worker = d->m_workersByRequest.take(req.data());

    if (0 != worker) {
        d->m_requestsByWorker.remove(worker);
    }
}

void
QContactTrackerEngine::onRequestDestroyed(QObject *req)
{
    // dynamic_cast<> does not work at this point (has a 0 result, or even crashes) because the
    // derived parts of the class have already been destroyed by the time the base QObject's
    // destructor has emitted this signal. So we do a regular static case, because
    // requestDestroyed(QContactAbstractRequest*) just compares the pointer value anyway.
    requestDestroyed(static_cast<QContactAbstractRequest *>(req));
}

bool
QContactTrackerEngine::waitForRequestFinished(QContactAbstractRequest *request, int msecs)
{
    if (not hasDebugFlag(SkipNagging)) {
        qctWarn(QString::fromLatin1("\n"
            "=============================================================================\n"
            "WARNING /!\\ - NEVER EVER CALL THIS FUNCTION FROM PRODUCTION CODE!!!\n"
            "=============================================================================\n"
            "QContactAbstractRequest::waitForFinished(), respectively\n"
            "QContactManagerEngine::waitForRequestFinished() must spin your\n"
            "application's event loop. Doing so will cause HAVOC AND PAIN for\n"
            "any non-trivial program.\n"
            "\n"
            "So please refactor your asynchronious code, or use the synchronious\n"
            "API if blocking your application is acceptable.\n"
            "\n"
            "WE MEAN IT!!!\n"
            "\n"
            "Offending application is %1 [%2].\n"
            "=============================================================================").
            arg(QCoreApplication::applicationFilePath(),
                QString::number(QCoreApplication::applicationPid())));
    }

    return waitForRequestFinishedImpl(request, msecs);
}

bool
QContactTrackerEngine::waitForRequestFinishedImpl(QContactAbstractRequest *request, int msecs)
{
    if (0 == request) {
        return false;
    }

    RequestEventLoop eventLoop(request, msecs);

    if (not eventLoop.isFinished()) {
        eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    return eventLoop.isFinished();
}

bool
QContactTrackerEngine::runSyncRequest(QContactAbstractRequest *request,
                                      QContactManager::Error *error) const
{
    // Copy the engine to prevent forwarding side-effects to the caller's engine.
    // It costs a bit, but guess that's a justified penalty to all sync API users.
    QContactTrackerEngine taskEngine(*this);

    QctTask *const task = taskEngine.startRequestImpl(request);

    if (0 != task && not QctTaskWaiter(task).wait(requestTimeout())) {
        qctPropagate(QContactManager::UnspecifiedError, error);
        return false;
    }

    qctPropagate(request->error(), error);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Synthesized contact details
////////////////////////////////////////////////////////////////////////////////////////////////////

QString
QContactTrackerEngine::synthesizedDisplayLabel(const QContact& contact,
                                               QContactManager::Error* error) const
{
    qctPropagate(QContactManager::NoError, error);

    return createDisplayLabel(contact);
}

QContact
QContactTrackerEngine::compatibleContact(const QContact &original, QContactManager::Error* error) const
{
    QContact contact = original;

    foreach(const QTrackerContactDetail &detail, schema(contact.type()).details()) {
        QList<QContactDetail> contactDetails = contact.details(detail.name());

        if (contactDetails.empty()) {
            continue;
        }

        // Check that detail respects our schema's cardinality
        if (not detail.isUnique()) {
            continue;
        }

        if (contactDetails.count() < 2) {
            continue;
        }

        qctWarn(QString::fromLatin1("Dropping odd details: %1 detail must be unique").
                arg(detail.name()));

        for(int i = 1; i < contactDetails.count(); ++i) {
            contact.removeDetail(&contactDetails[i]);
        }
    }

    // Check fields contents
    foreach(const QTrackerContactDetail &detail, schema(contact.type()).details()) {
        QList<QContactDetail> contactDetails = contact.details(detail.name());

        foreach (QContactDetail contactDetail, contactDetails) {
            const QVariantMap detailValues = contactDetail.variantValues();

            for (QVariantMap::ConstIterator it = detailValues.constBegin();
                 it != detailValues.constEnd(); ++it) {
                const QTrackerContactDetailField *field = detail.field(it.key());

                if (field == 0) {
                    // We can't validate custom fields, skip
                    continue;
                }

                QVariant computedValue;

                if (not field->makeValue(it.value(), computedValue)) {
                    // Apparently this is not an error, we just prune the field...
                    // makeValue already prints a debug message if it returns false,
                    // so we can stay silent here. No idea how UI can provide any
                    // useful feedback out of that.
                    contactDetail.removeValue(it.key());
                    break;
                }

                // The computed value is only useful to generate SPARQL queries,
                // we should not save it back to the field.
            }

            contact.saveDetail(&contactDetail);
        }
    }

    qctPropagate(QContactManager::NoError, error);

    return contact;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Unsupported functions
////////////////////////////////////////////////////////////////////////////////////////////////////

bool
QContactTrackerEngine::validateContact(const QContact&, QContactManager::Error* error) const
{
    REPORT_UNSUPPORTED_FUNCTION(error);
    return false;
}

bool
QContactTrackerEngine::validateDefinition(const QContactDetailDefinition&,
                                          QContactManager::Error* error) const
{
    REPORT_UNSUPPORTED_FUNCTION(error);
    return false;
}

bool
QContactTrackerEngine::cancelRequest(QContactAbstractRequest *request)
{
    if (0 == request) {
        return false;
    }

    QCT_SYNCHRONIZED(&d->m_requestLifeGuard);
    QCT_SYNCHRONIZED_READ(&d->m_tableLock);

    QTrackerAbstractRequest *const worker = d->m_workersByRequest.value(request);

    if (0 != worker) {
        return worker->cancel();
    }
    return false;
}

bool
QContactTrackerEngine::isRelationshipTypeSupported(const QString& relationshipType,
                                                   const QString& contactType) const
{
    if (relationshipType == QContactRelationship::HasMember
            && supportedContactTypes().contains(contactType)
            && (contactType == QContactType::TypeContact
                || contactType == QContactType::TypeGroup)) {
        return true;
    }

    return false;
}
