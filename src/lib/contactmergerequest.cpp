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

#include "contactmergerequest.h"
#include "threadutils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctContactMergeRequestData : public QObjectUserData
{
public: // attributes
    void setMergeIds(const QMultiMap<QContactLocalId, QContactLocalId> ids)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_mergeIds = ids;
    }

    QMultiMap<QContactLocalId, QContactLocalId> mergeIds() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_mergeIds;
    }

    static uint id()
    {
        static const uint userDataId = QObject::registerUserData();
        return userDataId;
    }

private: // fields
    mutable QReadWriteLock m_lock;

    QMultiMap<QContactLocalId, QContactLocalId> m_mergeIds;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctContactMergeRequest::QctContactMergeRequest(QObject *parent)
    : QContactRemoveRequest(parent)
{
    setUserData(QctContactMergeRequestData::id(), new QctContactMergeRequestData);
}

void
QctContactMergeRequest::setMergeIds(const QMultiMap<QContactLocalId, QContactLocalId> ids)
{
    data()->setMergeIds(ids);
    // All merge sources will be removed
    setContactIds(ids.values());
}

QMultiMap<QContactLocalId, QContactLocalId>
QctContactMergeRequest::mergeIds() const
{
    return data()->mergeIds();
}

const QctContactMergeRequestData *
QctContactMergeRequest::data() const
{
    return static_cast<const QctContactMergeRequestData *>
            (userData(QctContactMergeRequestData::id()));
}

QctContactMergeRequestData *
QctContactMergeRequest::data()
{
    return static_cast<QctContactMergeRequestData *>
            (userData(QctContactMergeRequestData::id()));
}
