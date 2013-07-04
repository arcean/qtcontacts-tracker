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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_QUERYBUILDER_H
#define UT_QTCONTACTS_TRACKERPLUGIN_QUERYBUILDER_H

#include "ut_qtcontacts_trackerplugin_common.h"

#include <dao/contactdetailschema.h>

#include <cubi.h>

QTM_USE_NAMESPACE

class ut_qtcontacts_trackerplugin_querybuilder : public ut_qtcontacts_trackerplugin_common
{
    Q_OBJECT

public:
    enum VerifyQueryFlag
    {
        RenameAnonymousVariables = (1 << 0),
        RenameNamedVariables = (1 << 1),
        RenameVariables = RenameAnonymousVariables | RenameNamedVariables,
        RenameIris = (1 << 2),

        VerifyQueryDefaults = RenameVariables
    };

    Q_DECLARE_FLAGS(VerifyQueryFlags, VerifyQueryFlag)

    explicit ut_qtcontacts_trackerplugin_querybuilder(QObject *parent = 0);

protected:
    QMap<QString, QString> makeEngineParams() const;

private slots:
    void initTestCase();

    void testAddressDetail();
    void testAnniversaryDetail();
    void testBirthdayDetail();
    void testEmailAddressDetail();
    void testFamilyDetail();
    void testFavoriteDetail();
    void testGenderDetail();
    void testGeoLocationDetail();
    void testGuidDetail();
    void testHobbyDetail();
    void testNameDetail();
    void testNicknameDetail();
    void testNoteDetail();
    void testOnlineAccountDetail();
    void testOnlineAvatarDetail();
    void testOrganizationDetail();
    void testPersonalAvatarDetail();
    void testPhoneNumberDetail();
    void testPresenceDetail();
    void testRelevanceDetail();
    void testRingtoneDetail();
    void testSocialAvatarDetail();
    void testSyncTargetDetail();
    void testTagDetail();
    void testTimestampDetail();
    void testUrlDetail();

    void testDetailSchema_data();
    void testDetailSchema();

    void testDetailCoverage();

    void testAllDetailsPossible_data();
    void testAllDetailsPossible();

    void testFetchRequest();
    void testFetchRequestQuery_data();
    void testFetchRequestQuery();

    void testLocalIdFetchRequestQuery_data();
    void testLocalIdFetchRequestQuery();

    void testContactLocalIdFilter_data();
    void testContactLocalIdFilter();
    void testContactLocalIdFilterQuery();
    void testIntersectionFilter();
    void testIntersectionFilterQuery();
    void testUnionFilter();
    void testUnionFilterQuery();
    void testDetailFilter();
    void testDetailFilterQuery_data();
    void testDetailFilterQuery();
    void testDetailRangeFilter();
    void testDetailRangeFilterQuery();
    void testChangeLogFilter();
    void testChangeLogFilterQuery();
    void testRelationshipFilter();
    void testRelationshipFilterQuery_data();
    void testRelationshipFilterQuery();

    void testRemoveRequest();
    void testRemoveRequestQuery();
    void testSaveRequestCreate();
    void testSaveRequestUpdate();
    void testSaveRequestQuery_data();
    void testSaveRequestQuery();
    void testGarbageCollectorQuery();

    void testParseAnonymousIri();
    void testParseEmailAddressIri();
    void testParseTelepathyIri();
    void testParsePresenceIri();

    void testMakeAnonymousIri();
    void testMakeEmailAddressIri();
    void testMakeTelepathyIri();
    void testMakePresenceIri();

    void testTelepathyIriConversion_data();
    void testTelepathyIriConversion();
    void testLocalPhoneNumberConversion_data();
    void testLocalPhoneNumberConversion();
    void testCamelCaseFunction();

    void testUpdateDetailUri();

    void testPhoneNumberIRI_data();
    void testPhoneNumberIRI();

    void missingTests_data();
    void missingTests();

public:
    bool verifyQuery(QString actualQuery, QString expectedQuery,
                     VerifyQueryFlags flags = VerifyQueryDefaults);

    void verifyContactDetail(int referenceId, const QString &detailName);

    void verifyFilter(const QContactFilter &filter, const QString &referenceFileName);
    void verifyFilterQuery(const QContactFilter &filter, const QString &referenceFileName,
                           const QString &contactType = QContactType::TypeContact);
    void verifyFilterQuery(const QContactFilter &filter, const QString &referenceFileName,
                           QStringList definitionNames,
                           const QString &contactType = QContactType::TypeContact);

    QSet<QUrl> inheritedClassIris(const QTrackerContactDetailSchema &schema) const;

private:
    QStringList mDebugFlags;

    bool mShowContact;
    bool mShowQuery;
    bool mShowSchema;
    bool mSilent;
    bool mWordDiff;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ut_qtcontacts_trackerplugin_querybuilder::VerifyQueryFlags)

#endif // UT_QTCONTACTS_TRACKERPLUGIN_PERFORMANCE_H
