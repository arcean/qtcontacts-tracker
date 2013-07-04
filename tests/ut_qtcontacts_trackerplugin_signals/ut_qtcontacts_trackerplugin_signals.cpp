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

#include "ut_qtcontacts_trackerplugin_signals.h"

#include "slots.h"

#include <lib/constants.h>
#include <lib/contactmergerequest.h>
#include <lib/customdetails.h>
#include <lib/sparqlresolver.h>
#include <lib/unmergeimcontactsrequest.h>

#include <cubi.h>

CUBI_USE_NAMESPACE

ut_qtcontacts_trackerplugin_signals::ut_qtcontacts_trackerplugin_signals(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)
{
}

void
ut_qtcontacts_trackerplugin_signals::testSignals_data()
{
    QTest::addColumn<int>("contactCount");

    QTest::newRow("10") << (10);

    // unit test for NB#212463 - "too many SQL variables" error when importing 500+ telepathy contacts
    QTest::newRow("limit-1") << (QctSparqlResolver::ColumnLimit - 1);
    QTest::newRow("limit") << (QctSparqlResolver::ColumnLimit);
    QTest::newRow("limit+1") << (QctSparqlResolver::ColumnLimit + 1);
    QTest::newRow("limit*2") << (QctSparqlResolver::ColumnLimit * 2);
}

void
ut_qtcontacts_trackerplugin_signals::testSignals()
{
    QFETCH(int, contactCount);

    Slots contactsAdded(contactCount);
    Slots contactsChanged(contactCount);
    Slots contactsRemoved(contactCount);

    connect(engine(), SIGNAL(contactsAdded(QList<QContactLocalId>)),
            &contactsAdded, SLOT(notifyIds(QList<QContactLocalId>)));
    connect(engine(), SIGNAL(contactsChanged(QList<QContactLocalId>)),
            &contactsChanged, SLOT(notifyIds(QList<QContactLocalId>)));
    connect(engine(), SIGNAL(contactsRemoved(QList<QContactLocalId>)),
            &contactsRemoved, SLOT(notifyIds(QList<QContactLocalId>)));

    // build test contacts
    const QStringList contactTypes = engine()->supportedContactTypes();
    QList<QContact> contacts;

    for(int i = 0; i < contactCount; ++i) {
        QContactName name;
        name.setFirstName(QString::fromLatin1("Signal Tester %1").arg(i + 1));
        name.setLastName(QLatin1String(QTest::currentDataTag()));

        QContact contact;
        contact.setType(contactTypes[i % contactTypes.count()]);
        QVERIFY(contact.saveDetail(&name));

        contacts.append(contact);
    }

    // save test contacts
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool success = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    // collect local ids
    QContactLocalIdList localIds;

    for(int i = 0; i < contacts.count(); ++i) {
        QVERIFY2(0 != contacts[i].localId(), qPrintable(QString::number(i)));
        localIds.append(contacts[i].localId());
    }

    // wait for signals as the sync API doesn't spin the event loop
    contactsAdded.wait(10000);

    // verify if added notifications where received for all test contacts
    QCOMPARE(contactsAdded.ids.count(), contacts.count());
    QCOMPARE(contactsChanged.ids.count(), 0);
    QCOMPARE(contactsRemoved.ids.count(), 0);

    for(int i = 0; i < localIds.count(); ++i) {
        QVERIFY2(contactsAdded.ids.contains(localIds[i]), qPrintable(QString::number(i)));
    }

    contactsAdded.clear();

    // modify test contacts
    for(int i = 0; i < contacts.count(); ++i) {
        QContactName name = contacts[i].detail<QContactName>();
        name.setPrefix(QLatin1String(i & 1 ? "Don" : "Donna"));
        contacts[i].saveDetail(&name);
    }

    // save modified test contacts
    error = QContactManager::UnspecifiedError;
    success = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    // wait for signals as the sync API doesn't spin the event loop
    contactsChanged.wait(10000);

    // verify if change notifications where received for all test contacts
    QCOMPARE(contactsAdded.ids.count(), 0);
    QCOMPARE(contactsChanged.ids.count(), contacts.count());
    QCOMPARE(contactsRemoved.ids.count(), 0);

    for(int i = 0; i < localIds.count(); ++i) {
        QVERIFY2(contactsChanged.ids.contains(localIds[i]), qPrintable(QString::number(i)));
    }

    contactsChanged.clear();

    // merge two contacts (0 <- 1 & im contact)

    // Insert IM contact
    QContactLocalId imContactId;
    QContactOnlineAccount imOnlineAccount;
    {
        const QString contactIri = QLatin1String("contact:testSignals:0");
        const QString &accountId = QLatin1String("test-0@bugfree.org");
        const QString &presence = QLatin1String("nco:presence-status-available");
        const QString &presenceDetails = QLatin1String("Hello world!");
        const QString &accountPath = QLatin1String("/org/freedesktop/testSignals/account/0");
        const QString &protocol = QLatin1String("jabber");
        const QString &provider = QLatin1String("bugfree.org");

        imOnlineAccount.setAccountUri(accountId);
        imOnlineAccount.setProtocol(protocol);
        imOnlineAccount.setServiceProvider(provider);
        imOnlineAccount.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
        imContactId = insertIMContact(contactIri, accountId, presence, presenceDetails,
                                      accountPath, protocol, provider);
    }

    QctContactMergeRequest mergeRequest;
    QVERIFY(localIds.count() > 2);
    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;
    mergeIds.insert(localIds[0], localIds[1]);
    mergeIds.insert(localIds[0], imContactId);
    mergeRequest.setMergeIds(mergeIds);

    QVERIFY(engine()->startRequest(&mergeRequest));
    QVERIFY(engine()->waitForRequestFinished(&mergeRequest, 0));
    QCOMPARE(mergeRequest.error(), QContactManager::NoError);

    // wait for signals as the sync API doesn't spin the event loop
    contactsChanged.wait(10000);

    // verify if contact notifications where received for all test contacts
    QCOMPARE(contactsAdded.ids.count(), 1 /* from insertIMContact() */);
    QCOMPARE(contactsChanged.ids.count(), 1 /* contact 0 merged */);
    QCOMPARE(contactsRemoved.ids.count(), 2 /* 1 & IM merged into contact 0 so removed*/);

    contactsAdded.clear();
    contactsChanged.clear();
    contactsRemoved.clear();

    // now unmerge the IM part of contact 0
    // first fetch the contact
    error = QContactManager::UnspecifiedError;
    QContact sourceContact = engine()->contact(localIds[0], QContactFetchHint(), &error);
    QCOMPARE(error, QContactManager::NoError);

    // then run the unmerge request
    QctUnmergeIMContactsRequest unmergeRequest;
    unmergeRequest.setUnmergeOnlineAccounts(QList<QContactOnlineAccount>() << imOnlineAccount);
    unmergeRequest.setSourceContact(sourceContact);

    QVERIFY(engine()->startRequest(&unmergeRequest));
    QVERIFY(engine()->waitForRequestFinished(&unmergeRequest, 0));
    QCOMPARE(unmergeRequest.error(), QContactManager::NoError);

    // wait for signals as the sync API doesn't spin the event loop
    contactsChanged.wait(10000);

    // verify if contact notifications where received for all test contacts
    QCOMPARE(contactsAdded.ids.count(), 1 /* IM unmerged */);
    QCOMPARE(contactsChanged.ids.count(), 1 /* contact 0 unmerged */);
    QCOMPARE(contactsRemoved.ids.count(), 0);

    contactsAdded.clear();
    contactsChanged.clear();

    // remove test contacts
    error = QContactManager::UnspecifiedError;
    success = engine()->removeContacts(localIds, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    // wait for signals as the sync API doesn't spin the event loop
    contactsRemoved.wait(10000);

    // verify if remove notifications where received for all test contacts
    QCOMPARE(contactsAdded.ids.count(), 0);
    QCOMPARE(contactsChanged.ids.count(), 0);
    // Contact 1 was already removed during the merge
    QCOMPARE(contactsRemoved.ids.count(), contacts.count() - 1);

    for(int i = 0; i < localIds.count(); ++i) {
        if (i == 1) continue; // 1 was merged into 0
        QVERIFY2(contactsRemoved.ids.contains(localIds[i]), qPrintable(QString::number(i)));
    }
}

void
ut_qtcontacts_trackerplugin_signals::testOmitPresenceChanges_data()
{
    QTest::addColumn<QString>("updateTimestampQuery");

    QTest::newRow("insert-or-replace-one")
            << QString::fromLatin1("INSERT OR REPLACE {"
                                   "  GRAPH <urn:x-random:graph> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "  GRAPH <%2> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}");

    QTest::newRow("insert-or-replace-two")
            << QString::fromLatin1("INSERT OR REPLACE {"
                                   "  GRAPH <urn:x-random:graph> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}"

                                   "INSERT OR REPLACE {"
                                   "  GRAPH <%2> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}");

    QTest::newRow("delete-and-insert")
            << QString::fromLatin1("DELETE {"
                                   "  ?contact nie:contentLastModified ?t"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact ;"
                                   "           nie:contentLastModified ?t"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}"
                                   "INSERT {"
                                   "  GRAPH <urn:x-random:graph> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}"

                                   "DELETE {"
                                   "  GRAPH <urn:x-random:graph> {"
                                   "    ?contact nie:contentLastModified ?t"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact ;"
                                   "           nie:contentLastModified ?t"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}"
                                   "INSERT {"
                                   "  GRAPH <%2> {"
                                   "    ?contact nie:contentLastModified %1"
                                   "  }"
                                   "} WHERE {"
                                   "  ?contact a nco:PersonContact"
                                   "  FILTER(tracker:id(?contact) = %3)"
                                   "}");
}


void
ut_qtcontacts_trackerplugin_signals::testOmitPresenceChanges()
{
    QFETCH(QString, updateTimestampQuery);

    const QString managerName = QLatin1String("tracker");

    QMap<QString, QString> managerParams;
    managerParams.insert(QLatin1String("omit-presence-changes"), QLatin1String("true"));

    // We need a custom manager to pass the ignore-presence-changes signals
    QContactManager manager(managerName, managerParams);
    QCOMPARE(manager.managerName(), managerName);

    Slots contactsChangedSlots(0);
    Slots contactsChangedFilteredSlots(0);

    connect(engine(), SIGNAL(contactsChanged(QList<QContactLocalId>)),
            &contactsChangedSlots, SLOT(notifyIds(QList<QContactLocalId>)));
    connect(&manager, SIGNAL(contactsChanged(QList<QContactLocalId>)),
            &contactsChangedFilteredSlots, SLOT(notifyIds(QList<QContactLocalId>)));

    // 1. Create a contact
    QContact c;
    QContactNickname nickname;
    nickname.setNickname(QUuid::createUuid().toString());
    c.saveDetail(&nickname);

    saveContact(c);

    // 2. Wait a bit for the contactsAdded signal to fire
    contactsChangedFilteredSlots.wait(2000);

    QCOMPARE(contactsChangedSlots.ids.count(), 0);
    QCOMPARE(contactsChangedFilteredSlots.ids.count(), 0);

    // 3. Normal change on the contact
    nickname.setNickname(QUuid::createUuid().toString());
    c.saveDetail(&nickname);
    saveContact(c);

    contactsChangedFilteredSlots.wait(2000);

    QCOMPARE(contactsChangedSlots.ids.count(), 1);
    QCOMPARE(contactsChangedFilteredSlots.ids.count(), 1);

    // 4. Update the timestamp of the contact in a "silent" way
    // We use the graph IRI of the default engine, that should be the same as
    // the one of the manager created above
    QScopedPointer<QSparqlResult> result(executeQuery(updateTimestampQuery.arg(LiteralValue(QDateTime::currentDateTime()).sparql(),
                                                                               QtContactsTrackerDefaultGraphIri,
                                                                               QString::number(c.localId())),
                                                      QSparqlQuery::InsertStatement));

    QVERIFY(not result.isNull());

    contactsChangedFilteredSlots.wait(2000);

    QCOMPARE(contactsChangedSlots.ids.count(), 2);
    QCOMPARE(contactsChangedFilteredSlots.ids.count(), 1);
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_signals)
