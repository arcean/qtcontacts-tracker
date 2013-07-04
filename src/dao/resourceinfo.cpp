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

#include "resourceinfo.h"
#include "ontologies/nco.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////////////////////////

const PropertyInfoBase piHasAffiliation = PropertyInfo<nco::hasAffiliation>();
const PropertyInfoBase piHasProperty = PropertyInfo<nao::hasProperty>();

////////////////////////////////////////////////////////////////////////////////////////////////////

class ResourceInfoData : public QSharedData
{
    friend class ClassInfoBase;
    friend class InstanceInfoBase;
    friend class PropertyInfoBase;
    friend class ResourceInfo;

protected:
    explicit ResourceInfoData(const ResourceValue &resource, const QString &iri, const QVariant &value)
        : m_value(value)
        , m_resource(resource)
        , m_iri(iri)
    {
    }

public:
    virtual ~ResourceInfoData()
    {
    }

protected:
    const QVariant m_value;
    const ResourceValue &m_resource;
    const QString &m_iri;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class PropertyInfoData : public ResourceInfoData
{
    friend class PropertyInfoBase;

protected:
    typedef PropertyInfoBase::Subject Subject;

    explicit PropertyInfoData(const ResourceValue &resource, const PredicateFunction &function,
                              const QString &iri, const QString &name,
                              Subject::Scheme domainScheme, Subject::Scheme rangeScheme,
                              const QString &domainIri, const QString &rangeIri, bool singleValued)
        : ResourceInfoData(resource, iri, name)
        , m_predicateFunction(function)
        , m_domainScheme(domainScheme)
        , m_rangeScheme(rangeScheme)
        , m_domainIri(domainIri)
        , m_rangeIri(rangeIri)
        , m_singleValued(singleValued)
        , m_conversion(0)
        , m_caseSensitivity(Qt::CaseSensitive)
        , m_parent(0)
        , m_readOnly(false)
        , m_inverse(false)
        , m_foreignKey(false)
        , m_hasDetailUri(false)
        , m_definesOwnership(false)
    {
    }

    const PredicateFunction & m_predicateFunction;
    const Subject::Scheme   m_domainScheme;
    const Subject::Scheme   m_rangeScheme;
    const QString &         m_domainIri;
    const QString &         m_rangeIri;
    bool                    m_singleValued;
    const Conversion *      m_conversion;
    Qt::CaseSensitivity     m_caseSensitivity;
    PropertyInfoBase        m_parent;

    bool m_readOnly         : 1;
    bool m_inverse          : 1;
    bool m_foreignKey       : 1;
    bool m_hasDetailUri     : 1;
    bool m_definesOwnership : 1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

ResourceInfo::ResourceInfo(ResourceInfoData *data)
    : d(data)
{
}

ResourceInfo::ResourceInfo(const ResourceInfo &other)
    : d(other.d)
{
}

ResourceInfo::~ResourceInfo()
{
}

ResourceInfo &
ResourceInfo::operator=(const ResourceInfo &other)
{
    return d = other.d, *this;
}

const QVariant &
ResourceInfo::value() const
{
    return d->m_value;
}

const ResourceValue &
ResourceInfo::resource() const
{
    return d->m_resource;
}

const QString &
ResourceInfo::iri() const
{
    if (not d) {
        static const QString none;
        return none;
    }

    return d->m_iri;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ClassInfoBase::ClassInfoBase(const ResourceValue &resource, const QString &iri, const QVariant &name)
    : ResourceInfo(new ResourceInfoData(resource, iri, name))
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

InstanceInfoBase::InstanceInfoBase(const Cubi::ResourceValue &resource,
                                   const QString &iri, const QVariant &value)
    : ResourceInfo(new ResourceInfoData(resource, iri, value))
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

PropertyInfoBase::PropertyInfoBase(const ResourceValue &resource, const PredicateFunction &function,
                                   const QString &iri, const QString &name,
                                   Subject::Scheme domainScheme, Subject::Scheme rangeScheme,
                                   const QString &domainIri, const QString &rangeIri, bool singleValued)
    : ResourceInfo(new PropertyInfoData(resource, function, iri, name, domainScheme, rangeScheme,
                                        domainIri, rangeIri, singleValued))
{
}

PropertyInfoBase::PropertyInfoBase(PropertyInfoData *data)
    : ResourceInfo(data)
{
}

PropertyInfoBase &
PropertyInfoBase::setReadOnly(bool value)
{
    data()->m_readOnly = value;
    return *this;
}
bool
PropertyInfoBase::isReadOnly() const
{
    return data()->m_readOnly;
}

PropertyInfoBase &
PropertyInfoBase::setInverse(bool value)
{
    data()->m_inverse = value;
    return *this;
}

bool
PropertyInfoBase::isInverse() const
{
    return data()->m_inverse;
}

PropertyInfoBase &
PropertyInfoBase::setForeignKey(bool value)
{
    data()->m_foreignKey = value;
    return *this;
}

bool
PropertyInfoBase::isForeignKey() const
{
    return data()->m_foreignKey;
}

PropertyInfoBase &
PropertyInfoBase::setHasDetailUri(bool value)
{
    data()->m_hasDetailUri = value;
    return *this;
}

bool
PropertyInfoBase::hasDetailUri() const
{
    return data()->m_hasDetailUri;
}

PropertyInfoBase &
PropertyInfoBase::setDefinesOwnership(bool value)
{
    data()->m_definesOwnership = value;
    return *this;
}

bool
PropertyInfoBase::definesOwnership() const
{
    return data()->m_definesOwnership;
}

PropertyInfoBase &
PropertyInfoBase::setCaseSensitivity(Qt::CaseSensitivity value)
{
    data()->m_caseSensitivity = value;
    return *this;
}

Qt::CaseSensitivity
PropertyInfoBase::caseSensitivity() const
{
    return data()->m_caseSensitivity;
}

PropertyInfoBase &
PropertyInfoBase::setConversion(const Conversion *conversion)
{
    data()->m_conversion = conversion;
    return *this;
}

const Conversion *
PropertyInfoBase::conversion() const
{
    return data()->m_conversion;
}

PropertyInfoBase &
PropertyInfoBase::setParent(const PropertyInfoBase &parent)
{
    data()->m_parent = parent;
    return *this;
}

const PropertyInfoBase &
PropertyInfoBase::parent() const
{
    return data()->m_parent;
}

const PredicateFunction &
PropertyInfoBase::predicateFunction() const
{
    return data()->m_predicateFunction;
}

PropertyInfoBase::Subject::Scheme
PropertyInfoBase::resourceIriScheme() const
{
    Subject::Scheme scheme = (not isInverse() ? rangeScheme() : domainScheme());

    if (Subject::None == scheme) {
        const PropertyInfoBase &parentInfo = parent();

        if (parentInfo) {
            scheme = (not parentInfo.isInverse() ? domainScheme() : rangeScheme());
        }
    }

    return scheme;
}

const QString &
PropertyInfoBase::resourceTypeIri() const
{
    return not isInverse() ? rangeIri() : domainIri();
}

PropertyInfoBase::Subject::Scheme
PropertyInfoBase::domainScheme() const
{
    return data()->m_domainScheme;
}

PropertyInfoBase::Subject::Scheme
PropertyInfoBase::rangeScheme() const
{
    return data()->m_rangeScheme;
}

const QString &
PropertyInfoBase::domainIri() const
{
    return data()->m_domainIri;
}

const QString &
PropertyInfoBase::rangeIri() const
{
    return data()->m_rangeIri;
}

bool
PropertyInfoBase::singleValued() const
{
    return data()->m_singleValued;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

PropertyInfoList::PropertyInfoList(const PropertyInfoList::ConstIterator &first,
                                   const PropertyInfoList::ConstIterator &last,
                                   const PropertyInfoList &context)
    : QList<PropertyInfoBase>(context)
{
    for(PropertyInfoList::ConstIterator pi = first; pi != last; ++pi) {
        append(*pi);
    }
}

bool
PropertyInfoList::startsWith(const PropertyInfoBase &predicate) const
{
    return BaseType::startsWith(predicate);
}

bool
PropertyInfoList::startsWith(const QList<PropertyInfoBase> &otherChain) const
{
    if (otherChain.length() > length()) {
        return false;
    }

    for(int i = otherChain.length() - 1; i >= 0; --i) {
        if (at(i) != otherChain.at(i)) {
            return false;
        }
    }

    return true;
}

bool
PropertyInfoList::hasInverseProperty() const
{
    foreach(const PropertyInfoBase &pi, *this) {
        if (pi.isInverse()) {
            return true;
        }
    }

    return false;
}

bool
PropertyInfoList::isSingleValued(bool strict) const
{
    // We only search until size()-1 since we allow the last property to be
    // mutivalued, we'll get the values coma-separated
    for (int i = 0; i < size() - (strict ? 0 : 1); i++) {
        if (not at(i).singleValued()) {
            return false;
        }
    }

    return true;
}

Pattern
PropertyInfoBase::bindProperty(const Value &subject, const Value &object) const
{
    const ResourceValue predicate = ResourceValue(iri());

    if (isInverse()) {
        return Pattern(object, predicate, subject);
    }

    return Pattern(subject, predicate, object);
}
