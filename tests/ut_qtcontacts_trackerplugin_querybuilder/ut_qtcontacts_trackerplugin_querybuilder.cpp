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

#include "ut_qtcontacts_trackerplugin_querybuilder.h"
#include "resourcecleanser.h"

#include <dao/contactdetail.h>
#include <dao/subject.h>
#include <dao/support.h>

#include <engine/contactfetchrequest.h>
#include <engine/contactidfetchrequest.h>
#include <engine/contactremoverequest.h>
#include <engine/contactsaverequest.h>
#include <engine/engine.h>

#include <lib/requestextensions.h>
#include <lib/phoneutils.h>
#include <lib/settings.h>
#include <lib/customdetails.h>
#include <lib/sparqlresolver.h>

#include <QtDebug>
#include <QtTest>

///////////////////////////////////////////////////////////////////////////////////////////////////

QTM_USE_NAMESPACE

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

static QStringList zip(const QStringList &l1, const QStringList &l2)
{
    QStringList ret;

    for(int i = 0; i < qMin(l1.size(), l2.size()); ++i) {
        ret.append(l1.at(i));
        ret.append(l2.at(i));
    }

    const int zipped = ret.size()/2;

    for(int i = zipped; i < l1.size(); ++i) {
        ret.append(l1.at(i));
    }

    for(int i = zipped; i < l2.size(); ++i) {
        ret.append(l2.at(i));
    }

    return ret;
}

static int
lineNumber(const QString &text, int length)
{
    length = qMin(text.length(), length);
    int lineNumber = 1;

    for(int i = 0; i < length; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++lineNumber;
        }
    }

    return lineNumber;
}

static QString
renameAllVariables(const QString &query, QRegExp regexp,
                   const QString &namePattern, QHash<QString, QString> &newNames)
{
    QRegExp sanityCheck(QRegExp::escape(namePattern).replace(QLatin1String("%1"),
                                                             QLatin1String("\\d+")));
    QStringList originalVariables;
    QStringList renamedVariables;
    bool badQuery = false;
    int varCounter = 0;

    // check for variable names conflicting with the name generator pattern
    for(int i = 0; (i = query.indexOf(sanityCheck, i)) != -1; i += sanityCheck.matchedLength()) {
        qctWarn(QString::fromLatin1("Name conflicts with generator pattern at line %1: %2").
                arg(QString::number(lineNumber(query, i)), sanityCheck.cap(0)));
        badQuery = true;
    }

    if (badQuery) {
        qctFail("Rejecting bad query");
    }

    // extract original variable names
    for(int i = 0; (i = query.indexOf(regexp, i)) != -1; i += regexp.matchedLength()) {
        originalVariables += regexp.cap(1);
    }

    // substitute variable names
    foreach(const QString &oldvar, originalVariables) {
        QString newvar = newNames.value(oldvar);

        if (newvar.isEmpty()) {
            newvar = namePattern.arg(varCounter++);
            newNames.insert(oldvar, newvar);
        }

        renamedVariables.append(newvar);
    }

    return zip(query.split(regexp), renamedVariables).join(QLatin1String(""));
}

static QString
renameAllVariables(const QString &query, const QRegExp &regexp, const QString &namePattern)
{
    QHash<QString, QString> newNames;
    return renameAllVariables(query, regexp, namePattern, newNames);
}

static QString normalizeQuery(QString query)
{
    static QRegExp emptyWhereClause(QLatin1String("WHERE\\s+\\{\\s*\\}"), Qt::CaseInsensitive);
    static QRegExp comments(QLatin1String("#.*\n"));
    static QRegExp curlies(QLatin1String("([{}])"));
    static QRegExp whitespace(QLatin1String("\\s+"));
    static QRegExp braces(QLatin1String("\\s*([\\(\\)])\\s*"));
    static QRegExp trailingEmptyStatement(QLatin1String("\\.\\s*\\}"));
    static QRegExp dotsBeforeOptional(QLatin1String("\\}\\s*\\.\\s*OPTIONAL")); // This is a Cubi "bug"
    static QRegExp dotsBeforeFilter(QLatin1String("\\}\\s*\\.\\s*FILTER")); // This is a Cubi "bug"
    static QRegExp commas(QLatin1String(",([^\\s])"));

    comments.setMinimal(true);

    return (qctReduceIris(query).
            replace(emptyWhereClause, QLatin1String("")).
            replace(comments, QLatin1String("\n")).
            replace(dotsBeforeFilter, QLatin1String("} FILTER")).
            replace(dotsBeforeOptional, QLatin1String("} OPTIONAL")).
            replace(curlies, QLatin1String(" \\1 ")).
            replace(braces, QLatin1String(" \\1 ")).
            replace(whitespace, QLatin1String(" ")).
            replace(trailingEmptyStatement, QLatin1String("}")).
            replace(commas, QLatin1String(", \\1")).
            trimmed());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

ut_qtcontacts_trackerplugin_querybuilder::ut_qtcontacts_trackerplugin_querybuilder(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)
    , mDebugFlags()
{
    mDebugFlags = QProcessEnvironment::systemEnvironment().
                  value(QLatin1String("DEBUG")).trimmed().toLower().
                  split(QRegExp(QLatin1String("\\s*(,|\\s)\\s*")));

    mShowContact = mDebugFlags.contains(QLatin1String("contact"));
    mShowQuery = mDebugFlags.contains(QLatin1String("query"));
    mShowSchema = mDebugFlags.contains(QLatin1String("schema"));
    mSilent = mDebugFlags.contains(QLatin1String("silent"));
    mWordDiff = mDebugFlags.contains(QLatin1String("wdiff"));
}

QMap<QString, QString>
ut_qtcontacts_trackerplugin_querybuilder::makeEngineParams() const
{
    QMap<QString, QString> params;

    params.insert(QLatin1String("avatar-types"), QLatin1String("all"));

    return params;
}

void
ut_qtcontacts_trackerplugin_querybuilder::initTestCase()
{
    changeSetting(QctSettings::NumberMatchLengthKey, 7);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static bool
showWordDiff(const QByteArray &current, const QByteArray &expected)
{
    QTemporaryFile currentFile, expectedFile;

    if (not currentFile.open() || currentFile.write(current) != current.length()) {
        qWarning() << "Writing current query failed:" << currentFile.errorString();
        return false;
    }

    if (not expectedFile.open() || expectedFile.write(expected) != expected.length()) {
        qWarning() << "Writing expected query failed:" << expectedFile.errorString();
        return false;
    }

    currentFile.close();
    expectedFile.close();

    QProcess wdiff;

    wdiff.start(QLatin1String("wdiff"),
                QStringList() << QLatin1String("-n")
                              << expectedFile.fileName()
                              <<  currentFile.fileName(),
                QFile::ReadOnly);

    if (not wdiff.waitForFinished()) {
        qWarning() << "Starting wdiff failed:" << wdiff.errorString();
        return false;
    }

    qWarning() << "generated and expected query differ:";

    QTextStream(stdout) << QString::fromLocal8Bit(wdiff.readAll()).
                           replace(QLatin1String("[-"), QLatin1String("\033[31m")).
                           replace(QLatin1String("{+"), QLatin1String("\033[32m")).
                           replace(QLatin1String("+}"), QLatin1String("\033[0m")).
                           replace(QLatin1String("-]"), QLatin1String("\033[0m"))
                        << QLatin1Char('\n');

    return true;
}

static bool
showWordDiff(const QString &current, const QString &expected)
{
    return showWordDiff(current.toLocal8Bit(), expected.toLocal8Bit());
}

bool
ut_qtcontacts_trackerplugin_querybuilder::verifyQuery(QString actualQuery, QString expectedQuery,
                                                      VerifyQueryFlags flags)
{
    if (not mSilent && mShowQuery) {
        qDebug() << qctReduceIris(actualQuery);
    }

    QHash<QString, QString> nameSubst;

    if (flags & RenameAnonymousVariables) {
        QRegExp anonymousRegExp(QLatin1String("(_:\\w+)"));
        const QString anonymousNamePattern = QLatin1String("_:%1");

        actualQuery = renameAllVariables(actualQuery, anonymousRegExp, anonymousNamePattern);
        expectedQuery = renameAllVariables(expectedQuery, anonymousRegExp, anonymousNamePattern,
                                           nameSubst);
    }

    if (flags & RenameNamedVariables) {
        QRegExp variableRegExp(QLatin1String("(\\?\\w+)"));
        const QString variableNamePattern = QLatin1String("?t%1");

        actualQuery = renameAllVariables(actualQuery, variableRegExp, variableNamePattern);
        expectedQuery = renameAllVariables(expectedQuery, variableRegExp, variableNamePattern,
                                           nameSubst);
    }

    if (flags & RenameIris) {
        QRegExp contactIriRegExp(QLatin1String("(<(?:contact|urn:uuid):[^>]+>)"));
        const QString dummyIriPattern = QLatin1String("<iri%1>");

        actualQuery = renameAllVariables(actualQuery, contactIriRegExp, dummyIriPattern);
        expectedQuery = renameAllVariables(expectedQuery, contactIriRegExp, dummyIriPattern);
    }

    const QHash<QString, QString>::ConstIterator end = nameSubst.constEnd();
    for(QHash<QString, QString>::ConstIterator it = nameSubst.constBegin(); it != end; ++it) {
        actualQuery = actualQuery.replace(it.value(), it.key());
        expectedQuery = expectedQuery.replace(it.value(), it.key());
    }

    actualQuery = normalizeQuery(actualQuery);
    expectedQuery = normalizeQuery(expectedQuery);

    QRegExp referencePattern(QRegExp::escape(expectedQuery).
                             replace(QLatin1String("<placeholder:guid>"),
                                     QLatin1String("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-"
                                                   "[0-9a-f]{4}-[0-9a-f]{12}")),
                             Qt::CaseSensitive);

    if (not referencePattern.exactMatch(actualQuery)) {
        QString expected = expectedQuery;
        QString actual = actualQuery;

        expected.replace(QLatin1String("<placeholder:guid>"),
                         QLatin1String("00000000-0000-0000-0000-000000000000"));

        if (mShowQuery) {
            qDebug() << qctReduceIris(actualQuery);
        }

        if (not mWordDiff || not showWordDiff(actual, expected)) {
            if (mSilent) {
                int i = referencePattern.matchedLength() - 3;

                if (i > 0) {
                    expected = QLatin1String("[...]") + expected.mid(i);
                    actual = QLatin1String("[...]") + actual.mid(i);
                }

                if (expected.length() > 140) {
                    expected = expected.left(135) + QLatin1String("[...]");
                }

                if (actual.length() > 140) {
                    actual = actual.left(135) + QLatin1String("[...]");
                }
            }

            qWarning()
                    << "\n       : generated query:" << actual
                    << "\n       :  expected query:" << expected;
        }

        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
ut_qtcontacts_trackerplugin_querybuilder::verifyContactDetail(int referenceId,
                                                              const QString &detailName)
{
    // build fetch request for selected detail ///////////////////////////////////////////////////
    QContactFetchHint fetchHint;

    fetchHint.setDetailDefinitionsHint(QStringList() << detailName);
    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships);

    QContactFetchRequest request;
    request.setFetchHint(fetchHint);
    QctRequestExtensions::get(&request)->setNameOrder(QContactDisplayLabel__FieldOrderNone);

    QTrackerContactFetchRequest requestImpl(&request, engine());

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        const QTrackerContactDetail *const detail = engine()->schema(contactType).detail(detailName);

        // don't test query builder for entirly unsupported details
        if (0 == detail) {
            continue;
        }

        // build query for selected detail
        QContactManager::Error error = QContactManager::NoError;
        Select query = requestImpl.query(contactType, error);
        QCOMPARE(QContactManager::NoError, error);

        // find and read reference query
        const QString referenceFile = QString::fromLatin1("%1-%2-%3.rq").
                                      arg(QString::number(referenceId),
                                          contactType, detailName);

        const QString referenceQuery(loadReferenceFile(referenceFile));
        QVERIFY2(not referenceQuery.isEmpty(), qPrintable(referenceFile));

        if (referenceQuery.startsWith(QLatin1String("SKIP THIS!"))) {
            QSKIP(qPrintable(referenceFile + QLatin1String(": ") +
                             referenceQuery.mid(10).replace(QRegExp(QLatin1String("\\s+|\n")),
                                                            QLatin1String(" ")).
                             trimmed()),
                  SkipSingle);
        }

        // compare with reference query
        QVERIFY2(verifyQuery(query.sparql(Options::DefaultSparqlOptions |
                                          Options::PrettyPrint), referenceQuery),
                 qPrintable(referenceFile));
    }
}

void ut_qtcontacts_trackerplugin_querybuilder::testAddressDetail()
{
    verifyContactDetail(101, QContactAddress::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testAnniversaryDetail()
{
    verifyContactDetail(102, QContactAnniversary::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testBirthdayDetail()
{
    verifyContactDetail(103, QContactBirthday::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testEmailAddressDetail()
{
    verifyContactDetail(104, QContactEmailAddress::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testFamilyDetail()
{
    verifyContactDetail(105, QContactFamily::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testFavoriteDetail()
{
    verifyContactDetail(106, QContactFavorite::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testGenderDetail()
{
    verifyContactDetail(107, QContactGender::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testGeoLocationDetail()
{
    verifyContactDetail(108, QContactGeoLocation::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testGuidDetail()
{
    verifyContactDetail(109, QContactGuid::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testHobbyDetail()
{
    verifyContactDetail(110, QContactHobby::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testNameDetail()
{
    verifyContactDetail(111, QContactName::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testNicknameDetail()
{
    verifyContactDetail(112, QContactNickname::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testNoteDetail()
{
    verifyContactDetail(113, QContactNote::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testOnlineAccountDetail()
{
    verifyContactDetail(114, QContactOnlineAccount::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testOnlineAvatarDetail()
{
    verifyContactDetail(115, QContactOnlineAvatar::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testOrganizationDetail()
{
    verifyContactDetail(116, QContactOrganization::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testPersonalAvatarDetail()
{
    verifyContactDetail(117, QContactPersonalAvatar::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testPhoneNumberDetail()
{
    verifyContactDetail(118, QContactPhoneNumber::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testPresenceDetail()
{
    verifyContactDetail(119, QContactPresence::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testRelevanceDetail()
{
    verifyContactDetail(120, QContactRelevance::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testRingtoneDetail()
{
    verifyContactDetail(121, QContactRingtone::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testSocialAvatarDetail()
{
    verifyContactDetail(122, QContactSocialAvatar::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testSyncTargetDetail()
{
    verifyContactDetail(123, QContactSyncTarget::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testTagDetail()
{
    verifyContactDetail(124, QContactTag::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testTimestampDetail()
{
    verifyContactDetail(125, QContactTimestamp::DefinitionName);
}

void ut_qtcontacts_trackerplugin_querybuilder::testUrlDetail()
{
    verifyContactDetail(126, QContactUrl::DefinitionName);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void
setFieldType(QContactDetailDefinition &d, const QString &key, QVariant::Type type)
{
    QContactDetailFieldDefinition f = d.fields().value(key);

    f.setDataType(type);

    d.removeField(key);
    d.insertField(key, f);
}

static QContactDetailDefinitionMap
makeExpectedSchema(const QString &contactType)
{
    QContactDetailFieldDefinition authStatusField;
    authStatusField.setAllowableValues(QVariantList() <<
                                       QContactPresence__AuthStatusRequested <<
                                       QContactPresence__AuthStatusYes <<
                                       QContactPresence__AuthStatusNo);
    authStatusField.setDataType(QVariant::String);

    // get the schema definitions, and modify them to match our capabilities.

    QContactDetailDefinitionMap definitions;

    definitions = QContactManagerEngine::
            schemaDefinitions(QTrackerContactDetailSchema::Version).
            value(contactType);

    definitions.detach();

    // modification: some details are unique

    definitions[QContactFavorite::DefinitionName].setUnique(true);
    definitions[QContactGender::DefinitionName].setUnique(true);
    definitions[QContactGeoLocation::DefinitionName].setUnique(true);
    definitions[QContactHobby::DefinitionName].setUnique(true);
    definitions[QContactGuid::DefinitionName].setUnique(true);
    definitions[QContactName::DefinitionName].setUnique(true);
    definitions[QContactNickname::DefinitionName].setUnique(true);
    definitions[QContactRingtone::DefinitionName].setUnique(true);

    // global modification: cleanup context field

    QSet<QString> noContextDetails;

    noContextDetails
            << QContactAnniversary::DefinitionName
            << QContactAvatar::DefinitionName;

    if (QContactType::TypeContact == contactType) {
        noContextDetails
                << QContactHobby::DefinitionName
                << QContactFavorite::DefinitionName
                << QContactNote::DefinitionName
                << QContactOrganization::DefinitionName
                << QContactTag::DefinitionName;
    } else {
        noContextDetails += definitions.keys().toSet();
    }

    // cannot use foreach() as it only gives access to a copy to the current element
    for(QContactDetailDefinitionMap::Iterator i =
        definitions.begin(); i != definitions.end(); ++i) {
        QContactDetailDefinition &d = i.value();

        if (d.isUnique() || noContextDetails.contains(d.name())) {
            d.removeField(QContactDetail::FieldContext);
        } else {
            static const QString contextHome = QContactDetail::ContextHome;
            static const QString contextWork = QContactDetail::ContextWork;

            QContactDetailFieldDefinitionMap fields(d.fields());

            if (not fields.contains(QContactDetail::FieldContext))
                continue;

            QContactDetailFieldDefinition f(fields[QContactDetail::FieldContext]);
            f.setAllowableValues(QVariantList() << contextHome << contextWork);
            d.removeField(QContactDetail::FieldContext);
            d.insertField(QContactDetail::FieldContext, f);
        }
    }

    // QContactAnniversary: change type of OriginalDate field to QDateTime
    {
        QContactDetailDefinition &d = definitions[QContactAnniversary::DefinitionName];
        setFieldType(d, QContactAnniversary::FieldOriginalDate, QVariant::DateTime);
    }

    // QContactBirthay: change type of Birthday field to QDateTime
    {
        QContactDetailDefinition &d = definitions[QContactBirthday::DefinitionName];
        setFieldType(d, QContactBirthday::FieldBirthday, QVariant::DateTime);
    }

    // QContactGeoLocation: remove unsupported fields
    {
        QContactDetailDefinition &d(definitions[QContactGeoLocation::DefinitionName]);
        d.removeField(QContactGeoLocation::FieldAltitudeAccuracy);
        d.removeField(QContactGeoLocation::FieldAccuracy);
        d.removeField(QContactGeoLocation::FieldHeading);
        d.removeField(QContactGeoLocation::FieldSpeed);
        d.removeField(QContactGeoLocation::FieldContext);
    }

    // QContactGlobalPresence: remove unsupported fields
    {
        QContactDetailDefinition &d(definitions[QContactGlobalPresence::DefinitionName]);
        d.removeField(QContactPresence::FieldPresenceStateImageUrl);
        d.removeField(QContactPresence::FieldPresenceStateText);
    }

    // QContactOnlineAccount: remove unsupported fields, restrict capabilities
    {
        QContactDetailDefinition &d(definitions[QContactOnlineAccount::DefinitionName]);
        QContactDetailFieldDefinition f(d.fields().value(QContactOnlineAccount::FieldCapabilities));

        f.setAllowableValues(QVariantList() <<
                             QContactOnlineAccount__CapabilityTextChat <<
                             QContactOnlineAccount__CapabilityMediaCalls <<
                             QContactOnlineAccount__CapabilityAudioCalls <<
                             QContactOnlineAccount__CapabilityVideoCalls <<
                             QContactOnlineAccount__CapabilityUpgradingCalls <<
                             QContactOnlineAccount__CapabilityFileTransfers <<
                             QContactOnlineAccount__CapabilityStreamTubes <<
                             QContactOnlineAccount__CapabilityDBusTubes);

        d.removeField(QContactOnlineAccount::FieldCapabilities);
        d.insertField(QContactOnlineAccount::FieldCapabilities, f);
        setFieldType(d, QContactOnlineAccount__FieldAccountPath, QVariant::String);
        setFieldType(d, QContactOnlineAccount::FieldProtocol, QVariant::String);
    }

    // QContactOrganization: change type of department field
    {
        QContactDetailDefinition &d(definitions[QContactOrganization::DefinitionName]);
        setFieldType(d, QContactOrganization::FieldAssistantName, QVariant::String);
        setFieldType(d, QContactOrganization::FieldDepartment, QVariant::String);
        setFieldType(d, QContactOrganization::FieldRole, QVariant::String);
    }

    // QContactPhoneNumber: remove unsupported subtypes
    {
        QContactDetailDefinition &d(definitions[QContactPhoneNumber::DefinitionName]);
        QContactDetailFieldDefinition f(d.fields().value(QContactPhoneNumber::FieldSubTypes));
        QVariantList allowableValues(f.allowableValues());
        d.removeField(QContactPhoneNumber::FieldSubTypes);

        allowableValues.removeOne(QLatin1String(QContactPhoneNumber::SubTypeAssistant));
        allowableValues.removeOne(QLatin1String(QContactPhoneNumber::SubTypeDtmfMenu));
        allowableValues.removeOne(QLatin1String(QContactPhoneNumber::SubTypeLandline));

        f.setAllowableValues(allowableValues);
        d.insertField(QContactPhoneNumber::FieldSubTypes, f);
    }

    // QContactPresence: remove unsupported fields
    {
        QContactDetailDefinition &d(definitions[QContactPresence::DefinitionName]);
        d.removeField(QContactPresence::FieldPresenceStateImageUrl);
        d.removeField(QContactPresence::FieldPresenceStateText);
        d.insertField(QContactPresence__FieldAuthStatusFrom, authStatusField);
        d.insertField(QContactPresence__FieldAuthStatusTo, authStatusField);
    }

    // QContactTimestamp: add accessed timestamp
    {
        QContactDetailDefinition &d(definitions[QContactTimestamp::DefinitionName]);
        setFieldType(d, QContactTimestamp__FieldAccessedTimestamp, QVariant::DateTime);
    }

    {
        QContactDetailDefinition d;
        setFieldType(d, QContactRelevance::FieldRelevance, QVariant::Double);
        d.setName(QContactRelevance::DefinitionName);
        d.setUnique(true);

        definitions.insert(d.name(), d);
    }

    if (QContactType::TypeGroup == contactType) {
        definitions.remove(QContactFamily::DefinitionName);
        definitions.remove(QContactGender::DefinitionName);
        definitions.remove(QContactHobby::DefinitionName);
        definitions.remove(QContactName::DefinitionName);
        definitions.remove(QContactOrganization::DefinitionName);
    }

    return definitions;
}

static bool variantLessByString(const QVariant &v1, const QVariant &v2)
{
    if (v1.type() < v2.type()) {
        return true;
    }

    Q_ASSERT(v1.canConvert(QVariant::String));
    Q_ASSERT(v2.canConvert(QVariant::String));

    return (v1.toString() < v2.toString());
}

void ut_qtcontacts_trackerplugin_querybuilder::testDetailSchema_data()
{
    QTest::addColumn<QString>("contactType");

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        QTest::newRow(qPrintable(contactType)) << contactType;
    }
}

void ut_qtcontacts_trackerplugin_querybuilder::testDetailSchema()
{
    QFETCH(QString, contactType);

    QContactDetailDefinitionMap actualSchema = engine()->schema(contactType).detailDefinitions();
    QContactDetailDefinitionMap expectedSchema = makeExpectedSchema(contactType);

    for(QContactDetailDefinitionMap::ConstIterator detailIter =
        actualSchema.constBegin(); detailIter != actualSchema.constEnd(); ++detailIter) {
        const QContactDetailDefinition &actualDetail = *detailIter;

        if (mShowSchema) {
            qDebug() << actualDetail;
        }

        // check if the detail is known
        QVERIFY2(not actualDetail.name().isEmpty(), qPrintable(detailIter.key()));

        const QContactDetailDefinitionMap::Iterator expectedDetail =
                expectedSchema.find(actualDetail.name());

        QVERIFY2(expectedDetail != expectedSchema.end(),
                 qPrintable(actualDetail.name()));

        // compare basic properties
        QCOMPARE(actualDetail.name(), expectedDetail->name());

        if (expectedDetail->isUnique()) {
            QVERIFY2(actualDetail.isUnique(), qPrintable(actualDetail.name()));
        } else {
            QVERIFY2(not actualDetail.isUnique(), qPrintable(actualDetail.name()));
        }

        // compare field properties
        const QContactDetailFieldDefinitionMap &actualFields(actualDetail.fields());
        QContactDetailFieldDefinitionMap expectedFields(expectedDetail->fields());

        const QContactDetailFieldDefinitionMap::ConstIterator end(actualFields.constEnd());
        for(QContactDetailFieldDefinitionMap::ConstIterator f(actualFields.constBegin()); f != end; ++f) {
            const QContactDetailFieldDefinitionMap::Iterator expected(expectedFields.find(f.key()));

            // check that the field is known
            QVERIFY2(expected != expectedFields.end(),
                     qPrintable(actualDetail.name() +
                                QLatin1String(": unexpected ") + f.key() +
                                QLatin1String(" field")));

            // check the field's datatype
            QVERIFY2(f->dataType() == expected->dataType(),
                     qPrintable(actualDetail.name() + QLatin1String(": ") +
                                f.key() + QLatin1String(", ") +
                                QLatin1String("actual type: ") +
                                QLatin1String(QVariant::typeToName(f->dataType())) +
                                QLatin1String(", ") +
                                QLatin1String("expected type: ") +
                                QLatin1String(QVariant::typeToName(expected->dataType()))));

            // compare allowable values
            QVariantList actualValues(f->allowableValues());
            QVariantList expectedValues(expected->allowableValues());

            // make sure expected and actually value list have same order
            qSort(actualValues.begin(), actualValues.end(), variantLessByString);
            qSort(expectedValues.begin(), expectedValues.end(), variantLessByString);

            if (actualValues != expectedValues) {
                qWarning() << "actual allowable values:" << actualValues;
                qWarning() << "expected allowable values:" << expectedValues;

                QFAIL(qPrintable(actualDetail.name() + QLatin1String(": ") +
                                 f.key() + QLatin1String(": Allowable values are not the same")));
            }

            // implementation is proper, remove from list
            expectedFields.erase(expected);
        }

        // list of expected fields shall be empty now
        QVERIFY2(expectedFields.isEmpty(),
                 qPrintable(actualDetail.name() + QLatin1String(": ") +
                            QStringList(expectedFields.keys()).join(QLatin1String(", "))));

        // implementation is proper, remove from list
        expectedSchema.erase(expectedDetail);
    }

    // list of expected detail shall be empty now
    QVERIFY2(expectedSchema.isEmpty(),
             qPrintable(QStringList(expectedSchema.keys()).join(QLatin1String(", "))));
}

void ut_qtcontacts_trackerplugin_querybuilder::testDetailCoverage()
{
    QSet<QString> definitionNames;
    QStringList missingDetails;

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        const QTrackerContactDetailSchema& schema = engine()->schema(contactType);

        foreach(const QString &name, schema.detailDefinitions().keys()) {
            if (not schema.isSyntheticDetail(name)) {
                definitionNames += name;
            }
        }
    }

    foreach(const QString &name, definitionNames) {
        const QString slotName = QLatin1String("test") % name % QLatin1String("Detail()");
        const int slotId = metaObject()->indexOfSlot(qPrintable(slotName));

        if (slotId < 0) {
            missingDetails += name;
            continue;
        }

        const QMetaMethod slotMethod = metaObject()->method(slotId);
        QVERIFY2(QMetaMethod::Private == slotMethod.access(), qPrintable(slotName));
    }

    QVERIFY2(missingDetails.isEmpty(), qPrintable(missingDetails.join(QLatin1String(", "))));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testAllDetailsPossible_data()
{
    QTest::addColumn<QString>("contactType");
    QTest::addColumn<QString>("definitionName");
    QTest::addColumn<QString>("fieldName");

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        const QTrackerContactDetailSchema &schema = engine()->schema(contactType);

        foreach(const QTrackerContactDetail &detail, schema.details()) {
            foreach(const QTrackerContactDetailField &field, detail.fields()) {
                if (not field.hasPropertyChain()) {
                    continue;
                }

                const QString label =
                        contactType % QLatin1String("/") %
                        detail.name() % QLatin1String("/") % field.name();
                QTest::newRow(qPrintable(label)) << contactType << detail.name() << field.name();
            }
        }
    }
}

QSet<QUrl>
ut_qtcontacts_trackerplugin_querybuilder::
inheritedClassIris(const QTrackerContactDetailSchema &schema) const
{
    QSet<QUrl> inheritedIris;

    inheritedIris += nie::InformationElement::iri();
    inheritedIris += rdfs::Resource::iri();

    for(int i = 1; i <= 3; ++i) {
        Variable contactClass(QLatin1String("subClass"));
        Variable subClass = contactClass, baseClass;
        ValueList contactClassIris;

        foreach(const QUrl &iri, schema.contactClassIris()) {
            contactClassIris.addValue(ResourceValue(iri.toString()));
            inheritedIris += iri;
        }

        Select query;

        query.setFilter(Functions::in.apply(contactClass, contactClassIris));

        for(int j = 0; j < i; ++j) {
            baseClass = Variable(QString::fromLatin1("baseClass%1").arg(j));
            query.addRestriction(subClass, rdfs::subClassOf::resource(), baseClass);
            subClass = baseClass;
        }

        query.addProjection(baseClass);

        QScopedPointer<QSparqlResult> result(executeQuery(query.sparql(),
                                                          QSparqlQuery::SelectStatement));

        if (not result->next()) {
            break;
        }

        do {
            inheritedIris += result->current().value(0).toUrl();
        } while(result->next());
    }

    return inheritedIris;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testAllDetailsPossible()
{
    QFETCH(QString, contactType);
    QFETCH(QString, definitionName);
    QFETCH(QString, fieldName);

    const QTrackerContactDetailSchema &schema = engine()->schema(contactType);
    const QTrackerContactDetail *const detail = schema.detail(definitionName);
    const QSet<QUrl> permittedClassIris = inheritedClassIris(schema);

    foreach(const QTrackerContactDetailField &field, detail->fields()) {
        if (field.name() != fieldName) {
            continue;
        }

        const QUrl &domain = field.propertyChain().first().domainIri();

        QVERIFY2(permittedClassIris.contains(domain),
                 qPrintable(domain.toString()));

        break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
ut_qtcontacts_trackerplugin_querybuilder::testFetchRequest()
{
    // insert reference contacts into tracker database from Turtle file
    const QStringList subjects = loadRawContacts();
    QVERIFY(not subjects.isEmpty());

    // load reference contacts from XML file
    QList<QContact> referenceContacts = loadReferenceContacts(ResolveReferenceContactIris);
    QCOMPARE(referenceContacts.count(), subjects.count());

    // fetch the just inserted contacts
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(definitionNames(referenceContacts).toList());
    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships);

    QContactLocalIdFilter filter;
    filter.setIds(localContactIds(referenceContacts));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList fetchedContacts = engine()->contacts(filter, NoSortOrders, fetchHint, &error);
    QCOMPARE(QContactManager::NoError, error);

    // compare fetched contacts with reference contacts from XML file
    verifyContacts(fetchedContacts, referenceContacts);
}

static QList<QContactSortOrder>
testSorting()
{
    QList<QContactSortOrder> sorting;

    sorting += QContactSortOrder();
    sorting.last().setDetailDefinitionName(QContactName::DefinitionName,
                                           QContactName::FieldFirstName);
    sorting.last().setBlankPolicy(QContactSortOrder::BlanksFirst);
    sorting.last().setCaseSensitivity(Qt::CaseInsensitive);
    sorting.last().setDirection(Qt::AscendingOrder);

    sorting += QContactSortOrder();
    sorting.last().setDetailDefinitionName(QContactName::DefinitionName,
                                           QContactName::FieldLastName);
    sorting.last().setBlankPolicy(QContactSortOrder::BlanksFirst);
    sorting.last().setCaseSensitivity(Qt::CaseInsensitive);
    sorting.last().setDirection(Qt::DescendingOrder);

    return sorting;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testFetchRequestQuery_data()
{
    QTest::addColumn<QString>("contactType");

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        QTest::newRow(qPrintable(contactType)) << contactType;
    }
}

void
ut_qtcontacts_trackerplugin_querybuilder::testFetchRequestQuery()
{
    QContactFetchRequest request;
    request.setSorting(testSorting());

    QTrackerContactFetchRequest worker(&request, engine());

    QFETCH(QString, contactType);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    Select query = worker.query(contactType, error);
    QCOMPARE(QContactManager::NoError, error);

    const QString referenceFile = QString::fromLatin1("100-%1.rq").arg(contactType);
    const QString referenceQuery = loadReferenceFile(referenceFile);
    QVERIFY2(not referenceQuery.isEmpty(), qPrintable(referenceFile));
    QVERIFY2(verifyQuery(query.sparql(Options::DefaultSparqlOptions |
                                      Options::PrettyPrint), referenceQuery),
             qPrintable(referenceFile));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testLocalIdFetchRequestQuery_data()
{
    QContactDetailFilter nicknameFilter;
    nicknameFilter.setDetailDefinitionName(QContactNickname::DefinitionName);
    nicknameFilter.setValue(QLatin1String("Havoc"));

    QContactDetailFilter noteFilter;
    noteFilter.setDetailDefinitionName(QContactNote::DefinitionName);
    noteFilter.setValue(QLatin1String("Chaos"));

    QContactDetailFilter genderFilter;
    genderFilter.setDetailDefinitionName(QContactGender::DefinitionName);
    genderFilter.setValue(QContactGender::GenderFemale);

    QTest::addColumn<QContactFilter>("filter");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("verifySorting");

    static const bool verifySorting = true;

    QTest::newRow("original-or-generic")
            << QContactFilter(QContactUnionFilter() << nicknameFilter << noteFilter)
            << "250-localIdFetchRequest-1.rq" << verifySorting;
    QTest::newRow("original-and-generic")
            << QContactFilter(QContactIntersectionFilter() << nicknameFilter << noteFilter)
            << "250-localIdFetchRequest-2.rq" << not verifySorting;
    QTest::newRow("original-both")
            << QContactFilter(nicknameFilter)
            << "250-localIdFetchRequest-3.rq" << not verifySorting;
    QTest::newRow("original-one")
            << QContactFilter(genderFilter)
            << "250-localIdFetchRequest-4.rq" << not verifySorting;
    QTest::newRow("generic-only")
            << QContactFilter(noteFilter)
            << "250-localIdFetchRequest-5.rq" << not verifySorting;
    QTest::newRow("original-nastyness")
            << QContactFilter(QContactUnionFilter() << nicknameFilter << genderFilter)
            << "250-localIdFetchRequest-6.rq" << not verifySorting;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testLocalIdFetchRequestQuery()
{
    QFETCH(QContactFilter, filter);
    QFETCH(bool, verifySorting);
    QFETCH(QString, fileName);

    QContactLocalIdFetchRequest request;
    request.setFilter(filter);

    if (verifySorting) {
        request.setSorting(testSorting());
    }

    QContactManager::Error error;
    bool canSort = false;
    QTrackerContactIdFetchRequest worker(&request, engine());
    QString query = worker.buildQuery(error, canSort);
    QCOMPARE(QContactManager::NoError, error);

    const QString referenceQuery = loadReferenceFile(fileName);
    QVERIFY2(verifyQuery(query, referenceQuery), qPrintable(fileName));
}

static QContactLocalIdList
wellKnownContactIds()
{
    return QContactLocalIdList() << 225543300 << 58390905 << 55500278 << 254908088;
}


template<class SetFilter> static SetFilter
makeSetFilter(const QContactLocalIdList &localIds)
{
    QContactLocalIdFilter localIds1;
    localIds1.setIds(QContactLocalIdList() <<
                     localIds[0] << localIds[1] << localIds[2]);

    QContactLocalIdFilter localIds2;
    localIds2.setIds(QContactLocalIdList() <<
                     localIds[0] << localIds[2] << localIds[3]);

    SetFilter filter;

    filter.append(localIds1);
    filter.append(localIds2);

    return filter;
}

template<class SetFilter> static SetFilter
makeSetFilter(const QStringList &subjects)
{
    QctTrackerIdResolver resolver(subjects);

    if (not resolver.lookupAndWait()) {
        return QContactInvalidFilter();
    }

    return makeSetFilter<SetFilter>(resolver.trackerIds());
}

void
ut_qtcontacts_trackerplugin_querybuilder::testContactLocalIdFilter_data()
{
    QTest::addColumn<QContactLocalIdList>("localIds");

    const QStringList subjects = loadRawContacts();
    QVERIFY(not subjects.isEmpty());

    const QContactLocalIdList localIds = resolveTrackerIds(subjects);
    QCOMPARE(localIds.count(), subjects.count());


    for(int i = 0; i < subjects.count(); ++i) {
        const QString name(QString::number(i + 1));
        QTest::newRow(qPrintable(name)) << localIds.mid(0, i + 1);
    }

    QCOMPARE(localIds.count(), 6);
}

void
ut_qtcontacts_trackerplugin_querybuilder::testContactLocalIdFilter()
{
    QFETCH(QContactLocalIdList, localIds);

    QContactLocalIdFilter filter;
    filter.setIds(localIds);

    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(QStringList() << QContactName::DefinitionName);

    QContactFetchRequest request;
    QctRequestExtensions::get(&request)->setNameOrder(QContactDisplayLabel__FieldOrderNone);
    request.setFetchHint(fetchHint);
    request.setFilter(filter);

    if (localIds.isEmpty()) {
        QTest::ignoreMessage(QtWarningMsg, "Bad arguments for \"QContactFilter::LocalIdFilter\" ");
    }

    const bool started = engine()->startRequest(&request);

    if (localIds.isEmpty()) {
        QCOMPARE(request.error(), QContactManager::BadArgumentError);
        QVERIFY2(not started, qPrintable(QString::number(localIds.count())));
        QVERIFY2(request.contacts().isEmpty(), qPrintable(QString::number(localIds.count())));
    } else {
        QCOMPARE(request.error(), QContactManager::NoError);
        QVERIFY2(started, qPrintable(QString::number(localIds.count())));

        const bool finished = engine()->waitForRequestFinishedImpl(&request, 0);
        QCOMPARE(request.error(), QContactManager::NoError);
        QVERIFY2(finished, qPrintable(QString::number(localIds.count())));

        const QString fileName = QLatin1String("300-localContactIdFilter-%1.xml");
        verifyContacts(request.contacts(), fileName.arg(localIds.count()));
        CHECK_CURRENT_TEST_FAILED;
    }
}

void
ut_qtcontacts_trackerplugin_querybuilder::verifyFilter(const QContactFilter &filter,
                                                       const QString &referenceFileName)
{
    QVERIFY(not loadRawContacts().isEmpty());

    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(QStringList() << QContactName::DefinitionName);

    QContactFetchRequest request;
    QctRequestExtensions::get(&request)->setNameOrder(QContactDisplayLabel__FieldOrderNone);
    request.setFetchHint(fetchHint);
    request.setFilter(filter);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QCOMPARE(request.error(), QContactManager::NoError);
    verifyContacts(request.contacts(), referenceFileName);
}

void
ut_qtcontacts_trackerplugin_querybuilder::verifyFilterQuery(const QContactFilter &filter,
                                                            const QString &referenceFileName,
                                                            const QString &contactType)
{
    QStringList definitionNames; (QStringList() << QContactName::DefinitionName);
    if (contactType == QContactType::TypeContact) {
        definitionNames << QContactName::DefinitionName;
    } else if (contactType == QContactType::TypeGroup) {
        definitionNames << QContactNickname::DefinitionName;
    }

    verifyFilterQuery(filter, referenceFileName, definitionNames, contactType);
}

void
ut_qtcontacts_trackerplugin_querybuilder::verifyFilterQuery(const QContactFilter &filter,
                                                            const QString &referenceFileName,
                                                            QStringList definitionNames,
                                                            const QString &contactType)
{
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(definitionNames);
    fetchHint.setOptimizationHints(QContactFetchHint::NoRelationships);

    QContactFetchRequest request;
    QctRequestExtensions::get(&request)->setNameOrder(QContactDisplayLabel__FieldOrderNone);
    request.setFetchHint(fetchHint);
    request.setFilter(filter);

    QTrackerContactFetchRequest requestImpl(&request, engine());

    QContactManager::Error error = QContactManager::NoError;
    const Select &query = requestImpl.query(contactType, error);
    QCOMPARE(error, QContactManager::NoError);

    const QString referenceQuery(loadReferenceFile(referenceFileName));
    QVERIFY2(not referenceQuery.isEmpty(), qPrintable(referenceFileName));
    QVERIFY2(verifyQuery(query.sparql(Options::DefaultSparqlOptions |
                                      Options::PrettyPrint), referenceQuery),
             qPrintable(referenceFileName));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testContactLocalIdFilterQuery()
{
    QContactLocalIdFilter filter;
    filter.setIds(QList<QContactLocalId>() << 1 << 7 << 23 << 42);
    verifyFilterQuery(filter, QLatin1String("300-localContactIdFilter.rq"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testIntersectionFilter()
{
    verifyFilter(makeSetFilter<QContactIntersectionFilter>(loadRawContacts()),
                 QLatin1String("301-testIntersectionFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testIntersectionFilterQuery()
{
    verifyFilterQuery(makeSetFilter<QContactIntersectionFilter>(wellKnownContactIds()),
                      QLatin1String("301-testIntersectionFilter.rq"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testUnionFilter()
{
    verifyFilter(makeSetFilter<QContactUnionFilter>(loadRawContacts()),
                 QLatin1String("302-testUnionFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testUnionFilterQuery()
{
    verifyFilterQuery(makeSetFilter<QContactUnionFilter>(wellKnownContactIds()),
                      QLatin1String("302-testUnionFilter.rq"));
}

static QContactFilter
makeDetailFilter()
{
    QContactDetailFilter filter1;
    filter1.setMatchFlags(QContactFilter::MatchCaseSensitive);
    filter1.setDetailDefinitionName(QContactEmailAddress::DefinitionName);
    filter1.setValue(QLatin1String("andre@andrews.com"));

    QContactDetailFilter filter2;
    filter2.setMatchFlags(QContactFilter::MatchFixedString);
    filter2.setDetailDefinitionName(QContactName::DefinitionName,
                                    QContactName::FieldFirstName);
    filter2.setValue(QLatin1String("babera"));

    QContactDetailFilter filter3;
    filter3.setMatchFlags(QContactFilter::MatchContains);
    filter3.setDetailDefinitionName(QContactUrl::DefinitionName,
                                    QContactUrl::FieldUrl);
    filter3.setValue(QLatin1String("Chris"));

    QContactDetailFilter filter4;
    filter4.setMatchFlags(QContactFilter::MatchFixedString);
    filter4.setDetailDefinitionName(QContactBirthday::DefinitionName,
                                    QContactBirthday::FieldBirthday);
    filter4.setValue(QDateTime::fromString(QLatin1String("2008-01-27T00:00:00Z"), Qt::ISODate));

    QContactDetailFilter filter5;
    filter5.setDetailDefinitionName(QContactBirthday::DefinitionName,
                                    QContactBirthday::FieldBirthday);
    filter5.setValue(QLatin1String("2009-04-05T00:00:00Z"));

    return QContactUnionFilter() << filter1 << filter2 << filter3 << filter4 << filter5;
}

static QContactFilter
makeDetailFilterPhoneNumber()
{
    QContactDetailFilter filter;

    filter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName,
                                   QContactPhoneNumber::FieldNumber);
    filter.setMatchFlags(QContactFilter::MatchEndsWith);
    filter.setValue(QLatin1String("4872444"));

    return filter;
}

static QContactFilter
makeDetailFilterPhoneNumberDTMF()
{
    QContactDetailFilter filter;

    filter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName,
                                   QContactPhoneNumber::FieldNumber);
    filter.setMatchFlags(QContactFilter::MatchPhoneNumber);
    filter.setValue(QLatin1String("4872444p4711"));

    return filter;
}

static QContactFilter
makeDetailFilterGender()
{
    QContactDetailFilter filter;

    filter.setDetailDefinitionName(QContactGender::DefinitionName,
                                   QContactGender::FieldGender);
    filter.setMatchFlags(QContactFilter::MatchExactly);
    filter.setValue(QContactGender::GenderFemale);

    return filter;
}

static QContactFilter
makeDetailFilterName(QContactFilter::MatchFlag matchFlags)
{
    QContactDetailFilter filter;

    filter.setMatchFlags(matchFlags);
    filter.setDetailDefinitionName(QContactName::DefinitionName);
    filter.setValue(QLatin1String("Dmitry"));

    return filter;
}

static QContactFilter
makeDetailFilterPresence(QContactFilter::MatchFlags flags, const QVariant &value)
{
    QContactDetailFilter filter;

    filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                   QContactOnlineAccount::FieldCapabilities);
    filter.setMatchFlags(flags);
    filter.setValue(value);

    return filter;
}

static QContactFilter
makeDetailFilterPresence()
{
    return (makeDetailFilterPresence(QContactFilter::MatchStartsWith, QLatin1String("text")) |
            makeDetailFilterPresence(QContactFilter::MatchEndsWith, QLatin1String("calls")) |
            makeDetailFilterPresence(QContactFilter::MatchContains, QLatin1String("tube")));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testDetailFilter()
{
    QSKIP("No reference file available yet", SkipAll);
    verifyFilter(makeDetailFilter(), QLatin1String("303-testDetailFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testDetailFilterQuery_data()
{
    QTest::addColumn<QStringList>("definitionNames");
    QTest::addColumn<QContactFilter>("filter");
    QTest::addColumn<QString>("fileName");

    QTest::newRow("1.various filters")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilter() << "303-testDetailFilter-1.rq";
    QTest::newRow("2.phone number pattern and name query")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilterPhoneNumber() << "303-testDetailFilter-2.rq";
    QTest::newRow("3.phone number pattern and query")
            << (QStringList() << QContactPhoneNumber::DefinitionName)
            << makeDetailFilterPhoneNumber() << "303-testDetailFilter-3.rq";
    QTest::newRow("4.phone number pattern and multiple queries")
            << (QStringList() << QContactName::DefinitionName << QContactGender::DefinitionName)
            << makeDetailFilterPhoneNumber() << "303-testDetailFilter-4.rq";
    QTest::newRow("5.gender filter")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilterGender() << "303-testDetailFilter-5.rq";
    QTest::newRow("6.name filter, starts-with, NB#182154")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilterName(QContactFilter::MatchStartsWith)
            << "303-testDetailFilter-6.rq";
    QTest::newRow("7.name filter, exactly, NB#182154")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilterName(QContactFilter::MatchExactly)
            << "303-testDetailFilter-7.rq";
    QTest::newRow("8.presence filter, starts-with/contains/ends-with")
            << (QStringList() << QContactName::DefinitionName)
            << makeDetailFilterPresence()
            << "303-testDetailFilter-8.rq";
    QTest::newRow("9.phone number with DTMF code")
            << (QStringList() << QContactPhoneNumber::DefinitionName)
            << makeDetailFilterPhoneNumberDTMF()
            << "303-testDetailFilter-9.rq";
}

void
ut_qtcontacts_trackerplugin_querybuilder::testDetailFilterQuery()
{
    QFETCH(QStringList, definitionNames);
    QFETCH(QContactFilter, filter);
    QFETCH(QString, fileName);

    verifyFilterQuery(filter, fileName, definitionNames);
}

static QContactFilter
makeDetailRangeFilter()
{
    QContactDetailRangeFilter filter1;
    filter1.setDetailDefinitionName(QContactName::DefinitionName,
                                    QContactName::FieldFirstName);
    filter1.setMatchFlags(QContactDetailFilter::MatchFixedString);
    filter1.setRange(QLatin1String("Andre"), QLatin1String("Xavier"));

    QContactDetailRangeFilter filter2;
    filter2.setDetailDefinitionName(QContactBirthday::DefinitionName,
                                    QContactBirthday::FieldBirthday);
    filter2.setMatchFlags(QContactDetailFilter::MatchFixedString);
    filter2.setRange(QDateTime::fromString(QLatin1String("2008-01-01T00:00:00Z"), Qt::ISODate),
                     QDateTime::fromString(QLatin1String("2009-12-31T00:00:00Z"), Qt::ISODate),
                     QContactDetailRangeFilter::ExcludeLower |
                     QContactDetailRangeFilter::IncludeUpper);


    QContactDetailRangeFilter filter3;
    filter3.setDetailDefinitionName(QContactBirthday::DefinitionName,
                                    QContactBirthday::FieldBirthday);
    filter3.setRange(QDateTime::fromString(QLatin1String("2010-01-01T00:00:00Z"), Qt::ISODate),
                     QDateTime::fromString(QLatin1String("2011-12-31T00:00:00Z"), Qt::ISODate));

    return QContactUnionFilter() << filter1 << filter2 << filter3;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testDetailRangeFilter()
{
    QSKIP("No reference file available yet", SkipAll);
    verifyFilter(makeDetailRangeFilter(), QLatin1String("304-testDetailRangeFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testDetailRangeFilterQuery()
{
    verifyFilterQuery(makeDetailRangeFilter(), QLatin1String("304-testDetailRangeFilter.rq"));
}

static QContactFilter
makeChangeLogFilter()
{
    QContactChangeLogFilter filter1;
    filter1.setEventType(QContactChangeLogFilter::EventAdded);
    filter1.setSince(QDateTime::fromString(QLatin1String("2008-01-01T00:00:00Z"), Qt::ISODate));

    QContactChangeLogFilter filter2;
    filter2.setEventType(QContactChangeLogFilter::EventChanged);
    filter2.setSince(QDateTime::fromString(QLatin1String("2009-01-01T00:00:00Z"), Qt::ISODate));

    return QContactUnionFilter() << filter1 << filter2;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testChangeLogFilter()
{
    QSKIP("No reference file available yet", SkipAll);
    verifyFilter(makeChangeLogFilter(), QLatin1String("305-testChangeLogFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testChangeLogFilterQuery()
{
    verifyFilterQuery(makeChangeLogFilter(), QLatin1String("305-testChangeLogFilter.rq"));
}



static QContactFilter
makeContactsinGroupRelationshipFilter(const QString &contactManagerUri)
{
    QContactRelationshipFilter filter;
    filter.setRelationshipType(QContactRelationship::HasMember);
    filter.setRelatedContactRole(QContactRelationship::First);
    QContactId contactId;
    contactId.setManagerUri(contactManagerUri);
    contactId.setLocalId(1234);
    filter.setRelatedContactId(contactId);

    return filter;
}

static QContactFilter
makeGroupsOfContactRelationshipFilter(const QString &contactManagerUri)
{
    QContactRelationshipFilter filter;
    filter.setRelationshipType(QContactRelationship::HasMember);
    filter.setRelatedContactRole(QContactRelationship::Second);
    QContactId contactId;
    contactId.setManagerUri(contactManagerUri);
    contactId.setLocalId(1234);
    filter.setRelatedContactId(contactId);

    return filter;
}

static QContactFilter
makeGroupsOfAndContactsInRelationshipFilter(const QString &contactManagerUri)
{
    QContactRelationshipFilter filter;
    filter.setRelationshipType(QContactRelationship::HasMember);
    filter.setRelatedContactRole(QContactRelationship::Either);
    QContactId contactId;
    contactId.setManagerUri(contactManagerUri);
    contactId.setLocalId(1234);
    filter.setRelatedContactId(contactId);

    return filter;
}

void
ut_qtcontacts_trackerplugin_querybuilder::testRelationshipFilter()
{
    QSKIP("No reference file available yet", SkipAll);
    verifyFilter(makeGroupsOfAndContactsInRelationshipFilter(engine()->managerUri()),
                 QLatin1String("306-testRelationshipFilter.xml"));
}

void
ut_qtcontacts_trackerplugin_querybuilder::testRelationshipFilterQuery_data()
{
    const QString managerUri = engine()->managerUri();

    QTest::addColumn<QContactFilter>("filter");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("contactType");

    QTest::newRow("1.contacts in given group query")
            << makeContactsinGroupRelationshipFilter(managerUri)
            << "306-testRelationshipFilter-1.rq"
            << QString::fromLatin1( QContactType::TypeContact.latin1() ); // TODO: could in theory also be groups
    QTest::newRow("2.groups of given contact query")
            << makeGroupsOfContactRelationshipFilter(managerUri)
            << "306-testRelationshipFilter-2.rq"
            << QString::fromLatin1( QContactType::TypeGroup.latin1() );
    QTest::newRow("3.groups of given group and contacts in itself query")
            << makeGroupsOfAndContactsInRelationshipFilter(managerUri)
            << "306-testRelationshipFilter-3.rq"
            << QString::fromLatin1( QContactType::TypeGroup.latin1() ); // TODO: can be both personcontacts and groups
}

void
ut_qtcontacts_trackerplugin_querybuilder::testRelationshipFilterQuery()
{
    QFETCH(QContactFilter, filter);
    QFETCH(QString, fileName);
    QFETCH(QString, contactType);

    // TODO: fetching with QRelationship::Either can contain both types of contacts, groups and contacts
    // not known to be used, so expanding test framework for that later
    if (fileName == QLatin1String("306-testRelationshipFilter-3.rq") ) {
        QSKIP("test framework needs extension to support test of QRelationship::Either", SkipSingle);
    }

    verifyFilterQuery(filter, fileName, contactType);
}

void
ut_qtcontacts_trackerplugin_querybuilder::testRemoveRequest()
{
    const QContactLocalIdList localIds = resolveTrackerIds(loadRawContacts());
    QVERIFY(not localIds.isEmpty());

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsRemoved = engine()->removeContacts(localIds, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsRemoved);

    error = QContactManager::UnspecifiedError;
    QContactList fetchedContacts = engine()->contacts(localIdFilter(localIds),
                                                      NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(fetchedContacts.isEmpty());
}

void ut_qtcontacts_trackerplugin_querybuilder::testRemoveRequestQuery()
{
    QContactRemoveRequest request;
    request.setContactIds(wellKnownContactIds());

    QString query(QTrackerContactRemoveRequest(&request, engine()).buildQuery());

    const QString fileName = QLatin1String("200-remove-request.rq");
    const QString referenceQuery = loadReferenceFile(fileName);

    QVERIFY2(not referenceQuery.isEmpty(), qPrintable(fileName));
    QVERIFY2(verifyQuery(query, referenceQuery), qPrintable(fileName));
}

static void
stripTags(QContact &contact)
{
    qWarning() << "stripping tag details until tag support is proper";

    foreach(QContactTag tag, contact.details<QContactTag>()) {
        QVERIFY2(contact.removeDetail(&tag),
                 qPrintable(QString::fromLatin1("Failed to remove %1 tag for <contact:%2>").
                            arg(tag.tag()).arg(contact.localId())));
    }
}

void ut_qtcontacts_trackerplugin_querybuilder::testSaveRequestCreate()
{
    QContactList savedContacts = loadReferenceContacts(IgnoreReferenceContactIris);
    QVERIFY(not savedContacts.isEmpty());

    if (mShowContact) {
        qDebug() << savedContacts;
    }

    const QDateTime start(QDateTime::currentDateTime().addSecs(-1));
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsSaved = engine()->saveContacts(&savedContacts, 0, &error);
    const QDateTime end = QDateTime::currentDateTime();

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved);

    for(int i = 0; i < savedContacts.count(); ++i) {
        QContactGuid guid(savedContacts[i].detail<QContactGuid>());
        QVERIFY2(not guid.isEmpty(), qPrintable(QString::number(i)));
        QCOMPARE(savedContacts[i].localId(), qHash(guid.guid()));

        QContact fetchedContact;
        fetchContact(savedContacts[i].localId(), fetchedContact);
        CHECK_CURRENT_TEST_FAILED;

        QCOMPARE(fetchedContact.localId(), savedContacts[i].localId());

        // verify fetched timestamp
        QContactTimestamp timestamp(fetchedContact.detail<QContactTimestamp>());
        QVERIFY2(not timestamp.isEmpty(), qPrintable(QString::number(i)));
        QVERIFY2(fetchedContact.removeDetail(&timestamp), qPrintable(QString::number(i)));

        QVERIFY2(timestamp.created() >= start,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.created().toString()).
                            arg(start.toString())));
        QVERIFY2(timestamp.created() <= end,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.created().toString()).
                            arg(end.toString())));
        QVERIFY2(timestamp.lastModified() >= start,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.lastModified().toString()).
                            arg(start.toString())));
        QVERIFY2(timestamp.lastModified() <= end,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.lastModified().toString()).
                            arg(end.toString())));

        // replace fetched timestamp with static one from reference file
        timestamp = savedContacts.at(i).detail<QContactTimestamp>();
        QVERIFY2(not timestamp.isEmpty(), qPrintable(QString::number(i)));
        QVERIFY2(fetchedContact.saveDetail(&timestamp), qPrintable(QString::number(i)));

        // XXX: strip tag details until tag support is proper
        stripTags(fetchedContact);
        CHECK_CURRENT_TEST_FAILED;

        // store fetched contact
        savedContacts[i] = fetchedContact;
    }

    verifyContacts(savedContacts);

    // TODO: also fetch the contacts to verify them
}

void ut_qtcontacts_trackerplugin_querybuilder::testSaveRequestUpdate()
{
    QVERIFY(not loadRawTuples(QLatin1String("001-minimal-contacts.ttl")).isEmpty());

    QContactList savedContacts = loadReferenceContacts(ResolveReferenceContactIris,
                                                       QLatin1String("000-contacts.xml"));
    QVERIFY(not savedContacts.isEmpty());

    if (mShowContact) {
        qDebug() << savedContacts;
    }

    const QDateTime start(QDateTime::currentDateTime().addSecs(-1));
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsSaved = engine()->saveContacts(&savedContacts, 0, &error);
    const QDateTime end = QDateTime::currentDateTime();

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved);

    for(int i = 0; i < savedContacts.count(); ++i) {
        QContact fetchedContact;
        fetchContact(savedContacts[i].localId(), fetchedContact);
        CHECK_CURRENT_TEST_FAILED;

        QCOMPARE(fetchedContact.localId(), savedContacts[i].localId());

        // verify fetched timestamp
        QContactTimestamp timestamp(fetchedContact.detail<QContactTimestamp>());
        QVERIFY2(not timestamp.isEmpty(), qPrintable(QString::number(i)));
        QVERIFY2(fetchedContact.removeDetail(&timestamp), qPrintable(QString::number(i)));

        QVERIFY2(timestamp.lastModified() >= start,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.lastModified().toString()).
                            arg(start.toString())));
        QVERIFY2(timestamp.lastModified() <= end,
                 qPrintable(QString::fromLatin1("<contact:%1> - actual: %2, limit: %3").
                            arg(fetchedContact.localId()).
                            arg(timestamp.lastModified().toString()).
                            arg(end.toString())));

        // replace fetched timestamp with static one from reference file
        timestamp = savedContacts.at(i).detail<QContactTimestamp>();
        QVERIFY2(not timestamp.isEmpty(), qPrintable(QString::number(i)));
        QVERIFY2(fetchedContact.saveDetail(&timestamp), qPrintable(QString::number(i)));

        // XXX: strip tag details until tag support is proper
        stripTags(fetchedContact);
        CHECK_CURRENT_TEST_FAILED;

        // store fetched contact
        savedContacts[i] = fetchedContact;
    }

    verifyContacts(savedContacts);

    // TODO: also fetch the contacts to verify them
}

void ut_qtcontacts_trackerplugin_querybuilder::testSaveRequestQuery_data()
{
    QTest::addColumn<QContact>("contact");
    QTest::addColumn<QString>("fileName");

    QContactList contacts = loadReferenceContacts(GenerateReferenceContactIds,
                                                  QLatin1String("000-contacts.xml"));
    QVERIFY(not contacts.isEmpty());

    if (mShowContact) {
        qDebug() << contacts;
    }

    for(int i = 0; i < contacts.count(); ++i) {
        if (i <= 1) {
            QContactGuid guid(contacts[i].detail<QContactGuid>());
            contacts[i].removeDetail(&guid);
        }

        if (i >= 1 && i <= 2) {
            QContactTimestamp timestamp(contacts[i].detail<QContactTimestamp>());
            contacts[i].removeDetail(&timestamp);
        }

        QString filename = QString::fromLatin1("202-save-request-%1.rq").arg(i + 1);
        QTest::newRow(filename) << contacts[i] << filename;

        if (i < 2) {
            QContact newContact = contacts[i];
            newContact.setId(QContactId());

            if (i > 0) {
                foreach(QContactDetail timestamp, newContact.details<QContactTimestamp>()) {
                    QVERIFY(newContact.removeDetail(&timestamp));
                }
            }

            filename.replace(QLatin1String(".rq"), (QLatin1String("-new.rq")));
            QTest::newRow(filename) << newContact << filename;
        }
    }
}

void ut_qtcontacts_trackerplugin_querybuilder::testSaveRequestQuery()
{
    QFETCH(QContact, contact);
    QFETCH(QString, fileName);

    QContactSaveRequest request;
    request.setContacts(QList<QContact>() << contact);

    QTrackerContactSaveRequest worker(&request, engine());
    worker.setTimestamp(QDateTime::fromString(QLatin1String("2010-05-04T09:30:00Z"), Qt::ISODate));

    const QString actualString = worker.queryString();
    const QString expectedQuery = loadReferenceFile(fileName);

    QVERIFY2(not expectedQuery.isEmpty(), qPrintable(fileName));
    QVERIFY2(verifyQuery(actualString, expectedQuery, VerifyQueryDefaults | RenameIris),
             qPrintable(fileName));
}

void ut_qtcontacts_trackerplugin_querybuilder::testGarbageCollectorQuery()
{
    const QString expectedQuery = loadReferenceFile(QLatin1String("203-garbage-collection.rq"));
    QVERIFY(verifyQuery(engine()->cleanupQueryString(), expectedQuery, VerifyQueryDefaults | RenameIris));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_querybuilder::testParseAnonymousIri()
{
    bool ok = false;
    QUuid uuid = parseAnonymousIri(QLatin1String("urn:uuid:4a50a916-1d00-d8c2-a598-8085976cc729"), &ok);
    QCOMPARE(uuid, QUuid("{4a50a916-1d00-d8c2-a598-8085976cc729}"));
    QVERIFY(ok);

    ok = true;
    parseAnonymousIri(QLatin1String("urn:uuid:"), &ok);
    QVERIFY(not ok);

    ok = true;
    parseAnonymousIri(QLatin1String("bad:"), &ok);
    QVERIFY(not ok);
}


void ut_qtcontacts_trackerplugin_querybuilder::testParseEmailAddressIri()
{
    bool ok = false;
    QString emailAddress = parseEmailAddressIri(QLatin1String("mailto:qt-info@nokia.com"), &ok);
    QCOMPARE(emailAddress, QLatin1String("qt-info@nokia.com"));
    QVERIFY(ok);

    ok = true;
    parseEmailAddressIri(QLatin1String("mailto:"), &ok);
    QVERIFY(not ok);

    parseEmailAddressIri(QLatin1String("bad:"), &ok);
    QVERIFY(not ok);
}

void ut_qtcontacts_trackerplugin_querybuilder::testParseTelepathyIri()
{
    bool ok = false;
    QString imAddress = parseTelepathyIri(QLatin1String("telepathy:/fake/cake!qt-info@nokia.com"), &ok);
    QCOMPARE(imAddress, QLatin1String("/fake/cake!qt-info@nokia.com"));
    QVERIFY(ok);

    ok = true;
    parseTelepathyIri(QLatin1String("telepathy:"), &ok);
    QVERIFY(not ok);

    parseTelepathyIri(QLatin1String("bad:"), &ok);
    QVERIFY(not ok);
}

void ut_qtcontacts_trackerplugin_querybuilder::testParsePresenceIri()
{
    bool ok = false;
    QString imAddress = parsePresenceIri(QLatin1String("presence:/fake/cake!qt-info@nokia.com"), &ok);
    QCOMPARE(imAddress, QLatin1String("/fake/cake!qt-info@nokia.com"));
    QVERIFY(ok);

    ok = true;
    parsePresenceIri(QLatin1String("presence:"), &ok);
    QVERIFY(not ok);

    parsePresenceIri(QLatin1String("bad:"), &ok);
    QVERIFY(not ok);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_querybuilder::testMakeAnonymousIri()
{
    QUrl iri(makeAnonymousIri(QString::fromLatin1("{4a50a916-1d00-d8c2-a598-8085976cc729}")));
    QCOMPARE(iri.toString(), QLatin1String("urn:uuid:4a50a916-1d00-d8c2-a598-8085976cc729"));
}

void ut_qtcontacts_trackerplugin_querybuilder::testMakeEmailAddressIri()
{
    QUrl iri(makeEmailAddressIri(QLatin1String("qt-info@nokia.com")));
    QCOMPARE(iri.toString(), QLatin1String("mailto:qt-info@nokia.com"));
}

void ut_qtcontacts_trackerplugin_querybuilder::testMakeTelepathyIri()
{
    QUrl iri(makeTelepathyIri(QLatin1String("/fake/cake!qt-info@nokia.com")));
    QCOMPARE(iri.toString(), QLatin1String("telepathy:/fake/cake!qt-info@nokia.com"));
}

void ut_qtcontacts_trackerplugin_querybuilder::testMakePresenceIri()
{
    QUrl iri(makePresenceIri(QLatin1String("/fake/cake!qt-info@nokia.com")));
    QCOMPARE(iri.toString(), QLatin1String("presence:/fake/cake!qt-info@nokia.com"));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_querybuilder::testTelepathyIriConversion_data()
{
    QTest::addColumn<QString>("rawValue");
    QTest::addColumn<QString>("iriValue");

    QTest::newRow("connection-path")
            << QString::fromLatin1("/fake/cake!qt-info@nokia.com")
            << QString::fromLatin1("telepathy:/fake/cake!qt-info@nokia.com");
    QTest::newRow("account-path")
            << QString::fromLatin1("/fake/cake")
            << QString::fromLatin1("telepathy:/fake/cake");
}

void ut_qtcontacts_trackerplugin_querybuilder::testTelepathyIriConversion()
{
    QVariant convertedValue;

    QFETCH(QString, rawValue);
    QFETCH(QString, iriValue);

    QVERIFY(TelepathyIriConversion::instance()->makeValue(rawValue, convertedValue));
    QCOMPARE(convertedValue.type(), QVariant::Url);
    QCOMPARE(convertedValue.toString(), iriValue);

    QVERIFY(TelepathyIriConversion::instance()->parseValue(iriValue, convertedValue));
    QCOMPARE(convertedValue.type(), QVariant::String);
    QCOMPARE(convertedValue.toString(), rawValue);
}

void ut_qtcontacts_trackerplugin_querybuilder::testLocalPhoneNumberConversion_data()
{
    QTest::addColumn<QString>("formattedNumber");
    QTest::addColumn<QString>("expectedLocalNumber");
    QTest::addColumn<int>("suffixLength");

    QTest::newRow("parenthesis")
            << QString::fromUtf8("(030) 1234-5678")
            << QString::fromUtf8("2345678") << 7;
    QTest::newRow("eight-digits")
            << QString::fromUtf8("12.34.56.78")
            << QString::fromUtf8("2345678") << 7;
    QTest::newRow("seven-digits")
            << QString::fromUtf8("1234567")
            << QString::fromUtf8("1234567") << 7;
    QTest::newRow("six-digits")
            << QString::fromUtf8("123456")
            << QString::fromUtf8("123456") << 7;


    QTest::newRow("arabic")
            << QString::fromUtf8("\331\240\331\241\331\242\331\243\331\244\331\245\331\246\331\247\331\250\331\251")
            << QString::fromUtf8("0123456789") << 10;
    QTest::newRow("persian")
            << QString::fromUtf8("\333\260\333\261\333\262\333\263\333\264\333\265\333\266\333\267\333\270\333\271")
            << QString::fromUtf8("0123456789") << 10;


    QTest::newRow("bengali")
            << QString::fromUtf8("\340\247\246\340\247\247\340\247\250\340\247\251\340\247\252\340\247\253\340\247\254\340\247\255\340\247\256\340\247\257")
            << QString::fromUtf8("0123456789") << 10;
    QTest::newRow("devanagari")
            << QString::fromUtf8("\340\245\246\340\245\247\340\245\250\340\245\251\340\245\252\340\245\253\340\245\254\340\245\255\340\245\256\340\245\257")
            << QString::fromUtf8("0123456789") << 10;
    QTest::newRow("tamil")
            << QString::fromUtf8("\340\257\246\340\257\247\340\257\250\340\257\251\340\257\252\340\257\253\340\257\254\340\257\255\340\257\256\340\257\257")
            << QString::fromUtf8("0123456789") << 10;
    QTest::newRow("thai")
            << QString::fromUtf8("\340\271\220\340\271\221\340\271\222\340\271\223\340\271\224\340\271\225\340\271\226\340\271\227\340\271\230\340\271\231")
            << QString::fromUtf8("0123456789") << 10;
}

void ut_qtcontacts_trackerplugin_querybuilder::testLocalPhoneNumberConversion()
{
    QFETCH(int, suffixLength);
    QFETCH(QString, formattedNumber);
    QFETCH(QString, expectedLocalNumber);

    changeSetting(QctSettings::NumberMatchLengthKey, suffixLength);

    // test simple formatted number
    QVariant value = formattedNumber;
    QVERIFY(LocalPhoneNumberConversion::instance()->makeValue(value, value));
    QCOMPARE(value.toString(), expectedLocalNumber);

    // test formatted number with DTMF code
    const QString dtmfSuffix = QLatin1String("p1234");
    value = formattedNumber + dtmfSuffix;
    QVERIFY(LocalPhoneNumberConversion::instance()->makeValue(value, value));
    QCOMPARE(value.toString(), expectedLocalNumber + dtmfSuffix);
}

void ut_qtcontacts_trackerplugin_querybuilder::testCamelCaseFunction()
{
    QCOMPARE(qctCamelCase(QLatin1String("Other")), QString::fromLatin1("Other"));
    QCOMPARE(qctCamelCase(QLatin1String("other")), QString::fromLatin1("Other"));
    QCOMPARE(qctCamelCase(QLatin1String("OTHER")), QString::fromLatin1("Other"));

    QCOMPARE(qctCamelCase(QLatin1String("CustomStuff")), QString::fromLatin1("CustomStuff"));
    QCOMPARE(qctCamelCase(QLatin1String("customStuff")), QString::fromLatin1("CustomStuff"));
    QCOMPARE(qctCamelCase(QLatin1String("customSTUFF")), QString::fromLatin1("CustomStuff"));

    QCOMPARE(qctCamelCase(QLatin1String("Custom Stuff and More")), QString::fromLatin1("Custom Stuff and More"));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_querybuilder::testUpdateDetailUri()
{
    QSKIP("restore this test", SkipAll);

    // FIXME: updateDetailUri() is not public anymore,
    // therefore another ways must be found to test this behavior
/*
    QContactPhoneNumber phoneNumber;
    phoneNumber.setDetailUri("tel:112233");
    phoneNumber.setNumber("445566");
    engine()->schema(QContactType::TypeContact).detail(phoneNumber.definitionName())->updateDetailUri(0, phoneNumber); //FIXTYPE
    QCOMPARE(phoneNumber.detailUri(), QString::fromLatin1("tel:445566"));

    QContactOnlineAccount onlineAccount;
    onlineAccount.setDetailUri("telepathy:badone");
    onlineAccount.setValue(QLatin1String("AccountPath"), "/fake/cake/1");
    onlineAccount.setAccountUri("first.last@talk.com");
    engine()->schema(QContactType::TypeContact).detail(onlineAccount.definitionName())->updateDetailUri(0, onlineAccount); //FIXTYPE
    QCOMPARE(onlineAccount.detailUri(), QString::fromLatin1("telepathy:/fake/cake/1!first.last@talk.com"));
*/
}

void ut_qtcontacts_trackerplugin_querybuilder::testPhoneNumberIRI_data()
{
    QTest::addColumn<QString>("generatedIri");
    QTest::addColumn<QString>("generatedResource");
    QTest::addColumn<QString>("expected");

    QContactPhoneNumber number;

    number.setNumber(QLatin1String("12345"));

    QTest::newRow("Simple phone number")
            << qctMakePhoneNumberIri(number)
            << qctMakePhoneNumberResource(number).sparql()
            << QString::fromLatin1("urn:x-maemo-phone:12345");

    number.setSubTypes(QStringList() << QContactPhoneNumber::SubTypeFax);

    QTest::newRow("Simple phone number with one subtype")
            << qctMakePhoneNumberIri(number)
            << qctMakePhoneNumberResource(number).sparql()
            << QString::fromLatin1("urn:x-maemo-phone:fax:12345");

    number.setSubTypes(QStringList()
                       << QContactPhoneNumber::SubTypeMobile
                       << QContactPhoneNumber::SubTypeFax);

    QTest::newRow("Simple phone number with many subtypes")
            << qctMakePhoneNumberIri(number)
            << qctMakePhoneNumberResource(number).sparql()
            << QString::fromLatin1("urn:x-maemo-phone:fax,mobile:12345");

    number.setNumber(QLatin1String("+3345678"));
    number.setSubTypes(QStringList());

    QTest::newRow("Number with +")
            << qctMakePhoneNumberIri(number)
            << qctMakePhoneNumberResource(number).sparql()
            << QString::fromLatin1("urn:x-maemo-phone:+3345678");

    number.setNumber(QLatin1String("\\<>\"{}|^`\x10\x20"));

    QTest::newRow("Number with forbidden chars")
            << qctMakePhoneNumberIri(number)
            << qctMakePhoneNumberResource(number).sparql()
            << QString::fromLatin1("urn:x-maemo-phone:%5c%3c%3e%22%7b%7d%7c%5e%60%10%20");
}

void ut_qtcontacts_trackerplugin_querybuilder::testPhoneNumberIRI()
{
    static const QRegExp forbiddenChars(QLatin1String("[\\<>\"{}|^`\\x00-\\x20]"));

    QVERIFY(forbiddenChars.isValid());

    QFETCH(QString, generatedIri);
    QFETCH(QString, generatedResource);
    QFETCH(QString, expected);

    QVERIFY(generatedResource.startsWith(QLatin1Char('<')));
    QVERIFY(generatedResource.endsWith(QLatin1Char('>')));

    // strip angle brackets from generatedResource
    generatedResource = generatedResource.mid(1, generatedResource.length() - 2);

    QCOMPARE(generatedIri, expected);
    QCOMPARE(generatedResource, expected);

    QCOMPARE(forbiddenChars.indexIn(generatedIri), -1);
    QCOMPARE(forbiddenChars.indexIn(generatedResource), -1);

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_querybuilder::missingTests_data()
{
    QTest::newRow("check if detail filter on contact type is working");
    QTest::newRow("check if detail filter on detail subtypes is working");
    QTest::newRow("check if detail filter on custom values is working");
    QTest::newRow("check if detail filter with wildcard values is working");
    QTest::newRow("check if range filter with wildcard values is working");
    QTest::newRow("check if there are subtypes for all known subclasses of nco:MediaType");
}

void ut_qtcontacts_trackerplugin_querybuilder::missingTests()
{
    QSKIP("not implement yet", SkipSingle);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_querybuilder);
