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

#ifndef QCTQUEUE_H
#define QCTQUEUE_H

/**
 * \class QctQueue
 *
 * Queue items are subclasses of the QctTask class. The lifetime of a
 * QctTask is handled either by the task itself (calling deleteLater when
 * it's done) or by QObject parenting. The tasks does not own external
 * objects, that is, QctResolverTask can delete the resolver because it
 * created it, but QctRequestTask does not delete the request. QctTask is a
 * controller, not a wrapper.
 *
 * QctQueue is NOT reentrant, that means if you nest calls modifying the
 * queue, you'll hit a deadlock. To make the queue reentrant, we'd have to
 * move some method calls to the mainloop, but then that fails is no global
 * mainloop is running, which is the case for tests for example.
 */

///////////////////////////////////////////////////////////////////////////////

#include <QtCore>

#include "libqtcontacts_extensions_tracker_global.h"

///////////////////////////////////////////////////////////////////////////////

typedef QQueue<class QctTask *> QctTaskQueue;

///////////////////////////////////////////////////////////////////////////////

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctTask : public QObject
{
    Q_OBJECT

public:
    explicit QctTask(QObject *parent = 0);
    virtual ~QctTask();

public slots: // abstract interface
    virtual void run() = 0;

signals:
    void finished(QctTask *task = 0);
};

///////////////////////////////////////////////////////////////////////////////

class QctQueueData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctQueue : public QObject
{
    Q_OBJECT

public:
    explicit QctQueue(QObject *parent = 0);
    virtual ~QctQueue();

public: // methods
    void enqueue(QctTask *task);
    void dequeue(QctTask *task);

private:
    void processQueue();

private slots:
    void onTaskDestroyed(QObject *object);

private:
    QExplicitlySharedDataPointer<QctQueueData> d;
};

///////////////////////////////////////////////////////////////////////////////

#endif // QCTQUEUE_H
