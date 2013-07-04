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

#include "contactsaverequest.h"
#include "engine.h"
#include "guidalgorithm.h"

#include <dao/contactdetail.h>
#include <dao/contactdetailschema.h>
#include <dao/subject.h>
#include <dao/support.h>

#include <lib/avatarutils.h>
#include <lib/constants.h>
#include <lib/customdetails.h>
#include <lib/garbagecollector.h>
#include <lib/requestextensions.h>
#include <lib/settings.h>
#include <lib/sparqlconnectionmanager.h>
#include <lib/sparqlresolver.h>

#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef QMap<QString, Value> AffiliationMap;
typedef QList<class ExplictObjectInfo> ExplictObjectInfoList;
typedef QList<class ForeignObjectInfo> ForeignObjectInfoList;
typedef QMultiHash<PropertyInfoList, const QString> ResourceTypes;
typedef QList<class DetailMapping> DetailMappingList;
typedef QHash<PropertyInfoList, Value> PredicateValueHash;

///////////////////////////////////////////////////////////////////////////////////////////////////

class DetailMappingData : public QSharedData
{
    friend class DetailMapping;

public: // constructors
    DetailMappingData(const QContactDetail &genericDetail,
                      const QTrackerContactDetail *trackerDetail)
        : m_genericDetail(genericDetail)
        , m_trackerDetail(trackerDetail)
        , m_contextsNormalized(false)
    {
    }

private: // fields
    QContactDetail                  m_genericDetail;
    const QTrackerContactDetail *   m_trackerDetail;
    bool                            m_contextsNormalized : 1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class DetailMapping
{
public: // constructors
    DetailMapping(const QContactDetail &genericDetail,
                  const QTrackerContactDetail *trackerDetail)
        : d(new DetailMappingData(genericDetail, trackerDetail))
    {
    }

public: // attributes
    /// a detail which has no own ontology representation
    bool isCustomDetail() const;

    /// is a detail which is generated from other details and not stored itself
    bool isSyntheticDetail() const;


    QString definitionName() const;
    QStringList contexts() const;
    bool hasContext() const;

    QString detailUri() const;
    QStringList linkedDetailUris() const;

    QVariant fieldValue(const QString &fieldName) const;
    QVariantMap fieldValues() const;

    const QList<QTrackerContactDetailField> & trackerFields() const;
    const QTrackerContactDetailField * subTypeField() const;
    QTrackerContactSubject::Scheme detailUriScheme() const;

public: // methods
    QString makeResourceIri() const;
    bool updateDetailUri(const DetailMappingList &mappings);

private: // fields
    QExplicitlySharedDataPointer<DetailMappingData> d;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class RelatedObjectInfoData : public QSharedData
{
    friend class RelatedObjectInfo;

public:
    virtual ~RelatedObjectInfoData() {}

protected: // constructor
    RelatedObjectInfoData(const QString &typeIri, const QTrackerContactDetail &detail)
        : m_typeIri(typeIri)
        , m_hasCustomFields(not detail.customValueChains().isEmpty())
    {
        QSet<QString> predicateIriSet;
        QSet<QString> subTypeIriSet;

        foreach(const QTrackerContactDetailField &field, detail.fields()) {
            if (field.permitsCustomValues()) {
                predicateIriSet += nao::hasProperty::iri();
            }

            if (field.hasSubTypes()) {
                foreach(const ClassInfoBase &ci, field.subTypeClasses()) {
                    subTypeIriSet += ci.iri();
                }

                continue;
            }

            if (field.isWithoutMapping() || field.isReadOnly() || field.isInverse()) {
                continue;
            }

            predicateIriSet += field.propertyChain().last().iri();

            foreach(const PropertyInfoBase &pi, field.computedProperties()) {
                predicateIriSet += pi.iri();
            }
        }

        m_predicateIris = predicateIriSet.toList();
        m_subTypeIris = subTypeIriSet.toList();
    }

private: // fields
    QString m_typeIri;
    QStringList m_predicateIris;
    QStringList m_subTypeIris;
    bool m_hasCustomFields : 1;
};

class RelatedObjectInfo
{
protected: // constructor
    RelatedObjectInfo(RelatedObjectInfoData *data)
        : d(data)
    {
    }

public: // operators
    RelatedObjectInfo(const RelatedObjectInfo &other)
        : d(other.d)
    {
    }

    RelatedObjectInfo & operator=(const RelatedObjectInfo &other)
    {
        return d = other.d, *this;
    }

public: // attributes
    const QString & typeIri() const { return d->m_typeIri; }
    const QStringList & predicateIris() const { return d->m_predicateIris; }
    const QStringList & subTypeIris() const { return d->m_subTypeIris; }
    bool hasCustomFields() const { return d->m_hasCustomFields; }

protected: // fields
    QExplicitlySharedDataPointer<RelatedObjectInfoData> d;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class ExplictObjectInfoData : public RelatedObjectInfoData
{
    friend class ExplictObjectInfo;

private: // constructor
    ExplictObjectInfoData(const QString &typeIri, const QString &objectIri,
                          const QTrackerContactDetail &detail)
        : RelatedObjectInfoData(typeIri, detail)
        , m_objectIri(objectIri)
    {
    }

private: // fields
    QString m_objectIri;
};

class ExplictObjectInfo : public RelatedObjectInfo
{
public: // constructor
    ExplictObjectInfo(const QString &typeIri, const QString &objectIri,
                      const QTrackerContactDetail &detail)
        : RelatedObjectInfo(new ExplictObjectInfoData(typeIri, objectIri, detail))
    {
    }

public: // attributes
    const QString & objectIri() const { return data()->m_objectIri; }

protected:
    const ExplictObjectInfoData * data() const
    {
        return static_cast<const ExplictObjectInfoData *>(d.data());
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class ForeignObjectInfoData : public RelatedObjectInfoData
{
    friend class ForeignObjectInfo;

private: // constructor
    ForeignObjectInfoData(const QString &typeIri, const QString &keyPropertyIri,
                          const QVariant &keyValue, const QTrackerContactDetail &detail)
        : RelatedObjectInfoData(typeIri, detail)
        , m_keyPropertyIri(keyPropertyIri)
        , m_keyValue(keyValue)
    {
    }

private: // fields
    QString m_keyPropertyIri;
    QVariant m_keyValue;
};

class ForeignObjectInfo : public RelatedObjectInfo
{
public: // constructor
    ForeignObjectInfo(const QString &typeIri, const QString &keyPropertyIri,
                      const QVariant &keyValue, const QTrackerContactDetail &detail)
        : RelatedObjectInfo(new ForeignObjectInfoData(typeIri, keyPropertyIri, keyValue, detail))
    {
    }

public: // attributes
    const QString & keyPropertyIri() const { return data()->m_keyPropertyIri; }
    const QVariant & keyValue() const { return data()->m_keyValue; }

protected:
    const ForeignObjectInfoData * data() const
    {
        return static_cast<const ForeignObjectInfoData *>(d.data());
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class UpdateBuilder
{
public: // constructors
    UpdateBuilder(const QTrackerContactSaveRequest *request,
                  const QContact &contact, const QString &contactIri,
                  const QHash<QString, QContactDetail> &detailsByUri,
                  const QStringList &detailMask = QStringList());

public: // attributes
    const QContactGuid & guidDetail() const { return m_guidDetail; }

    const QDateTime & lastAccessTimestamp() const { return m_lastAccessTimestamp; }
    const QDateTime & lastChangeTimestamp() const { return m_lastChangeTimestamp; }
    const QDateTime & creationTimestamp() const { return m_creationTimestamp; }

    bool preserveGuid() const { return m_preserveGuid; }
    bool preserveTimestamp() const { return m_preserveTimestamp; }
    bool preserveSyncTarget() const { return m_preserveSyncTarget; }

    const DetailMappingList & detailMappings() const { return m_detailMappings; }
    const QTrackerContactDetailSchema & schema() const { return m_schema; }

    QString queryString();

    bool isExistingContact() const { return 0 != m_contactLocalId; }
    bool isPartialSaveRequest() const { return isExistingContact() && not m_detailMask.isEmpty(); }
    bool isUnknownDetail(const QString &name) const { return isPartialSaveRequest() && not m_detailMask.contains(name); }
    template <class T> bool isUnknownDetail() const { return isUnknownDetail(T::DefinitionName); }

protected: // attributes
    const QctLogger & qctLogger() const { return m_logger; }

protected: // methods
    void collectInsertions();

    QString makeUniqueName(const QString &basename = QLatin1String("_"));

    Value & lookupAffiliation(const QString &context);

    enum InsertValueMode { EnforceNewValue, PreserveOldValue };

    void insertValue(const Value &subject, const Value &predicate, const Value &value,
                     InsertValueMode mode = EnforceNewValue);

    void insertCustomValues(const Value &subject, const QString &detailName,
                            const QString &fieldName, const QVariant &value,
                            const QVariantList &allowableValues = QVariantList());

    void insertCustomValues(const Value &subject, const QString &detailName,
                            const QTrackerContactDetailField &field, const QVariant &value);

    void insertCustomDetail(const Value &subject, const DetailMapping &detail);

    void insertDetailField(const Value &subject, const DetailMapping &detail,
                           const QTrackerContactDetailField &field, const QVariant &value,
                           const ResourceTypes &types, PredicateValueHash &objectCache);

    void appendPredicateChain(const QString &prefix,
                              const PropertyInfoList::ConstIterator &begin,
                              const PropertyInfoList::ConstIterator &end,
                              const QString &target, const QString &suffix);

    void collectRelatedObjects();
    void deleteRelatedObjects();
    void deleteContactProperties();
    void deleteAllContactProperties();
    void deleteMaskedContactProperties();

    void insertForeignKeyObjects();

private: // fields
    const QctLogger &m_logger;
    const QTrackerContactDetailSchema &m_schema;
    const QStringList m_detailMask;

    const ResourceValue m_graphIri;
    const QString m_graphIriSparql;

    const QContactLocalId m_contactLocalId;
    const ResourceValue m_contactIri;
    const QString m_contactIriSparql;

    QContactGuid m_guidDetail;
    QDateTime m_lastAccessTimestamp;
    QDateTime m_lastChangeTimestamp;
    QDateTime m_creationTimestamp;
    QString m_syncTarget;

    DetailMappingList m_detailMappings;

    AffiliationMap m_affiliations;

    bool m_preserveGuid : 1;
    bool m_preserveTimestamp : 1;
    bool m_preserveSyncTarget : 1;

    QList<QTrackerContactDetail> m_implictlyRelatedObjects;
    QMap<QString, ExplictObjectInfoList> m_explicitlyRelatedObjects;
    QMap<QString, ForeignObjectInfoList> m_foreignKeyRelatedObjects;
    QList<Pattern> m_explicitInsertStatements;
    PatternGroup m_explicitInsertRestrictions;
    QList<Insert> m_implicitInsertStatements;
    QString m_queryString;
    int m_variableCounter;

    const Options::SparqlOptions m_sparqlOptions;
    const ValueList m_weakSyncTargets;
};

///////////////////////////////////////////////////////////////////////////////////////////////////


template <typename T>
inline bool operator<(const QList<T> &a, const QList<T> &b)
{
    for(int i = 0; i < a.count() && i < b.count(); ++i) {
        if (a[i] < b[i]) {
            return true;
        } else if (b[i] < a[i]) {
            return false;
        }
    }

    return a.count() < b.count();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

inline bool
DetailMapping::isCustomDetail() const
{
    return (0 == d->m_trackerDetail);
}

inline bool
DetailMapping::isSyntheticDetail() const
{
    return (0 != d->m_trackerDetail && d->m_trackerDetail->isSynthesized());
}

QStringList
DetailMapping::contexts() const
{
    if (0 != d->m_trackerDetail && not d->m_trackerDetail->hasContext()) {
        return QStringList();
    }

    if (not d->m_contextsNormalized) {
        QStringList normalizedContexts;

        foreach(const QString &context, d->m_genericDetail.contexts().toSet()) {
            normalizedContexts += qctCamelCase(context);
        }

        d->m_genericDetail.setContexts(normalizedContexts);
        d->m_contextsNormalized = true;
    }

    return d->m_genericDetail.contexts();
}

bool
DetailMapping::hasContext() const
{
    return (0 != d->m_trackerDetail && d->m_trackerDetail->hasContext());
}

inline QString
DetailMapping::definitionName() const
{
    return d->m_genericDetail.definitionName();
}

inline QString
DetailMapping::detailUri() const
{
    return d->m_genericDetail.detailUri();
}

inline QStringList
DetailMapping::linkedDetailUris() const
{
    return d->m_genericDetail.linkedDetailUris();
}

inline QVariant
DetailMapping::fieldValue(const QString &fieldName) const
{
    return d->m_genericDetail.variantValue(fieldName);
}

inline QVariantMap
DetailMapping::fieldValues() const
{
    return d->m_genericDetail.variantValues();
}

inline const QList<QTrackerContactDetailField> &
DetailMapping::trackerFields() const
{
    if (0 == d->m_trackerDetail) {
        static const QList<QTrackerContactDetailField> nothing;
        return nothing;
    }

    return d->m_trackerDetail->fields();
}

inline const QTrackerContactDetailField *
DetailMapping::subTypeField() const
{
    return (0 != d->m_trackerDetail ? d->m_trackerDetail->subTypeField() : 0);
}

inline QTrackerContactSubject::Scheme
DetailMapping::detailUriScheme() const
{
    return (0 != d->m_trackerDetail ? d->m_trackerDetail->detailUriScheme()
                                    : QTrackerContactSubject::None);
}

QString
DetailMapping::makeResourceIri() const
{
    QTrackerContactSubject::Scheme scheme = QTrackerContactSubject::None;
    QVariantList values;

    // build new detail URI from current detail content
    foreach(const QTrackerContactDetailField &field, trackerFields()) {
        PropertyInfoList::ConstIterator detailUriProperty = field.detailUriProperty();

        if (detailUriProperty == field.propertyChain().constEnd()) {
            continue;
        }

        // don't mess with read only fields
        if (field.propertyChain().first().isReadOnly()) {
            continue;
        }

        // get schema from very first detail URI field
        if (values.isEmpty()) {
            scheme = detailUriProperty->resourceIriScheme();

            if (QTrackerContactSubject::None == scheme) {
                qctWarn(QString::fromLatin1("detail URI property of %1 detail's %2 field "
                                            "doesn't have an IRI scheme assigned").
                        arg(definitionName(), field.name()));
                continue;
            }
        }

        // collect value of this detail URI field
        values.append(fieldValue(field.name()));
    }

    // detail URI doesn't use field values, abort
    if (values.isEmpty()) {
        return QString();
    }

    return QTrackerContactSubject::makeIri(scheme, values);
}

bool
DetailMapping::updateDetailUri(const DetailMappingList &mappings)
{
    // skip custom details
    if (isCustomDetail()) {
        return false;
    }

    const QString oldDetailUri = detailUri();
    const QString newDetailUri = makeResourceIri();

    // abort if nothing to see
    if (newDetailUri.isEmpty() || newDetailUri == oldDetailUri) {
        return false;
    }

    // update detail URI
    d->m_genericDetail.setDetailUri(newDetailUri);

    // update details pointing to this one
    foreach(const DetailMapping &otherDetail, mappings) {
        QStringList linkedDetails = otherDetail.linkedDetailUris();

        // try to remove old URI from list of linked details
        if (linkedDetails.removeOne(oldDetailUri)) {
            // detail was linked, therefore update the link
            otherDetail.d->m_genericDetail.setLinkedDetailUris(linkedDetails << newDetailUri);
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactSaveRequest::QTrackerContactSaveRequest(QContactAbstractRequest *request,
                                                       QContactTrackerEngine *engine,
                                                       QObject *parent)
    : QTrackerBaseRequest<QContactSaveRequest>(engine, parent)
    , m_contacts(staticCast(request)->contacts())
    , m_detailMask(staticCast(request)->definitionMask())
    , m_nameOrder(QctRequestExtensions::get(request)->nameOrder())
    , m_timestamp(QDateTime::currentDateTime())
    , m_batchSize(0)
    , m_updateCount(0)
{
    if (not engine->mangleAllSyncTargets()) {
        m_weakSyncTargets.addValue(LiteralValue(QString()));

        foreach(const QString &target, engine->weakSyncTargets()) {
            m_weakSyncTargets.addValue(LiteralValue(target));
        }
    }
}

QTrackerContactSaveRequest::~QTrackerContactSaveRequest()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static ResourceTypes
collectResourceTypes(const DetailMapping &detail)
{
    ResourceTypes types;

    foreach(const QTrackerContactDetailField &field, detail.trackerFields()) {
        if (field.propertyChain().isEmpty()) {
            continue;
        }

        PropertyInfoList predicates;

        const PropertyInfoList::ConstIterator end(field.propertyChain().constEnd() - 1);
        for(PropertyInfoList::ConstIterator pi(field.propertyChain().constBegin()); pi != end; ++pi) {
            const PropertyInfoBase &npi(*(pi + 1)); // next predicate

            predicates += *pi;

            if (npi.isInverse()) {
                types.insertMulti(predicates, npi.rangeIri());
            } else {
                types.insertMulti(predicates, npi.domainIri());
            }

            if (pi->isInverse()) {
                types.insertMulti(predicates, pi->domainIri());
            } else {
                types.insertMulti(predicates, pi->rangeIri());
            }
        }

        QVariant fieldValue = detail.fieldValue(field.name());

        if (field.hasSubTypeClasses() && not fieldValue.isNull()) {
            QSet<QString> subTypes;

            if (fieldValue.isNull() && not field.defaultValue().isNull()) {
                fieldValue = field.defaultValue();
            }

            switch(fieldValue.type()) {
            case QVariant::String:
                subTypes.insert(fieldValue.toString());
                break;

            case QVariant::StringList:
                subTypes.unite(fieldValue.toStringList().toSet());
                break;

            default:
                qctWarn(QString::fromLatin1("Invalid type '%1' for subtype field %3 of %2 detail").
                        arg(QLatin1String(fieldValue.typeName()),
                            detail.definitionName(), field.name()));
                continue;
            }

            predicates.append(field.propertyChain().last());

            foreach(const ClassInfoBase &i, field.subTypeClasses()) {
                if (subTypes.contains(i.text())) {
                    types.insertMulti(predicates, i.iri());
                }
            }
        }
    }

    return types;
}

static QVariant
findSubTypeValue(const DetailMapping &detail, const QTrackerContactDetailField *const subTypeField)
{
    QVariant value;

    if (0 != subTypeField && subTypeField->hasSubTypeProperties()) {
        value = detail.fieldValue(subTypeField->name());

        if (QVariant::StringList == value.type()) {
            // courteously pick first list element if the user ignores our detail schema
            QStringList valueList = value.toStringList();

            if (not valueList.isEmpty()) {
                value = valueList.first();
            } else {
                value = subTypeField->defaultValue();
            }

            if (valueList.size() > 1) {
                qctWarn(QString::fromLatin1("Subtype field %3 of %2 detail is of type %1, "
                                            "picking first value from supplied list.").
                        arg(QLatin1String(QVariant::typeToName(subTypeField->dataType())),
                            detail.definitionName(), subTypeField->name()));
            }
        }

        // barf if the subtype value is not useful at all
        if (not value.isNull() && QVariant::String != value.type()) {
            qctWarn(QString::fromLatin1("Invalid value type %1 for subtype field %3 of %2 detail").
                    arg(QLatin1String(value.typeName()), detail.definitionName(),
                        subTypeField->name()));
            value = QVariant();
        }
    }

    return value;

}

static const ResourceValue
findSubTypePredicate(const DetailMapping &detail, const QTrackerContactDetailField &field,
                     const QTrackerContactDetailField *const subTypeField)
{
    if (subTypeField) {
        const QVariant subTypeValue = findSubTypeValue(detail, subTypeField);

        if (subTypeValue.isValid()) {
            foreach(const PropertyInfoBase &pi, subTypeField->subTypeProperties()) {
                if (pi.value() == subTypeValue) {
                    return pi.resource();
                }
            }
        }
    }

    return field.propertyChain().last().resource();
}

QString
UpdateBuilder::makeUniqueName(const QString &basename)
{
    return basename + QString::number(m_variableCounter++);
}

static QString
name(const QString &prefix, const QString &suffix)
{
    if (not suffix.isEmpty()) {
        return prefix + QLatin1Char('_') + suffix;
    }

    return prefix;
}

Value &
UpdateBuilder::lookupAffiliation(const QString &context)
{
    AffiliationMap::Iterator affiliation = m_affiliations.find(context);

    // create nco::Affiliation resource when needed
    if (affiliation == m_affiliations.end()) {
        const QString baseName = name(QLatin1String("Affiliation"), context);
        affiliation = m_affiliations.insert(context, BlankValue(makeUniqueName(baseName)));
   }

    return *affiliation;
}

void
UpdateBuilder::insertCustomValues(const Value &subject,
                                  const QString &detailName, const QString &fieldName,
                                  const QVariant &value, const QVariantList &allowableValues)
{
    QVariantList customValues;

    switch(value.type()) {
    case QVariant::StringList:
        foreach(const QString &element, value.toStringList()) {
            if (not element.isEmpty() && not allowableValues.contains(element)) {
                customValues.append(element);
            }
        }

        break;

    case QVariant::List:
        foreach(const QVariant &element, value.toList()) {
            if (not element.isNull() && not allowableValues.contains(element)) {
                customValues.append(element);
            }
        }

        break;

    default:
        if (not value.isNull() && not allowableValues.contains(value)) {
            customValues.append(value);
        }
    }

    foreach(const QVariant &element, customValues) {
        Value property = BlankValue(makeUniqueName(name(detailName, fieldName)));

        insertValue(subject, rdf::type::resource(), nie::InformationElement::resource());
        insertValue(subject, nao::hasProperty::resource(), property);

        insertValue(property, rdf::type::resource(), nao::Property::resource());
        insertValue(property, nao::propertyName::resource(), LiteralValue(fieldName));
        insertValue(property, nao::propertyValue::resource(), LiteralValue(element));
    }
}

void
UpdateBuilder::insertCustomValues(const Value &subject, const QString &detailName,
                                  const QTrackerContactDetailField &field,
                                  const QVariant &value)
{
    insertCustomValues(subject, detailName, field.name(), value,
                       field.describeAllowableValues());
}

static PropertyInfoList::ConstIterator
endOfPropertyChain(const QTrackerContactDetailField &field)
{
    if (field.isWithoutMapping() || not field.hasLiteralValue()) {
        return field.propertyChain().constEnd();
    }

    return field.propertyChain().constEnd() - 1;
}

static QString
makeSubjectIri(const DetailMapping &detail,
               const QTrackerContactDetailField &field,
               const PropertyInfoList::ConstIterator &pi,
               const QVariant &value)
{
    const PropertyInfoList &propertyChain = field.propertyChain();

    QString subjectIri;

    const PropertyInfoList::ConstIterator tail = propertyChain.constEnd() - 1;
    const QTrackerContactSubject::Scheme resourceIriScheme = pi->resourceIriScheme();

    if (QTrackerContactSubject::isContentScheme(resourceIriScheme)) {
        // Prefer the detail's explicit URI for tail nodes, but only if the
        // property's subject scheme matches the detail's scheme. This additional
        // check is needed because details like Organization randomly spread their
        // fields over many different resource types.
        if (pi == tail && resourceIriScheme == detail.detailUriScheme()) {
            subjectIri = detail.detailUri();
        }

        // If we don't have an resource IRI yet generate it from property and value.
        if (subjectIri.isEmpty()) {
            subjectIri = pi->makeResourceIri(value);
        }
    }

    return subjectIri;
}

void
UpdateBuilder::insertDetailField(const Value &subject, const DetailMapping &detail,
                                 const QTrackerContactDetailField &field, const QVariant &value,
                                 const ResourceTypes &types, PredicateValueHash &objectCache)
{
    if (not value.isValid()) {
        qctWarn(QString::fromLatin1("Value must be valid for %2 field of %1 detail").
                arg(detail.definitionName(), field.name()));
        return;
    }

    const QTrackerContactDetailField *const subTypeField = detail.subTypeField();
    const PropertyInfoList &propertyChain = field.propertyChain();

    if (propertyChain.isEmpty()) {
        qctWarn(QString::fromLatin1("RDF property chain needed for %2 field of %1 detail").
                arg(detail.definitionName(), field.name()));
        return;
    }

    if (propertyChain.first().isReadOnly()) {
        return;
    }

    PropertyInfoList predicateChain;
    QList<Value> axis;
    axis.append(subject);

    const PropertyInfoList::ConstIterator end = endOfPropertyChain(field);

    const Value literalValue = qctMakeCubiValue(value);
    bool literalValuePending = true;

    for(PropertyInfoList::ConstIterator pi = propertyChain.constBegin(); pi != end; ++pi) {
        if (pi->isReadOnly()) {
            break;
        }

        PredicateValueHash::ConstIterator object = objectCache.find(predicateChain << *pi);

        if (object == objectCache.constEnd()) {
            // figure out subject IRI for this field

            const PropertyInfoList::ConstIterator npi = pi + 1;

            const QString subjectIri =
                    npi != propertyChain.constEnd() && not npi->isForeignKey()
                    ? makeSubjectIri(detail, field, npi, value)
                    : QString();

            // name object variable and assign subject IRI
            Value objectVariable;

            if (not subjectIri.isEmpty()) {
                // assign generated subject IRI
                objectVariable = ResourceValue(subjectIri);
            } else if (npi != propertyChain.constEnd() || field.isWithoutMapping()) {
                // create named blank variable if no subject IRI could be built. If the
                // field has no mapping, then it's saved using a nao:Property, so create
                // a blank node for that resource.
                const QString basename = name(detail.definitionName(), QLatin1String("Resource"));

                // if we're on the end of the property chanin this objectVariable
                // is about to become axis.last()
                // Now we have to check if it's a foreign value and make it a normal
                // variable, not a named blank variable if that's the case
                if ((not field.isWithoutMapping()) &&
                    field.propertyChain().last().isForeignKey() &&
                    (npi + 1 == propertyChain.constEnd())) {
                    objectVariable = Variable(makeUniqueName(basename));
                } else {
                    objectVariable = BlankValue(makeUniqueName(basename));
                }
            } else {
                objectVariable = literalValue;
                literalValuePending = false;
            }

            object = objectCache.insert(predicateChain, objectVariable);

            if (npi == propertyChain.constEnd() || not npi->isForeignKey()) {
                // assign RDF classes to this field
                ResourceTypes::ConstIterator fieldResource = types.find(predicateChain);

                if (types.constEnd() == fieldResource) {
                    qctWarn(QString::fromLatin1("Cannot find resource types for %3 property "
                                                "of %1 detail's %2 field").
                            arg(detail.definitionName(), field.name(),
                                predicateChain.last().iri()));
                }

                for(; fieldResource != types.constEnd() && fieldResource.key() == predicateChain; ++fieldResource) {
                    // create insert statement of this resource type
                    insertValue(*object, rdf::type::resource(), ResourceValue(fieldResource.value()));
                }
            }
        }

        // create insert statement with the field's value
        if (pi->isInverse()) {
            insertValue(*object, pi->resource(), axis.last());
        } else {
            insertValue(axis.last(), pi->resource(), *object);
        }

        axis.append(*object);
    }

    // insert custom values for subTypes field
    if (subTypeField && subTypeField->permitsCustomValues()) {
        insertCustomValues(axis.last(), detail.definitionName(), *subTypeField,
                           detail.fieldValue(subTypeField->name()));
    }

    // find proper type of the value to insert
    if (not field.isWithoutMapping()) {
        const ResourceValue subTypePredicate = findSubTypePredicate(detail, field, subTypeField);
        const PropertyInfoBase & valueProperty = field.propertyChain().last();

        if (valueProperty.isForeignKey()) {
            // restrict object variable if current property describes a foreign key
            switch(valueProperty.caseSensitivity()) {
            case Qt::CaseSensitive:
                m_explicitInsertRestrictions.addPattern(axis.last(), valueProperty.resource(), literalValue);
                break;

            case Qt::CaseInsensitive:
                Variable var;
                PatternGroup caseInsensitiveFilter;
                caseInsensitiveFilter.addPattern(axis.last(), valueProperty.resource(), var);
                caseInsensitiveFilter.setFilter(Functions::equal.apply(Functions::lowerCase.apply(var),
                                                                       Functions::lowerCase.apply(literalValue)));
                m_explicitInsertRestrictions.addPattern(caseInsensitiveFilter);
                break;
            }
        } else if (literalValuePending) {
            // store field value
            if (valueProperty.isInverse()) {
                insertValue(literalValue, subTypePredicate, axis.last());
            } else {
                insertValue(axis.last(), subTypePredicate, literalValue);
            }
        }

    }

    // insert computed values
    foreach(const PropertyInfoBase &pi, field.computedProperties()) {
        QVariant computedValue;

        if (pi.conversion()->makeValue(value, computedValue)) {
            insertValue(axis.last(), pi.resource(), qctMakeCubiValue(computedValue));
        }
    }

    // store custom values
    if (field.permitsCustomValues()) {
        insertCustomValues(axis.last(), detail.definitionName(), field, value);
    }
}

UpdateBuilder::UpdateBuilder(const QTrackerContactSaveRequest *request,
                             const QContact &contact, const QString &contactIri,
                             const QHash<QString, QContactDetail> &detailsByUri,
                             const QStringList &detailMask)
    : m_logger(request->qctLogger())
    , m_schema(request->engine()->schema(contact.type()))
    , m_detailMask(detailMask)
    , m_graphIri(QtContactsTrackerDefaultGraphIri)
    , m_graphIriSparql(m_graphIri.sparql())
    , m_contactLocalId(contact.localId())
    , m_contactIri(contactIri)
    , m_contactIriSparql(m_contactIri.sparql())
    , m_variableCounter(1)
    , m_sparqlOptions(request->engine()->updateQueryOptions())
    , m_weakSyncTargets(request->m_weakSyncTargets)
{
   // figure out the contact's timestamps
   const QContactTimestamp timestampDetail = contact.detail<QContactTimestamp>();

   m_lastAccessTimestamp = timestampDetail.variantValue(QContactTimestamp__FieldAccessedTimestamp).toDateTime();
   m_lastChangeTimestamp = timestampDetail.lastModified();
   m_creationTimestamp = timestampDetail.created();

   if (m_lastAccessTimestamp.isNull()) {
       m_lastAccessTimestamp = request->timestamp();
   }

   if (m_creationTimestamp.isNull()) {
       m_creationTimestamp = request->timestamp();
       m_preserveTimestamp = (0 != m_contactLocalId); // no need to delete timestamp for new contacts
   } else {
       m_preserveTimestamp = isUnknownDetail<QContactTimestamp>();
   }

   if (m_lastChangeTimestamp.isNull() || 0 != m_contactLocalId) {
       // only preserve the lastModified field for new contacts...
       m_lastChangeTimestamp = request->timestamp();
   }

   // figure out the contact's GUID
   m_guidDetail = contact.detail<QContactGuid>();

   if (m_guidDetail.guid().isEmpty()) {
       m_guidDetail = request->engine()->guidAlgorithm().makeGuid(contact);
       m_preserveGuid = (0 != m_contactLocalId); // no need to delete GUID for new contacts
   } else {
       m_preserveGuid = isUnknownDetail<QContactGuid>();
   }

   // figure out default sync target
   m_syncTarget = contact.detail<QContactSyncTarget>().syncTarget();

   if (m_syncTarget.isEmpty() || request->engine()->isWeakSyncTarget(m_syncTarget)) {
       m_syncTarget = request->engine()->syncTarget();
       m_preserveSyncTarget = false;
   } else {
       m_preserveSyncTarget = not m_weakSyncTargets.values().isEmpty();
   }

   // collect details and update their URIs
   foreach(const QContactDetail &detail, contact.details()) {
       const QString detailDefinitionName = detail.definitionName();

       // skip those not in the detail mask if given
       if (isUnknownDetail(detailDefinitionName)) {
           continue;
       }

       if (detail.accessConstraints() & QContactDetail::ReadOnly) {
           continue; // don't mess with read-only details, e.g. those merged from social providers
       }

       const QTrackerContactDetail *definition = m_schema.detail(detailDefinitionName);

       if (0 == definition) {
           if (m_schema.isSyntheticDetail(detailDefinitionName)) {
               continue; // don't store synthesized details as nao:Property
           }
       } else {
           // Details like are QContactAvatar are synthetized, and must be turned into
           // internal details like QContactPersonalAvatar or QContactOnlineAvatar.
           definition = definition->findImplementation(m_schema, detailsByUri, detail);
       }

       m_detailMappings.append(DetailMapping(detail, definition));
   }

   foreach(DetailMapping detail, m_detailMappings) {
       detail.updateDetailUri(m_detailMappings);
   }
}

void
UpdateBuilder::insertValue(const Value &subject, const Value &predicate,
                           const Value &value, InsertValueMode mode)
{
    if (mode == PreserveOldValue) {
        Insert insert;

        {
            Graph g = Graph(m_graphIri);
            g.addPattern(subject, predicate, value);
            insert.addData(g);
        }

        {
            Graph g = Graph(m_graphIri);
            g.addPattern(subject, predicate, Variable());

            Exists exists;
            exists.addPattern(g);

            PatternGroup filter;
            filter.setFilter(Functions::not_.apply(Filter(exists)));
            insert.addRestriction(filter);
        }

        m_implicitInsertStatements += insert;
    } else {
        m_explicitInsertStatements.append(Pattern(subject, predicate, value));
    }
}

void
UpdateBuilder::insertCustomDetail(const Value &subject, const DetailMapping &detail)
{
    const QString detailName = detail.definitionName();
    const QVariantMap fields = detail.fieldValues();

    Value detailProperty = BlankValue(makeUniqueName(detailName));

    insertValue(subject, nao::hasProperty::resource(), detailProperty);
    insertValue(detailProperty, rdf::type::resource(), nao::Property::resource());
    insertValue(detailProperty, nao::propertyName::resource(), LiteralValue(detailName));

    for(QVariantMap::ConstIterator i = fields.constBegin(); i != fields.constEnd(); ++i) {
        insertCustomValues(detailProperty, detailName, i.key(), i.value());
    }
}

void
UpdateBuilder::collectInsertions()
{
    m_explicitInsertStatements.clear();

    // Removing Role properties also removed the contact's PersonContact type,
    // so we have to restore it for quick property deletion.
    foreach(const QString &classIri, schema().contactClassIris()) {
        insertValue(m_contactIri, rdf::type::resource(), ResourceValue(classIri));
    }

    // Prepare affiliation for work context in case the contact has an organization detail.
    // This one should be stored with the same affiliation as the work context.
    Value affiliation = lookupAffiliation(QContactDetail::ContextWork);

    // Collect inserts for each regular detail.
    foreach(const DetailMapping &detail, detailMappings()) {
        // Store custom details via nao:Proprty.
        if (detail.isCustomDetail()) {
            insertCustomDetail(m_contactIri, detail);
            continue;
        }

        const QVariantMap detailValues = detail.fieldValues();
        const ResourceTypes resourceTypes = collectResourceTypes(detail);
        PredicateValueHash objectCache;

        foreach(const QTrackerContactDetailField &field, detail.trackerFields()) {
            if (field.hasSubTypes()) {
                // Subtypes are stored indirectly by assigning the proper resource class
                // to the RDF resource representing this field, or by selecting the matching
                // RDF property for storing the detail value.
                continue;
            }

            if (field.isSynthesized()) {
                // Don't save synthesized fields like SyncTarget
                continue;
            }

            const QVariantMap::ConstIterator fieldValue = detailValues.find(field.name());

            if (fieldValue == detailValues.constEnd()) {
                // Could not find value for this field.
                continue;
            }

            QVariant rdfValue;

            if (not field.makeValue(*fieldValue, rdfValue)) {
                // Could not compute the RDF value for this field.
                continue;
            }

            if (not detail.hasContext()) {
                insertDetailField(m_contactIri, detail, field, rdfValue, resourceTypes, objectCache);
            } else {
                QSet<QString> detailContexts = detail.contexts().toSet();

                if (detailContexts.isEmpty()) {
                    detailContexts.insert(QString());
                }

                foreach(const QString &contextName, detailContexts) {
                    Value context = lookupAffiliation(contextName);

                    insertValue(context, rdf::type::resource(), nco::Affiliation::resource());

                    if (not contextName.isEmpty()) {
                        insertValue(context, rdfs::label::resource(), LiteralValue(contextName));
                    }

                    insertValue(m_contactIri, nco::hasAffiliation::resource(), context);
                    insertDetailField(context, detail, field, rdfValue, resourceTypes, objectCache);
                }
            }
        }
    }

    // Check if the prepared affiliation was used for binding contact details.
    // In that case this prepare affiliation must be turned into a proper one.
    // we need to insert _:affiliation nco:org _:Organization before
    // <contact:42> nco:hasAffiliation _:Organization, so we look for the first
    // use of _:affiliation as an object
    const QString affiliationSparql = affiliation.sparql();

    for (int i = 0; i < m_explicitInsertStatements.size(); ++i) {
        const Pattern &statement = m_explicitInsertStatements.at(i);

        if (statement.object().sparql() == affiliationSparql) {
            insertValue(affiliation, rdfs::label::resource(), LiteralValue(QContactDetail::ContextWork));
            insertValue(affiliation, rdf::type::resource(), nco::Affiliation::resource());
            break;
        }
    }

    // Insert timestamps and such.
    insertValue(m_contactIri, nie::contentLastModified::resource(), LiteralValue(lastChangeTimestamp()));
    insertValue(m_contactIri, nie::contentCreated::resource(), LiteralValue(creationTimestamp()),
                preserveTimestamp() ? PreserveOldValue : EnforceNewValue);
    insertValue(m_contactIri, nco::contactUID::resource(), LiteralValue(guidDetail().guid()),
                preserveGuid() ? PreserveOldValue : EnforceNewValue);

    if (not isExistingContact()) {
        // For new contacts just enforce the chosen sync target.
        // For existing contacts some additional measures are taken below.
        insertValue(m_contactIri, nie::generator::resource(), LiteralValue(m_syncTarget));
    }

    // Add prepared INSERT statements to the query string.
    Graph graph(m_graphIri);
    foreach (const Pattern& data, m_explicitInsertStatements) {
        graph.addPattern(data);
    }

    Insert insert;
    insert.addData(graph);
    insert.addRestriction(m_explicitInsertRestrictions);

    m_queryString += insert.sparql(m_sparqlOptions);

    foreach(const Insert &statement, m_implicitInsertStatements) {
        m_queryString += statement.sparql(m_sparqlOptions);
    }

    if (isExistingContact()) {
        // Add "addressbook" sync target if this is an existing contact, but no explict
        // sync target is set yet. Notice, that for new contacts the sync target is
        // written without additional checks. Check the insertValue() call above.
        // For contacts that have their sync target set to "telepathy", we need to
        // reset the sync target to "addressbook". This is done so that the contact
        // will not get deleted by contactsd if the IM account is removed, and the
        // user will not loose any data he/she might have added to the contact.

        Graph graph = Graph(m_graphIri);
        graph.addPattern(m_contactIri, nie::generator::resource(), LiteralValue(m_syncTarget));

        Insert insert(Insert::Replace);
        insert.addData(graph);

        if (not m_preserveSyncTarget) {
            // Apply tracker:coalesce function to ensure we always work with a xsd:string, even
            // if there is no nie:generator property and the property function returns null:
            // Null and '' are not equal in tracker, therefore the IN filter wouldn't match.
            Value generator =  Functions::coalesce.apply(nie::generator::function().apply(m_contactIri),
                                                         LiteralValue(QString()));

            PatternGroup restriction;
            restriction.setFilter(Filter(Functions::in.apply(generator, m_weakSyncTargets)));
            insert.addRestriction(restriction);
        }

        m_queryString += insert.sparql(m_sparqlOptions);
    }
}

void
UpdateBuilder::collectRelatedObjects()
{
    m_implictlyRelatedObjects.clear();
    m_explicitlyRelatedObjects.clear();
    m_foreignKeyRelatedObjects.clear();

    foreach(const QTrackerContactDetail &detail, schema().details()) {
        // skip those not in the detail mask if given
        if (isUnknownDetail(detail.name())) {
            continue;
        }

        const QTrackerContactDetailField *const subjectField = detail.resourceIriField();

        if (0 != subjectField) {
            const PropertyInfoList &subjectPropertyChain = subjectField->propertyChain();
            const PropertyInfoBase &subjectProperty = *subjectField->detailUriProperty();

            if (subjectPropertyChain.first().isReadOnly()) {
                continue;
            }

            if (subjectField->isForeignKey()) {
                foreach(const DetailMapping &mapping, detailMappings()) {
                    const QString &typeIri = subjectProperty.resourceTypeIri();
                    const QString &keyIri = subjectField->propertyChain().last().iri();
                    const QVariant &value = mapping.fieldValue(subjectField->name());

                    if (value.isNull()) {
                        continue; // no such field
                    }

                    m_foreignKeyRelatedObjects[detail.name()] +=
                            ForeignObjectInfo(typeIri, keyIri, value, detail);
                }

                continue; // next detail
            }

            if (QTrackerContactSubject::isContentScheme(subjectProperty.resourceIriScheme())) {
                // delete related objects by their content IRI
                foreach(const DetailMapping &mapping, detailMappings()) {
                    if (mapping.definitionName() != detail.name()) {
                        continue;
                    }

                    const QString &typeIri = subjectProperty.resourceTypeIri();
                    const QString &objectIri = mapping.makeResourceIri();

                    if (objectIri.isEmpty()) {
                        qctWarn(QString::fromLatin1("Empty object IRI for %1 detail which "
                                                    "has a content IRI").arg(detail.name()));
                        continue;
                    }

                    m_explicitlyRelatedObjects[detail.name()] +=
                            ExplictObjectInfo(typeIri, objectIri, detail);
                }

                continue; // next detail
            }
        }

        // no content IRI known, delete via association
        if (not detail.predicateChains().isEmpty()) {
            m_implictlyRelatedObjects += detail;
        }
    }
}

void
UpdateBuilder::appendPredicateChain(const QString &prefix,
                                    const PropertyInfoList::ConstIterator &begin,
                                    const PropertyInfoList::ConstIterator &end,
                                    const QString &target, const QString &suffix)
{
    if (begin != end) {
        PropertyInfoList::ConstIterator predicate = begin;

        m_queryString += prefix;
        m_queryString += QLatin1Char('<');
        m_queryString += predicate->iri();
        m_queryString += QLatin1String("> ");

        while(++predicate != end) {
            m_queryString += QLatin1String("[ <");
            m_queryString += predicate->iri();
            m_queryString += QLatin1String("> ");
        }

        m_queryString += target;

        for(int i = (end - begin); i > 1; --i) {
            m_queryString += QLatin1String(" ]");
        }

        m_queryString += suffix;
    }
}

void
UpdateBuilder::deleteRelatedObjects()
{
    // SPARQL fragments used for building the query.
    static const QString implicitQueryPrefix = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    ?subject %1 ?object .\n"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n");
    static const QString implicitQuerySuffix = QLatin1String
            ("    ?subject %1 ?object .\n"
             "  }\n"
             "}\n");

    static const QString implicitResourcePrefix = QLatin1String("  %1 ");
    static const QString implicitResourceSuffix = QLatin1String(" .\n");

    static const QString explictObjectPredicateQuery = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  %1 ?predicate ?object .\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  %1 ?predicate ?object .\n"
             "  FILTER(?predicate IN (%2)) .\n"
             "}\n");
    static const QString explictObjectSubTypeQuery = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  ?resource a %1 .\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  ?resource a %2 .\n"
             "  FILTER(?resource IN (%3)) .\n"
             "}\n");
    static const QString foreignObjectQuery = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  ?resource nao:hasProperty ?property .\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  ?resource nao:hasProperty ?property ; %3 ?key .\n"
             "  FILTER(?key IN (%4)) .\n"
             "}\n"
             "\n"
             "DELETE\n"
             "{\n"
             "  ?resource a %1 .\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  ?resource a %2 ; %3 ?key .\n"
             "  FILTER(?key IN (%4)) .\n"
             "}\n");

    // Glue together the SPARQL fragments.

#ifndef QT_NO_DEBUG
    m_queryString += QLatin1String
            ("\n"
             "#----------------------------------------------------.\n"
             "# Delete associated objects via their property chain |\n"
             "#----------------------------------------------------'\n");
#endif // QT_NO_DEBUG


    if (0 != m_contactLocalId) {
        foreach(const QTrackerContactDetail &detail, m_implictlyRelatedObjects) {
            foreach(const PropertyInfoList &chain, detail.possessedChains()) {
                const QString predicateIri = ResourceValue(chain.last().iri()).sparql();

                if (detail.hasContext()) {
                    PropertyInfoList contextChain;
                    contextChain += piHasAffiliation;
                    contextChain += chain;

                    m_queryString += implicitQueryPrefix.arg(predicateIri, m_graphIriSparql);
                    appendPredicateChain(implicitResourcePrefix.arg(m_contactIriSparql),
                                         contextChain.constBegin(), contextChain.constEnd() - 1,
                                         QLatin1String("?subject"),
                                         implicitResourceSuffix);
                    m_queryString += implicitQuerySuffix.arg(predicateIri);
                }

                if (chain.length() > 1) {
                    m_queryString += implicitQueryPrefix.arg(predicateIri, m_graphIriSparql);
                    appendPredicateChain(implicitResourcePrefix.arg(m_contactIriSparql),
                                         chain.constBegin(), chain.constEnd() - 1,
                                         QLatin1String("?subject"),
                                         implicitResourceSuffix);
                    m_queryString += implicitQuerySuffix.arg(predicateIri);
                }
            }
        };
    }

#ifndef QT_NO_DEBUG
    m_queryString += QLatin1String
            ("\n"
             "#------------------------------------------------------------.\n"
             "# Delete associated objects via their well known content IRI |\n"
             "#------------------------------------------------------------'\n");
#endif // QT_NO_DEBUG

    foreach(const ExplictObjectInfoList &objects, m_explicitlyRelatedObjects) {
        QStringList objectIriList;
        QStringList subTypeIriList;

        foreach(const ExplictObjectInfo &info, objects) {
            const QString objectSparql = ResourceValue(info.objectIri()).sparql();

            if (not info.predicateIris().isEmpty()) {
                QStringList predicateIriList;

                foreach (const QString &iri, info.predicateIris()) {
                    predicateIriList += ResourceValue(iri).sparql();
                }

                const QString predicateIris = predicateIriList.join(QLatin1String(", "));
                m_queryString += explictObjectPredicateQuery.arg(objectSparql, predicateIris);
            }

            if (not info.subTypeIris().isEmpty()) {
                objectIriList += objectSparql;
            }
        }

        foreach (const QString &iri, objects.first().subTypeIris()) {
            subTypeIriList += ResourceValue(iri).sparql();
        }

        if (not objects.first().subTypeIris().isEmpty()) {
            const QString &subTypeIris = subTypeIriList.join(QLatin1String(", "));
            const QString &objectIris = objectIriList.join(QLatin1String(", "));
            const QString &typeIri = ResourceValue(objects.first().typeIri()).sparql();

            m_queryString += explictObjectSubTypeQuery.arg(subTypeIris, typeIri, objectIris);
        }
    }

#ifndef QT_NO_DEBUG
    m_queryString += QLatin1String
            ("\n"
             "#-------------------------------------------------------.\n"
             "# Delete associated objects via their foreign key value |\n"
             "#-------------------------------------------------------'\n");
#endif // QT_NO_DEBUG

    foreach(const ForeignObjectInfoList &objects, m_foreignKeyRelatedObjects) {
        QStringList foreignKeyList;
        QString keyPropertyIri;
        QStringList subTypeIriList;

        foreach(const ForeignObjectInfo &info, objects) {
            if (not info.subTypeIris().isEmpty()) {
                if (foreignKeyList.isEmpty()) {
                    foreach (const QString &iri, info.subTypeIris()) {
                        subTypeIriList += ResourceValue(iri).sparql();
                    }
                    keyPropertyIri = info.keyPropertyIri();
                }

                foreignKeyList += qctMakeCubiValue(info.keyValue()).sparql();
            }
        }

        if (not foreignKeyList.isEmpty()) {
            const QString subTypeIris = subTypeIriList.join(QLatin1String(", "));
            const QString &keyLiterals = foreignKeyList.join(QLatin1String(", "));
            const QString &typeIri = ResourceValue(objects.first().typeIri()).sparql();

            // FIXME: consider case sensitivity
            m_queryString += foreignObjectQuery.arg(subTypeIris, typeIri,
                                                    keyPropertyIri, keyLiterals);
        }
    }
}

void
UpdateBuilder::deleteContactProperties()
{
    if (0 != m_contactLocalId) {
        if (isPartialSaveRequest()) {
            deleteMaskedContactProperties();
        } else {
            deleteAllContactProperties();
        }
    }
}

void
UpdateBuilder::deleteAllContactProperties()
{
    // SPARQL fragments used for building the query.
    static const QString queryPrefix = QLatin1String
            ("\n"
#ifndef QT_NO_DEBUG
             "#---------------------------------------------------------.\n"
             "# Delete all contact properties within the plugin's graph |\n"
             "#---------------------------------------------------------'\n"
             "\n"
#endif // QT_NO_DEBUG
             "DELETE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 ?predicate ?object .\n"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 ?predicate ?object .\n"
             "    FILTER(?predicate NOT IN(rdf:type,nco:belongsToGroup");
    static const QString contactUIDFilter = QLatin1String(",nco:contactUID");
    static const QString contentCreatedFilter = QLatin1String(",nie:contentCreated");
    static const QString querySuffix = QLatin1String(")) .\n"
             "  }\n"
             "}\n");

    // Glue together the SPARQL fragments.
    m_queryString += queryPrefix.arg(m_contactIriSparql, m_graphIriSparql);

    if (preserveGuid()) {
        // Preserve possibly existing GUID if the caller didn't provide one.
        m_queryString += contactUIDFilter;
    }

    if (preserveTimestamp()) {
        // Preserve possibly existing creation timestamp  if the caller didn't provide one.
        m_queryString += contentCreatedFilter;
    }

    m_queryString += querySuffix;
}

void
UpdateBuilder::deleteMaskedContactProperties()
{
    // SPARQL templates used for building the query.
    static const QString propertiesOnContactQuery = QLatin1String
            ("\n"
#ifndef QT_NO_DEBUG
             "#------------------------------------------------------------.\n"
             "# Delete masked contact properties within the plugin's graph |\n"
             "#------------------------------------------------------------'\n"
             "\n"
#endif // QT_NO_DEBUG
             "DELETE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 ?predicate ?object .\n"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 ?predicate ?object .\n"
             "    FILTER(?predicate IN(%3)) .\n"
             "  }\n"
             "}\n");

    static const QString customDetailsOnContactQuery = QLatin1String
            ("\n"
#ifndef QT_NO_DEBUG
             "#----------------------------------------------------------------.\n"
             "# Delete masked contact custom details within the plugin's graph |\n"
             "#----------------------------------------------------------------'\n"
             "\n"
#endif // QT_NO_DEBUG
             "DELETE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 nao:hasProperty ?property .\n"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 nao:hasProperty ?property .\n"
             "    ?property nao:propertyName ?propertyName .\n"
             "    FILTER(?propertyName IN(\"%3\")) .\n"
             "  }\n"
             "}\n");

    static const QString propertiesOnAffiliationQuery = QLatin1String
            ("\n"
             "DELETE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "%3"
             "  }\n"
             "}\n"
             "WHERE\n"
             "{\n"
             "  GRAPH %2\n"
             "  {\n"
             "    %1 nco:hasAffiliation ?context .\n"
             "%3"
             "  }\n"
             "}\n");
    static const QString propertyOnAffiliationTriple = QLatin1String("    ?context %1 ?object .\n");

    QSet<QString> predicatesOnContact;
    QSet<QString> predicatesOnAffiliation;
    QSet<QString> customDetails;

    // always to be removed
    predicatesOnContact << nie::contentLastModified::resource().sparql();

    if (not preserveGuid()) {
        predicatesOnContact << nco::contactUID::resource().sparql();
    }

    // collect predicates/custom details from details
    QList<const QTrackerContactDetail *> details;

    foreach (const QString &detailDefinitionName, m_detailMask) {
        const QTrackerContactDetail *detail = schema().detail(detailDefinitionName);

        if (detail != 0) {
            details << detail->implementations(schema());
            continue;
        }

        if (schema().isSyntheticDetail(detailDefinitionName)) {
            continue;
        }

        customDetails << detailDefinitionName;
    }

    foreach(const QTrackerContactDetail *detail, details) {
        foreach (const QTrackerContactDetailField &field, detail->fields()) {
            // TODO: not check somewhere else? also do log about this
            if (field.isReadOnly()) {
                continue;
            }

            if (field.isSynthesized()) {
                continue;
            }

            if (field.hasSubTypes()) {
                // Subtypes are stored indirectly by assigning the proper resource class
                // to the RDF resource representing this field, or by selecting the matching
                // RDF property for storing the detail value.
                continue;
            }

            const PropertyInfoList &propertyChain = field.propertyChain();

            if (propertyChain.isEmpty()) {
                qctWarn(QString::fromLatin1("RDF property chain needed for %2 field of %1 detail").
                        arg(detail->name(), field.name()));
                continue;
            }

            ResourceValue firstProperty = propertyChain.first().resource();
            bool isOnAffiliation;

            // is QContactOrganization detail field?
            if (firstProperty == nco::hasAffiliation::resource()) {
                if (propertyChain.count() < 2) {
                    qctWarn(QString::fromLatin1("RDF property chain only has nco:hasAffiliation for %2 field of %1 detail").
                            arg(detail->name(), field.name()));
                    continue;
                }

                firstProperty = propertyChain.at(1).resource();
                isOnAffiliation = true;
            } else {
                isOnAffiliation = detail->hasContext();
            }

            if (isOnAffiliation) {
                predicatesOnAffiliation.insert(firstProperty.sparql());
            } else {
                predicatesOnContact.insert(firstProperty.sparql());
            }
       }
    }

    // finally write to query string
    if (not predicatesOnContact.isEmpty()) {
        const QString predicatesList = QStringList(predicatesOnContact.toList()).join(QLatin1String(","));
        m_queryString += propertiesOnContactQuery.arg(m_contactIriSparql, m_graphIriSparql, predicatesList);
    }

    if (not customDetails.isEmpty()) {
        const QString customDetailDefinitionList = QStringList(customDetails.toList()).join(QLatin1String("\",\""));
        m_queryString += customDetailsOnContactQuery.arg(m_contactIriSparql, m_graphIriSparql, customDetailDefinitionList);
    }

    if (not predicatesOnAffiliation.isEmpty()) {
#ifndef QT_NO_DEBUG
        m_queryString += QLatin1String
                ("\n"
                "#------------------------------------------------------------------------------------.\n"
                "# Delete masked contact properties per context affiliation within the plugin's graph |\n"
                "#------------------------------------------------------------------------------------'\n");
#endif // QT_NO_DEBUG

        // cannot do like for predicatesOnContact, as we do not have the iri of the affiliations
        // instead have to delete each predicate in an own update query
        foreach(const QString &predicate, predicatesOnAffiliation) {
            const QString triple = propertyOnAffiliationTriple.arg(predicate);
            m_queryString += propertiesOnAffiliationQuery.arg(m_contactIriSparql, m_graphIriSparql, triple);
        }
    }
}

QString
UpdateBuilder::queryString()
{
    m_queryString.clear();

    // collect DELETE statements
    collectRelatedObjects();
    deleteRelatedObjects();
    deleteContactProperties();

    // collect INSERT statements
    insertForeignKeyObjects();
    collectInsertions();

    return m_queryString;
}

QContactManager::Error
QTrackerContactSaveRequest::normalizeContact(QContact &contact, QHash<QString, QContactDetail> &detailsByUri) const
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    contact = engine()->compatibleContact(contact, &error);

    if (error != QContactManager::NoError) {
        return error;
    }

    // "The manager will automatically synthesize the display label of the contact when it is saved"
    // http://doc.qt.nokia.com/qtmobility-1.2/qcontactmanager.html#saveContact
    // But for now only if we are not doing just partial saving.
    // Subject to https://bugreports.qt.nokia.com/browse/QTMOBILITY-1788
    if (isFullSaveRequest(contact)) {
        engine()->updateDisplayLabel(contact, m_nameOrder);
    }

    // Collect details by their URI
    foreach(const QContactDetail &detail, contact.details()) {
        if (not detail.detailUri().isEmpty()) {
            detailsByUri.insert(detail.detailUri(), detail);
        }
    }

    // Verify linked detail URIs
    error = QContactManager::NoError;

    foreach(const QContactDetail &detail, contact.details()) {
        foreach(const QString &uri, detail.linkedDetailUris()) {
            if (not detailsByUri.contains(uri)) {
                qctWarn(QString::fromLatin1("%1 detail linking to unknown detail URI %2").
                        arg(detail.definitionName(), uri));
                error = QContactManager::InvalidDetailError;
            }
        }
    }

    return error;
}

static QSet<QString>
findAccountDetailUris(const QContact &contact)
{
    QSet<QString> onlineAccountUris;

    foreach(const QContactOnlineAccount &detail, contact.details<QContactOnlineAccount>()) {
        onlineAccountUris += detail.detailUri();
    }

    return onlineAccountUris;
}

static bool
isLinkedDetail(const QContactDetail &detail, const QSet<QString> &linkedDetailUris)
{
    foreach(const QString &uri, detail.linkedDetailUris()) {
        if (linkedDetailUris.contains(uri)) {
            return true;
        }
    }

    return false;
}

QContactManager::Error
QTrackerContactSaveRequest::writebackThumbnails(QContact &contact)
{
    // skip if thumbnails not in detail mask, if existing
    if (isUnknownDetail(contact, QContactThumbnail::DefinitionName)) {
        return QContactManager::NoError;
    }

    const QSet<QString> accountDetailUris = findAccountDetailUris(contact);

    // Find user supplied thumbnail of the contact itself.
    const QList<QContactThumbnail> thumbnailDetails = contact.details<QContactThumbnail>();
    QContactThumbnail defaultThumbnail;

    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
        qDebug()
                << metaObject()->className() << m_stopWatch.elapsed()
                << ": found" << thumbnailDetails.count() << "thumbnail detail(s)";
    }

    foreach(const QContactThumbnail &thumbnail, thumbnailDetails) {
        if (thumbnail.thumbnail().isNull()) {
            continue;
        }

        if (not isLinkedDetail(thumbnail, accountDetailUris)) {
            defaultThumbnail = thumbnail;
            break;
        }
    }

    if (defaultThumbnail.isEmpty()) {
        return QContactManager::NoError;
    }

    // Find default avatar of the contact itself.
    QContactAvatar defaultAvatar;

    foreach(const QContactAvatar &avatar, contact.details<QContactAvatar>()) {
        if (not isLinkedDetail(avatar, accountDetailUris)) {
            defaultAvatar = avatar;
            break;
        }
    }

    // Get avatar thumbnail from detail and scale down when needed.
    QFile::FileError fileError = QFile::NoError;
    QImage thumbnailImage = defaultThumbnail.thumbnail();
    const QString avatarCacheFileName = qctWriteThumbnail(thumbnailImage, &fileError);

    if (avatarCacheFileName.isEmpty()) {
        // failed to save avatar picture - remove avatar detail
        contact.removeDetail(&defaultAvatar);

        return (QFile::ResizeError == fileError ? QContactManager::OutOfMemoryError
                                                : QContactManager::UnspecifiedError);
    }

    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
        qDebug() << metaObject()->className() << m_stopWatch.elapsed()
                 << ": writing default thumbnail:" << avatarCacheFileName;
    }

    // everything went fine - update avatar detail and proceed
    defaultAvatar.setImageUrl(QUrl::fromLocalFile(avatarCacheFileName));
    contact.saveDetail(&defaultAvatar);

    if (isUnknownDetail(contact, QContactAvatar::DefinitionName)) {
        m_detailMask += QContactAvatar::DefinitionName;
    }

    return QContactManager::NoError;
}

void
UpdateBuilder::insertForeignKeyObjects()
{
    // SPARQL fragments used for building the query.
    static const QString queryPrefix = QLatin1String
           ("\n"
            "INSERT\n"
            "{\n"
            "  GRAPH %1 { %2 a %3 ; %4 %5 }\n"
            "}\n"
            "WHERE\n"
            "{\n"
            "  FILTER(NOT EXISTS"
            "  {\n");
    static const QString caseSensitiveFilter = QLatin1String
           ("    ?resource %1 %2 .\n");
    static const QString caseInsensitiveFilter = QLatin1String
           ("    ?resource %1 ?value .\n"
            "    FILTER(fn:lower-case(%2) = fn:lower-case(?value)) .\n");
    static const QString querySuffix = QLatin1String
           ("  })\n"
            "}\n");

    // Glue together the SPARQL fragments.
#ifndef QT_NO_DEBUG
    m_queryString += QLatin1String
            ("\n"
             "#--------------------------------------------------.\n"
             "# Create shared objects referenced by foreign keys |\n"
             "#--------------------------------------------------'\n");
#endif // QT_NO_DEBUG

    foreach(const DetailMapping &detail, detailMappings()) {
        foreach(const QTrackerContactDetailField &field, detail.trackerFields()) {
            const PropertyInfoList::ConstIterator pi = field.foreignKeyProperty();

            if (field.propertyChain().constEnd() == pi) {
                continue; // skip field if no foreign key property found
            }

            QVariant fieldValue = detail.fieldValue(field.name());

            if (fieldValue.isNull()) {
                continue; // skip field if no matching value stored in actual detail
            }

            if (not field.makeValue(fieldValue, fieldValue)) {
                continue; // skip field if value cannot be converted to storage type
            }


            QString domainIri = ResourceValue(pi->domainIri()).sparql();
            PropertyInfoBase parent = pi->parent();

            if (parent && pi->domainIri() != parent.rangeIri()) {
                domainIri += QLatin1String(", ") + ResourceValue(parent.rangeIri()).sparql();
            }

            const QString predicateIri = ResourceValue(pi->iri()).sparql();
            const QString literalValue = qctMakeCubiValue(fieldValue).sparql();

            if (literalValue.isEmpty()) {
                qctWarn(QString::fromLatin1("Empty literal value for %2 field of %1 detail: %3.").
                        arg(detail.definitionName(), field.name(), fieldValue.toString()));
                continue;
            }

            const QString subjectIri = makeSubjectIri(detail, field, pi, fieldValue);
            const QString subjectSparql = subjectIri.isEmpty() ? QString::fromLatin1("_:_")
                                                               : ResourceValue(subjectIri).sparql();

            m_queryString += queryPrefix.arg(m_graphIriSparql, subjectSparql,
                                             domainIri, predicateIri, literalValue);

            switch(pi->caseSensitivity()) {
            case Qt::CaseSensitive:
                m_queryString += caseSensitiveFilter.arg(predicateIri, literalValue);
                break;
            case Qt::CaseInsensitive:
                m_queryString += caseInsensitiveFilter.arg(predicateIri, literalValue);
                break;
            }

            m_queryString += querySuffix;
        }
    }
}

QString
QTrackerContactSaveRequest::queryString() const
{
    QString queryString;

    foreach(QContact contact, m_contacts) {
        const QString iri = makeAnonymousIri(QUuid::createUuid());
        QHash<QString, QContactDetail> detailsByUri;

        if (QContactManager::NoError == normalizeContact(contact, detailsByUri)) {
            queryString += UpdateBuilder(this, contact, iri, detailsByUri).queryString();
        } else {
            queryString +=  QLatin1String("\n# normalizeContact failed\n");
        }
    }

    return queryString;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
QTrackerContactSaveRequest::resolveContactIris()
{
    // Collect contacts ids that must get resolved into IRIs.
    // Group by type since separate resolvers must be used for each contact type.
    typedef QList<uint> TrackerIdList;
    QHash<QString, TrackerIdList> idsPerContactType;

    foreach(const QContact &contact, m_contacts) {
        TrackerIdList &idsToLookup = idsPerContactType[contact.type()];
        const QContactId &contactId = contact.id();

        if (contactId.managerUri() == engine()->managerUri() || contactId.managerUri().isEmpty()) {
            idsToLookup += contactId.localId();
        } else {
            idsToLookup += 0;
        }
    }

    // Resolve contact IRIs for each relevant contact type.
    QHash<QContactLocalId, QString> contactIriCache;

    for(QHash<QString, TrackerIdList>::ConstIterator
        it = idsPerContactType.constBegin(); it != idsPerContactType.constEnd(); ++it) {
        QctResourceIriResolver resolver(it.value());
        resolver.setClassIris(engine()->schema(it.key()).contactClassIris());

        if (not resolver.lookupAndWait()) {
            reportError(resolver.errors(), QLatin1String("Cannot resolve local ids of saved contacts"));
            return false;
        }

        // Collect contact IRIs.
        // The resolver returns null IRIs when the id was 0, or when no such contact exists.
        for(int i = 0, l = resolver.trackerIds().count(); i < l; ++i) {
            const QContactLocalId id = resolver.trackerIds().at(i);
            const QString &iri = resolver.resourceIris().at(i);

            if (not iri.isNull()) {
                contactIriCache.insert(id, iri);
            }
        }
    }

    for(int i = 0; i < m_contacts.count(); ++i) {
        const QContactLocalId localId = m_contacts.at(i).localId();
        const QString &iri = localId ? contactIriCache.value(localId, QString::null)
                                     : makeAnonymousIri(QUuid::createUuid());

        if (not iri.isNull()) {
            m_contactIris += iri;
        } else {
            m_errorMap.insert(i, QContactManager::DoesNotExistError);
            setLastError(QContactManager::DoesNotExistError);
            m_contactIris += QString();
        }
    }

    // Check if number of resolved tracker ids matches expectation.
    if (m_contacts.count() != m_contactIris.count()) {
        reportError("Resolver provides invalid number of tracker ids");
        return false;
    }

    return true;
}

bool
QTrackerContactSaveRequest::resolveContactIds()
{
    // We left the saveContact() loop, and don't have any pending updates or
    // pending commits. Let's figure out the local ids of the contacts.
    QctTrackerIdResolver resolver(m_contactIris);

    if (not resolver.lookupAndWait()) {
        reportError(resolver.errors(), QLatin1String("Cannot resolve local ids of saved contacts"));
        return false;
    }

    // Assign resolved ids to the individual contacts
    const QList<QContactLocalId> &resolvedIds = resolver.trackerIds();

    for(int i = 0; i < resolvedIds.count(); ++i) {
        if (0 != resolvedIds[i]) {
            QContactId id = m_contacts.at(i).id();
            id.setManagerUri(engine()->managerUri());
            id.setLocalId(resolvedIds[i]);
            m_contacts[i].setId(id);
        } else if (not m_errorMap.contains(i)) {
            qctWarn(QString::fromLatin1("Cannot resolve local id for contact %1/%2 (%3)").
                    arg(QString::number(i + 1), QString::number(m_contacts.count()),
                        resolver.resourceIris().at(i)));
            m_errorMap.insert(i, QContactManager::UnspecifiedError);
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerAbstractRequest::Dependencies
QTrackerContactSaveRequest::dependencies() const
{
    return ResourceCache | GuidAlgorithm;
}

void
QTrackerContactSaveRequest::run()
{
    if (not turnIrreversible()) {
        return;
    }

    if (m_contacts.isEmpty()) {
        return;
    }

    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
        qDebug()
                << metaObject()->className() << 0
                << ": number of contacts:" << m_contacts.count();
    }

    m_stopWatch.start();

    // figure out resource ids for the passed contacts
    if (not resolveContactIris()) {
        return;
    }

    // update tracker with the contacts
    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        reportError("Cannot save contacts: No valid QtSparql connection.");
        return;
    }

    for(int i = 0; i < m_contacts.count(); ++i) {
        QContact &contact = m_contacts[i];
        const QContactId contactId = contact.id();
        const QString contactManagerUri = contactId.managerUri();

        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
            qDebug()
                    << metaObject()->className() << m_stopWatch.elapsed()
                    << ": contact" << i << "- updating";
        }

        if (m_contactIris.at(i).isEmpty()) {
            continue; // skip contacts with non-exisitant, non-zero local id
        }

        // Conforms to behaviour of Symbian backend, but not clear if this is the right behaviour,
        // see QTMOBILITY-1816.
        //
        // hasselmm: It's also not entirely clear how to deal with some valid local id, but an
        // empty manager URI. QTM unit tests don't seem to test this, but relationship tests
        // and API suggest that and empty URI shall be interpreted as placeholder for the
        // actual manager URI.
        if (contactManagerUri != engine()->managerUri() && not contactManagerUri.isEmpty()) {
            qctWarn(QString::fromLatin1("Bad id passed for contact %1/%2: manager: \"%3\", id:%4").
                    arg(QString::number(i + 1), QString::number(m_contacts.count()),
                    contactManagerUri, QString::number(contactId.localId())));
            m_errorMap.insert(i, QContactManager::DoesNotExistError);
            continue;
        }

        // reject new contacts on partial save
        if (engine()->isRestrictive() && contactId.localId() == 0 && not m_detailMask.isEmpty()) {
            qctWarn(QString::fromLatin1("Partial saving not allowed for the new contact %1/%2").
                    arg(QString::number(i + 1), QString::number(m_contacts.count())));
            m_errorMap.insert(i, QContactManager::BadArgumentError);
            continue;
        }

        // drop odd details, add missing details, collect named details
        QHash<QString, QContactDetail> detailsByUri;

        {
            const QContactManager::Error error = normalizeContact(contact, detailsByUri);

            if (error != QContactManager::NoError) {
                qctWarn(QString::fromLatin1("Cannot normalize details for contact %1/%2").
                        arg(QString::number(i + 1), QString::number(m_contacts.count())));
                m_errorMap.insert(i, error);
                continue;
            }
        }

        // normalize and save avatar
        {
            const QContactManager::Error error = writebackThumbnails(contact);

            if (error != QContactManager::NoError) {
                qctWarn(QString::fromLatin1("Cannot save avatar thumbnail for contact %1/%2").
                        arg(QString::number(i + 1), QString::number(m_contacts.count())));
                m_errorMap.insert(i, error);
                continue;
            }
        }

        // build the update query
        UpdateBuilder builder(this, contact, m_contactIris.at(i), detailsByUri, m_detailMask);
        const QString queryString = builder.queryString();

        if (builder.isExistingContact()) {
            ++m_updateCount;
        }

        // run the update query
        const QSparqlQuery query(queryString, QSparqlQuery::InsertStatement);
        const QSparqlQueryOptions &options = (m_contacts.count() > 1 ? SyncBatchQueryOptions
                                                                     : SyncQueryOptions);
        QScopedPointer<QSparqlResult> result(runQuery(query, options, connection));

        if (result.isNull()) {
            qctWarn(QString::fromLatin1("Save request failed for contact %1/%2").
                arg(QString::number(i + 1), QString::number(m_contacts.count())));
            m_errorMap.insert(i, lastError());
            continue;
        }

        if (result->hasError()) {
            qctWarn(QString::fromLatin1("Save request failed for contact %1/%2: %3").
                arg(QString::number(i + 1), QString::number(m_contacts.count()),
                    qctTruncate(result->lastError().message())));
            m_errorMap.insert(i, translateError(result->lastError()));
            continue;
        }
    }

    // update contact ids
    if (not resolveContactIds()) {
        return;
    }

    // report result
    if (lastError() == QContactManager::NoError && not m_errorMap.isEmpty()) {
        setLastError(m_errorMap.constBegin().value());
    }

    // Pump the garbage collector, but only if we really updated contacts:
    // For new contacts we don't have to break any old contact-resource relationships,
    // therefore adding new contacts doesn't increase the amount of garbage.
    if (m_updateCount > 0) {
        const double pollutionLevel = double(m_updateCount) / engine()->gcLimit();
        QctGarbageCollector::trigger(engine()->gcQueryId(), pollutionLevel);
    }
}


void
QTrackerContactSaveRequest::updateRequest(QContactManager::Error error)
{
    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowTiming)) {
        qDebug()
                << metaObject()->className()  << m_stopWatch.elapsed()
                << ": reporting result" << error;
    }

    engine()->updateContactSaveRequest(staticCast(engine()->request(this).data()),
                                       m_contacts, error, m_errorMap,
                                       QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
