/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "ut_qtcontacts_trackerplugin_settings.h"

// test
#include "testsettings.h"
// Qt
#include <QtTest/QtTest>

QTM_USE_NAMESPACE


// neutral values for making sure the setting is different from the test values used
static const int avatarSizeNeutralValue = 18;
static const int avatarSizeTestValue = 999;
static const int concurrencyLevelNeutralValue = 2;
static const int concurrencyLevelTestValue = 8192;
static const QString guidAlgorithmNameNeutralValue = QLatin1String("default");
static const QString guidAlgorithmNameTestValue = QLatin1String("magicCubeRolling");
static const QString lastMSISDNNeutralValue = QLatin1String("12345678");
static const QString lastMSISDNTestValue = QLatin1String("00000000");
static const QString nameOrderNeutralValue = QLatin1String("NoOrder");
static const QString nameOrderTestValue = QLatin1String("RandomOrder");
const int localPhoneNumberLengthNeutralValue = 7;
const int localPhoneNumberLengthTestValue = 42;
static const QStringList sparqlBackendsNeutralValue = QStringList()
    << QLatin1String("tracker");
static const QStringList sparqlBackendsTestValue = QStringList()
    << QLatin1String("userdialog") << QLatin1String("/dev/random");
static const int someIntNeutralValue = 9;
static const int someIntTestValue = 125;
static const QString someStringNeutralValue = QLatin1String("someStringNeutral");
static const QString someStringTestValue = QLatin1String("someStringTest");
static const QStringList someStringListNeutralValue = QStringList()
    << QLatin1String("neutral");
static const QStringList someStringListTestValue = QStringList()
    << QLatin1String("some") << QLatin1String("stringlist") << QLatin1String("test") << QLatin1String("value");


ut_qtcontacts_trackerplugin_settings::ut_qtcontacts_trackerplugin_settings(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)

{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_settings::checkValuesChangedSignal(QSignalSpy &valuesChangedSpy,
                                                                    const QString &key,
                                                                    const QVariant &referenceValue) const
{
    QCOMPARE(valuesChangedSpy.count(), 1);
    const QList<QVariant> arguments = valuesChangedSpy.takeFirst();
    QCOMPARE(arguments.count(), 1);
    const QHash<QString,QVariant> changedValues = qvariant_cast<QHash<QString,QVariant> >(arguments.at(0));
    QCOMPARE(changedValues.count(), 1);
    QVERIFY(changedValues.contains(key));
    QCOMPARE(changedValues.value(key), referenceValue);
}

void ut_qtcontacts_trackerplugin_settings::setQctSettingsNeutralValues(QctSettings &settings) const
{
    settings.setAvatarSize(avatarSizeNeutralValue);
    settings.setConcurrencyLevel(concurrencyLevelNeutralValue);
    settings.setGuidAlgorithmName(guidAlgorithmNameNeutralValue);
    settings.setLastMSISDN(lastMSISDNNeutralValue);
    settings.setLocalPhoneNumberLength(localPhoneNumberLengthNeutralValue);
    settings.setNameOrder(nameOrderNeutralValue);
    settings.setSparqlBackends(sparqlBackendsNeutralValue);
}
void ut_qtcontacts_trackerplugin_settings::setTestSettingsNeutralValues(TestSettings &settings) const
{
    setQctSettingsNeutralValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    settings.setUT__SomeIntSetting(someIntNeutralValue);
    settings.setUT__SomeStringSetting(someStringNeutralValue);
    settings.setUT__SomeStringListSetting(someStringListNeutralValue);
}
void ut_qtcontacts_trackerplugin_settings::checkQctSettingsNeutralValues(const QctSettings &settings) const
{
    QCOMPARE(settings.avatarSize(), QSize(avatarSizeNeutralValue, avatarSizeNeutralValue));
    QCOMPARE(settings.concurrencyLevel(), concurrencyLevelNeutralValue);
    QCOMPARE(settings.guidAlgorithmName(), guidAlgorithmNameNeutralValue);
    QCOMPARE(settings.lastMSISDN(), lastMSISDNNeutralValue);
    QCOMPARE(settings.localPhoneNumberLength(), localPhoneNumberLengthNeutralValue);
    QCOMPARE(settings.nameOrder(), nameOrderNeutralValue);
    QCOMPARE(settings.sparqlBackends(), sparqlBackendsNeutralValue);
}
void ut_qtcontacts_trackerplugin_settings::checkTestSettingsNeutralValues(const TestSettings &settings) const
{
    checkQctSettingsNeutralValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(settings.ut__someIntSetting(), someIntNeutralValue);
    QCOMPARE(settings.ut__someStringSetting(), someStringNeutralValue);
    QCOMPARE(settings.ut__someStringListSetting(), someStringListNeutralValue);
}

void ut_qtcontacts_trackerplugin_settings::setQctSettingsTestValues(QctSettings &settings) const
{
    settings.setAvatarSize(avatarSizeTestValue);
    settings.setConcurrencyLevel(concurrencyLevelTestValue);
    settings.setGuidAlgorithmName(guidAlgorithmNameTestValue);
    settings.setLastMSISDN(lastMSISDNTestValue);
    settings.setLocalPhoneNumberLength(localPhoneNumberLengthTestValue);
    settings.setNameOrder(nameOrderTestValue);
    settings.setSparqlBackends(sparqlBackendsTestValue);
}
void ut_qtcontacts_trackerplugin_settings::setTestSettingsTestValues(TestSettings &settings) const
{
    setQctSettingsTestValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    settings.setUT__SomeIntSetting(someIntTestValue);
    settings.setUT__SomeStringSetting(someStringTestValue);
    settings.setUT__SomeStringListSetting(someStringListTestValue);
}
void ut_qtcontacts_trackerplugin_settings::checkQctSettingsTestValues(const QctSettings &settings) const
{
    QCOMPARE(settings.avatarSize(), QSize(avatarSizeTestValue, avatarSizeTestValue));
    QCOMPARE(settings.concurrencyLevel(), concurrencyLevelTestValue);
    QCOMPARE(settings.guidAlgorithmName(), guidAlgorithmNameTestValue);
    QCOMPARE(settings.lastMSISDN(), lastMSISDNTestValue);
    QCOMPARE(settings.localPhoneNumberLength(), localPhoneNumberLengthTestValue);
    QCOMPARE(settings.nameOrder(), nameOrderTestValue);
    QCOMPARE(settings.sparqlBackends(), sparqlBackendsTestValue);
}
void ut_qtcontacts_trackerplugin_settings::checkTestSettingsTestValues(const TestSettings &settings) const
{
    checkQctSettingsTestValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    QCOMPARE(settings.ut__someIntSetting(), someIntTestValue);
    QCOMPARE(settings.ut__someStringSetting(), someStringTestValue);
    QCOMPARE(settings.ut__someStringListSetting(), someStringListTestValue);
}

void ut_qtcontacts_trackerplugin_settings::checkQctSettingsSignalling(QctSettings &settings,
                                                                      QSignalSpy &valuesChangedSpy) const
{
    settings.setAvatarSize(avatarSizeTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::AvatarSizeKey, avatarSizeTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setConcurrencyLevel(concurrencyLevelTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::ConcurrencyLevelKey, concurrencyLevelTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setGuidAlgorithmName(guidAlgorithmNameTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::GuidAlgorithmNameKey, guidAlgorithmNameTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setLastMSISDN(lastMSISDNTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::LastMSISDNKey, lastMSISDNTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setLocalPhoneNumberLength(localPhoneNumberLengthTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::NumberMatchLengthKey, localPhoneNumberLengthTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setNameOrder(nameOrderTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::NameOrderKey, nameOrderTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setSparqlBackends(sparqlBackendsTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             QctSettings::SparqlBackendsKey, sparqlBackendsTestValue);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::checkTestSettingsSignalling(TestSettings &settings,
                                                                       QSignalSpy &valuesChangedSpy) const
{
    checkQctSettingsSignalling(settings, valuesChangedSpy);
    CHECK_CURRENT_TEST_FAILED;

    // TestSettings settings
    settings.setUT__SomeIntSetting(someIntTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             TestSettings::UT__SomeIntSettingKey, someIntTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setUT__SomeStringSetting(someStringTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             TestSettings::UT__SomeStringSettingKey, someStringTestValue);
    CHECK_CURRENT_TEST_FAILED;

    settings.setUT__SomeStringListSetting(someStringListTestValue);
    checkValuesChangedSignal(valuesChangedSpy,
                             TestSettings::UT__SomeStringListSettingKey, someStringListTestValue);
    CHECK_CURRENT_TEST_FAILED;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_settings::init()
{
    // backup current values
    TestSettings settings;
    mSettingsDataBackup.avatarSize = settings.avatarSize();
    mSettingsDataBackup.concurrencyLevel = settings.concurrencyLevel();
    mSettingsDataBackup.guidAlgorithmName = settings.guidAlgorithmName();
    mSettingsDataBackup.lastMSISDN = settings.lastMSISDN();
    mSettingsDataBackup.localPhoneNumberLength = settings.localPhoneNumberLength();
    mSettingsDataBackup.nameOrder = settings.nameOrder();
    mSettingsDataBackup.sparqlBackends = settings.sparqlBackends();
    mSettingsDataBackup.ut__someIntSetting = settings.ut__someIntSetting();
    mSettingsDataBackup.ut__someStringSetting = settings.ut__someStringSetting();
    mSettingsDataBackup.ut__someStringListSetting = settings.ut__someStringListSetting();
}

void ut_qtcontacts_trackerplugin_settings::cleanup()
{
    // set old values back
    TestSettings settings;
    settings.setAvatarSize(mSettingsDataBackup.avatarSize.width());
    settings.setConcurrencyLevel(mSettingsDataBackup.concurrencyLevel);
    settings.setGuidAlgorithmName(mSettingsDataBackup.guidAlgorithmName);
    settings.setLastMSISDN(mSettingsDataBackup.lastMSISDN);
    settings.setLocalPhoneNumberLength(mSettingsDataBackup.localPhoneNumberLength);
    settings.setNameOrder(mSettingsDataBackup.nameOrder);
    settings.setSparqlBackends(mSettingsDataBackup.sparqlBackends);
    settings.setUT__SomeIntSetting(mSettingsDataBackup.ut__someIntSetting);
    settings.setUT__SomeStringSetting(mSettingsDataBackup.ut__someStringSetting);
    settings.setUT__SomeStringListSetting(mSettingsDataBackup.ut__someStringListSetting);
    QCOMPARE(settings.avatarSize(), mSettingsDataBackup.avatarSize);
    QCOMPARE(settings.concurrencyLevel(), mSettingsDataBackup.concurrencyLevel);
    QCOMPARE(settings.guidAlgorithmName(), mSettingsDataBackup.guidAlgorithmName);
    QCOMPARE(settings.lastMSISDN(), mSettingsDataBackup.lastMSISDN);
    QCOMPARE(settings.localPhoneNumberLength(), mSettingsDataBackup.localPhoneNumberLength);
    QCOMPARE(settings.nameOrder(), mSettingsDataBackup.nameOrder);
    QCOMPARE(settings.sparqlBackends(), mSettingsDataBackup.sparqlBackends);
    QCOMPARE(settings.ut__someIntSetting(), mSettingsDataBackup.ut__someIntSetting);
    QCOMPARE(settings.ut__someStringSetting(), mSettingsDataBackup.ut__someStringSetting);
    QCOMPARE(settings.ut__someStringListSetting(), mSettingsDataBackup.ut__someStringListSetting);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void ut_qtcontacts_trackerplugin_settings::testSaveFetchSettings()
{
    QctSettings settings;

    // "reset" to neutral values
    setQctSettingsNeutralValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    // set test values
    setQctSettingsTestValues(settings);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testSaveFetchCustomSettings()
{
    TestSettings settings;

    // "reset" to neutral values
    setTestSettingsNeutralValues(settings);
    checkTestSettingsNeutralValues(settings);
    CHECK_CURRENT_TEST_FAILED;
    // set test values
    setTestSettingsTestValues(settings);
    checkTestSettingsTestValues(settings);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testUpdateOfOtherSettingsInstance()
{
    QctSettings settings;

    QctSettings otherSettings;

    // "reset" to neutral values
    setQctSettingsNeutralValues(settings);
    // test that values are automatically changed also in the other instance of QctSettings
    checkQctSettingsNeutralValues(otherSettings);
    CHECK_CURRENT_TEST_FAILED;

    // set test values
    setQctSettingsTestValues(settings);
    // test that values are automatically changed also in the other instance of QctSettings
    checkQctSettingsTestValues(otherSettings);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testUpdateOfOtherCustomSettingsInstance()
{
    TestSettings settings;

    TestSettings otherSettings;

    // "reset" to neutral values
    setTestSettingsNeutralValues(settings);
    // test that values are automatically changed also in the other instance of TestSettings
    checkTestSettingsNeutralValues(otherSettings);
    CHECK_CURRENT_TEST_FAILED;

    // set test values
    setTestSettingsTestValues(settings);
    // test that values are automatically changed also in the other instance of TestSettings
    checkTestSettingsTestValues(otherSettings);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testSettingsChangeSignalling()
{
    QctSettings settings;

    // "reset" to neutral values
    setQctSettingsNeutralValues(settings);

    // test signalling on change
    qRegisterMetaType<QHash<QString,QVariant> >("QHash<QString,QVariant>");
    QSignalSpy valuesChangedSpy(&settings, SIGNAL(valuesChanged(QHash<QString,QVariant>)));

    checkQctSettingsSignalling(settings, valuesChangedSpy);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testChangeSignallingOfOtherSettingsInstance()
{
    QctSettings settings;
    QctSettings otherSettings;

    // "reset" to neutral values
    setQctSettingsNeutralValues(settings);

    // test signalling on change
    qRegisterMetaType<QHash<QString,QVariant> >("QHash<QString,QVariant>");
    QSignalSpy valuesChangedSpy(&otherSettings, SIGNAL(valuesChanged(QHash<QString,QVariant>)));

    checkQctSettingsSignalling(settings, valuesChangedSpy);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testCustomSettingsChangeSignalling()
{
    TestSettings settings;

    // "reset" to neutral values
    setTestSettingsNeutralValues(settings);

    // test signalling on change
    qRegisterMetaType<QHash<QString,QVariant> >("QHash<QString,QVariant>");
    QSignalSpy valuesChangedSpy(&settings, SIGNAL(valuesChanged(QHash<QString,QVariant>)));

    checkTestSettingsSignalling(settings, valuesChangedSpy);
    CHECK_CURRENT_TEST_FAILED;
}

void ut_qtcontacts_trackerplugin_settings::testChangeSignallingOfOtherCustomSettingsInstance()
{
    TestSettings settings;
    TestSettings otherSettings;

    // "reset" to neutral values
    setTestSettingsNeutralValues(settings);

    // test signalling on change
    qRegisterMetaType<QHash<QString,QVariant> >("QHash<QString,QVariant>");
    QSignalSpy valuesChangedSpy(&otherSettings, SIGNAL(valuesChanged(QHash<QString,QVariant>)));

    checkTestSettingsSignalling(settings, valuesChangedSpy);
    CHECK_CURRENT_TEST_FAILED;
}


void ut_qtcontacts_trackerplugin_settings::testSignallingOfSettingsChangeInOtherProcess_data()
{
    QTest::addColumn<QString>("settingsFieldId");
    QTest::addColumn<QVariant>("settingsFieldNeutralValue");
    QTest::addColumn<QVariant>("settingsFieldTestValue");

    QTest::newRow("AvatarSize")
            << QctSettings::AvatarSizeKey
            << QVariant(avatarSizeNeutralValue)
            << QVariant(avatarSizeTestValue);

    QTest::newRow("ConcurrencyLevel")
            << QctSettings::ConcurrencyLevelKey
            << QVariant(concurrencyLevelNeutralValue)
            << QVariant(concurrencyLevelTestValue);

    QTest::newRow("GuidAlgorithmName")
            << QctSettings::GuidAlgorithmNameKey
            << QVariant(guidAlgorithmNameNeutralValue)
            << QVariant(guidAlgorithmNameTestValue);

    QTest::newRow("LastMSISDN")
            << QctSettings::LastMSISDNKey
            << QVariant(lastMSISDNNeutralValue)
            << QVariant(lastMSISDNTestValue);

    QTest::newRow("NumberMatchLength")
            << QctSettings::NumberMatchLengthKey
            << QVariant(localPhoneNumberLengthNeutralValue)
            << QVariant(localPhoneNumberLengthTestValue);

    QTest::newRow("NameOrder")
            << QctSettings::NameOrderKey
            << QVariant(nameOrderNeutralValue)
            << QVariant(nameOrderTestValue);

    QTest::newRow("SparqlBackends")
            << QctSettings::SparqlBackendsKey
            << QVariant(sparqlBackendsNeutralValue)
            << QVariant(sparqlBackendsTestValue);

    QTest::newRow("UT__SomeIntSetting")
            << TestSettings::UT__SomeIntSettingKey
            << QVariant(someIntNeutralValue)
            << QVariant(someIntTestValue);

    QTest::newRow("UT__SomeStringSetting")
            << TestSettings::UT__SomeStringSettingKey
            << QVariant(someStringNeutralValue)
            << QVariant(someStringTestValue);

    QTest::newRow("UT__SomeStringListSetting")
            << TestSettings::UT__SomeStringListSettingKey
            << QVariant(someStringListNeutralValue)
            << QVariant(someStringListTestValue);
}


void ut_qtcontacts_trackerplugin_settings::testSignallingOfSettingsChangeInOtherProcess()
{
    // wait a little so the first change to the settings file is not hidden by one done before
    QTest::qWait(1000);

    // tests creates and saves contacts, each with a different value for a field of a custom detail
    // then for each value contacts are fetched with that value as detailfilter
    QFETCH(QString,  settingsFieldId);
    QFETCH(QVariant, settingsFieldNeutralValue);
    QFETCH(QVariant, settingsFieldTestValue);

    // use generic method setValue()
    // otherwise a big if-else would be needed to call the right method
    TestSettings settings;
    settings.setValue(settingsFieldId, settingsFieldNeutralValue);
    settings.sync();

    qRegisterMetaType<QHash<QString,QVariant> >("QHash<QString,QVariant>");
    QSignalSpy valuesChangedSpy(&settings, SIGNAL(valuesChanged(QHash<QString,QVariant>)));

    // execute helper program in another process, to change the setting for the given field
    QProcess settingsChanger;
    QStringList arguments;
    arguments << settingsFieldId;
    if (QVariant::StringList == settingsFieldTestValue.type()) {
        arguments << settingsFieldTestValue.toStringList();
    } else {
        arguments << settingsFieldTestValue.toString();
    }

    const QString helperProgramPath = qApp->arguments().first() + QLatin1String("_helper");
    settingsChanger.start(helperProgramPath, arguments);
    settingsChanger.waitForFinished();
    QCOMPARE(settingsChanger.exitStatus(), QProcess::NormalExit);
    QCOMPARE(settingsChanger.exitCode(), 0);

    const int waitForFilesystemSignalInMS = 500;
    QTest::qWait(waitForFilesystemSignalInMS);

    // check if local settings instance emitted the signal for change by other process
    checkValuesChangedSignal(valuesChangedSpy, settingsFieldId, settingsFieldTestValue);
    CHECK_CURRENT_TEST_FAILED;
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_settings)
