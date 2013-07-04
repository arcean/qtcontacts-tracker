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

#ifndef QTRACKERCONTACTDETAIL_P_H
#define QTRACKERCONTACTDETAIL_P_H

#include "contactdetail.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailData : public QSharedData
{
    friend class QTrackerContactDetail;

protected: // constructors
    explicit QTrackerContactDetailData(const QString &name)
        : m_hasContext(true)
        , m_isUnique(false)
        , m_isInternal(false)
        , m_chainsDirty(false)
        , m_name(qctInternString(name))
        , m_detailUriScheme(QTrackerContactSubject::None)
    {
    }

protected: // virtual methods
    virtual const QTrackerContactDetail *
    findImplementation(const QTrackerContactDetailSchema &/*schema*/,
                       const QHash<QString, QContactDetail> &/*otherDetails*/,
                       const QContactDetail &/*actualDetail*/) const
    {
        return 0;
    }

    virtual QList<const QTrackerContactDetail *>
    implementations(const QTrackerContactDetailSchema &/*schema*/) const
    {
        return QList<const QTrackerContactDetail *>();
    }

protected: // methods
    bool isPossessedChain(const PropertyInfoList &chain) const;
    void computePredicateChains();

protected: // fields
    QList<QTrackerContactDetailField>                   m_fields;
    QSet<PropertyInfoList>                              m_predicateChains;
    QSet<PropertyInfoList>                              m_foreignKeyChains;
    QSet<PropertyInfoList>                              m_possessedChains;
    QSet<PropertyInfoList>                              m_customValueChains;
    bool                                                m_hasContext : 1;
    bool                                                m_isUnique : 1;
    bool                                                m_isInternal : 1;
    /// flag which tells whether all chains need to be updated because the base data changed
    bool                                                m_chainsDirty : 1;
    QString                                             m_name;
    QTrackerContactSubject::Scheme                      m_detailUriScheme;
    mutable QScopedPointer<QTrackerContactDetailField>  m_detailUriField;
    QSet<QString>                                       m_dependencies;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTDETAIL_P_H
