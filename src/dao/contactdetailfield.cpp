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

#include "contactdetailfield.h"
#include "contactdetail.h"
#include "support.h"

#include <lib/logger.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailFieldData : public QSharedData
{
    friend class QTrackerContactDetailField;

private: // constructors
    QTrackerContactDetailFieldData(const QString &name)
        : m_name(qctInternString(name))
        , m_dataType(QVariant::String)
        , m_original(false)
        , m_synthesized(false)
        , m_withoutMapping(false)
        , m_permitsCustomValues(false)
        , m_hasOwner(true)
        , m_conversion(0)
        , m_transform(0)
    {
    }

private: // fields
    QString                 m_name;
    QVariant::Type          m_dataType;
    QVariant                m_defaultValue;
    PropertyInfoList        m_properties;
    PropertyInfoList        m_computedProperties;
    ClassInfoList           m_subTypeClasses;
    PropertyInfoList        m_subTypeProperties;
    InstanceInfoList        m_allowableInstances;
    QVariantList            m_allowableValues;
    bool                    m_original : 1;
    bool                    m_synthesized : 1;
    bool                    m_withoutMapping : 1;
    bool                    m_permitsCustomValues : 1;
    bool                    m_hasOwner : 1;
    const Conversion *      m_conversion;
    const SparqlTransform * m_transform;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetailField::QTrackerContactDetailField(const QString &name)
    : d(new QTrackerContactDetailFieldData(name))
{
}

QTrackerContactDetailField::~QTrackerContactDetailField()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetailField::QTrackerContactDetailField(const QTrackerContactDetailField &other)
    : d(other.d)
{
}

QTrackerContactDetailField &
QTrackerContactDetailField::operator=(const QTrackerContactDetailField &other)
{
    return d = other.d, *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const QString &
QTrackerContactDetailField::name() const
{
    return d->m_name;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setDataType(QVariant::Type dataType)
{
    d->m_dataType = dataType;
    return *this;
}

QVariant::Type
QTrackerContactDetailField::dataType() const
{
    return d->m_dataType;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setDefaultValue(const QVariant &value)
{
    d->m_defaultValue = value;
    return *this;
}

const QVariant &
QTrackerContactDetailField::defaultValue() const
{
    return d->m_defaultValue;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setOriginal(bool value)
{
    d->m_original = value;
    return *this;
}

bool
QTrackerContactDetailField::isOriginal() const
{
    return d->m_original;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setSynthesized(bool value)
{
    d->m_synthesized = value;
    return *this;
}

bool
QTrackerContactDetailField::isSynthesized() const
{
    return d->m_synthesized;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setWithoutMapping(bool value)
{
    d->m_withoutMapping = value;
    return *this;
}

bool
QTrackerContactDetailField::isWithoutMapping() const
{
    return d->m_withoutMapping;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setHasOwner(bool value)
{
    d->m_hasOwner = value;
    return *this;
}

bool
QTrackerContactDetailField::hasOwner() const
{
    return d->m_hasOwner;
}


///////////////////////////////////////////////////////////////////////////////////////////////////

static void
linkPropertyChain(const PropertyInfoList &properties)
{
    PropertyInfoBase lastProperty;

    foreach(PropertyInfoBase thisProperty, properties) {
        thisProperty.setParent(lastProperty);
        lastProperty = thisProperty;
    }
}

QTrackerContactDetailField &
QTrackerContactDetailField::setPropertyChain(const PropertyInfoList &properties)
{
    d->m_properties = properties;
    linkPropertyChain(properties);

    // XXX: Ensure computed properties are linked to last regular property.
    setComputedProperties(d->m_computedProperties);

    return *this;
}

const PropertyInfoList &
QTrackerContactDetailField::propertyChain() const
{
    return d->m_properties;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setComputedProperties(const PropertyInfoList &computedProperties)
{
    // XXX: Right computed properties only can be used at the leafes of property chains.
    // For the moment this is sufficient, but this might have to be improved later.

    d->m_computedProperties = computedProperties;

    // link computed properties with last regular property
    PropertyInfoBase lastProperty;

    if (hasPropertyChain()) {
        lastProperty = propertyChain().last();
    }

    foreach(PropertyInfoBase thisProperty, d->m_computedProperties) {
        thisProperty.setParent(lastProperty);
    }

    return *this;
}

const PropertyInfoList &
QTrackerContactDetailField::computedProperties() const
{
    return d->m_computedProperties;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setSubTypeProperties(const PropertyInfoList &subTypeProperties)
{
    d->m_subTypeProperties = subTypeProperties;
    linkPropertyChain(d->m_subTypeProperties);
    return *this;
}

const PropertyInfoList &
QTrackerContactDetailField::subTypeProperties() const
{
    return d->m_subTypeProperties;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setSubTypeClasses(const ClassInfoList &subTypeClasses)
{
    d->m_subTypeClasses = subTypeClasses;
    return *this;
}

const ClassInfoList &
QTrackerContactDetailField::subTypeClasses() const
{
    return d->m_subTypeClasses;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setPermitsCustomValues(bool value)
{
    d->m_permitsCustomValues = value;
    return *this;
}

bool
QTrackerContactDetailField::permitsCustomValues() const
{
    return d->m_permitsCustomValues;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setAllowableValues(const QVariantList &values)
{
    d->m_allowableValues = values;
    return *this;
}

QTrackerContactDetailField &
QTrackerContactDetailField::setAllowableInstances(const InstanceInfoList &values)
{
    d->m_allowableInstances = values;
    return *this;
}

const QVariantList &
QTrackerContactDetailField::allowableValues() const
{
    return d->m_allowableValues;
}

const InstanceInfoList &
QTrackerContactDetailField::allowableInstances() const
{
    return d->m_allowableInstances;
}

bool
QTrackerContactDetailField::allowsMultipleValues() const
{
    if (not restrictsValues()) {
        return false;
    }

    return QVariant::StringList == dataType()
        || QVariant::List == dataType();
}

QTrackerContactDetailField &
QTrackerContactDetailField::setConversion(const Conversion *conversion)
{
    d->m_conversion = conversion;
    return *this;
}

const Conversion *
QTrackerContactDetailField::conversion() const
{
    return d->m_conversion;
}

PropertyInfoList::ConstIterator
QTrackerContactDetailField::foreignKeyProperty() const
{
    for(PropertyInfoList::ConstIterator pi = d->m_properties.constBegin(); pi != d->m_properties.constEnd(); ++pi) {
        if (pi->isForeignKey()) {
            return pi;
        }
    }

    return d->m_properties.constEnd();
}

bool
QTrackerContactDetailField::isForeignKey() const
{
    return foreignKeyProperty() != d->m_properties.constEnd();
}

PropertyInfoList::ConstIterator
QTrackerContactDetailField::detailUriProperty() const
{
    for(PropertyInfoList::ConstIterator pi = d->m_properties.constBegin(); pi != d->m_properties.constEnd(); ++pi) {
        if (pi->hasDetailUri()) {
            return pi;
        }
    }

    return d->m_properties.constEnd();
}

bool
QTrackerContactDetailField::hasDetailUri() const
{
    return detailUriProperty() != d->m_properties.constEnd();
}

PropertyInfoList::ConstIterator
QTrackerContactDetailField::ownershipDefiningProperty() const
{
    for(PropertyInfoList::ConstIterator pi = d->m_properties.constBegin(); pi != d->m_properties.constEnd(); ++pi) {
        if (pi->definesOwnership()) {
            return pi;
        }
    }

    return d->m_properties.constEnd();
}

bool
QTrackerContactDetailField::definesOwnership() const
{
    return ownershipDefiningProperty() != d->m_properties.constEnd();
}

bool
QTrackerContactDetailField::isInverse() const
{
    for(PropertyInfoList::ConstIterator pi = d->m_properties.constBegin(); pi != d->m_properties.constEnd(); ++pi) {
        if (pi->isInverse()) {
            return true;
        }
    }

    return false;
}

bool
QTrackerContactDetailField::isReadOnly() const
{
    for(PropertyInfoList::ConstIterator pi = d->m_properties.constBegin(); pi != d->m_properties.constEnd(); ++pi) {
        if (pi->isReadOnly()) {
            return true;
        }
    }

    return false;
}

QVariant::Type
QTrackerContactDetailField::rdfType() const
{
    if (d->m_conversion) {
        return d->m_conversion->makeType();
    }

    return dataType();
}

bool
QTrackerContactDetailField::hasLiteralValue() const
{
    return QVariant::Url != rdfType();
}

QTrackerContactDetailField &
QTrackerContactDetailField::setSparqlTransform(const SparqlTransform * value)
{
    d->m_transform = value;

    return *this;
}

const SparqlTransform *
QTrackerContactDetailField::sparqlTransform() const
{
    return d->m_transform;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool
QTrackerContactDetailField::makeValue(const QVariant &from, QVariant &to) const
{
    if (restrictsValues()) {
        if (0 != d->m_conversion) {
            qctWarn("Cannot have conversions when values are restricted");
            return false;
        }

        if (not allowableInstances().isEmpty()) {
            foreach(const InstanceInfoBase &pi, allowableInstances()) {
                if (qctEquals(from, pi.value(), Qt::CaseInsensitive)) {
                    to.setValue(pi.resource());
                    return true;
                }
            }
        }

        if (not allowableValues().isEmpty()) {
            switch(dataType()) {
            case QVariant::StringList: {
                QSet<QString> fromValues = from.toStringList().toSet();
                QStringList stringValues;

                foreach(const QVariant &v, allowableValues()) {
                    if (not v.canConvert(QVariant::String)) {
                        qctWarn(QString::fromLatin1("Could not convert value of type %1 to string").
                                                    arg(QLatin1String(v.typeName())));
                        continue;
                    }

                    // FIXME: this will not work for any datatype... But does for most of them.
                    const QString strValue = v.toString();

                    if (fromValues.contains(strValue)) {
                        stringValues.append(v.toString());
                    }
                }

                to.setValue(stringValues);
                return true;
            }

            case QVariant::String: {
                const QString fromValue = from.toString();

                foreach(const QVariant &v, allowableValues()) {
                    if (not v.canConvert(QVariant::String)) {
                        qctWarn(QString::fromLatin1("Could not convert value of type %1 to string").
                                                    arg(QLatin1String(v.typeName())));
                        continue;
                    }

                    // FIXME: this will not work for any datatype... But does for most of them.
                    const QString strValue = v.toString();

                    if (strValue == fromValue) {
                        to.setValue(strValue);
                        return true;
                    }
                }

                return false;
            }

            case QVariant::Int: {
                const int fromValue = from.toInt();

                foreach(const QVariant &v, allowableValues()) {
                    bool converted = false;
                    const int intValue = v.toInt(&converted);

                    if (not converted) {
                        qctWarn(QString::fromLatin1("Could not convert value of type %1 to int").
                                                    arg(QLatin1String(v.typeName())));
                        continue;
                    }

                    if (intValue == fromValue) {
                        to.setValue(intValue);
                        return true;
                    }
                }

                return false;
            }

            default:
                qctWarn(QString::fromLatin1("Unsupported restricted value type %1").
                        arg(QLatin1String(QVariant::typeToName(dataType()))));
                return false;
            }
        }

        return false;
    }

    if (d->m_conversion) {
        return d->m_conversion->makeValue(from, to);
    }

    if (&from != &to) {
        to.setValue(from);
    }

    return to.convert(dataType());
}

bool
QTrackerContactDetailField::parseValue(const QVariant &from, QVariant &to) const
{
    if (restrictsValues()) {
        if (0 != d->m_conversion) {
            qctWarn("Cannot have conversions when values are restricted");
            return false;
        }

        foreach(const InstanceInfoBase &pi, allowableInstances()) {
            if (from == pi.iri()) {
                to.setValue(pi.value());
                return true;
            }
        }

        return false;
    }

    if (d->m_conversion) {
        return d->m_conversion->parseValue(from, to);
    }

    if (&from != &to) {
        to.setValue(from);
    }

    if (QVariant::StringList == dataType()) {
        if (QVariant::StringList == to.type()) {
            return true;
        }

        if (not to.convert(QVariant::String)) {
            return false;
        }

        to.setValue(to.toString().split(QLatin1Char('\x1f')));

        return true;
    }

    if (not to.convert(dataType())) {
        return false;
    }

    if (QVariant::DateTime == to.type()) {
        to.setValue(to.toDateTime().toLocalTime());
    }

    return true;
}

QVariantList
QTrackerContactDetailField::describeAllowableValues() const
{
    // FIXME: maybe cache result
    QVariantList result(d->m_allowableValues);

    foreach(const PropertyInfoBase &i, subTypeProperties()) {
        result.append(i.value());
    }

    foreach(const ClassInfoBase &i, subTypeClasses()) {
        result.append(i.value());
    }

    foreach(const InstanceInfoBase &i, allowableInstances()) {
        result.append(i.value());
    }

    if (not result.isEmpty() && hasDefaultValue() && not result.contains(defaultValue())) {
        result.append(defaultValue());
    }

    return result;
}

QContactDetailFieldDefinition
QTrackerContactDetailField::describe() const
{
    // a field cannot describe subTypes and instances at the same time
    Q_ASSERT(not restrictsValues() || not hasSubTypes());

    // cannot have conversions when values are restricted
    Q_ASSERT(not restrictsValues() || 0 == conversion());

    // there must be a property chain unless this field describes subtypes or is synthesized
    Q_ASSERT(hasPropertyChain() || hasSubTypes() || isSynthesized());

    // subtype fields must have String or StringList type
    Q_ASSERT(not hasSubTypes() ||
             QVariant::String == dataType() ||
             QVariant::StringList == dataType());

    // currently custom values only work with string list fields
    Q_ASSERT(not permitsCustomValues() ||
             QVariant::String == dataType() ||
             QVariant::StringList == dataType());

    // detail URI property must have a proper scheme
    Q_ASSERT(propertyChain().constEnd() == detailUriProperty() ||
             QTrackerContactSubject::None != detailUriProperty()->resourceIriScheme());

    // no more than one computed property per field permitted - AFAIK
    Q_ASSERT(1 >= computedProperties().count());

    QContactDetailFieldDefinition definition;
    definition.setAllowableValues(describeAllowableValues());
    definition.setDataType(dataType());
    return definition;
}
