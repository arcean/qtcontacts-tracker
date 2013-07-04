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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_GROUPS_H
#define UT_QTCONTACTS_TRACKERPLUGIN_GROUPS_H

#include "ut_qtcontacts_trackerplugin_common.h"

QTM_USE_NAMESPACE


#define SETUP_TEST_GROUPCONTACT(c)  setupTestGroupContact(c, QLatin1String(__func__))

class ut_qtcontacts_trackerplugin_groups : public ut_qtcontacts_trackerplugin_common
{
    Q_OBJECT

public:
    ut_qtcontacts_trackerplugin_groups(QObject *parent = 0);

protected:
    QMap<QString, QString> makeEngineParams() const;

private slots:
    void checkDatabaseEmpty();

    void testSaveGroupContact();
    void testSaveGroupContactSyncCall();

    void testFetchSavedGroupContactId();
    void testFetchSavedGroupContactIdSyncCall();

    void testFetchSavedGroupContact();
    void testFetchSavedGroupContactSyncCall();

    void testSaveHasMemberRelationship_data();
    void testSaveHasMemberRelationship();

    void testFetchSavedHasMemberRelationship_data();
    void testFetchSavedHasMemberRelationship();

    void testRemoveSavedHasMemberRelationship_data();
    void testRemoveSavedHasMemberRelationship();

    void testFetchContactsOfGivenGroup_data();
    void testFetchContactsOfGivenGroup();

    void testFetchGroupsOfGivenContact_data();
    void testFetchGroupsOfGivenContact();

    void testPartialSavingKeepsGroups();

private:
    void setupTestGroupContact(QContact &groupContact, const QString &id) const;
    void setupTestHasMemberRelationship(QContactRelationship &relationship,
                                        const QContact &groupContact, const QContact &contact) const;
    void setupContactsForFetchWithRelationshipFilterTests(
        QContact groupContacts[],
        int contactsCountInGroup[],
        int firstContactListOffset[],
        int contactsGroupCount,
        QContact contacts[],
        int groupsCountForContact[],
        int contactsCount,
        int contactsSetCount,
        const QString &memberType = QContactType::TypeContact
    );
};

#endif /* UT_QTCONTACTS_TRACKERPLUGIN_GROUPS_H */
