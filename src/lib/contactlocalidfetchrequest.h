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

#ifndef QCTCONTACTLOCALIDFETCHREQUEST_H
#define QCTCONTACTLOCALIDFETCHREQUEST_H

#include "qtcontactsglobal.h"
#include "qcontactlocalidfetchrequest.h"

#include "libqtcontacts_extensions_tracker_global.h"

QTM_USE_NAMESPACE

/*!
 * \class QctContactLocalIdFetchRequest
 * \brief Custom qtcontacts-tracker request adding a few parameters to \c QContactLocalIdFetchRequest
 *
 * \sa QContactLocalIdFetchRequest
 * \note type() returns QContactAbstractRequest::ContactLocalIdFetchRequest.
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctContactLocalIdFetchRequestData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctContactLocalIdFetchRequest : public QContactLocalIdFetchRequest
{
    Q_OBJECT

public:
    QctContactLocalIdFetchRequest(QObject *parent = 0);

    /*! Sets the limit number of results to be returned (-1 for no limit) */
    void setLimit(int limit);
    /*! Returns the limit of this request */
    int limit() const;

    /*!
     * Forbids the use of a QContactFetchRequest if sorting can't be done in SPARQL
     *
     * In this case, a QContactManager::NotSupported error will be emited if the ID
     * fetch request would have used a QContactFetchRequest otherwise.
     */
    void setForceNative(bool force);
    /*! Returns true if the use of a QContactFetchRequest is forbidden for this request */
    bool forceNative() const;

protected:
    const QctContactLocalIdFetchRequestData * data() const;
    QctContactLocalIdFetchRequestData * data();

private:
    Q_DISABLE_COPY(QctContactLocalIdFetchRequest)
    friend class QContactManagerEngine;
};

#endif // QCTCONTACTLOCALIDFETCHREQUEST_H
