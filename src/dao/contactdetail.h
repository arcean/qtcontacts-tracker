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

#ifndef QTRACKERCONTACTDETAIL_H
#define QTRACKERCONTACTDETAIL_H

#include "contactdetailfield.h"

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class PropertyInfoBase;
class QTrackerContactDetailSchema;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailData;
class QTrackerContactDetail
{
public: // constructors
    explicit QTrackerContactDetail(QTrackerContactDetailData *data);
    explicit QTrackerContactDetail(const QString &name);
    virtual ~QTrackerContactDetail();

public: // operators
    QTrackerContactDetail(const QTrackerContactDetail &other);
    QTrackerContactDetail & operator=(const QTrackerContactDetail &other);

public: // attributes
    const QString & name() const;

    void setHasContext(bool flag);
    bool hasContext() const;

    void setUnique(bool flag);
    bool isUnique() const;

    void setInternal(bool flag);
    bool isInternal() const;

    void addField(const QTrackerContactDetailField &field);
    const QList<QTrackerContactDetailField> & fields() const;

    const QSet<PropertyInfoList> & predicateChains() const;
    const QSet<PropertyInfoList> & foreignKeyChains() const;
    const QSet<PropertyInfoList> & possessedChains() const;
    const QSet<PropertyInfoList> & customValueChains() const;

    void addDependency(const QString &detail);
    const QSet<QString> & dependencies() const;
    bool isSynthesized() const { return not dependencies().isEmpty(); }

    const QTrackerContactDetailField *field(const QString &name) const;
    const QTrackerContactDetailField *resourceIriField() const;
    const QTrackerContactDetailField *detailUriField() const;
    const QTrackerContactDetailField *subTypeField() const;

    void setDetailUriScheme(QTrackerContactSubject::Scheme scheme);
    QTrackerContactSubject::Scheme detailUriScheme() const;
    QTrackerContactSubject::Scheme resourceIriScheme() const;

    bool hasDetailUri() const;

public: // methods
    QContactDetailDefinition describe() const;

    const QTrackerContactDetail *
    findImplementation(const QTrackerContactDetailSchema &schema,
                       const QHash<QString, QContactDetail> &otherDetails,
                       const QContactDetail &actualDetail) const;

    QList<const QTrackerContactDetail *> implementations(const QTrackerContactDetailSchema &schema) const;

private: // fields
    QExplicitlySharedDataPointer<QTrackerContactDetailData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerUniqueContactDetail : public QTrackerContactDetail
{
protected: // constructors
    explicit QTrackerUniqueContactDetail(const QString &name)
        : QTrackerContactDetail(name)
    {
        setHasContext(false);
        setUnique(true);
    }

    explicit QTrackerUniqueContactDetail(QTrackerContactDetailData *data)
        : QTrackerContactDetail(data)
    {
        setHasContext(false);
        setUnique(true);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTDETAIL_H
