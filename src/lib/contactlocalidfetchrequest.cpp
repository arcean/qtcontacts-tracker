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

#include "contactlocalidfetchrequest.h"
#include "threadutils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctContactLocalIdFetchRequestData : public QObjectUserData
{
public:
    QctContactLocalIdFetchRequestData()
        : QObjectUserData()
        , m_limit(-1)
        , m_forceNative(false)
    {
    }

public: // attributes
    void setLimit(int limit)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_limit = limit;
    }

    int limit() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_limit;
    }

    void setForceNative(bool force)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_forceNative = force;
    }

    bool forceNative() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_forceNative;
    }

    static uint id()
    {
        static const uint userDataId = QObject::registerUserData();
        return userDataId;
    }

private: // fields
    mutable QReadWriteLock m_lock;

    int m_limit;
    bool m_forceNative : 1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctContactLocalIdFetchRequest::QctContactLocalIdFetchRequest(QObject *parent)
    : QContactLocalIdFetchRequest(parent)
{
    setUserData(QctContactLocalIdFetchRequestData::id(), new QctContactLocalIdFetchRequestData);
}

void
QctContactLocalIdFetchRequest::setLimit(int limit)
{
    data()->setLimit(limit);
}

int
QctContactLocalIdFetchRequest::limit() const
{
    return data()->limit();
}

void
QctContactLocalIdFetchRequest::setForceNative(bool force)
{
    data()->setForceNative(force);
}

bool
QctContactLocalIdFetchRequest::forceNative() const
{
    return data()->forceNative();
}

const QctContactLocalIdFetchRequestData *
QctContactLocalIdFetchRequest::data() const
{
    return static_cast<const QctContactLocalIdFetchRequestData *>
            (userData(QctContactLocalIdFetchRequestData::id()));
}

QctContactLocalIdFetchRequestData *
QctContactLocalIdFetchRequest::data()
{
    return static_cast<QctContactLocalIdFetchRequestData *>
            (userData(QctContactLocalIdFetchRequestData::id()));
}
