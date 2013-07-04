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

#ifndef TRACKERCHANGELISTENER_H_
#define TRACKERCHANGELISTENER_H_

#include <qtcontacts.h>

#include <QtCore/QTimer>
#include <QtSparqlTrackerExtensions/TrackerChangeNotifier>

#include "libqtcontacts_extensions_tracker_global.h"

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QContactTrackerEngineParameters;

class QctQueue;
class QctTrackerIdResolver;

////////////////////////////////////////////////////////////////////////////////////////////////////

/*!
 * \class QctTrackerChangeListener
 * \brief Translates signals from tracker and to contact signals
 * Listens for tracker signals, computes which contacts are (and what is) changed and emits
 * signals about contact change. Initially designed to compute change signals for
 * QContactTrackerEngine
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctTrackerChangeListener : public QObject
{
    Q_OBJECT

public:
    /*! This enum describes the listener's debug mode */
    enum DebugFlag {
        NoDebug                = 0,     /*!< No debug messages will be printed */
        PrintSignals           = 1 << 0 /*!< Debug messages will be printed for each change signal */
    };
    Q_DECLARE_FLAGS(DebugFlags, DebugFlag)

    /*! This enum describes the listener's change filtering policy */
    enum ChangeFilterMode {
        AllChanges, /*!< Emit signals for all changes */
        IgnorePresenceChanges, /*!< Ignore changes if only the QContactPresence detail was changed */
        OnlyPresenceChanges /*!< Ignore changes if a detail other than QContactPresence was changed */
    };

    /*! The default signal coalescing delay (in ms) */
    static const int DefaultCoalescingDelay = 10;

    /*!
     * Constructs a new change listener watching the classes specified in \p classIris .
     *
     * \note The classes watched must be annotated with the \c tracker:notify property
     * in the ontology, else Tracker will emit no change signals for them.
     *
     * If many signals are received in a short interval, they will be batched together.
     * The batching interval can be controlled using the \p coalescingDelay parameter.
     */
    explicit QctTrackerChangeListener(const QStringList &classIris,
                                      QObject *parent = 0);
    virtual ~QctTrackerChangeListener();

public:
    QStringList classIris() const;

    DebugFlags debugFlags() const;
    void setDebugFlags(DebugFlags mode);

    ChangeFilterMode changeFilterMode() const;

    /*!
     * Sets the listener's change filtering mode. Change filtering can be used to
     * reduce the amount of signals received by the application.
     *
     * \sa QctTrackerChangeListener::ChangeFilterMode
     */
    void setChangeFilterMode(ChangeFilterMode mode);

    int coalescingDelay() const;

    /*!
     * Sets the listener's coalescing delay. Changes will be batched together so that
     * no more than one signal every \p delay milliseconds will be emitted.
     *
     * \sa QctTrackerChangeListener::DebugFlags
     */
    void setCoalescingDelay(int delay);

Q_SIGNALS:
    // signals are with the same semantics as in QContactManagerEngine
    void contactsAdded(const QList<QContactLocalId>& contactIds);
    void contactsChanged(const QList<QContactLocalId>& contactIds);
    void contactsRemoved(const QList<QContactLocalId>& contactIds);
    void relationshipsAdded(const QList<QContactLocalId>& affectedContactIds);
    void relationshipsRemoved(const QList<QContactLocalId>& affectedContactIds);

private Q_SLOTS:
    void onGraphChanged(const QList<TrackerChangeNotifier::Quad>& deletes,
                        const QList<TrackerChangeNotifier::Quad>& inserts);
    void onTrackerIdsResolved();
    void onGraphInsertQueryFinished();

    void emitQueuedNotifications();

private:
    void resolveTrackerIds();
    void processNotifications(QList<TrackerChangeNotifier::Quad> &notifications,
                              QSet<QContactLocalId> &additionsOrRemovals,
                              QSet<QContactLocalId> &relationshipChanges,
                              QSet<QContactLocalId> &propertyChanges,
                              bool matchTaggedSignals);

    void resetTaskQueue();

private:
    const QStringList m_classIris;

    QList<TrackerChangeNotifier::Quad> m_deleteNotifications;
    QList<TrackerChangeNotifier::Quad> m_insertNotifications;


    QctTrackerIdResolver *m_resolver;
    QctQueue *m_taskQueue;

    QTimer m_signalCoalescingTimer;

    struct {
        QList<uint> contactClasses;
        uint belongsToGroup;
        uint contactLocalUID;
        uint rdfType;
        uint contentLastModified;
        uint graph;
    } m_trackerIds;

    DebugFlags m_debugFlags;
    ChangeFilterMode m_changeFilterMode;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QctTrackerChangeListener::DebugFlags)

#endif /* TRACKERCHANGELISTENER_H_ */
