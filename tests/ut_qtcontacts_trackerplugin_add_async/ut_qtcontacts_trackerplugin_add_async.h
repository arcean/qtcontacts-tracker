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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_ADD_ASYNC_H
#define UT_QTCONTACTS_TRACKERPLUGIN_ADD_ASYNC_H

// test
#include <ut_qtcontacts_trackerplugin_common.h>
// Qt Mobility
#include <QContact>
#include <QContactFetchRequest>

QTM_USE_NAMESPACE

class ut_qtcontacts_trackerplugin_add_async : public ut_qtcontacts_trackerplugin_common
{
Q_OBJECT
public:
    ut_qtcontacts_trackerplugin_add_async();

// Private slots are called by the QTest framework.
private slots:
    // Per test-function:
    void cleanup();

    // Test functions:
    void ut_testAddContact();
    void ut_checkSubTypes();

// Protected or public slots are _not_ called by the QTest framework.
protected slots:
    void onContactFetchRequestProgress();
    void onContactFoundThenRemoveAndTest();
    void onTimeoutAddContact();
    void onContactFoundThenCheck();
    void onContactFoundThenRemoveAndStop();

private:

    typedef void (ut_qtcontacts_trackerplugin_add_async::*FinishedCallbackFunc)(void);

    // Get the contact ID for the test contact if it exists already.
    void getExistingContact(FinishedCallbackFunc finishedCallback);

    // wait (allowing the mainloop to respond) until this->waiting is false.
    bool waitForStop();

    //A hacky way to bind an extra parameter to the Qt slot.
    FinishedCallbackFunc getExistingContactFinishedCallback;

    QContact contact;
    QContactFetchRequest contactFetchRequest;

    bool waiting;
};

#endif /* UT_CONTACTSGUI_H_ */
