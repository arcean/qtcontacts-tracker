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

#ifndef QTRACKERCONTACTDETAILFIELD_H
#define QTRACKERCONTACTDETAILFIELD_H

#include "resourceinfo.h"
#include "transform.h"

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailFieldData;
class QTrackerContactDetailField
{
public: // constructors
    explicit QTrackerContactDetailField(const QString &name);
    virtual ~QTrackerContactDetailField();

public: // operators
    QTrackerContactDetailField(const QTrackerContactDetailField &other);
    QTrackerContactDetailField & operator=(const QTrackerContactDetailField &other);

public: // attributes
    const QString & name() const;

    QTrackerContactDetailField & setDataType(QVariant::Type dataType);
    QVariant::Type dataType() const;

    QTrackerContactDetailField & setDefaultValue(const QVariant &value);
    const QVariant & defaultValue() const;


    QTrackerContactDetailField & setOriginal(bool value = true);
    bool isOriginal() const;

    QTrackerContactDetailField & setSynthesized(bool value = true);
    bool isSynthesized() const;

    QTrackerContactDetailField & setWithoutMapping(bool value = true);
    bool isWithoutMapping() const;

    QTrackerContactDetailField & setPropertyChain(const PropertyInfoList &properties);
    const PropertyInfoList & propertyChain() const;

    QTrackerContactDetailField & setComputedProperties(const PropertyInfoList &computedProperties);
    const PropertyInfoList & computedProperties() const;

    QTrackerContactDetailField & setSubTypeProperties(const PropertyInfoList &subTypeProperties);
    const PropertyInfoList & subTypeProperties() const;

    QTrackerContactDetailField & setSubTypeClasses(const ClassInfoList &subTypeClasses);
    const ClassInfoList & subTypeClasses() const;

    QTrackerContactDetailField & setPermitsCustomValues(bool value = true);
    bool permitsCustomValues() const;

    QTrackerContactDetailField & setAllowableValues(const QVariantList &values);
    QTrackerContactDetailField & setAllowableInstances(const InstanceInfoList &values);
    const InstanceInfoList & allowableInstances() const;
    const QVariantList & allowableValues() const;
    bool allowsMultipleValues() const;

    QTrackerContactDetailField & setConversion(const Conversion *conversion);
    const Conversion * conversion() const;

    QTrackerContactDetailField & setHasOwner(bool value = true);
    bool hasOwner() const;

    bool hasDefaultValue() const { return not defaultValue().isNull(); }
    bool hasPropertyChain() const { return not propertyChain().isEmpty(); }
    bool hasSubTypeProperties() const { return not subTypeProperties().isEmpty(); }
    bool hasSubTypeClasses() const { return not subTypeClasses().isEmpty(); }
    bool hasSubTypes() const { return hasSubTypeClasses() || hasSubTypeProperties(); }
    bool restrictsValues() const { return not allowableValues().isEmpty() ||
                                          not allowableInstances().isEmpty(); }

    PropertyInfoList::ConstIterator foreignKeyProperty() const;
    bool isForeignKey() const;

    PropertyInfoList::ConstIterator detailUriProperty() const;
    bool hasDetailUri() const;

    PropertyInfoList::ConstIterator ownershipDefiningProperty() const;
    bool definesOwnership() const;

    bool isInverse() const;
    bool isReadOnly() const;

    QVariant::Type rdfType() const;
    bool hasLiteralValue() const;

    QTrackerContactDetailField & setSparqlTransform(const SparqlTransform *transform);
    const SparqlTransform * sparqlTransform() const;

public: // methods
    bool makeValue(const QVariant &from, QVariant &to) const;
    bool parseValue(const QVariant &from, QVariant &to) const;

    QVariantList describeAllowableValues() const;
    QContactDetailFieldDefinition describe() const;

private: // fields
    QExplicitlySharedDataPointer<QTrackerContactDetailFieldData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTDETAILFIELD_H
