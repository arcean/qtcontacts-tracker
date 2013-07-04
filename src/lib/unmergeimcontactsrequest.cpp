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

#include "unmergeimcontactsrequest.h"
#include "threadutils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctUnmergeIMContactsRequestData : public QObjectUserData
{
public: // attributes
    void setSourceContact(const QContact &contact)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_sourceContact = contact;
    }

    QContact sourceContact() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_sourceContact;
    }

    void setUnmergeOnlineAccounts(const QList<QContactOnlineAccount> &accounts)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_unmergeOnlineAccounts = accounts;
    }

    QList<QContactOnlineAccount> unmergeOnlineAccounts() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_unmergeOnlineAccounts;
    }

    void setUnmergedContactIds(const QList<QContactLocalId> &ids)
    {
        QCT_SYNCHRONIZED_WRITE(&m_lock);
        m_unmergedContactIds = ids;
    }

    QList<QContactLocalId> unmergedContactIds() const
    {
        QCT_SYNCHRONIZED_READ(&m_lock);
        return m_unmergedContactIds;
    }

    static uint id()
    {
        static const uint userDataId = QObject::registerUserData();
        return userDataId;
    }

private: // fields
    mutable QReadWriteLock m_lock;

    // We don't use the contacts list from QContactSaveRequest, because it gets
    // modified by Qt Mobility
    QContact m_sourceContact;
    QList<QContactOnlineAccount> m_unmergeOnlineAccounts;
    QList<QContactLocalId> m_unmergedContactIds;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctUnmergeIMContactsRequest::QctUnmergeIMContactsRequest(QObject *parent)
    : QContactSaveRequest(parent)
{
    setUserData(QctUnmergeIMContactsRequestData::id(), new QctUnmergeIMContactsRequestData);
}

void
QctUnmergeIMContactsRequest::setUnmergeOnlineAccounts(const QList<QContactOnlineAccount> &onlineAccounts)
{
    data()->setUnmergeOnlineAccounts(onlineAccounts);
}

QList<QContactOnlineAccount>
QctUnmergeIMContactsRequest::unmergeOnlineAccounts() const
{
    return data()->unmergeOnlineAccounts();
}

void
QctUnmergeIMContactsRequest::setSourceContact(const QContact &contact)
{
    data()->setSourceContact(contact);
}

QContact
QctUnmergeIMContactsRequest::sourceContact() const
{
    return data()->sourceContact();
}

QList<QContactLocalId>
QctUnmergeIMContactsRequest::unmergedContactIds() const
{
    return data()->unmergedContactIds();
}

void
QctUnmergeIMContactsRequest::setUnmergedContactIds(const QList<QContactLocalId> &ids)
{
    data()->setUnmergedContactIds(ids);
}

const QctUnmergeIMContactsRequestData *
QctUnmergeIMContactsRequest::data() const
{
    return static_cast<const QctUnmergeIMContactsRequestData *>
            (userData(QctUnmergeIMContactsRequestData::id()));
}

QctUnmergeIMContactsRequestData *
QctUnmergeIMContactsRequest::data()
{
    return static_cast<QctUnmergeIMContactsRequestData *>
            (userData(QctUnmergeIMContactsRequestData::id()));
}
