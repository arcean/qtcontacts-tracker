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

#ifndef QTRACKERCONTACTIDFETCHREQUEST_H
#define QTRACKERCONTACTIDFETCHREQUEST_H

#include "baserequest.h"

#include <QtSparql>

////////////////////////////////////////////////////////////////////////////////////////////////////

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactIdFetchRequest : public QTrackerBaseRequest<QContactLocalIdFetchRequest>
{
    Q_DISABLE_COPY(QTrackerContactIdFetchRequest)
    Q_OBJECT

public:
    explicit QTrackerContactIdFetchRequest(QContactAbstractRequest *request,
                                            QContactTrackerEngine *engine,
                                            QObject *parent = 0);
    virtual ~QTrackerContactIdFetchRequest();


    QString buildQuery(QContactManager::Error &error, bool &sortable);

protected: // QTrackerAbstractRequest API
    void run();
    void updateRequest(QContactManager::Error error);

private: // methods
    void runNative(const QString &queryString);
    void runEmulated();

private: // fields
    const QContactFilter  m_filter;
    QList<QContactLocalId> m_localIds;
    QList<QContactSortOrder> m_sorting;
    int m_limit;
    bool m_forceNative : 1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTIDFETCHREQUEST_H
