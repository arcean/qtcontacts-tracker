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

#include "ut_qtcontacts_trackerplugin_fetch.h"

#include <QContactManager>
#include <QContactName>
#include <QContactFetchRequest>
#include <QContactUrl>

typedef QSet<QString> QStringSet;

ut_qtcontacts_trackerplugin_fetch::ut_qtcontacts_trackerplugin_fetch(QObject *parent) :
        ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                           QDir(QLatin1String(SRCDIR)), parent)
{
    m_uudi = QUuid::createUuid();
    m_firstName = QLatin1String("ut_qtcontacts_fetch_firstname_") + m_uudi;
    m_lastName = QLatin1String("ut_qtcontacts_fetch_lastname_") + m_uudi;
    m_webPage = QLatin1String("ut_qtcontacts_fetch_url_") + m_uudi;

    mNameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    mNameFilter.setMatchFlags(QContactFilter::MatchExactly);
    mNameFilter.setValue(m_firstName);
}

void ut_qtcontacts_trackerplugin_fetch::setupTestContact(QContact &contact)
{
    QContactName name;
    name.setFirstName(m_firstName);
    name.setLastName(m_lastName);
    QVERIFY(contact.saveDetail(&name));

    QContactUrl url;
    url.setUrl(m_webPage);
    QVERIFY(contact.saveDetail(&url));
}

void ut_qtcontacts_trackerplugin_fetch::checkDatabaseEmpty()
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList contacts = engine()->contacts(mNameFilter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    // dump unexpected contacts
    foreach(const QContact &c, contacts) {
        qWarning() << "unexpected contact" << c;
    }

    // verify that there really is no test contact yet
    QVERIFY(contacts.isEmpty());
}

void ut_qtcontacts_trackerplugin_fetch::testSaveContact()
{
    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named contact and save it
    QContact contact;

    setupTestContact(contact);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(contact);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_fetch::testSaveContactCopy()
{
    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named contact
    QContact contact;

    setupTestContact(contact);
    CHECK_CURRENT_TEST_FAILED;

    // add copy of the contact to database
    QContact copy(contact);

    saveContact(copy);
    CHECK_CURRENT_TEST_FAILED;
}

// NB#175259 - QContactPhoneNumber::match returns all contacts if there are no matches
void ut_qtcontacts_trackerplugin_fetch::testFetchSavedContact()
{
    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // create a named contact and save it
    QContact savedContact;

    setupTestContact(savedContact);
    CHECK_CURRENT_TEST_FAILED;

    saveContact(savedContact);
    CHECK_CURRENT_TEST_FAILED;

    // fetch the saved contact
    QContact fetchedContact;
    fetchContact(mNameFilter, fetchedContact);
    CHECK_CURRENT_TEST_FAILED;

    QVERIFY(0 != fetchedContact.localId());

    // check that the fetched contact has the expected properties
    QList<QContactDetail> details(fetchedContact.details(QContactName::DefinitionName));

    QCOMPARE(details.count(), 1);
    QCOMPARE(details[0].value(QContactName::FieldFirstName), m_firstName);
    QCOMPARE(details[0].value(QContactName::FieldLastName), m_lastName);

    details = fetchedContact.details(QContactUrl::DefinitionName);

    QCOMPARE(details.count(), 1);
    QCOMPARE(details[0].value(QContactUrl::FieldUrl), m_webPage);
}

void ut_qtcontacts_trackerplugin_fetch::testMatchPhoneNumber()
{
    const QString sample(QLatin1String("55667788"));

    // check that we start with a clean database
    checkDatabaseEmpty();
    CHECK_CURRENT_TEST_FAILED;

    // insert some contacts without phone number
    QList<QContact> savedContacts;

    for(int i = 1; i <= 5; ++i) {
        QContactName name;
        name.setLastName(QLatin1String(__func__));
        name.setFirstName(QString::number(i));

        QContact contact;
        QVERIFY(contact.saveDetail(&name));
        savedContacts.append(contact);
    }

    saveContacts(savedContacts);
    CHECK_CURRENT_TEST_FAILED;

    // fetch few contacts
    QList<QContact> fetchedContacts;
    fetchContacts(QContactPhoneNumber::match(sample), fetchedContacts);
    QCOMPARE(fetchedContacts.count(), 0);

    // add matching phone number to one contact
    QContactPhoneNumber phoneNumber;
    phoneNumber.setNumber(sample);
    QVERIFY(savedContacts[2].saveDetail(&phoneNumber));
    saveContact(savedContacts[2]);
    CHECK_CURRENT_TEST_FAILED;

    // again fetch few contacts
    QCOMPARE(fetchedContacts.count(), 0);
    fetchContacts(QContactPhoneNumber::match(sample), fetchedContacts);
    QCOMPARE(fetchedContacts.count(), 1);
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_fetch)

