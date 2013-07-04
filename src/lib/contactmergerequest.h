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

#ifndef QCTCONTACTMERGEREQUEST_H
#define QCTCONTACTMERGEREQUEST_H

#include "qtcontactsglobal.h"
#include "qcontactremoverequest.h"

#include "libqtcontacts_extensions_tracker_global.h"

QTM_USE_NAMESPACE

/*!
 * \class QctContactMergeRequest
 * \brief Custom qtcontacts-tracker request for mashing details from multiple source contacts before removal.
 *
 * Note: this is experimental code - might be removed soon by QContactRelationshipSaveRequest or mash request.
 *
 * This requests uses a multi-hash to describe the merge instructions. Each key of the hash is
 * a merge "destination", and the associated values are the merge "sources".
 * If the hash is { 3 -> (4, 5); 6 -> (7, 8) } then 4 and 5 will be merged into 3, and 7 and 8
 * will be merged into 6.
 * 4, 5, 7 and 8 will be removed once the merge is complete.
 *
 * Note how specific the implementation is:
 * QContactAbstractRequest doesnt support extension. Instead, QctContactMergeRequest is extending
 * QContactRemoveRequest - as request logic is: copy content from from multiple source contacts
 * to one target contact before removing source contacts.
 * Following the logic, QContactRemoveRequest is specific ContactsMergeRequest where contacts' content
 * is merged with nothing before head (nco:PersonContact) uri is removed.
 *
 * \sa type() returns QContactAbstractRequest::ContactRemoveRequest.
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctContactMergeRequestData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctContactMergeRequest : public QContactRemoveRequest
{
    Q_OBJECT

public:
    /*! Constructs a new contacts merge request whose parent is the specified \a parent */
    QctContactMergeRequest(QObject *parent = 0);

    /*! Sets the hash describing the merge operation */
    void setMergeIds(const QMultiMap<QContactLocalId, QContactLocalId> ids);
    /*! Retrieves the hash describing the merge operation */
    QMultiMap<QContactLocalId, QContactLocalId> mergeIds() const;

protected:
    const QctContactMergeRequestData * data() const;
    QctContactMergeRequestData * data();

private:
    Q_DISABLE_COPY(QctContactMergeRequest)
    friend class QContactManagerEngine;
};

#endif // QCTCONTACTMERGEREQUEST_H
