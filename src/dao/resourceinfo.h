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

#ifndef QTRACKERRESOURCEINFO_H
#define QTRACKERRESOURCEINFO_H

#include "conversion.h"

#include <cubi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef QList<class ClassInfoBase>      ClassInfoList;
typedef QList<class InstanceInfoBase>   InstanceInfoList;
typedef QList<class ResourceInfo>       ResourceInfoList;

////////////////////////////////////////////////////////////////////////////////////////////////////

class ResourceInfoData;
class ResourceInfo
{
    friend uint qHash(const ResourceInfo &ri);

protected: // prevent construction of this class
    explicit ResourceInfo(ResourceInfoData *data);
    virtual ~ResourceInfo();

public: // operators
    ResourceInfo(const ResourceInfo &other);
    ResourceInfo & operator=(const ResourceInfo &other);

    operator bool() const { return 0 != d.data(); }
    bool operator!() const { return !bool(*this); }

    bool operator==(const ResourceInfo &other) const
    {
        return d.data() == other.d.data() || iri() == other.iri();
    }

    bool operator!=(const ResourceInfo &other) const { return not (*this == other); }

public: // attributes
    const QString text() const { return value().toString(); }
    const QVariant & value() const;
    const QString & iri() const;
    const Cubi::ResourceValue & resource() const;

protected: // fields of this class
    QExplicitlySharedDataPointer<ResourceInfoData> d;
};

inline uint
qHash(const ResourceInfo &ri)
{
    return qHash(ri.iri());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

class ClassInfoBase : public ResourceInfo
{
protected:
    explicit ClassInfoBase(const Cubi::ResourceValue &resource,
                           const QString &iri, const QVariant &name);
};

template<class Class>
class ClassInfo : public ClassInfoBase {
public:
    explicit ClassInfo(const QString &name) :
        ClassInfoBase(Class::resource(), Class::iri(), name)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class InstanceInfoBase : public ResourceInfo
{
protected:
    explicit InstanceInfoBase(const Cubi::ResourceValue &resource,
                              const QString &iri, const QVariant &value);
};

template<class Instance>
class InstanceInfo : public InstanceInfoBase {
public:
    explicit InstanceInfo(const QVariant &value) :
        InstanceInfoBase(Instance::resource(), Instance::iri(), value)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class PropertyInfoData;
class PropertyInfoBase : public ResourceInfo
{
public:
    typedef QTrackerContactSubject Subject;

protected:
    explicit PropertyInfoBase(const Cubi::ResourceValue &value,
                              const Cubi::PredicateFunction &function,
                              const QString &iri, const QString &name,
                              Subject::Scheme domainScheme, Subject::Scheme rangeScheme,
                              const QString &domainIri, const QString &rangeIri, bool singleValued);

public:
    explicit PropertyInfoBase(PropertyInfoData *data = 0);

public:
    QString makeResourceIri(const QVariant &value) const
    {
        return Subject::makeIri(resourceIriScheme(), QVariantList() << value);
    }

    const Cubi::PredicateFunction & predicateFunction() const;

    Subject::Scheme resourceIriScheme() const;
    Subject::Scheme domainScheme() const;
    Subject::Scheme rangeScheme() const;

    const QString & resourceTypeIri() const;

    const QString & domainIri() const;
    const QString & rangeIri() const;

    bool singleValued() const;

    PropertyInfoBase & setReadOnly(bool value = true);
    bool isReadOnly() const;

    PropertyInfoBase & setInverse(bool value = true);
    bool isInverse() const;

    PropertyInfoBase & setForeignKey(bool value = true);
    bool isForeignKey() const;

    PropertyInfoBase & setHasDetailUri(bool value = true);
    bool hasDetailUri() const;

    PropertyInfoBase & setDefinesOwnership(bool value = true);
    bool definesOwnership() const;

    PropertyInfoBase & setCaseSensitivity(Qt::CaseSensitivity value);
    Qt::CaseSensitivity caseSensitivity() const;

    PropertyInfoBase & setConversion(const Conversion *conversion);
    const Conversion * conversion() const;

    PropertyInfoBase & setParent(const PropertyInfoBase &parent);
    const PropertyInfoBase & parent() const;

    Cubi::Pattern bindProperty(const Cubi::Value &source,
                               const Cubi::Value &target = Cubi::Value()) const;

protected:
    template<class ResourceClass>
    static const QString & resourceIri()
    {
        return ResourceClass::iri();
    }

protected:
    const PropertyInfoData * data() const
    {
        return reinterpret_cast<const PropertyInfoData *>(qGetPtrHelper(d));
    }

    PropertyInfoData * data()
    {
        return reinterpret_cast<PropertyInfoData *>(qGetPtrHelper(d));
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

template<> inline const QString &
PropertyInfoBase::resourceIri<double>()
{
    return Cubi::Resources::xsd::double_::iri();
}

template<> inline const QString &
PropertyInfoBase::resourceIri<QDateTime>()
{
    return Cubi::Resources::xsd::dateTime::iri();
}

template<> inline const QString &
PropertyInfoBase::resourceIri<QString>()
{
    return Cubi::Resources::xsd::string::iri();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Property>
class PropertyInfo : public PropertyInfoBase {
public:
    explicit PropertyInfo(const QString &name = QString()) :
        PropertyInfoBase(Property::resource(), Property::function(), Property::iri(), name,
                         Subject::fromResource<typename Property::Domain>(),
                         Subject::fromResource<typename Property::Range>(),
                         resourceIri<typename Property::Domain>(),
                         resourceIri<typename Property::Range>(),
                         Property::singleValued())
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class PropertyInfoList : public QList<PropertyInfoBase>
{
public: // nested types
    typedef QList<PropertyInfoBase> BaseType;

public: // constructors
    PropertyInfoList() : QList<PropertyInfoBase>() {}
    PropertyInfoList(const PropertyInfoBase &pi) : QList<PropertyInfoBase>() { append(pi); }
    PropertyInfoList(const QList<PropertyInfoBase> &other) : QList<PropertyInfoBase>(other) {}
    PropertyInfoList(const PropertyInfoList::ConstIterator &first,
                     const PropertyInfoList::ConstIterator &last,
                     const PropertyInfoList &context = PropertyInfoList());

public: // methods
    bool startsWith(const PropertyInfoBase &predicate) const;
    bool startsWith(const QList<PropertyInfoBase> &otherChain) const;
    // Returns @c true if there is an inverse property in the chain.
    bool hasInverseProperty() const;
    // Returns @c true if all properties in the chain except the last one are single-valued.
    // This function is mainly useful if you want to know whether you can use
    // predicate functions to select a field.
    // In non-strict mode, allows the last property in the chain to be multi-valued.
    bool isSingleValued(bool strict = false) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

inline QDebug
operator<<(QDebug debug, const ResourceInfo &resource)
{
    return debug << "Resource<" << qPrintable(resource.iri()) << ">";
}

////////////////////////////////////////////////////////////////////////////////////////////////////

extern const PropertyInfoBase piHasAffiliation;
extern const PropertyInfoBase piHasProperty;

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERRESOURCEINFO_H
