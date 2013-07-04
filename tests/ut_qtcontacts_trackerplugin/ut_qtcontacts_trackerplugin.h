/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2009-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_H
#define UT_QTCONTACTS_TRACKERPLUGIN_H

#include "ut_qtcontacts_trackerplugin_common.h"


// FIXME: This type name doesn't fit anything!!!
struct editStruct;
typedef class QMap<QString, QtMobility::QContactDetailFieldDefinition> QContactDetailFieldDefinitionMap;
typedef QPair<QContactDetail, QString> ContactDetailSample;

/**
 * QtContacts Tracker plugin unittests
 */
class ut_qtcontacts_trackerplugin : public ut_qtcontacts_trackerplugin_common
{
    Q_OBJECT

public:
    ut_qtcontacts_trackerplugin(QObject *parent = 0);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void testFetchAll();
    void testFetchById_data();
    void testFetchById();

    void testTorture_data();
    void testTorture();

    void testSaveNothing();
    void testSavePhoneNumber_data();
    void testSavePhoneNumber();
    void testSimilarPhoneNumber();
    void testEasternArabicPhoneNumber_data();
    void testEasternArabicPhoneNumber();
    void testSamePhoneNumber();
    void testPhoneNumberContext();
    void testWritingOnlyWorkMobile();
    void testContacts();
    void testContact();
    void testAvatar();
    void testOnlineAvatar_data();
    void testOnlineAvatar();
    void testAvatarTypes_data();
    void testAvatarTypes();
    void testDateDetail_data();
    void testDateDetail();

    void testSaveEmailAddress();
    void testSaveFullname();
    void testSaveName();
    void testSaveNameUnique();
    void testSaveNonLatin1Name_data();
    void testSaveNonLatin1Name();
    void testSaveAddress();
    void testExtendedAddress();
    void testSaveOrganization();
    void testSaveCustomValues();

    void testRemoveContact();
    void testRemoveSelfContact();
    void testSaveContacts();
    void testRemoveContacts();
    void testUrl_data();
    void testUrl();
    void testMultipleUrls();
    void testOrganization();
    void testUniqueDetails_data();
    void testUniqueDetails();
    void testCustomDetails();

    void testRemoveSubType_data();
    void testRemoveSubType();
    void testTags_data();
    void testTags();
    void testFavouriteTag();

//    void testGroups();
//    void testGroup();
//    void testSaveGroup();
//    void testRemoveGroup();
//    void testDetailDefinitions();
//    void testDetailDefinition();
//    void testSaveDetailDefinition();
//    void testRemoveDetailDefinition();
    void testSyncContactManagerContactsAddedSince();
    void testSyncTrackerEngineContactsIdsAddedSince();
    void testSyncContactManagerContactIdsAddedSince();
    void testContactsAddedSince();
    void testContactsModifiedSince();
    void testContactsRemovedSince();
//    void testGroupsAddedSince();
//    void testGroupsModifiedSince();
//    void testGroupsRemovedSince();
    void testMergeOnlineContacts();
    void testMergingContacts();
    void testMergingGarbage();
    void testMergeSyncTarget_data();
    void testMergeSyncTarget();
    void testAsyncReadContacts();

    void testSortContacts();
    void testSparqlSorting_data();
    void testSparqlSorting();

    void testLimit_data();
    void testLimit();

    void testFilterContacts();
    void testFilterContactsEndsWithAndPhoneNumber();
    void testNormalizePhoneNumber_data();
    void testNormalizePhoneNumber();
    void testFilterDTMFNumber();

// NB#208065
    void testFilterContactsMatchPhoneNumberWithShortNumber_data();
    void testFilterContactsMatchPhoneNumberWithShortNumber();
// end NB#208065
    void testFilterTwoNameFields();
    void testFilterContactsWithBirthday_data();
    void testFilterContactsWithBirthday();
    void testLocalIdFilterManyIds_data();
    void testLocalIdFilterManyIds();
    void testContactFilter_data();
    void testContactFilter();
    void testFilterOnContainsSubTypesByClass();
    void testFilterOnSubTypesByProperty();
    void testFilterOnContainsSubTypesByNaoProperty();
    void testFilterOnCustomDetail_data();
    void testFilterOnCustomDetail();
    void testFilterOnContactType();
    void testFilterOnQContactOnlineAccount();
    void testFilterOnDetailFieldWildcard_data();
    void testFilterOnDetailFieldWildcard();
    void testFilterOnDetailExists_data();
    void testFilterOnDetailExists();
    void testFilterOnContext_data();
    void testFilterOnContext();

    void testFilterOnDetailFieldValueWithSingleSpaceOrEmptyString_data();
    void testFilterOnDetailFieldValueWithSingleSpaceOrEmptyString();

    void testFilterInvalid();

    void testIMContactsAndMetacontactMasterPresence();
    void testIMContactsFiltering();
    void testContactsWithoutMeContact();
    void testDefinitionNames();
    void testMeContact();

    void testDisplayLabelFetch_data();
    void testDisplayLabelFetch();
    void testDisplayLabelSaved_data();
    void testDisplayLabelSaved();
    void testDisplayLabelSynthesized_data();
    void testDisplayLabelSynthesized();
    void testDisplayLabelSynthesizedWithNameOrders_data();
    void testDisplayLabelSynthesizedWithNameOrders();

    void testSyncTarget_data();
    void testSyncTarget();

// NB#161788 and its dependant bugs
    void testEditCombinations_email();
    void testEditCombinations_url();
    void testEditCombinations_address();
    void testEditCombinations_phone();
// end NB#161788

// NB#173388
    void testSaveThumbnail_data();
    void testSaveThumbnail();
// end NB#173388

    void testVCardsAndSync_data();
    void testVCardsAndSync();
    void testCreateUuid();
    void testPreserveUID();

    void testFuzzing_data();
    void testFuzzing();

    void testUnionFilterUniqueness();
    void testPartialSave_data();
    void testPartialSave();
    void testPartialSaveAndWeakSyncTargets_data();
    void testPartialSaveAndWeakSyncTargets();
    void testPartialSaveAndThumbnails_data();
    void testPartialSaveAndThumbnails();
    void testPartialSaveFuzz_data();
    void testPartialSaveFuzz();

    void testFetchingNonQctResourcesAsReadOnlyDetails_data();
    void testFetchingNonQctResourcesAsReadOnlyDetails();
    void testNotSavingReadOnlyDetails();

    void testDetailsFromServicesAreLinked_data();
    void testDetailsFromServicesAreLinked();
    void testDetailsFromServicesAreLinkedWithMergedContacts();
    void testDetailsFromServicesAreLinkedWithMergedContacts_data();

    void testFavorite();
    void testAnniversary();
    void testOnlineAccount();

    void testMergeContactsWithGroups();

    void testDeleteFromStateChangedHandler_data();
    void testDeleteFromStateChangedHandler();

    void testDetailUriEncoding();

private:
    // FIXME: Most of the following methods are editStruct methods!!!
    void setName(editStruct &es, QString first = QString(), QString last = QString());
    void setEmail(editStruct &es, QString email, QString context = QString());
    void setUrl(editStruct &es, QString url, QString context = QString(), QString subType = QString());
    void setPhone(editStruct &es, QString phone, QString context = QString(), QString subType = QString());
    void setAddress(editStruct &es, QString street, QString postcode = QString(), QString pobox = QString(), QString locality = QString(), QString region = QString(), QString country = QString(), QString context = QString(), QString subType = QString());
    void saveEditsToContact(editStruct &es, QContact &c);

    void verifyEdits(QContact &verify, editStruct &es);
    void runEditList(QList<editStruct> &editList);

    void syncContactsAddedSinceHelper(QDateTime& start, QList<QContactLocalId>& addedIds);

    void updateIMContactStatus(const QString &contactIri, const QString &imStatus);
    QContact contact(QContactLocalId uid, const QStringList &detailsToLoad = QStringList());
    QContact contact(const QString &iri, const QStringList &detailsToLoad = QStringList());
    QList<QContact> contacts(const QList<QContactLocalId> &uids, const QStringList &detailsToLoad = QStringList());

    void fuzzContact(QContact &result, const QStringList &skipDetailDefinitions = QStringList()) const;
    void fuzzDetailType(QList<QContactDetail> &result, const QString &definitionName) const;
    void mergeContacts(QContact &contactTarget, const QList<QContactLocalId> &sourceContactIds);
    void unmergeContact(QContact &contact, const QList<QContactOnlineAccount> &unmergeCriteria, /*out*/ QList<QContactLocalId> &newUnmergedContactIds);

    /// creates fuzzed values for the given @p field and for each value a contact of @p contactType with only that detail and only that field set,
    /// then stores and adds these contacts to @p contacts
    void makeFuzzedSingleDetailFieldContacts(QContactList &contacts,
                                             const QString &detailName, const QString &fieldName, const QContactDetailFieldDefinition &field,
                                             const QString &contactType);
    /// sets the field definitions of @p detailDefinition to @p fields,
    /// without any for the fields Context and SubType/SubTypes
    void fieldsWithoutContextAndSubType(QContactDetailFieldDefinitionMap &fields,
                                        const QString &contactType, const QString &definitionName);

    const QList<ContactDetailSample> displayLabelDetailSamples(bool preferNickname, bool preferLastname);

private:
    QList<QContactLocalId> addedContacts;
};

class EvilRequestKiller : QObject
{
    Q_OBJECT

public:
    EvilRequestKiller(QContactAbstractRequest *request,
                      Qt::ConnectionType connectionType,
                      QContactAbstractRequest::State deletionState,
                      QObject *parent = 0);

private slots:
    void onStateChanged(QContactAbstractRequest::State state);

private:
    QContactAbstractRequest *const m_request;
    QContactAbstractRequest::State const m_deletionState;
};

#endif
