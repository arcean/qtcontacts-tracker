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

#ifndef QCTUNMERGEIMCONTACTSREQUEST_H
#define QCTUNMERGEIMCONTACTSREQUEST_H

#include "qtcontactsglobal.h"
#include <QContactSaveRequest>
#include <QContactOnlineAccount>

#include "libqtcontacts_extensions_tracker_global.h"

QTM_USE_NAMESPACE

/*!
 * \class QctUnmergeIMContactsRequest
 * \brief Custom qtcontacts-tracker request for unmerging mashed instant messaging contacts (added by contactsd)
 *
 * Note: this is EXPERIMENTAL code - will be removed as soon as API in QtMobility is defined. Try not to use it.
 *
 * Use setContact() to set the source contact, and setUnmergeOnlineAccounts() to set the list
 * of QContactOnlineAccount to detach from this contact. For each detached (unmerged) online
 * account, a new contact will be created.
 *
 * \note QContactAbstractRequest doesnt support extension. Instead, QctUnmergeIMContactsRequest is extending
 * QContactSaveRequest, so QContactAbstractRequest::type() will return QContactAbstractRequest::ContactSaveRequest .
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctUnmergeIMContactsRequestData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctUnmergeIMContactsRequest : public QContactSaveRequest
{
    Q_OBJECT

public:
    /*! Constructs a new contacts merge request whose parent is the specified \a parent */
    QctUnmergeIMContactsRequest(QObject *parent = 0);

    /*! Sets the criteria for unmerging - \sa unmergeOnlineAccounts() content realted to online contact is moved to new contact. */
    void setUnmergeOnlineAccounts(const QList<QContactOnlineAccount> &onlineAccounts);

    /*! The criteria for unmerging - \sa setUnmergeOnlineAccounts() */
    QList<QContactOnlineAccount> unmergeOnlineAccounts() const;

    /*! Sets contact which \sa unmergeOnlineAccounts() details should be unmerged to new contacts */
    void setSourceContact(const QContact &contact);

    /*! Contact which \sa unmergeOnlineAccounts() details should be unmerged to new contacts */
    QContact sourceContact() const;

    /*! Result of operation - new contacts created containing data related to \sa unmergeOnlineAccounts()*/
    QList<QContactLocalId> unmergedContactIds() const;

protected:
    void setUnmergedContactIds(const QList<QContactLocalId> &ids);

    const QctUnmergeIMContactsRequestData *data() const;
    QctUnmergeIMContactsRequestData *data();

private:
    Q_DISABLE_COPY(QctUnmergeIMContactsRequest)

    friend class QContactManagerEngine;
    friend class QTrackerContactSaveOrUnmergeRequest;
};

#endif // QCTUNMERGEIMCONTACTSREQUEST_H
