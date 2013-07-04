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

#include "contactdetailschema.h"

#include "contactdetail_p.h"

#include <lib/customdetails.h>
#include <lib/logger.h>

#include <ontologies.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailSchemaData : public QSharedData
{
    Q_DISABLE_COPY(QTrackerContactDetailSchemaData)
    friend class QTrackerContactDetailSchema;

protected:
    typedef QTrackerContactDetailSchema::AvatarTypes AvatarTypes;

    explicit QTrackerContactDetailSchemaData(const QString &contactType,
                                             const QStringList &contactClassIris,
                                             const QStringList &ontologyPrefixes,
                                             AvatarTypes avatarTypes);

protected:
    bool isOriginalIri(const QString &iri) const;
    void define(QTrackerContactDetail detail);

private: // constant fields
    const QContactDetailDefinitionMap m_baseDefinitions;
    const QStringList m_contactClassIris;
    const QStringList m_ontologyPrefixes;
    const QString m_contactType;
    AvatarTypes m_avatarTypes;

private: // mutable fields
    mutable QContactDetailDefinitionMap m_definitions;
    mutable QSet<QVariant::Type> m_dataTypes;
    QTrackerContactDetailMap m_details;

private: // flags
    /// whether to write back presence information to tracker
    bool m_writeBackPresence : 1;

    /// whether detail contexts is stored or not (using nco:Affiliation)
    const bool m_contextSupported : 1;

    /// whether we should convert phone numbers in Latin script
    bool m_convertNumbersToLatin : 1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerPersonContactDetailSchemaData : public QTrackerContactDetailSchemaData
{
public:
    explicit QTrackerPersonContactDetailSchemaData(AvatarTypes avatarTypes);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactGroupDetailSchemaData : public QTrackerContactDetailSchemaData
{
public:
    explicit QTrackerContactGroupDetailSchemaData(AvatarTypes avatarTypes);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

static QVariantList
instanceListValues(const InstanceInfoList &instances)
{
    QVariantList values;

    foreach(const InstanceInfoBase &i, instances) {
        values.append(i.value());
    }

    return values;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactAddress : public QTrackerContactDetail
{
public:
    QTrackerContactAddress()
        : QTrackerContactDetail(QContactAddress::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactAddress::FieldCountry).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>().setHasDetailUri()
                               << PropertyInfo<nco::country>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldExtendedAddress).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::extendedAddress>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldLocality).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::locality>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldPostOfficeBox).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::pobox>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldPostcode).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::postalcode>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldRegion).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::region>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldStreet).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::streetAddress>()));

        addField(QTrackerContactDetailField(QContactAddress::FieldSubTypes).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPostalAddress>()).
                 setSubTypeClasses(SubTypeClasses).
                 setDataType(QVariant::StringList).
                 setPermitsCustomValues());
    }

    static const ClassInfoList SubTypeClasses;
};

class QTrackerContactAnniversary : public QTrackerContactDetail
{
public:
    QTrackerContactAnniversary()
        : QTrackerContactDetail(QContactAnniversary::DefinitionName)
    {
        // FIXME: mark ncal:uid as key

        addField(QTrackerContactDetailField(QContactAnniversary::FieldCalendarId).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<ncal::anniversary>()
                               << PropertyInfo<ncal::uid>()));

        addField(QTrackerContactDetailField(QContactAnniversary::FieldOriginalDate).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<ncal::anniversary>()
                               << PropertyInfo<ncal::dtstart>()
                               << PropertyInfo<ncal::dateTime>()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        addField(QTrackerContactDetailField(QContactAnniversary::FieldEvent).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<ncal::anniversary>()
                               << PropertyInfo<ncal::description>()));

        addField(QTrackerContactDetailField(QContactAnniversary::FieldSubType).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<ncal::anniversary>()
                               << PropertyInfo<ncal::categories>()).
                 setAllowableValues(SubTypeValues));

        setHasContext(false);
    }

    static const QVariantList SubTypeValues;
};

class QTrackerContactAvatarData : public QTrackerContactDetailData
{
    friend class QTrackerContactAvatar;

private:
    QTrackerContactAvatarData()
        : QTrackerContactDetailData(QContactAvatar::DefinitionName)
    {
    }

private: // QTrackerContactDetailData API
    virtual const QTrackerContactDetail *
    findImplementation(const QTrackerContactDetailSchema &schema,
                       const QHash<QString, QContactDetail> &otherDetails,
                       const QContactDetail &actualDetail) const
    {
        foreach(const QString &uri, actualDetail.linkedDetailUris()) {
            const QHash<QString, QContactDetail>::ConstIterator linkedDetail = otherDetails.find(uri);

            if (linkedDetail != otherDetails.constEnd() &&
                    QContactOnlineAccount::DefinitionName == linkedDetail->definitionName()) {
                return schema.detail(QContactOnlineAvatar::DefinitionName);
            }
        }

        return schema.detail(QContactPersonalAvatar::DefinitionName);
    }

    virtual QList<const QTrackerContactDetail *>
    implementations(const QTrackerContactDetailSchema &schema) const
    {
        QList<const QTrackerContactDetail *> implementations;

        foreach(const QString &definitionName, m_dependencies) {
            implementations += schema.detail(definitionName);
        }

        return implementations;
    }
};

class QTrackerContactAvatar : public QTrackerContactDetail
{
public:
    QTrackerContactAvatar(QTrackerContactDetailSchema::AvatarTypes avatarTypes)
        : QTrackerContactDetail(new QTrackerContactAvatarData)
    {
        addField(QTrackerContactDetailField(QContactAvatar::FieldImageUrl).
                 setDataType(QVariant::Url).
                 setSynthesized());

        addField(QTrackerContactDetailField(QContactAvatar::FieldVideoUrl).
                 setDataType(QVariant::Url).
                 setSynthesized());

        if (avatarTypes & QTrackerContactDetailSchema::PersonalAvatar) {
            addDependency(QContactPersonalAvatar::DefinitionName);
        }
        if (avatarTypes & QTrackerContactDetailSchema::OnlineAvatar) {
            addDependency(QContactOnlineAvatar::DefinitionName);
        }
        if (avatarTypes & QTrackerContactDetailSchema::SocialAvatar) {
            addDependency(QContactSocialAvatar::DefinitionName);
        }

        setHasContext(false);
    }
};

class QTrackerContactPersonalAvatar : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactPersonalAvatar()
        : QTrackerUniqueContactDetail(QContactPersonalAvatar::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactPersonalAvatar::FieldImageUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::photo>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactPersonalAvatar::FieldVideoUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::video>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        setHasContext(false);
        setInternal(true);
    }
};

class QTrackerContactSocialAvatar : public QTrackerContactDetail
{
public:
    QTrackerContactSocialAvatar(bool readOnly)
        : QTrackerContactDetail (QContactSocialAvatar::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactSocialAvatar::FieldImageUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAvatar>()
                               << PropertyInfo<nie::relatedTo>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactSocialAvatar::FieldSubType).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAvatar>()
                               << PropertyInfo<nie::relatedTo>()
                               << PropertyInfo<rdfs::label>()));

        // needs to be set manually: none of above fields is set by a predicate in the graph of tp contact
        addField(QTrackerContactDetailField(QContactDetail::FieldLinkedDetailUris).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)).
                 setDataType(QVariant::StringList));

        // Context must be considered because so all associanted IMAddresses are matched,
        // those directly attached to the contact, but also those attached to affiliations.
        setHasContext(true);
        setInternal(true);
    }
};

class QTrackerContactOnlineAvatar : public QTrackerContactDetail
{
public:
    QTrackerContactOnlineAvatar(bool readOnly)
        : QTrackerContactDetail (QContactOnlineAvatar::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactOnlineAvatar::FieldImageUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAvatar>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactOnlineAvatar::FieldSubType).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAvatar>()
                               << PropertyInfo<rdfs::label>()));

        // needs to be set manually: none of above fields is set by a predicate in the graph of tp contact
        addField(QTrackerContactDetailField(QContactDetail::FieldLinkedDetailUris).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)).
                 setDataType(QVariant::StringList));

        // Context must be considered because so all associanted IMAddresses are matched,
        // those directly attached to the contact, but also those attached to affiliations.
        setHasContext(true);
        setInternal(true);
    }
};

class QTrackerContactBirthday : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactBirthday()
        : QTrackerUniqueContactDetail(QContactBirthday::DefinitionName)
    {
        // FIXME: mark ncal:uid as key

        addField(QTrackerContactDetailField(QContactBirthday::FieldBirthday).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::birthDate>()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        addField(QTrackerContactDetailField(QContactBirthday::FieldCalendarId).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<ncal::birthday>()
                               << PropertyInfo<ncal::uid>()));
    }
};

class QTrackerContactEmailAddress : public QTrackerContactDetail
{
public:
    QTrackerContactEmailAddress()
        : QTrackerContactDetail(QContactEmailAddress::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactEmailAddress::FieldEmailAddress).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasEmailAddress>().
                                  setHasDetailUri().setDefinesOwnership()
                               << PropertyInfo<nco::emailAddress>().
                                  setForeignKey()));
    }
};

class QTrackerContactGeoLocation : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactGeoLocation()
        : QTrackerUniqueContactDetail(QContactGeoLocation::DefinitionName)
    {
        // NOTICE: FieldLatitude and FieldLongitude also are marked as optional,
        // because FieldAltitude also gives a reasonable useful geo location

        addField(QTrackerContactDetailField(QContactGeoLocation::FieldLabel).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasLocation>()
                               << PropertyInfo<nie::title>()));

        addField(QTrackerContactDetailField(QContactGeoLocation::FieldLatitude).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasLocation>()
                               << PropertyInfo<slo::latitude>()).
                 setDataType(QVariant::Double));

        addField(QTrackerContactDetailField(QContactGeoLocation::FieldLongitude).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasLocation>()
                               << PropertyInfo<slo::longitude>()).
                 setDataType(QVariant::Double));

        addField(QTrackerContactDetailField(QContactGeoLocation::FieldAltitude).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasLocation>()
                               << PropertyInfo<slo::altitude>()).
                 setDataType(QVariant::Double));

        addField(QTrackerContactDetailField(QContactGeoLocation::FieldTimestamp).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasLocation>()
                               << PropertyInfo<slo::timestamp>()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        // TODO: mapping for QContactGeoLocation::FieldAccuracy
        // TODO: mapping for QContactGeoLocation::FieldAltitudeAccuracy
        // TODO: mapping for QContactGeoLocation::FieldHeading
        // TODO: mapping for QContactGeoLocation::FieldSpeed
    }
};

class QTrackerContactGlobalPresence : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactGlobalPresence()
        : QTrackerUniqueContactDetail(QContactGlobalPresence::DefinitionName)
    {
        // NOTICE: there are no RDF properties declared for this detail because
        // this detail is fully synthetisized.

        // TODO: move synthetisation code from engine to this detail

        addField(QTrackerContactDetailField(QContactGlobalPresence::FieldNickname).
                 setSynthesized());

        addField(QTrackerContactDetailField(QContactGlobalPresence::FieldCustomMessage).
                 setSynthesized());

        addField(QTrackerContactDetailField(QContactGlobalPresence::FieldTimestamp).
                 setDataType(QVariant::DateTime).
                 setSynthesized());

        addField(QTrackerContactDetailField(QContactGlobalPresence::FieldPresenceState).
                 setDefaultValue(QContactPresence::PresenceUnknown).
                 setAllowableValues(PresenceStateValues).
                 setDataType(QVariant::Int).
                 setSynthesized());

        addDependency(QContactPresence::DefinitionName);

        // TODO: define QContactGlobalPresence::FieldPresenceStateText
        // TODO: define QContactGlobalPresence::FieldPresenceStateImageUrl
    }

    static const QVariantList PresenceStateValues;
};

class QTrackerContactGuid : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactGuid()
        : QTrackerUniqueContactDetail(QContactGuid::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactGuid::FieldGuid).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::contactUID>().setReadOnly()));
    }
};

class QTrackerContactNote : public QTrackerContactDetail
{
public:
    QTrackerContactNote()
        : QTrackerContactDetail(QContactNote::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactNote::DefinitionName).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::note>()));

        setHasContext(false);
    }
};

class QTrackerContactOnlineAccount : public QTrackerContactDetail
{
public:
    QTrackerContactOnlineAccount(bool readOnly)
        : QTrackerContactDetail(QContactOnlineAccount::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactOnlineAccount__FieldAccountPath).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setHasDetailUri()
                               << PropertyInfo<nco::hasIMContact>().setInverse()).
                 setConversion(TelepathyIriConversion::instance()));

        addField(QTrackerContactDetailField(QContactOnlineAccount::FieldAccountUri).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setHasDetailUri()
                               << PropertyInfo<nco::imID>().setDefinesOwnership()));

        addField(QTrackerContactDetailField(QContactOnlineAccount::FieldServiceProvider).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly).setDefinesOwnership()
                               << PropertyInfo<nco::hasIMContact>().setInverse()
                               << PropertyInfo<nco::imDisplayName>()));

        addField(QTrackerContactDetailField(QContactOnlineAccount::FieldProtocol).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>()
                               << PropertyInfo<nco::imProtocol>()));

        addField(QTrackerContactDetailField(QContactOnlineAccount::FieldCapabilities).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imCapability>()).
                 setAllowableInstances(CapabilityValues).
                 setDataType(QVariant::StringList).
                 setPermitsCustomValues());

        addField(QTrackerContactDetailField(QContactOnlineAccount::FieldSubTypes).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>()).
                 setDataType(QVariant::StringList).
                 setPermitsCustomValues().
                 setWithoutMapping());
    }

    static const InstanceInfoList CapabilityValues;
};

class QTrackerContactPhoneNumber : public QTrackerContactDetail
{
public:
    QTrackerContactPhoneNumber(bool convertToLatin)
        : QTrackerContactDetail(QContactPhoneNumber::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactPhoneNumber::FieldNumber).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPhoneNumber>().setHasDetailUri().setDefinesOwnership()
                               << PropertyInfo<nco::phoneNumber>()).
                 setComputedProperties(PropertyInfoList()
                                    << PropertyInfo<maemo::localPhoneNumber>().
                                       setConversion(LocalPhoneNumberConversion::instance())).
                 setConversion(convertToLatin ? static_cast<Conversion*>(LatinPhoneNumberConversion::instance())
                                              : static_cast<Conversion*>(IdentityConversion<QVariant::String>::instance())));

        addField(QTrackerContactDetailField(QContactPhoneNumber::FieldSubTypes).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasPhoneNumber>().setHasDetailUri()).
                 setDefaultValue(QStringList(QContactPhoneNumber::SubTypeVoice)).
                 setSubTypeClasses(SubTypeClasses).
                 setDataType(QVariant::StringList).
                 setPermitsCustomValues());
    }

    static const ClassInfoList SubTypeClasses;
};

class QTrackerContactPresence : public QTrackerContactDetail
{
public:
    QTrackerContactPresence(bool readOnly)
        : QTrackerContactDetail(QContactPresence::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactPresence::FieldNickname).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imNickname>()));

        addField(QTrackerContactDetailField(QContactPresence::FieldCustomMessage).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imStatusMessage>()));

        addField(QTrackerContactDetailField(QContactPresence::FieldTimestamp).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::presenceLastModified>()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        addField(QTrackerContactDetailField(QContactPresence::FieldPresenceState).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().
                                  setReadOnly(readOnly).setHasDetailUri()
                               << PropertyInfo<nco::imPresence>()).
                 setDefaultValue(QContactPresence::PresenceUnknown).
                 setAllowableInstances(PresenceStateValues).
                 setDataType(QVariant::Int));

        addField(QTrackerContactDetailField(QContactPresence__FieldAuthStatusFrom).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAddressAuthStatusFrom>()).
                 setAllowableInstances(QTrackerContactPresence::PresenceAuthStatusValues));

        addField(QTrackerContactDetailField(QContactPresence__FieldAuthStatusTo).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasIMAddress>().setReadOnly(readOnly)
                               << PropertyInfo<nco::imAddressAuthStatusTo>()).
                 setAllowableInstances(QTrackerContactPresence::PresenceAuthStatusValues));

        setDetailUriScheme(QTrackerContactSubject::Presence);

        // TODO: generate QContactPresence::FieldPresenceStateText
        // TODO: generate QContactPresence::FieldPresenceStateImageUrl
    }

    static const InstanceInfoList PresenceStateValues;
    static const InstanceInfoList PresenceAuthStatusValues;
};

class QTrackerContactRelevance : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactRelevance()
        : QTrackerUniqueContactDetail(QContactRelevance::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactRelevance::FieldRelevance).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::relevance>().setReadOnly()).
                 setDataType(QVariant::Double));
    }
};

class QTrackerContactTag : public QTrackerContactDetail
{
public:
    QTrackerContactTag()
        : QTrackerContactDetail(QContactTag::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactTag::FieldTag).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nao::hasTag>().setDefinesOwnership()
                               << PropertyInfo<nao::prefLabel>().
                                  setCaseSensitivity(Qt::CaseInsensitive).
                                  setForeignKey()));

        // FIXME: Disabled right now for efficiency.
        // No idea how to efficiently clear the tag resource for update.
        /*
        addField(QTrackerContactDetailField(QContactTag__FieldDescription).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nao::hasTag>()
                               << PropertyInfo<nao::description>()));
        */

        setHasContext(false);
    }
};

class QTrackerContactTimestamp : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactTimestamp()
        : QTrackerUniqueContactDetail(QContactTimestamp::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactTimestamp::FieldCreationTimestamp).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nie::contentCreated>().setReadOnly()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        addField(QTrackerContactDetailField(QContactTimestamp::FieldModificationTimestamp).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nie::contentLastModified>().setReadOnly()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));

        addField(QTrackerContactDetailField(QContactTimestamp__FieldAccessedTimestamp).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nie::contentAccessed>()).
                 setDataType(QVariant::DateTime).
                 setSparqlTransform(DateTimeTransform::instance()).
                 setConversion(DateTimeOffsetConversion::instance()));
    }
};

class QTrackerContactSyncTarget : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactSyncTarget()
        : QTrackerUniqueContactDetail(QContactSyncTarget::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactSyncTarget::FieldSyncTarget).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nie::generator>()).
                 setSynthesized());
    }
};

class QTrackerContactUrl : public QTrackerContactDetail
{
public:
    QTrackerContactUrl()
        : QTrackerContactDetail(QContactUrl::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactUrl::FieldUrl).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::url>()));

        addField(QTrackerContactDetailField(QContactUrl::FieldSubType).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::url>()).
                 setDefaultValue(QContactUrl::SubTypeFavourite).
                 setSubTypeProperties(SubTypeProperties));
    }

    static const PropertyInfoList SubTypeProperties;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerPersonContactGender : public QTrackerUniqueContactDetail
{
public:
    QTrackerPersonContactGender()
        : QTrackerUniqueContactDetail(QContactGender::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactGender::FieldGender).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::gender>()).
                 setDefaultValue(QContactGender::GenderUnspecified).
                 setAllowableInstances(GenderValues));
    }

    static const InstanceInfoList GenderValues;
};

class QTrackerPersonContactHobby : public QTrackerUniqueContactDetail
{
public:
    QTrackerPersonContactHobby()
        : QTrackerUniqueContactDetail(QContactHobby::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactHobby::FieldHobby).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::hobby>()));
    }
};

class QTrackerPersonContactName : public QTrackerUniqueContactDetail
{
public:
    QTrackerPersonContactName()
        : QTrackerUniqueContactDetail(QContactName::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactName::FieldPrefix).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::nameHonorificPrefix>()));
        addField(QTrackerContactDetailField(QContactName::FieldFirstName).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::nameGiven>()));
        addField(QTrackerContactDetailField(QContactName::FieldMiddleName).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::nameAdditional>()));
        addField(QTrackerContactDetailField(QContactName::FieldLastName).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::nameFamily>()));
        addField(QTrackerContactDetailField(QContactName::FieldSuffix).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::nameHonorificSuffix>()));
        addField(QTrackerContactDetailField(QContactName::FieldCustomLabel).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::fullname>()));
    }
};

class QTrackerPersonContactNickname : public QTrackerUniqueContactDetail
{
public:
    QTrackerPersonContactNickname()
        : QTrackerUniqueContactDetail(QContactNickname::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactNickname::DefinitionName).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::nickname>()).
                 setOriginal());
    }
};

class QTrackerPersonContactOrganization : public QTrackerContactDetail
{
public:
    QTrackerPersonContactOrganization()
        : QTrackerContactDetail(QContactOrganization::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactOrganization::FieldDepartment).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>().setHasDetailUri()
                               << PropertyInfo<nco::department>()));

        addField(QTrackerContactDetailField(QContactOrganization::FieldTitle).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::title>()));

        addField(QTrackerContactDetailField(QContactOrganization::FieldRole).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::role>()));

        addField(QTrackerContactDetailField(QContactOrganization::FieldLocation).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::org>()
                               << PropertyInfo<nco::hasPostalAddress>()
                               << PropertyInfo<nco::locality>()));

        addField(QTrackerContactDetailField(QContactOrganization::FieldLogoUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::org>()
                               << PropertyInfo<nco::logo>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactOrganization::FieldName).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::org>()
                               << PropertyInfo<nco::fullname>()));

        addField(QTrackerContactDetailField(QContactOrganization::FieldAssistantName).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<nco::hasAffiliation>()
                               << PropertyInfo<nco::org>()).
                 setPermitsCustomValues().
                 setWithoutMapping());
        setHasContext(false);
    }
};

class QTrackerPersonContactRingtone : public QTrackerUniqueContactDetail
{
public:
    QTrackerPersonContactRingtone()
        : QTrackerUniqueContactDetail(QContactRingtone::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactRingtone::FieldAudioRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::contactAudioRingtone>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactRingtone::FieldVideoRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::contactVideoRingtone>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactRingtone::FieldVibrationRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::contactVibrationRingtone>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactGroupNickname : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactGroupNickname()
        : QTrackerUniqueContactDetail(QContactNickname::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactNickname::DefinitionName).
                 setPropertyChain(PropertyInfoList() << PropertyInfo<nco::contactGroupName>()));
    }
};

class QTrackerContactGroupRingtone : public QTrackerUniqueContactDetail
{
public:
    QTrackerContactGroupRingtone()
        : QTrackerUniqueContactDetail(QContactRingtone::DefinitionName)
    {
        addField(QTrackerContactDetailField(QContactRingtone::FieldAudioRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::contactGroupAudioRingtone>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactRingtone::FieldVideoRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                               << PropertyInfo<maemo::contactGroupVideoRingtone>().setDefinesOwnership()
                               << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));

        addField(QTrackerContactDetailField(QContactRingtone::FieldVibrationRingtoneUrl).
                 setPropertyChain(PropertyInfoList()
                              << PropertyInfo<maemo::contactGroupVibrationRingtone>().setDefinesOwnership()
                              << PropertyInfo<nie::url>().setForeignKey()).
                 setConversion(UriAsForeignKeyConversion::instance()).
                 setDataType(QVariant::Url));
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

const ClassInfoList QTrackerContactAddress::SubTypeClasses = ClassInfoList()
        << ClassInfo<nco::DomesticDeliveryAddress>(QContactAddress::SubTypeDomestic)
        << ClassInfo<nco::InternationalDeliveryAddress>(QContactAddress::SubTypeInternational)
        << ClassInfo<nco::ParcelDeliveryAddress>(QContactAddress::SubTypeParcel)
        << ClassInfo<maemo::PostalAddress>(QContactAddress::SubTypePostal);

const QVariantList QTrackerContactAnniversary::SubTypeValues = QVariantList()
        << QContactAnniversary::SubTypeWedding
        << QContactAnniversary::SubTypeEngagement
        << QContactAnniversary::SubTypeHouse
        << QContactAnniversary::SubTypeEmployment
        << QContactAnniversary::SubTypeMemorial;

const InstanceInfoList QTrackerPersonContactGender::GenderValues = InstanceInfoList()
        << InstanceInfo<nco::gender_female>(QContactGender::GenderFemale)
        << InstanceInfo<nco::gender_male>(QContactGender::GenderMale)
        << InstanceInfo<nco::gender_other>(QContactGender::GenderUnspecified);

const InstanceInfoList QTrackerContactOnlineAccount::CapabilityValues = InstanceInfoList()
        << InstanceInfo<nco::im_capability_text_chat>(QContactOnlineAccount__CapabilityTextChat)
        << InstanceInfo<nco::im_capability_media_calls>(QContactOnlineAccount__CapabilityMediaCalls)
        << InstanceInfo<nco::im_capability_audio_calls>(QContactOnlineAccount__CapabilityAudioCalls)
        << InstanceInfo<nco::im_capability_video_calls>(QContactOnlineAccount__CapabilityVideoCalls)
        << InstanceInfo<nco::im_capability_upgrading_calls>(QContactOnlineAccount__CapabilityUpgradingCalls)
        << InstanceInfo<nco::im_capability_file_transfers>(QContactOnlineAccount__CapabilityFileTransfers)
        << InstanceInfo<nco::im_capability_stream_tubes>(QContactOnlineAccount__CapabilityStreamTubes)
        << InstanceInfo<nco::im_capability_dbus_tubes>(QContactOnlineAccount__CapabilityDBusTubes);

const ClassInfoList QTrackerContactPhoneNumber::SubTypeClasses = ClassInfoList()
        << ClassInfo<nco::BbsNumber>(QContactPhoneNumber::SubTypeBulletinBoardSystem)
        << ClassInfo<nco::CarPhoneNumber>(QContactPhoneNumber::SubTypeCar)
        << ClassInfo<nco::FaxNumber>(QContactPhoneNumber::SubTypeFax)
        << ClassInfo<nco::MessagingNumber>(QContactPhoneNumber::SubTypeMessagingCapable)
        << ClassInfo<nco::CellPhoneNumber>(QContactPhoneNumber::SubTypeMobile)
        << ClassInfo<nco::ModemNumber>(QContactPhoneNumber::SubTypeModem)
        << ClassInfo<nco::PagerNumber>(QContactPhoneNumber::SubTypePager)
        << ClassInfo<nco::VideoTelephoneNumber>(QContactPhoneNumber::SubTypeVideo)
        << ClassInfo<nco::VoicePhoneNumber>(QContactPhoneNumber::SubTypeVoice);

const InstanceInfoList QTrackerContactPresence::PresenceStateValues = InstanceInfoList()
        << InstanceInfo<nco::presence_status_unknown>(QContactPresence::PresenceUnknown)
        << InstanceInfo<nco::presence_status_available>(QContactPresence::PresenceAvailable)
        << InstanceInfo<nco::presence_status_hidden>(QContactPresence::PresenceHidden)
        << InstanceInfo<nco::presence_status_busy>(QContactPresence::PresenceBusy)
        << InstanceInfo<nco::presence_status_away>(QContactPresence::PresenceAway)
        << InstanceInfo<nco::presence_status_extended_away>(QContactPresence::PresenceExtendedAway)
        << InstanceInfo<nco::presence_status_offline>(QContactPresence::PresenceOffline);

const InstanceInfoList QTrackerContactPresence::PresenceAuthStatusValues = InstanceInfoList()
        << InstanceInfo<nco::predefined_auth_status_no>(QContactPresence__AuthStatusNo)
        << InstanceInfo<nco::predefined_auth_status_requested>(QContactPresence__AuthStatusRequested)
        << InstanceInfo<nco::predefined_auth_status_yes>(QContactPresence__AuthStatusYes);

const QVariantList QTrackerContactGlobalPresence::PresenceStateValues =
        instanceListValues(QTrackerContactPresence::PresenceStateValues);

const PropertyInfoList QTrackerContactUrl::SubTypeProperties = PropertyInfoList()
        << PropertyInfo<nco::websiteUrl>(QContactUrl::SubTypeHomePage)
        << PropertyInfo<nco::blogUrl>(QContactUrl::SubTypeBlog);

///////////////////////////////////////////////////////////////////////////////////////////////////

template<class T> static bool isUniqueDetail() { return false; }
template<> bool isUniqueDetail<QContactFavorite>() { return true; }

template<class T> static void
cleanupDetailDefinition(const QString &contactType, QContactDetailDefinitionMap &definitions)
{
    QContactDetailDefinition detail = definitions.value(T::DefinitionName);

    if (detail.isEmpty()) {
        qctWarn(QString::fromLatin1("%1 detail not found in default engine's %2 schema").
                arg(T::DefinitionName, contactType));
        return;
    }

    detail.removeField(T::FieldContext);
    detail.setUnique(isUniqueDetail<T>());

    definitions.insert(T::DefinitionName, detail);
}

static QContactDetailDefinitionMap
makeBaseDefinitions(const QString &contactType)
{
    QContactDetailDefinitionMap definitions =
            QContactManagerEngine::schemaDefinitions(QTrackerContactDetailSchema::Version).
            value(contactType);

    // cleanup Anniversary detail
    cleanupDetailDefinition<QContactAnniversary>(contactType, definitions);
    cleanupDetailDefinition<QContactFamily>(contactType, definitions);
    cleanupDetailDefinition<QContactFavorite>(contactType, definitions);

    // drop details that don't make any sense for groups
    if (QContactType::TypeGroup == contactType) {
        definitions.remove(QContactBirthday::DefinitionName);
        definitions.remove(QContactFamily::DefinitionName);
        definitions.remove(QContactGender::DefinitionName);
        definitions.remove(QContactHobby::DefinitionName);
        definitions.remove(QContactName::DefinitionName);
        definitions.remove(QContactOrganization::DefinitionName);
    }

    // return the result
    return definitions;
}

static bool
checkContextSupported(const QStringList &contactClassIris)
{
    // FIXME: Right now we only support contexts via nco:hasAffiliation.
    // This function will change or disappear if we ever introduce generic context support.
    // It would use a specialized NCO property, or just a nao:Property.
    return contactClassIris.contains(nco::PersonContact::iri());
}

QTrackerContactDetailSchemaData::QTrackerContactDetailSchemaData(const QString &contactType,
                                                                 const QStringList &contactClassIris,
                                                                 const QStringList &ontologyPrefixes,
                                                                 AvatarTypes avatarTypes)
    : m_baseDefinitions(makeBaseDefinitions(contactType))
    , m_contactClassIris(contactClassIris)
    , m_ontologyPrefixes(ontologyPrefixes)
    , m_contactType(contactType)
    , m_avatarTypes(avatarTypes)
    , m_writeBackPresence(false) // presence should not be writable by default
    , m_contextSupported(checkContextSupported(contactClassIris))
    , m_convertNumbersToLatin(true)
{
    define(QTrackerContactAvatar(m_avatarTypes));
    define(QTrackerContactAddress());
    define(QTrackerContactAnniversary());
    define(QTrackerContactBirthday());
    define(QTrackerContactEmailAddress());
    define(QTrackerContactGeoLocation());
    define(QTrackerContactGlobalPresence());
    define(QTrackerContactGuid());
    define(QTrackerContactNote());
    define(QTrackerContactOnlineAccount(not m_writeBackPresence));
    define(QTrackerContactPhoneNumber(m_convertNumbersToLatin));
    define(QTrackerContactPresence(not m_writeBackPresence));
    define(QTrackerContactRelevance());
    define(QTrackerContactTag());
    define(QTrackerContactTimestamp());
    define(QTrackerContactSyncTarget());
    define(QTrackerContactUrl());

    if (m_avatarTypes & QTrackerContactDetailSchema::PersonalAvatar) {
        define(QTrackerContactPersonalAvatar());
    }
    if (m_avatarTypes & QTrackerContactDetailSchema::OnlineAvatar) {
        define(QTrackerContactOnlineAvatar(not m_writeBackPresence));
    }
    if (m_avatarTypes & QTrackerContactDetailSchema::SocialAvatar) {
        define(QTrackerContactSocialAvatar(not m_writeBackPresence));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
QTrackerContactDetailSchemaData::isOriginalIri(const QString &iri) const
{
    if (nco::Contact::iri() == iri) {
        return false;
    }

    return m_contactClassIris.contains(iri);
}

void
QTrackerContactDetailSchemaData::define(QTrackerContactDetail detail)
{
    // track which fields need RDF domains that originally belong to this schema.
    foreach(QTrackerContactDetailField field, detail.fields()) {
        if (field.isOriginal()) {
            continue; // nothing to check
        }

        if (field.hasPropertyChain()) {
            const QString &domainIri = field.propertyChain().first().domainIri();
            field.setOriginal(isOriginalIri(domainIri));
        } else if (field.hasSubTypeProperties()) {
            const QString &domainIri = field.subTypeProperties().first().domainIri();
            field.setOriginal(isOriginalIri(domainIri));
        }
    }

    // finish detail attributes
    detail.setHasContext(detail.hasContext() && m_contextSupported);

    // finally store the finished detail
    m_details.insert(detail.name(), detail);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetailSchema::QTrackerContactDetailSchema(QTrackerContactDetailSchemaData *data)
    : d(data)
{
    // fill QContactDetail's string pool for stock details like QContactType
    foreach(const QContactDetailDefinition &definition, d->m_baseDefinitions) {
        if (not d->m_definitions.contains(definition.name())) {
            qctInternString(definition.name());

            foreach(const QString &fieldName, definition.fields().keys()) {
                qctInternString(fieldName);
            }
        }
    }
}

QTrackerContactDetailSchema::~QTrackerContactDetailSchema()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetailSchema::QTrackerContactDetailSchema(const QTrackerContactDetailSchema &other)
    : d(other.d)
{
}

QTrackerContactDetailSchema &
QTrackerContactDetailSchema::operator=(const QTrackerContactDetailSchema &other)
{
    return d = other.d, *this;
}

const QString &
QTrackerContactDetailSchema::contactType() const
{
    return d->m_contactType;
}

const QTrackerContactDetailMap &
QTrackerContactDetailSchema::details() const
{
    return d->m_details;
}

const QStringList &
QTrackerContactDetailSchema::contactClassIris() const
{
    return d->m_contactClassIris;
}

QSet<QString>
QTrackerContactDetailSchema::requiredResourceIris() const
{
    QSet<QString> requiredIris;

    foreach(const QTrackerContactDetail &detail, details()) {
        foreach(const QTrackerContactDetailField &field, detail.fields()) {
            // add IRIs for subtype classes
            foreach(const ClassInfoBase &subType, field.subTypeClasses()) {
                requiredIris += subType.iri();
            }

            // add IRIs for fixed values
            if (not field.allowableInstances().isEmpty()) {
                foreach(InstanceInfoBase instance, field.allowableInstances()) {
                    requiredIris += instance.iri();
                }

                if (field.restrictsValues() && field.hasPropertyChain()) {
                    field.propertyChain().last().rangeIri();
                }
            }
        }
    }

    return requiredIris;
}

const QTrackerContactDetail *
QTrackerContactDetailSchema::detail(const QString &name) const
{
    const QTrackerContactDetailMap::ConstIterator i = details().find(name);

    if (i != details().constEnd()) {
        return &i.value();
    }

    return 0;
}

QContactDetailDefinition
QTrackerContactDetailSchema::describe(const QString &name) const
{
    const QTrackerContactDetail *const detail = this->detail(name);

    if (0 != detail && not detail->isInternal()) {
        return detail->describe();
    }

    return d->m_baseDefinitions.value(name);
}

const QContactDetailDefinitionMap &
QTrackerContactDetailSchema::detailDefinitions() const
{
    if (d->m_definitions.isEmpty()) {
        foreach(const QTrackerContactDetail &detail, details()) {
            if (not detail.isInternal()) {
                d->m_definitions.insert(detail.name(), detail.describe());
            }
        }

        foreach(const QContactDetailDefinition &detail, d->m_baseDefinitions) {
            if (not d->m_definitions.contains(detail.name())) {
                d->m_definitions.insert(detail.name(), detail);
            }
        }
    }

    return d->m_definitions;
}

QSet<QVariant::Type>
QTrackerContactDetailSchema::supportedDataTypes() const
{
    if (d->m_dataTypes.isEmpty()) {
        QSet<QVariant::Type> dataTypeSet;

        foreach(const QTrackerContactDetail &d, details()) {
            foreach(const QTrackerContactDetailField &f, d.fields()) {
                dataTypeSet.insert(f.dataType());
            }
        }

        d->m_dataTypes = dataTypeSet;
    }

    return d->m_dataTypes;
}

void
QTrackerContactDetailSchema::setWriteBackPresence(bool setting)
{
    if (setting != d->m_writeBackPresence) {
        d->m_writeBackPresence = setting;
        d->m_definitions.clear();
    }
}

bool
QTrackerContactDetailSchema::writeBackPresence() const
{
    return d->m_writeBackPresence;
}

void
QTrackerContactDetailSchema::setConvertNumbersToLatin(bool setting)
{
    if (setting != d->m_convertNumbersToLatin) {
        d->m_convertNumbersToLatin = setting;
        d->m_definitions.clear();
    }
}

bool
QTrackerContactDetailSchema::convertNumbersToLatin() const
{
    return d->m_convertNumbersToLatin;
}

QTrackerContactDetailSchema::AvatarTypes
QTrackerContactDetailSchema::avatarTypes() const
{
    return d->m_avatarTypes;
}

const QSet<QString> &
QTrackerContactDetailSchema::syntheticDetails()
{
    static QSet<QString> definitionNames = QSet<QString>()
            << QContactAvatar::DefinitionName
            << QContactDisplayLabel::DefinitionName
            << QContactGlobalPresence::DefinitionName
            << QContactThumbnail::DefinitionName
            << QContactType::DefinitionName;

    return definitionNames;
}

bool
QTrackerContactDetailSchema::isSyntheticDetail(const QString &name)
{
    return syntheticDetails().contains(name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerPersonContactDetailSchema::QTrackerPersonContactDetailSchema(AvatarTypes avatarTypes)
    : QTrackerContactDetailSchema(new QTrackerPersonContactDetailSchemaData(avatarTypes))
{
}

QTrackerPersonContactDetailSchemaData::QTrackerPersonContactDetailSchemaData(AvatarTypes avatarTypes)
    : QTrackerContactDetailSchemaData(QContactType::TypeContact,
                                      QStringList() << nco::PersonContact::iri(),
                                      QStringList() << Ontologies::nco::iri(),
                                      avatarTypes)
{
    define(QTrackerPersonContactGender());
    define(QTrackerPersonContactHobby());
    define(QTrackerPersonContactName());
    define(QTrackerPersonContactNickname());
    define(QTrackerPersonContactOrganization());
    define(QTrackerPersonContactRingtone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactGroupDetailSchema::QTrackerContactGroupDetailSchema(AvatarTypes avatarTypes)
    : QTrackerContactDetailSchema(new QTrackerContactGroupDetailSchemaData(avatarTypes))
{
}

QTrackerContactGroupDetailSchemaData::QTrackerContactGroupDetailSchemaData(AvatarTypes avatarTypes)
    : QTrackerContactDetailSchemaData(QContactType::TypeGroup,
                                      QStringList() << nco::ContactGroup::iri()
                                                    << nco::Contact::iri(),
                                      QStringList() << Ontologies::nco::iri(),
                                      avatarTypes)
{
    define(QTrackerContactGroupNickname());
    define(QTrackerContactGroupRingtone());
}
