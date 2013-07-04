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

#ifndef QTRACKERCONTACTFETCHREQUEST_H
#define QTRACKERCONTACTFETCHREQUEST_H

#include "basecontactfetchrequest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////


class QTrackerContactFetchRequest : public QTrackerBaseContactFetchRequest<QContactFetchRequest>
{
    Q_DISABLE_COPY(QTrackerContactFetchRequest)
    Q_OBJECT

public:
    explicit QTrackerContactFetchRequest(QContactAbstractRequest *request,
                                         QContactTrackerEngine *engine,
                                         QObject *parent = 0);
    virtual ~QTrackerContactFetchRequest();

protected: // QTrackerAbstractContactFetchRequest API
    void processResults(const ContactCache &results);

protected: // QTrackerAbstractRequest API
    void updateRequest(QContactManager::Error error);

private:
    void mergeContactLists(QList<QContact> &l1, const QList<QContact> &l2);

private: // fields
    const QString   m_nameOrder;

    QList<QContact> m_contacts;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTFETCHREQUEST_H
