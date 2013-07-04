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

#ifndef QCTREQUESTEXTENSIONS_H
#define QCTREQUESTEXTENSIONS_H

#include <QContactAbstractRequest>

#include "libqtcontacts_extensions_tracker_global.h"

QTM_USE_NAMESPACE

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctRequestExtensions : public QObjectUserData
{
public:
    static QctRequestExtensions * get(QContactAbstractRequest *request);

    void setNameOrder(const QString &order);
    QString nameOrder() const;

private: // fields
    QString m_nameOrder;
};

/// @deprecated: redundant with QctRequestExtensions::get()
Q_DECL_DEPRECATED static inline QString
qctRequestGetNameOrder(QContactAbstractRequest *request)
{
    return QctRequestExtensions::get(request)->nameOrder();
}

/// @deprecated: redundant with QctRequestExtensions::get()
Q_DECL_DEPRECATED static inline void
qctRequestSetNameOrder(QContactAbstractRequest *request, const QString &order)
{
    QctRequestExtensions::get(request)->setNameOrder(order);
}

#endif // QCTREQUESTEXTENSIONS_H
