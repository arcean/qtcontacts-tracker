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

#include "queue.h"

#include "logger.h"
#include "threadutils.h"

////////////////////////////////////////////////////////////////////////////////

class QctQueueData : public QSharedData
{
public:
    QThread m_backgroundThread;
    QctTaskQueue *m_queue;
    QMutex m_queueMutex;
};

////////////////////////////////////////////////////////////////////////////////

QctTask::QctTask(QObject *parent)
    : QObject(parent)
{
}

QctTask::~QctTask()
{
    Q_ASSERT(QThread::currentThread() == thread());
}

////////////////////////////////////////////////////////////////////////////////

QctQueue::QctQueue(QObject *parent)
    : QObject(parent)
    , d(new QctQueueData)
{
    d->m_queue = new QctTaskQueue;
    d->m_backgroundThread.start();
}

QctQueue::~QctQueue()
{
    {
        QCT_SYNCHRONIZED(&d->m_queueMutex);

        // Remember the queue, so that we can clean it up,
        // but prevent that any new tasks are added or removed.
        QScopedPointer<QctTaskQueue> shutdownTaskQueue(d->m_queue);
        d->m_queue = 0;

        // Also keep the queue mutex locked for the following cleanup steps to prevent that
        // the currently active task emits finish() and therefore quits the background thread
        // before we got the chance to request deletion of the still queued tasks.
        if (shutdownTaskQueue->isEmpty()) {
            // If there is no active task just quit the background thread now.
            d->m_backgroundThread.quit();
        } else {
            // Otherwise take the currently running task out of the shooting line:
            // We rely on its destroyed() signal for terminating the background thread.
            shutdownTaskQueue->dequeue();

            // All other tasks in the queue have not been started yet, so we can just destroy them.
            // We only must make sure to disconnect from their destroyed() signal first, to avoid
            // that emission of their destroyed() signal quits the background thread.
            foreach(QctTask *task, *shutdownTaskQueue) {
                task->disconnect(this);
                task->deleteLater(); // ensure it is deleted in the background thread
            }
        }
    }

    // Ensure the background quits even if the application currently gets shutdown
    // and event delivery is becoming disfunctional.
    if (qApp->closingDown()) {
        d->m_backgroundThread.quit();
    }

    // Wait for the background thread's run() method to finish.
    // At latest it will once the last task object is being destroyed and emits its
    // destroyed() signal. If the queue already was empty when this destructor was entered,
    // then the thread just quit just above.
    while(not d->m_backgroundThread.wait(3000)) {
        qctWarn("The task queue's background thread stalled");
    }
}

////////////////////////////////////////////////////////////////////////////////

/*!
 * Puts the \p task into the queue. The queue takes ownership of the \p task.
 */
void
QctQueue::enqueue(QctTask *task)
{
    if (task->thread() != QThread::currentThread()) {
        qctWarn(QString().sprintf("Current thread (%p) is not the task's thread (%p).",
                                  QThread::currentThread(), task->thread()));
        delete task; // avoid leaking the task
        return;
    }

    QCT_SYNCHRONIZED(&d->m_queueMutex);

    if (0 == d->m_queue) {
        qctWarn("Cannot accept new tasks, the queue has been closed.");
        delete task; // avoid leaking the task
        return;
    }

    // Take ownership of the task. Must move to background thread here,
    // since moveToThread() only works from an object's current thread.
    task->setParent(0);
    task->moveToThread(&d->m_backgroundThread);
    d->m_queue->enqueue(task);

    // Connect signals to remove finished tasks from queue.
    connect(task, SIGNAL(finished()),
            task, SLOT(deleteLater()), Qt::QueuedConnection);
    connect(task, SIGNAL(destroyed(QObject*)),
            this, SLOT(onTaskDestroyed(QObject*)), Qt::DirectConnection);

    // Run the task when it was empty before.
    if (task == d->m_queue->head()) {
        processQueue();
    }
}

void
QctQueue::dequeue(QctTask *task)
{
    QCT_SYNCHRONIZED(&d->m_queueMutex);

    if (0 == d->m_queue) {
        // This was the last task of an shutdown queue.
        // So it's about time to quit the background thread.
        d->m_backgroundThread.quit();
    } else if (not d->m_queue->isEmpty()) {
        QctTask *const head = d->m_queue->head();

        if (d->m_queue->removeOne(task) && task == head) {
            // Run next task if the remove task was the currently active task.
            processQueue();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void
QctQueue::processQueue()
{
    const bool queueWasNotLocked = d->m_queueMutex.tryLock();

    if (queueWasNotLocked) {
        qctWarn("Queue must be called with the queue locked!");
    }

    if (d->m_queue != 0 && not d->m_queue->isEmpty()) {
        // QctTask::run() shall run within the background thread. It is always queued, even
        // when processQueue() is called from that said thread, to prevent a dead locking,
        // when run() terminates instantly and emits() finished from inside.
        QctTask *const task = d->m_queue->head();
        QctTask::staticMetaObject.invokeMethod(task, "run", Qt::QueuedConnection);
    }

    if (queueWasNotLocked) {
        d->m_queueMutex.unlock();
    }
}

////////////////////////////////////////////////////////////////////////////////

void
QctQueue::onTaskDestroyed(QObject *object)
{
    dequeue(static_cast<QctTask *>(object));
}
