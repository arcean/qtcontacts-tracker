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

#include "ut_qtcontacts_trackerplugin_add_async.h"

#include <QContactManager>
#include <QContactName>
#include <QContactDetailFilter>
#include <QContactPhoneNumber>
#include <QtTest/QtTest>
#include <QDebug>

// Note that we try to avoid using any names that might already be in the database:
const char* TESTNAME_FIRST = "ut_qtcontacts_add_firstname";
const char* TESTNAME_LAST = "ut_qtcontacts_add_firstlast";

ut_qtcontacts_trackerplugin_add_async::ut_qtcontacts_trackerplugin_add_async()
: ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)), QDir(QLatin1String(SRCDIR))),
  getExistingContactFinishedCallback(0),
  waiting(false)
{
}

void
ut_qtcontacts_trackerplugin_add_async::cleanup()
{
    waiting = false;

    ut_qtcontacts_trackerplugin_common::cleanup();
}

bool
ut_qtcontacts_trackerplugin_add_async::waitForStop()
{
    waiting = true;

    const int max_secs = 100000;

    // wait for signal
    int i = 0;
    while(waiting && i++ < max_secs) {
        // Allow the mainloop to run:
        QTest::qWait(10);
    }

    return !waiting;
}

void
ut_qtcontacts_trackerplugin_add_async::onContactFetchRequestProgress()
{
    //qDebug() << "onContactFetchRequestProgress";
    if (!contactFetchRequest.isFinished())
        return;

    //Store the contact so the callback can use it.
    if(!(contactFetchRequest.contacts().isEmpty())) {
        contact = contactFetchRequest.contacts()[0];
        QVERIFY(contact.localId() != 0);
        contactFetchRequest.cancel(); //Stop any more slot calls.
    }

    //qDebug() << "debug: fetched localId=" << contact.localId();

    //Avoid more slot calls, though this is unlikely because it has finished.
    contactFetchRequest.cancel();

    //Call the callback method that was specified to getExistingContact():
    if(getExistingContactFinishedCallback) {
        FinishedCallbackFunc func = getExistingContactFinishedCallback;
        getExistingContactFinishedCallback = 0;
        (this->*func)();
    }
}


void
ut_qtcontacts_trackerplugin_add_async::getExistingContact(FinishedCallbackFunc finishedCallback)
{
    // Stop pending fetch requests
    if (contactFetchRequest.isActive())
        contactFetchRequest.cancel();

    // Initialize the result.
    contact = QContact();

    //TODO: How can we AND on both the first and last name?
    getExistingContactFinishedCallback = finishedCallback; //Call this when the contact has been retrieved.
    connect(&contactFetchRequest, SIGNAL(resultsAvailable()),
        SLOT(onContactFetchRequestProgress()));

    QContactDetailFilter nameFilter;
    nameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    nameFilter.setValue(QLatin1String(TESTNAME_FIRST));
    nameFilter.setMatchFlags(QContactFilter::MatchExactly);
    contactFetchRequest.setFilter(nameFilter);

    //qDebug() << "debug: start request";
    const bool success = engine()->startRequest(&contactFetchRequest);
    QVERIFY(success);
}

//This is our actual test function:
void
ut_qtcontacts_trackerplugin_add_async::ut_testAddContact()
{
    //qDebug() << "debug: ut_testAddContact";
    //Make sure that the contact is not already in the database.
    getExistingContact(&ut_qtcontacts_trackerplugin_add_async::onContactFoundThenRemoveAndTest);

    //Block (allowing the mainloop to run) until we have finished.
    waitForStop();
}

void
ut_qtcontacts_trackerplugin_add_async::onContactFoundThenRemoveAndTest()
{
    //qDebug() << "debug: Removing the existing contact, if it exists.";

    //If the contact was not found then don't bother trying to remove it.
    //The caller does not bother checking - it just wants to make sure that
    //it's deleted if it does exist.
    //TODO: If the caller checked then it would improve the test.
    if(contact.localId() != 0) {
        //TODO: Find and use an async API that tells us when it has finished.
        QContactManager::Error error = QContactManager::NoError;
        const bool success = engine()->removeContact(contact.localId(), &error);
        //qWarning() << Q_FUNC_INFO << ": removeContact(" << contact.localId() << "): error: " << error;
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(success);
    }

    // Run the Qt main loop:
    QTimer::singleShot(2000, this, SLOT(onTimeoutAddContact()));
}

void
ut_qtcontacts_trackerplugin_add_async::onTimeoutAddContact()
{
    //qDebug() << "debug: Trying to add contact.";

    // Offer a UI to edit a prefilled contact.
    QContactName name;
    name.setFirstName(QLatin1String(TESTNAME_FIRST));
    name.setLastName(QLatin1String(TESTNAME_LAST));
    //TODO: Find and use an async API that tells us when it has finished.
    contact.saveDetail(&name);
    //const bool saved = contact.saveDetail(&name);
    //Q_ASSERT(saved); //This won't necessarily be useful because our implementation doesn't support sync methods.

    //Save the contact.
    //But note that our QContactManager backend does not set localId when returning.
    QContactManager::Error error = QContactManager::NoError;
    const bool success = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    //This works too:
    //QContact copy(contact);
    //manager->saveContact(&copy);

    //Check that it was really saved:
    //qDebug() << "debug: checking that the contact was saved.";
    getExistingContact(&ut_qtcontacts_trackerplugin_add_async::onContactFoundThenCheck);
}

void
ut_qtcontacts_trackerplugin_add_async::onContactFoundThenCheck()
{
    //Check that it was really saved:
    // The ContactManager::saveContact() documentation suggests that localeId=0 is for non-saved contacts.
    //QEXPECT_FAIL("", "QContactManager::saveContact() saves the contact (see it by running the contacts UI), but returns false and doesn't set error(). Find out why.", Continue);
    QVERIFY(contact.localId() != 0);

    //Check that the correct details were saved:
    const QContactName name = contact.detail<QContactName>();
    QVERIFY(name.firstName() == QLatin1String(TESTNAME_FIRST));
    QVERIFY(name.lastName() == QLatin1String(TESTNAME_LAST));

    //Try to restore original conditions:
    getExistingContact(&ut_qtcontacts_trackerplugin_add_async::onContactFoundThenRemoveAndStop);
}

void
ut_qtcontacts_trackerplugin_add_async::onContactFoundThenRemoveAndStop()
{
    //qDebug() << "debug: ut_qtcontacts_add::onContactFoundThenRemoveAndStop";
    QContactManager::Error error = QContactManager::NoError;
    const bool success = engine()->removeContact(contact.localId(), &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    // Allow the actual test function to return:
    waiting = false;
}

void
ut_qtcontacts_trackerplugin_add_async::ut_checkSubTypes()
{
    //TODO: There are no actual checks in this method.

    QContactPhoneNumber phone;
    phone.setSubTypes(QContactPhoneNumber::SubTypeMobile);
    phone.setContexts(QContactPhoneNumber::ContextHome);
    phone.setNumber(QLatin1String("12345"));

    QContact contact;
    contact.saveDetail(&phone);

    QContactManager::Error error = QContactManager::NoError;
    bool success = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    phone.setSubTypes(QContactPhoneNumber::SubTypeFax);

    contact.saveDetail(&phone);

    error = QContactManager::NoError;
    success = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_add_async)
