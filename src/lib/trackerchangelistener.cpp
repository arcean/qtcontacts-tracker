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

#include "trackerchangelistener.h"

#include "constants.h"
#include "logger.h"
#include "resolvertask.h"
#include "sparqlconnectionmanager.h"
#include "sparqlresolver.h"

#include <ontologies/nco.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////////////////////////

QctTrackerChangeListener::QctTrackerChangeListener(const QStringList &classIris,
                                                   QObject *parent)
    : QObject(parent)
    , m_classIris(classIris)
    , m_debugFlags(NoDebug)
    , m_changeFilterMode(AllChanges)
{
    // setup m_signalCoalescingTimer
    m_signalCoalescingTimer.setInterval(DefaultCoalescingDelay);
    m_signalCoalescingTimer.setSingleShot(true);
    m_signalCoalescingTimer.stop();

    connect(&m_signalCoalescingTimer, SIGNAL(timeout()), SLOT(emitQueuedNotifications()));

    // Connecting to those signals early should prevent losing signals while
    // we wait for the tracker:id() resolver.
    foreach(const QString &iri, classIris) {
        connect(new TrackerChangeNotifier(iri, this),
                SIGNAL(changed(QList<TrackerChangeNotifier::Quad>,
                               QList<TrackerChangeNotifier::Quad>)),
                SLOT(onGraphChanged(QList<TrackerChangeNotifier::Quad>,
                                    QList<TrackerChangeNotifier::Quad>)));
    }

    // Cannot use the engine's task queue because change listeners are attached to their thread,
    // not their engine.
    m_taskQueue = new QctQueue(this);

    resolveTrackerIds();
}

QctTrackerChangeListener::~QctTrackerChangeListener()
{
    // Basically to make sure we wait for pending tasks: We cannot rely on QObject's children
    // deletion here because its behavior is too random for our purposes here. For instance,
    // depending on object initialization order, m_resolver could get deleted by that mechanism
    // before m_taskQueue, leading to m_taskQueue operating with a dead resolver. Note that
    // QScopedPointer suffers from some similiar problem: There we'd depend on order of
    // declaration.
    resetTaskQueue();
}

QStringList
QctTrackerChangeListener::classIris() const
{
    return m_classIris;
}

QctTrackerChangeListener::DebugFlags
QctTrackerChangeListener::debugFlags() const
{
    return m_debugFlags;
}

void
QctTrackerChangeListener::setDebugFlags(DebugFlags flags)
{
    m_debugFlags = flags;
}

QctTrackerChangeListener::ChangeFilterMode
QctTrackerChangeListener::changeFilterMode() const
{
    return m_changeFilterMode;
}

void
QctTrackerChangeListener::setChangeFilterMode(ChangeFilterMode mode)
{
    m_changeFilterMode = mode;
}

int
QctTrackerChangeListener::coalescingDelay() const
{
    return m_signalCoalescingTimer.interval();
}

void
QctTrackerChangeListener::setCoalescingDelay(int delay)
{
    m_signalCoalescingTimer.setInterval(delay);
}

void
QctTrackerChangeListener::resolveTrackerIds()
{
    // Store some frequently needed resource ids.
    const QStringList resourceIris = QStringList() << nco::belongsToGroup::iri()
                                                   << nco::contactLocalUID::iri()
                                                   << rdf::type::iri()
                                                   << nie::contentLastModified::iri()
                                                   << QtContactsTrackerDefaultGraphIri
                                                   << classIris();

    // Keep ownership of the object, so that we can reliably access it in onTrackerIdsResolved().
    m_resolver = new QctTrackerIdResolver(resourceIris, this);
    QScopedPointer<QctResolverTask> task(new QctResolverTask(m_resolver));

    // The task will run in a different thread, therefore this signal will be delivered per
    // queued connection. Note that the task might have been deleted already in its thread,
    // when the listener finally processes its finished() signal.
    connect(task.data(), SIGNAL(finished()),
            this, SLOT(onTrackerIdsResolved()));
    m_taskQueue->enqueue(task.take());
}

void
QctTrackerChangeListener::onTrackerIdsResolved()
{
    // This slot really must run in the listener's own thread. Direct connection would be harmful.
    Q_ASSERT(QThread::currentThread() == thread());

    if (0 == m_resolver) {
        qctWarn("the change listener's resolver has been deleted already");
        return;
    }

    if (m_resolver->resourceIris().count() != m_resolver->trackerIds().count()) {
        qctWarn("the change listener's resolver task failed");
        return;
    }

    QList<uint> trackerIds = m_resolver->trackerIds();
    m_trackerIds.belongsToGroup = trackerIds.takeFirst();
    m_trackerIds.contactLocalUID = trackerIds.takeFirst();
    m_trackerIds.rdfType = trackerIds.takeFirst();
    m_trackerIds.contentLastModified = trackerIds.takeFirst();
    m_trackerIds.graph = trackerIds.takeFirst();

    // If the listener is instantiated before any contact was saved in qct's graph,
    // then the graph will not exist and its tracker:id will be 0. In that case, we
    // create the graph and redo the resolving
    if (m_trackerIds.graph == 0) {
        const QString sparqlQuery = QString::fromLatin1("INSERT {"
                                                        "  <%1> a rdfs:Resource;"
                                                        "  rdfs:label \"QtContacts-Tracker graph\""
                                                        "}").arg(QtContactsTrackerDefaultGraphIri);

        QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();
        QScopedPointer<QSparqlResult> result(connection.exec(QSparqlQuery(sparqlQuery, QSparqlQuery::InsertStatement)));
        connect(result.take(), SIGNAL(finished()), this, SLOT(onGraphInsertQueryFinished()));

        return;
    }

    // Only fill contactClasses at this point, so that if we returned early
    // to insert the graph we'll still queue the notifications
    // (onGraphChanged() will start m_signalCoalescingTimer if contactClasses
    // is not empty)
    m_trackerIds.contactClasses = trackerIds;

    emitQueuedNotifications();

    resetTaskQueue();
}

void
QctTrackerChangeListener::onGraphInsertQueryFinished()
{
    QSparqlResult *result = qobject_cast<QSparqlResult*>(sender());

    if (result == 0) {
        qctWarn("Unexpected null QSparqlResult");
        return;
    }

    if (result->hasError()) {
        qctWarn("Could not create the graph for QtContacts-Tracker");
        result->deleteLater();
        return;
    }

    result->deleteLater();
    resolveTrackerIds();
}

void
QctTrackerChangeListener::resetTaskQueue()
{
    // Delete the task queue before we delete the resolver to avoid the risk of operating
    // on a dead resolver. The destructor will block if currently some task is running.

    delete m_taskQueue;
    m_taskQueue = 0;

    delete m_resolver;
    m_resolver = 0;
}

void
QctTrackerChangeListener::processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                                               QSet<QContactLocalId> &additionsOrRemovals,
                                               QSet<QContactLocalId> &relationshipChanges,
                                               QSet<QContactLocalId> &propertyChanges,
                                               bool matchTaggedSignals)
{
    QSet<QContactLocalId> presenceChangedIds;

    foreach(const TrackerChangeNotifier::Quad &quad, notifications) {
        // Contactsd will always "tag" the updates on nco:PersonContact with its
        // own graph if the update is not significant for clients not interested
        // in IM stuff (presence change would be tagged, adding a birthday/phone
        // number would not be tagged). If we detect an update on a contact that
        // is outside our graph (and not in the default one), then we don't emit
        // a change signal for that contact.
        // The tagging of the signal is done by contactsd by inserting a "dummy"
        // nie:contentLastModified statement in contactsd's graph that does not
        // change the final inserted data. So the data present in Tracker in the
        // end does not change, but the GraphUpdated signal mentions this dummy
        // insert in contactsd graph.
        if (matchTaggedSignals && m_changeFilterMode != AllChanges) {
            if (uint(quad.graph) != m_trackerIds.graph && quad.graph != 0
             && uint(quad.predicate) == m_trackerIds.contentLastModified) {
                presenceChangedIds += quad.subject;
                continue;
            }
        }

        // Changes to rdf:type are the most reliable indication of resource creation and removal.
        if (m_trackerIds.rdfType == uint(quad.predicate)) {
            if (m_trackerIds.contactClasses.contains(quad.object)) {
                additionsOrRemovals += quad.subject;
            }
            continue;
        }

        // Changes to nco:belongsToGroup indicate group membership changes.
        if (m_trackerIds.belongsToGroup == uint(quad.predicate)) {
            relationshipChanges += quad.subject;
            relationshipChanges += quad.object;
            continue;
        }

        // All other property changes indicate a contact change,
        // except for changes to nco:contactLocalUID which happen
        // when that property is updated for backward compatiblity.
        if (m_trackerIds.contactLocalUID != uint(quad.predicate)) {
            propertyChanges += quad.subject;
        }
    }

    // We only ignore IDs for property changes, contact creation/deletion is
    // always significant, and relationships are handled only through contacts
    // application
    switch (m_changeFilterMode) {
    case IgnorePresenceChanges:
        propertyChanges -= presenceChangedIds;
        break;
    case OnlyPresenceChanges:
        propertyChanges = presenceChangedIds;
        break;
    default:
        break;
    }

    // Hopefully not bogus optimization:
    // Avoid change notifications wich overlap with addition or removal notifications.
    propertyChanges -= additionsOrRemovals;

    // Clear processed notifcation queue.
    notifications.clear();
}

void
QctTrackerChangeListener::onGraphChanged(const QList<TrackerChangeNotifier::Quad>& deletes,
                                         const QList<TrackerChangeNotifier::Quad>& inserts)
{
    if (m_debugFlags.testFlag(PrintSignals)) {
        TrackerChangeNotifier *const notifier = static_cast<TrackerChangeNotifier *>(sender());

        qDebug() << notifier->watchedClass() << "- deletes:" << deletes;
        qDebug() << notifier->watchedClass() << "- inserts:" << inserts;
    }

    m_deleteNotifications += deletes;
    m_insertNotifications += inserts;

    // minimally delay signal emission to give a chance for signals getting coalesced
    if (not m_trackerIds.contactClasses.isEmpty() && not m_signalCoalescingTimer.isActive()) {
        m_signalCoalescingTimer.start();
    }
}

void
QctTrackerChangeListener::emitQueuedNotifications()
{
    QSet<QContactLocalId> contactsAddedIds, contactsRemovedIds, contactsChangedIds;
    QSet<QContactLocalId> relationshipsAddedIds, relationshipsRemovedIds;

    // process notification queues...
    // We never match tagged signals on DELETEs, since the graph information is
    // not reliable there (see GB#659936)
    processNotifications(m_deleteNotifications, contactsRemovedIds,
                         relationshipsRemovedIds, contactsChangedIds,
                         false);
    processNotifications(m_insertNotifications, contactsAddedIds,
                         relationshipsAddedIds, contactsChangedIds,
                         true);

    // ...report identified changes when requested...
    if (m_debugFlags.testFlag(PrintSignals)) {
        qDebug() << "added contacts:" << contactsAddedIds.count() << contactsAddedIds;
        qDebug() << "changed contacts:" << contactsChangedIds.count() << contactsChangedIds;
        qDebug() << "removed contacts:" << contactsRemovedIds.count() << contactsRemovedIds;
        qDebug() << "added relationships:" << relationshipsAddedIds.count() << relationshipsAddedIds;
        qDebug() << "removed relationships:" << relationshipsRemovedIds.count() << relationshipsRemovedIds;
    }

    // ...and finally emit the signals.
    if (not contactsAddedIds.isEmpty()) {
        emit contactsAdded(contactsAddedIds.toList());
    }

    if (not contactsChangedIds.isEmpty()) {
        emit contactsChanged(contactsChangedIds.toList());
    }

    if (not contactsRemovedIds.isEmpty()) {
        emit contactsRemoved(contactsRemovedIds.toList());
    }

    if (not relationshipsAddedIds.isEmpty()) {
        emit relationshipsAdded(relationshipsAddedIds.toList());
    }

    if (not relationshipsRemovedIds.isEmpty()) {
        emit relationshipsRemoved(relationshipsRemovedIds.toList());
    }
}
