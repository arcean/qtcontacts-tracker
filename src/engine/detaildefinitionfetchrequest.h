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

#ifndef QTRACKERDETAILDEFINITIONFETCHREQUEST_H_
#define QTRACKERDETAILDEFINITIONFETCHREQUEST_H_

#include "baserequest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerDetailDefinitionFetchRequest: public QTrackerBaseRequest<QContactDetailDefinitionFetchRequest>
{
    Q_DISABLE_COPY(QTrackerDetailDefinitionFetchRequest)
    Q_OBJECT

public:
    QTrackerDetailDefinitionFetchRequest(QContactAbstractRequest *request,
                                         QContactTrackerEngine *engine,
                                         QObject *parent = 0);
    virtual ~QTrackerDetailDefinitionFetchRequest();

protected: // QTrackerAbstractRequest API
    void run();
    void updateRequest(QContactManager::Error error);

private: // fields
    const QString m_contactType;
    const QStringList m_definitionNames;

    QMap<QString, QContactDetailDefinition> m_detailDefinitions;
    QMap<int, QContactManager::Error> m_errorMap;
};

#endif /* QTRACKERDETAILDEFINITIONFETCHREQUEST_H_ */
