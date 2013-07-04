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

#include "ut_qtcontacts_trackerplugin_groups.h"

#include <qtcontacts.h>


typedef QSet<QString> QStringSet;

Q_DECLARE_METATYPE(QContactRelationship::Role)

static QContactDetailFilter
createNicknameFilter(const QContact &groupContact)
{
    QContactDetailFilter nicknameFilter;
    nicknameFilter.setDetailDefinitionName(QContactNickname::DefinitionName, QContactNickname::FieldNickname);
    nicknameFilter.setMatchFlags(QContactFilter::MatchExactly);
    nicknameFilter.setValue(groupContact.detail<QContactNickname>().nickname());

    return nicknameFilter;
}

/// returns a sortOrder by ascending QContactNickname::FieldNickname
static QContactSortOrder
nickNameAscendingSortOrder()
{
    QContactSortOrder result;
    result.setDirection(Qt::AscendingOrder);
    result.setDetailDefinitionName(QContactNickname::DefinitionName,
                                   QContactNickname::FieldNickname);
    return result;
}

ut_qtcontacts_trackerplugin_groups::ut_qtcontacts_trackerplugin_groups(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)
{
}

QMap<QString, QString>
ut_qtcontacts_trackerplugin_groups::makeEngineParams() const
{
    QMap<QString, QString> params;
    return params;
}

void ut_qtcontacts_trackerplugin_groups::setupTestGroupContact(QContact &groupContact,
                                                               const QString &id) const
{
    setTestNicknameToContact(groupContact, id);
    CHECK_CURRENT_TEST_FAILED;

    QContactAvatar avatarUrlDetail;
    avatarUrlDetail.setImageUrl(QUrl(QLatin1String("smile.png")));
    QVERIFY(groupContact.saveDetail(&avatarUrlDetail));

    QContactRingtone ringtoneDetail;
    ringtoneDetail.setAudioRingtoneUrl(QUrl(QLatin1String("trampampam")));
    QVERIFY(groupContact.saveDetail(&ringtoneDetail));

    groupContact.setType(QContactType::TypeGroup);
    QVERIFY(groupContact.type() == QContactType::TypeGroup);
}

void ut_qtcontacts_trackerplugin_groups::setupTestHasMemberRelationship(QContactRelationship &relationship,
                                                                        const QContact &groupContact,
                                                                        const QContact &contact) const
{
    relationship.setRelationshipType(QContactRelationship::HasMember);
    relationship.setFirst(groupContact.id());
    relationship.setSecond(contact.id());

    if (m_verbose) {
        qDebug() << "Setup relationship HasMember with group " << groupContact.localId()
                 << " and contact " << contact.localId();
    }
}

void
ut_qtcontacts_trackerplugin_groups::setupContactsForFetchWithRelationshipFilterTests(
        QContact groupContacts[],
        int contactsCountInGroup[],
        int firstContactListOffset[],
        int contactsGroupCount,
        QContact contacts[],
        int groupsCountForContact[],
        int contactsCount,
        int contactsSetCount,
        const QString &memberType)
{
    // setup contacts
    // the number of contacts is x times the number of groups, divided in sets
    // first group contains all, second group contains all but first set, etc...
    for (int g = 0; g<contactsGroupCount; ++g) {
        SETUP_TEST_GROUPCONTACT(groupContacts[g]);
        CHECK_CURRENT_TEST_FAILED;

        firstContactListOffset[g] = g * contactsSetCount;
        contactsCountInGroup[g] = contactsCount - firstContactListOffset[g];
    }

    for (int c=0; c<contactsCount; ++c) {
        contacts[c].setType(memberType);
        SET_TESTNICKNAME_TO_CONTACT(contacts[c]);
        CHECK_CURRENT_TEST_FAILED;

        groupsCountForContact[c] = (c / contactsSetCount) + 1;
    }

    // save contacts
    for (int g=0; g<contactsGroupCount; ++g) {
        saveContact(groupContacts[g]);
        CHECK_CURRENT_TEST_FAILED;
    }
    for (int c=0; c<contactsCount; ++c) {
        saveContact(contacts[c]);
        CHECK_CURRENT_TEST_FAILED;
    }

    // create a hasMember relationships and save them,
    // adding the contacts of the first also to second and third and of second also to third
    for (int g=0; g<contactsGroupCount; ++g) {
        for (int c=firstContactListOffset[g]; c<contactsCount; ++c) {
            QContactRelationship relationship;
            setupTestHasMemberRelationship(relationship, groupContacts[g], contacts[c]);
            CHECK_CURRENT_TEST_FAILED;

            saveRelationship(relationship);
            CHECK_CURRENT_TEST_FAILED;
        }
    }
}

void ut_qtcontacts_trackerplugin_groups::checkDatabaseEmpty()
{
    QContactDetailFilter nameFilter;
    nameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    nameFilter.setMatchFlags(QContactFilter::MatchStartsWith);
    nameFilter.setValue(uniqueTestId());

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList contacts = engine()->contacts(nameFilter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    // dump unexpected contacts
    foreach(const QContact &c, contacts) {
        qWarning() << "unexpected contact" << c;
    }

    // verify that there really is no test contact yet
    QVERIFY(contacts.isEmpty());
}

void ut_qtcontacts_trackerplugin_groups::testSaveGroupContact()
{
    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named group contact and save it
    QContact groupContact;
    SETUP_TEST_GROUPCONTACT(groupContact);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(groupContact);
    CHECK_CURRENT_TEST_FAILED;

    // create another named group contact and save it
    QContact groupContact2;
    SETUP_TEST_GROUPCONTACT(groupContact2);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(groupContact2);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_groups::testSaveGroupContactSyncCall()
{
    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named group contact and save it
    QContact groupContact;
    SETUP_TEST_GROUPCONTACT(groupContact);
    CHECK_CURRENT_TEST_FAILED;

    QContactManager::Error error = QContactManager::UnspecifiedError;
    engine()->saveContact(&groupContact, &error);
    registerForCleanup(groupContact);
    QCOMPARE(error, QContactManager::NoError);

    // create another named group contact and save it
    QContact groupContact2;
    SETUP_TEST_GROUPCONTACT(groupContact2);
    CHECK_CURRENT_TEST_FAILED;

    error = QContactManager::UnspecifiedError;
    engine()->saveContact(&groupContact2, &error);
    registerForCleanup(groupContact2);
    QCOMPARE(error, QContactManager::NoError);
}

void ut_qtcontacts_trackerplugin_groups::testFetchSavedGroupContactId()
{
    const int groupCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create three named group contacts and save them
    QContact groupContacts[groupCount];
    for (int i=0; i<groupCount; ++i) {
        SETUP_TEST_GROUPCONTACT(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        saveContact(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;
    }

    // fetch the saved group contact id TODO: fetch all to see that not too many were inserted
    for (int i=0; i<groupCount; ++i) {
        QContactLocalId fetchedGroupContactLocalId;
        QContactDetailFilter nicknameFilter = createNicknameFilter(groupContacts[i]);
        fetchContactLocalId(nicknameFilter, fetchedGroupContactLocalId);
        CHECK_CURRENT_TEST_FAILED;

        QCOMPARE(fetchedGroupContactLocalId, groupContacts[i].localId());
    }
    // fetch the saved group contact ids
    QList<QContactSortOrder> sortOrders;
    sortOrders.append(nickNameAscendingSortOrder());

    QList<QContactLocalId> fetchedLocalIds;
    fetchContactLocalIds(TESTNICKNAME_FILTER, sortOrders, fetchedLocalIds);

    QCOMPARE(fetchedLocalIds.count(), groupCount);
    for (int i=0; i<groupCount; ++i) {
        QCOMPARE(fetchedLocalIds[i], groupContacts[i].localId());
    }
}

void ut_qtcontacts_trackerplugin_groups::testFetchSavedGroupContactIdSyncCall()
{
    const int groupCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create three named group contacts and save them
    QContact groupContacts[groupCount];
    for (int i=0; i<groupCount; ++i) {
        SETUP_TEST_GROUPCONTACT(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&groupContacts[i], &error);
        registerForCleanup(groupContacts[i]);
        QCOMPARE(error, QContactManager::NoError);
    }

    // fetch the saved group contact ids
    QList<QContactSortOrder> sortOrders;
    sortOrders.append(nickNameAscendingSortOrder());

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QList<QContactLocalId> fetchedLocalIds = engine()->contactIds(TESTNICKNAME_FILTER, sortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetchedLocalIds.count(), groupCount);
    for (int i=0; i<groupCount; ++i) {
        QCOMPARE(fetchedLocalIds[i], groupContacts[i].localId());
    }
}

void ut_qtcontacts_trackerplugin_groups::testFetchSavedGroupContact()
{
    const int groupCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named group contact and save it
    QContact groupContacts[groupCount];
    QList<QContactLocalId> localIds;

    for (int i=0; i<groupCount; ++i) {
        SETUP_TEST_GROUPCONTACT(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        saveContact(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        localIds.append(groupContacts[i].localId());
    }

    // fetch the saved group contacts and check them
    QContactLocalIdFilter localIdFilter;
    localIdFilter.setIds(localIds);

    QList<QContactSortOrder> sortOrders;
    sortOrders.append(nickNameAscendingSortOrder());


    QList<QContact> fetchedGroupContacts;
    fetchContacts(localIdFilter, sortOrders, fetchedGroupContacts);
    CHECK_CURRENT_TEST_FAILED;

    QCOMPARE(fetchedGroupContacts.count(), groupCount);

    for (int i=0; i<groupCount; ++i) {
        QCOMPARE(fetchedGroupContacts[i].localId(), groupContacts[i].localId());

        // check that the fetched group contact has the expected properties
        QCOMPARE(fetchedGroupContacts[i].type(), QLatin1String(QContactType::TypeGroup.latin1()));

        QList<QContactDetail> details;

        details = fetchedGroupContacts[i].details(QContactNickname::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value(QContactNickname::FieldNickname),
                 groupContacts[i].detail<QContactNickname>().nickname());

        details = fetchedGroupContacts[i].details(QContactAvatar::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value<QUrl>(QContactAvatar::FieldImageUrl),
                 groupContacts[i].detail<QContactAvatar>().imageUrl());

        details = fetchedGroupContacts[i].details(QContactRingtone::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value<QUrl>(QContactRingtone::FieldAudioRingtoneUrl),
                 groupContacts[i].detail<QContactRingtone>().audioRingtoneUrl());
    }
}

void ut_qtcontacts_trackerplugin_groups::testFetchSavedGroupContactSyncCall()
{
    const int groupCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named group contact and save it
    QContact groupContacts[groupCount];
    QContactLocalIdList localIds;

    for (int i = 0; i < groupCount; ++i) {
        SETUP_TEST_GROUPCONTACT(groupContacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&groupContacts[i], &error);
        registerForCleanup(groupContacts[i]);
        QCOMPARE(error, QContactManager::NoError);

        localIds.append(groupContacts[i].localId());
    }

    // fetch the saved group contacts and check them
    QContactLocalIdFilter localIdFilter;
    localIdFilter.setIds(localIds);

    QList<QContactSortOrder> sortOrders;
    sortOrders.append(nickNameAscendingSortOrder());

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QList<QContact> fetchedGroupContacts =
            engine()->contacts(localIdFilter, sortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetchedGroupContacts.count(), groupCount);

    for (int i=0; i<groupCount; ++i) {
        QCOMPARE(fetchedGroupContacts[i].localId(), groupContacts[i].localId());

        // check that the fetched group contact has the expected properties
        QCOMPARE(fetchedGroupContacts[i].type(),
                 QLatin1String(QContactType::TypeGroup));

        QList<QContactDetail> details;

        details = fetchedGroupContacts[i].details(QContactNickname::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value(QContactNickname::FieldNickname),
                 groupContacts[i].detail<QContactNickname>().nickname());

        details = fetchedGroupContacts[i].details(QContactAvatar::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value<QUrl>(QContactAvatar::FieldImageUrl),
                 groupContacts[i].detail<QContactAvatar>().imageUrl());

        details = fetchedGroupContacts[i].details(QContactRingtone::DefinitionName);
        QCOMPARE(details.count(), 1);
        QCOMPARE(details[0].value<QUrl>(QContactRingtone::FieldAudioRingtoneUrl),
                 groupContacts[i].detail<QContactRingtone>().audioRingtoneUrl());
    }
}


void ut_qtcontacts_trackerplugin_groups::testSaveHasMemberRelationship_data()
{
    QTest::addColumn<bool>("useSyncEngineCall");
    QTest::addColumn<QString>("memberType");

    const QStringList types = QStringList() << QContactType::TypeContact << QContactType::TypeGroup;

    for (int i=0; i<2 ; ++i) {
        const bool useSyncEngineCall = (i != 0);
        const QString syncNote = QLatin1String(useSyncEngineCall ? "sync" : "async");

        foreach(const QString &type, types) {
            QTest::newRow(qPrintable(type+QLatin1Char('-')+syncNote))
                << useSyncEngineCall << type;
        }
    }

}


void ut_qtcontacts_trackerplugin_groups::testSaveHasMemberRelationship()
{
    QFETCH(bool, useSyncEngineCall);
    QFETCH(QString, memberType);

    const int contactCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // setup contacts
    QContact groupContact;
    SETUP_TEST_GROUPCONTACT(groupContact);
    CHECK_CURRENT_TEST_FAILED;

    QContact contacts[contactCount];
    for (int i=0; i<contactCount; ++i) {
        contacts[i].setType(memberType);
        SET_TESTNICKNAME_TO_CONTACT(contacts[i]);
        CHECK_CURRENT_TEST_FAILED;
    }

    // save all
    if (useSyncEngineCall) {
        saveContact(groupContact);
        CHECK_CURRENT_TEST_FAILED;

        for (int i=0; i<contactCount; ++i) {
            saveContact(contacts[i]);
            CHECK_CURRENT_TEST_FAILED;
        }
    } else {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&groupContact, &error);
        registerForCleanup(groupContact);
        QCOMPARE(error, QContactManager::NoError);

        for (int i=0; i<contactCount; ++i) {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            engine()->saveContact(&contacts[i], &error);
            registerForCleanup(contacts[i]);
            QCOMPARE(error, QContactManager::NoError);
        }
    }
    // create a hasMember relationships and save them
    for (int i=0; i<contactCount; ++i) {
        QContactRelationship relationship;
        setupTestHasMemberRelationship(relationship, groupContact, contacts[i]);
        CHECK_CURRENT_TEST_FAILED;

        if (useSyncEngineCall) {
            saveRelationship(relationship);
            CHECK_CURRENT_TEST_FAILED;
        } else {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            engine()->saveRelationship(&relationship, &error);
            QCOMPARE(error, QContactManager::NoError);
        }
    }
}


/// Checks that @param relationships consists of those,
/// where first contact is @param firstContact, type is @param expectedRelationshipType
/// and all the second contacts are in @param secondContacts.
static void
compareRelationshipsWithContacts(const QList<QContactRelationship> &relationships,
                                 const QContact &firstContact, const QList<QContact> &secondContacts,
                                 const QString &expectedRelationshipType = QContactRelationship::HasMember)
{
    QCOMPARE(relationships.count(), secondContacts.count());

    // relationships are unsorted, so create lookup table
    QHash<QContactId,QContactRelationship> relationshipsBySecondId;
    foreach (const QContactRelationship &relationship, relationships) {
        relationshipsBySecondId.insert(relationship.second(), relationship);
    }

    // check for all contacts
    foreach (const QContact &secondContact, secondContacts) {
        QHash<QContactId,QContactRelationship>::Iterator it = relationshipsBySecondId.find(secondContact.id());
        QVERIFY(it != relationshipsBySecondId.constEnd());

        const QContactRelationship &relationship = *it;
        QCOMPARE(relationship.relationshipType(), expectedRelationshipType);
        QCOMPARE(relationship.first(), firstContact.id());
        QCOMPARE(relationship.second(), secondContact.id());

        // make sure there is one relationship per contact
        relationshipsBySecondId.erase(it);
    }
}

/// Removes identic items for all items in @param relationshipsToRemove from @param relationships, if existing.
/// Only removes one identic item per item in @param relationshipsToRemove.
static void
removeRelationshipsFromList(QList<QContactRelationship> &relationships, const QList<QContactRelationship> &relationshipsToRemove)
{
    foreach (const QContactRelationship &relationshipToRemove, relationshipsToRemove) {
        relationships.removeOne(relationshipToRemove);
    }
}

void
ut_qtcontacts_trackerplugin_groups::testFetchSavedHasMemberRelationship_data()
{
    QTest::addColumn<bool>("useSyncEngineCall");
    QTest::addColumn<bool>("useGroupContact");
    QTest::addColumn<QContactRelationship::Role>("relationshipContactRole");

    for (int i=0; i<2 ; ++i) {
        const bool useSyncEngineCall = (i != 0);
        const QString syncNote = QLatin1String(useSyncEngineCall ? "sync" : "async");
        QTest::newRow(qPrintable(QLatin1String("GroupContact ")+syncNote))
            << useSyncEngineCall << true  << QContactRelationship::First;
        QTest::newRow(qPrintable(QLatin1String("all with first ")+syncNote))
            << useSyncEngineCall << false << QContactRelationship::First;
        QTest::newRow(qPrintable(QLatin1String("all with second ")+syncNote))
            << useSyncEngineCall << false << QContactRelationship::Second;
        QTest::newRow(qPrintable(QLatin1String("all with either ")+syncNote))
            << useSyncEngineCall << false << QContactRelationship::Either;
    }
}

void
ut_qtcontacts_trackerplugin_groups::testFetchSavedHasMemberRelationship()
{
    QFETCH(bool, useSyncEngineCall);
    QFETCH(bool, useGroupContact);
    QFETCH(QContactRelationship::Role, relationshipContactRole);

    const int contactsCount = 3;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;
    QList<QContactRelationship> otherRelationships;
    if (not useGroupContact) {
        // note the existing relationships in the database (because we cannot filter like it's done for contacts)
        if (useSyncEngineCall) {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            otherRelationships =
                engine()->relationships(QContactRelationship::HasMember, QContactId(), relationshipContactRole,
                                        &error);
            QCOMPARE(error, QContactManager::NoError);
        } else {
            fetchRelationships(QContactRelationship::HasMember, QContactId(), relationshipContactRole, otherRelationships);
            CHECK_CURRENT_TEST_FAILED;
        }
    }

    // setup contacts
    QContact groupContact;
    SETUP_TEST_GROUPCONTACT(groupContact);
    CHECK_CURRENT_TEST_FAILED;
    QList<QContact> contacts;
    for (int i=0; i<contactsCount; ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        CHECK_CURRENT_TEST_FAILED;

        contacts << contact;
    }

    // save contacts
    if (useSyncEngineCall) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&groupContact, &error);
        registerForCleanup(groupContact);
        QCOMPARE(error, QContactManager::NoError);

        for (int i=0; i<contacts.count(); ++i) {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            engine()->saveContact(&contacts[i], &error);
            registerForCleanup(contacts[i]);
            QCOMPARE(error, QContactManager::NoError);
            CHECK_CURRENT_TEST_FAILED;
        }
    } else {
        saveContact(groupContact);
        CHECK_CURRENT_TEST_FAILED;
        for (int i=0; i<contacts.count(); ++i) {
            saveContact(contacts[i]);
            CHECK_CURRENT_TEST_FAILED;
        }
    }

    // create a hasMember relationships and save them
    QList<QContactRelationship> relationships;
    foreach (const QContact &contact, contacts) {
        QContactRelationship relationship;
        setupTestHasMemberRelationship(relationship, groupContact, contact);
        CHECK_CURRENT_TEST_FAILED;

        if (useSyncEngineCall) {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            engine()->saveRelationship(&relationship, &error);
            QCOMPARE(error, QContactManager::NoError);
        } else {
            saveRelationship(relationship);
            CHECK_CURRENT_TEST_FAILED;
        }
        relationships << relationship;
    }

    // finally fetch the saved relationships and do the check
    QList<QContactRelationship> fetchedRelationships;
    if (useSyncEngineCall) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        fetchedRelationships =
            engine()->relationships(QContactRelationship::HasMember,
                                    useGroupContact ? groupContact.id() : QContactId(),
                                    relationshipContactRole,
                                    &error);
        QCOMPARE(error, QContactManager::NoError);
    } else {
        fetchRelationships(QContactRelationship::HasMember,
                           useGroupContact ? groupContact.id() : QContactId(),
                           relationshipContactRole,
                           fetchedRelationships);
        CHECK_CURRENT_TEST_FAILED;
    }
    if (not useGroupContact) {
        removeRelationshipsFromList(fetchedRelationships, otherRelationships);
    }
    compareRelationshipsWithContacts(fetchedRelationships, groupContact, contacts);
    CHECK_CURRENT_TEST_FAILED;
}

void
ut_qtcontacts_trackerplugin_groups::testFetchContactsOfGivenGroup_data()
{
    QTest::addColumn<QContactRelationship::Role>("relatedContactRole");
    QTest::addColumn<QString>("memberType");

    const QStringList types = QStringList() << QContactType::TypeContact << QContactType::TypeGroup;
    foreach(const QString &type, types) {
        QTest::newRow(QLatin1String("first-") + type) << QContactRelationship::First << type;
        QTest::newRow(QLatin1String("either-") + type)  << QContactRelationship::Either << type;
    }
}

void
ut_qtcontacts_trackerplugin_groups::testFetchContactsOfGivenGroup()
{
    QFETCH(QContactRelationship::Role, relatedContactRole);
    QFETCH(QString, memberType);

    const int contactsGroupCount = 3;
    const int contactsSetCount = 5;
    const int contactsCount = contactsGroupCount * contactsSetCount;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // setup contacts
    // the number of contacts is x times the number of groups, divided in sets
    // first group contains all, second group contains all but first set, etc...
    QContact groupContacts[contactsGroupCount];
    int contactsCountInGroup[contactsGroupCount];
    int firstContactListOffset[contactsGroupCount];
    QContact contacts[contactsCount];
    int groupsCountForContact[contactsCount];
    setupContactsForFetchWithRelationshipFilterTests(
        groupContacts, contactsCountInGroup, firstContactListOffset, contactsGroupCount,
        contacts, groupsCountForContact, contactsCount, contactsSetCount, memberType
    );
    CHECK_CURRENT_TEST_FAILED;

    // prepare fetching
    QList<QContactSortOrder> contactsSortOrders;
    contactsSortOrders.append(nickNameAscendingSortOrder());

    // fetch all contacts for each group
    for (int g=0; g<contactsGroupCount; ++g) {
        QContactRelationshipFilter isInGroupFilter;
        isInGroupFilter.setRelationshipType(QContactRelationship::HasMember);
        isInGroupFilter.setRelatedContactRole(relatedContactRole);
        isInGroupFilter.setRelatedContactId(groupContacts[g].id());

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedContactsInGroup =
                engine()->contacts(isInGroupFilter, contactsSortOrders, NoFetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);

        QCOMPARE(fetchedContactsInGroup.count(), contactsCountInGroup[g]);

        int fc = 0;
        for (int c=firstContactListOffset[g]; c<contactsCount; ++c,++fc) {
            const QContact &contact = contacts[c];
            const QContact &fetchedContact = fetchedContactsInGroup[fc];

            QCOMPARE(fetchedContact.id(), contact.id());
            // TODO: fetching also fills implied detail values, so this comparison does not work
            // query builder test or *_common or the generic test should have a function for custom comparison
            // use also for other testFetch*WithRelationshipFilter() methods
//             QCOMPARE(fetchedContact, contact);

            // check relationships of the fetched group contact
            const QList<QContactRelationship> fetchedRelationships = fetchedContact.relationships();
            // estimate all groups this contact belongs to
            QList<QContactId> expectedGroupIds;
            for (int cg = 0; cg < groupsCountForContact[c]; ++cg) {
                expectedGroupIds << groupContacts[cg].id();
            }

            foreach (const QContactRelationship &fetchedRelationship, fetchedRelationships) {
                QCOMPARE(fetchedRelationship.relationshipType(), QLatin1String(QContactRelationship::HasMember));
                QCOMPARE(fetchedRelationship.second(), contact.id());
                QVERIFY(expectedGroupIds.contains(fetchedRelationship.first()));
                expectedGroupIds.removeOne(fetchedRelationship.first());
            }
            QVERIFY2(expectedGroupIds.isEmpty(), "Not all expected groups found in relationships.");
        }
    }
}

void
ut_qtcontacts_trackerplugin_groups::testFetchGroupsOfGivenContact_data()
{
    QTest::addColumn<QContactRelationship::Role>("relatedContactRole");
    QTest::addColumn<QString>("memberType");

    const QStringList types = QStringList() << QContactType::TypeContact << QContactType::TypeGroup;
    foreach(const QString &type, types) {
        QTest::newRow(QLatin1String("second-") + type) << QContactRelationship::Second << type;
        QTest::newRow(QLatin1String("either-") + type)  << QContactRelationship::Either << type;
    }
}

void
ut_qtcontacts_trackerplugin_groups::testFetchGroupsOfGivenContact()
{
    QFETCH(QContactRelationship::Role, relatedContactRole);
    QFETCH(QString, memberType);

    const int contactsGroupCount = 3;
    const int contactsSetCount = 5;
    const int contactsCount = contactsGroupCount * contactsSetCount;

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // setup contacts
    // the number of contacts is x times the number of groups, divided in sets
    // first group contains all sets, second group contains all but first set, etc., so
    // last group contains only one set, the last one
    QContact groupContacts[contactsGroupCount];
    int contactsCountInGroup[contactsGroupCount];
    int firstContactListOffset[contactsGroupCount];
    QContact contacts[contactsCount];
    int groupsCountForContact[contactsCount];
    setupContactsForFetchWithRelationshipFilterTests(
        groupContacts, contactsCountInGroup, firstContactListOffset, contactsGroupCount,
        contacts, groupsCountForContact, contactsCount, contactsSetCount, memberType
    );
    CHECK_CURRENT_TEST_FAILED;

    // prepare fetching
    QList<QContactSortOrder> groupsSortOrders;
    groupsSortOrders.append(nickNameAscendingSortOrder());

    // fetch all groups for each contact
    for (int c=0; c<contactsCount; ++c) {
        QContactRelationshipFilter hasContactAsMemberFilter;
        hasContactAsMemberFilter.setRelationshipType(QContactRelationship::HasMember);
        hasContactAsMemberFilter.setRelatedContactRole(relatedContactRole);
        hasContactAsMemberFilter.setRelatedContactId(contacts[c].id());

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedGroupsWithContact =
                engine()->contacts(hasContactAsMemberFilter, groupsSortOrders, NoFetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);

        QCOMPARE(fetchedGroupsWithContact.count(), groupsCountForContact[c]);

        for (int g=0; g<groupsCountForContact[c]; ++g) {
            const QContact &groupContact = groupContacts[g];
            const QContact &fetchedGroupContact = fetchedGroupsWithContact[g];

            QCOMPARE(fetchedGroupContact.id(), groupContact.id());
            // TODO: fetching also fills implied detail values, so this comparison does not work
            // query builder test or *_common or the generic test should have a function for custom comparison
            // use also for other testFetch*WithRelationshipFilter() methods
//             QCOMPARE(fetchedGroupContact, groupContact);

            // check relationships of the fetched group contact
            const QList<QContactRelationship> fetchedRelationships = fetchedGroupContact.relationships();
            // estimate all contacts of this group
            QList<QContactId> expectedMemberIds;
            for (int mc=firstContactListOffset[g]; mc<contactsCount; ++mc) {
                expectedMemberIds << contacts[mc].id();
            }

            foreach (const QContactRelationship &fetchedRelationship, fetchedRelationships) {
                QCOMPARE(fetchedRelationship.relationshipType(), QLatin1String(QContactRelationship::HasMember));
                QCOMPARE(fetchedRelationship.first(), groupContact.id());
                QVERIFY(expectedMemberIds.contains(fetchedRelationship.second()));
                expectedMemberIds.removeOne(fetchedRelationship.second());
            }
            QVERIFY2(expectedMemberIds.isEmpty(), "Not all expected members found in relationships.");
        }
    }

    // TODO: also test for groups as member of other groups (but not yet used by clients)
}

void
ut_qtcontacts_trackerplugin_groups::testRemoveSavedHasMemberRelationship_data()
{
    QTest::addColumn<QString>("memberType");

    const QStringList types = QStringList() << QContactType::TypeContact << QContactType::TypeGroup;

    foreach(const QString &type, types) {
        QTest::newRow(qPrintable(type)) << type;
    }

}


void
ut_qtcontacts_trackerplugin_groups::testRemoveSavedHasMemberRelationship()
{
    QFETCH(QString, memberType);

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // setup contacts
    QContact groupContact;
    SETUP_TEST_GROUPCONTACT(groupContact);
    CHECK_CURRENT_TEST_FAILED;
    QContact contact1;
    contact1.setType(memberType);
    SET_TESTNICKNAME_TO_CONTACT(contact1);
    CHECK_CURRENT_TEST_FAILED;
    QContact contact2;
    contact2.setType(memberType);
    SET_TESTNICKNAME_TO_CONTACT(contact2);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(groupContact);
    CHECK_CURRENT_TEST_FAILED;
    saveContact(contact1);
    CHECK_CURRENT_TEST_FAILED;
    saveContact(contact2);
    CHECK_CURRENT_TEST_FAILED;

    // create a hasMember relationships and save them
    QContactRelationship relationship1;
    setupTestHasMemberRelationship(relationship1, groupContact, contact1);
    CHECK_CURRENT_TEST_FAILED;

    saveRelationship(relationship1);
    CHECK_CURRENT_TEST_FAILED;

    QContactRelationship relationship2;
    setupTestHasMemberRelationship(relationship2, groupContact, contact2);
    CHECK_CURRENT_TEST_FAILED;

    saveRelationship(relationship2);
    CHECK_CURRENT_TEST_FAILED;


    // fetch the saved relationships
    QContactRelationship savedRelationship1;
    fetchRelationship(groupContact.id(), QContactRelationship::HasMember, contact1.id(), savedRelationship1);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(savedRelationship1.relationshipType(), QLatin1String(QContactRelationship::HasMember.latin1()));
    QCOMPARE(savedRelationship1.first(), groupContact.id());
    QCOMPARE(savedRelationship1.second(), contact1.id());

    QContactRelationship savedRelationship2;
    fetchRelationship(groupContact.id(), QContactRelationship::HasMember, contact2.id(), savedRelationship2);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(savedRelationship2.relationshipType(), QLatin1String(QContactRelationship::HasMember.latin1()));
    QCOMPARE(savedRelationship2.first(), groupContact.id());
    QCOMPARE(savedRelationship2.second(), contact2.id());

    // remove first relationship
    removeRelationship(relationship1);
    CHECK_CURRENT_TEST_FAILED;

    // test there is only the second relationship saved
    QList<QContactRelationship> savedRelationships;
    fetchRelationships(QContactRelationship::HasMember, groupContact.id(), QContactRelationship::First, savedRelationships);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(savedRelationships.count(), 1);
    QCOMPARE(savedRelationships[0].relationshipType(), QLatin1String(QContactRelationship::HasMember.latin1()));
    QCOMPARE(savedRelationships[0].first(), groupContact.id());
    QCOMPARE(savedRelationships[0].second(), contact2.id());

    // remove second relationship
    removeRelationship(relationship2);
    CHECK_CURRENT_TEST_FAILED;

    // test there are no more saved relationships
    fetchRelationships(QContactRelationship::HasMember, groupContact.id(), QContactRelationship::First, savedRelationships);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(savedRelationships.count(), 0);
}

static void
checkRelationships(const QList<QContactRelationship> &relationships,
                   const QContactId &expectedParentId, QList<QContactId> expectedChildIds,
                   const QString &expectedRelationshipType = QContactRelationship::HasMember)
{
    QCOMPARE(relationships.count(), expectedChildIds.count());

    foreach(const QContactRelationship &r, relationships) {
        QCOMPARE(r.relationshipType(), expectedRelationshipType);
        QCOMPARE(r.first(), expectedParentId);
        QVERIFY(expectedChildIds.removeOne(r.second()));
    }

    QVERIFY(expectedChildIds.isEmpty());
}


void
ut_qtcontacts_trackerplugin_groups::testPartialSavingKeepsGroups()
{
    // setup contacts
    QContact group;
    SETUP_TEST_GROUPCONTACT(group);
    CHECK_CURRENT_TEST_FAILED;

    QContact contact1;
    SET_TESTNICKNAME_TO_CONTACT(contact1);
    CHECK_CURRENT_TEST_FAILED;

    QContact contact2;
    SET_TESTNICKNAME_TO_CONTACT(contact2);
    CHECK_CURRENT_TEST_FAILED;

    // save contacts
    saveContact(group);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(contact1);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(contact2);
    CHECK_CURRENT_TEST_FAILED;

    // add contact to group
    QContactRelationship savedMembership;
    savedMembership.setRelationshipType(QContactRelationship::HasMember);
    savedMembership.setFirst(group.id());
    savedMembership.setSecond(contact1.id());
    saveRelationship(savedMembership);
    CHECK_CURRENT_TEST_FAILED;

    savedMembership.setSecond(contact2.id());
    saveRelationship(savedMembership);
    CHECK_CURRENT_TEST_FAILED;

    // check relationships
    QList<QContactRelationship> fetchedMemberships;
    fetchRelationships(QContactRelationship::HasMember,
                       group.id(), QContactRelationship::First,
                       fetchedMemberships);
    CHECK_CURRENT_TEST_FAILED;

    checkRelationships(fetchedMemberships, group.id(),
                       QList<QContactId>() << contact1.id() << contact2.id());
    CHECK_CURRENT_TEST_FAILED;

    // attach avatar to first contact
    QContactAvatar savedAvatar;
    savedAvatar.setImageUrl(QUrl::fromEncoded("file://home/user/500-pounds-gorilla.jpg"));
    QVERIFY(contact1.saveDetail(&savedAvatar));

    // save just the avatar detail of first contact.
    // explicitly use contact manager to get partial saving without tricks.
    QContactManager cm(engine()->managerName(), makeEngineParams());
    QList<QContact> partialSavingContactList = QList<QContact>() << contact1;
    const QStringList partialSavingDetailList(savedAvatar.definitionName());

    QMap<int, QContactManager::Error> errors;
    QVERIFY(cm.saveContacts(&partialSavingContactList, partialSavingDetailList, &errors));
    QVERIFY(errors.isEmpty());

    // check that the avatar indeed was saved
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactAvatar fetchedAvatar =
            engine()->contactImpl(contact1.localId(), NoFetchHint, &error).
            detail<QContactAvatar>();
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(fetchedAvatar.imageUrl(), savedAvatar.imageUrl());

    // check that relationships are preserved
    fetchRelationships(QContactRelationship::HasMember,
                       group.id(), QContactRelationship::First,
                       fetchedMemberships);
    CHECK_CURRENT_TEST_FAILED;

    checkRelationships(fetchedMemberships, group.id(),
                       QList<QContactId>() << contact1.id() << contact2.id());
    CHECK_CURRENT_TEST_FAILED;

}


QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_groups)
