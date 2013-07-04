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

#ifndef BASECONTACTFETCHREQUEST_H
#define BASECONTACTFETCHREQUEST_H

#include "abstractcontactfetchrequest.h"

template<class T>
class QTrackerBaseContactFetchRequest : public QTrackerAbstractContactFetchRequest
{
protected:
    QTrackerBaseContactFetchRequest(QContactAbstractRequest *request,
                                    const QContactFilter &filter,
                                    const QContactFetchHint &fetchHint,
                                    const QList<QContactSortOrder> &sorting,
                                    QContactTrackerEngine *engine,
                                    QObject *parent = 0)
        : QTrackerAbstractContactFetchRequest(request, filter, fetchHint, sorting, engine, parent)
    {
    }

protected:
    static const T * staticCast(const QContactAbstractRequest *request)
    {
        return static_cast<const T *>(request);
    }

    static T * staticCast(QContactAbstractRequest *request)
    {
        return static_cast<T *>(request);
    }
};

#endif // BASECONTACTFETCHREQUEST_H
