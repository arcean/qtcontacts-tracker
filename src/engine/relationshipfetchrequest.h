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

#ifndef QTRACKERRELATIONSHIPFETCHREQUEST_H_
#define QTRACKERRELATIONSHIPFETCHREQUEST_H_

#include "baserequest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerRelationshipFetchRequest: public QTrackerBaseRequest<QContactRelationshipFetchRequest>
{
    Q_DISABLE_COPY(QTrackerRelationshipFetchRequest)
    Q_OBJECT

public:
    QTrackerRelationshipFetchRequest(QContactAbstractRequest *request,
                                     QContactTrackerEngine *engine,
                                     QObject *parent = 0);
    virtual ~QTrackerRelationshipFetchRequest();

protected: // QTrackerAbstractRequest API
    void run();
    void updateRequest(QContactManager::Error error);

private: // fields
    const QContactId m_firstContactId;
    const QContactId m_secondContactId;
    const QString m_relationshipType;

    QList<QContactRelationship> m_relationships;
};

#endif /* QTRACKERRELATIONSHIPFETCHREQUEST_H_ */
