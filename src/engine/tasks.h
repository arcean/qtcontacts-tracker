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

#ifndef TASKS_H
#define TASKS_H

///////////////////////////////////////////////////////////////////////////////

#include "abstractrequest.h"

#include <lib/queue.h>

///////////////////////////////////////////////////////////////////////////////

class QctSparqlResolver;
class QTrackerContactDetailSchema;

///////////////////////////////////////////////////////////////////////////////

class QctResourceCacheTask : public QctTask
{
    Q_OBJECT

public:
    explicit QctResourceCacheTask(const QMap<QString, QTrackerContactDetailSchema> &schemas,
                                  QObject *parent = 0);
    virtual ~QctResourceCacheTask();

public: // QctTask interface
    void run();

private:
    const QStringList m_iris;
};

///////////////////////////////////////////////////////////////////////////////

class QctGuidAlgorithmTask : public QctTask
{
    Q_OBJECT

public:
    QctGuidAlgorithmTask(QContactTrackerEngine *engine,
                         QObject *parent = 0);
    virtual ~QctGuidAlgorithmTask();

public: // QctTask interface
    void run();

private slots:
    void resolverReady();

private: // members
    QContactTrackerEngine *m_engine;
};

///////////////////////////////////////////////////////////////////////////////

class QctRequestTask : public QctTask
{
    Q_OBJECT

public:
    explicit QctRequestTask(QContactAbstractRequest *request,
                            QTrackerAbstractRequest *worker,
                            QObject *parent = 0);
    virtual ~QctRequestTask();

public: // attributes
    QTrackerAbstractRequest::Dependencies dependencies() const;

public: // QctTask interface
    void run();

private slots:
    void onStateChanged(QContactAbstractRequest::State state);
    void onRequestDestroyed();

private:
    QTrackerAbstractRequest *m_worker;
};

///////////////////////////////////////////////////////////////////////////////

#endif // TASKS_H
