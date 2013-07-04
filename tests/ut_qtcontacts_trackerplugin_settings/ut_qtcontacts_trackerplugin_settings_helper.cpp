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

// test
#include "testsettings.h"
// Qt
#include <QtCore/QCoreApplication>

static const int NoError = 0;
static const int TooFewArgumentsError = 1;
static const int UnknownSettingsKeyError = 2;

/// Helper program for ut_qtcontacts_trackerplugin_settings
///
/// Takes as arguments a settings identifier and a value for it.
/// then creates a QctSettings instance and sets the corresponding entry to the given value.
/// If the value is a list, simply pass all entries as normal arguments.
/// Call it as:
/// ut_qtcontacts_trackerplugin_settings_helper keyOfSettingsField newValue
int main(int argc, char **argv)
{
    QCoreApplication programCore(argc, argv);

    QStringList arguments = programCore.arguments();
    arguments.removeFirst(); // first was program name
    // not both setting name and value as arguments?
    if (arguments.count() < 2) {
        return TooFewArgumentsError;
    }

    TestSettings settings;

    const QString settingKey = arguments.takeFirst();
    // QctSettings settings
    if (QctSettings::AvatarSizeKey == settingKey) {
        const int avatarSize = arguments.takeFirst().toInt();
        settings.setAvatarSize(avatarSize);
    }
    else if (QctSettings::ConcurrencyLevelKey == settingKey) {
        const int concurrencyLevel = arguments.takeFirst().toInt();
        settings.setConcurrencyLevel(concurrencyLevel);
    }
    else if (QctSettings::GuidAlgorithmNameKey == settingKey) {
        const QString guidAlgorithmName = arguments.takeFirst();
        settings.setGuidAlgorithmName(guidAlgorithmName);
    }
    else if (QctSettings::LastMSISDNKey == settingKey) {
        const QString lastMSISDN = arguments.takeFirst();
        settings.setLastMSISDN(lastMSISDN);
    }
    else if (QctSettings::NumberMatchLengthKey == settingKey) {
        const int numberMatchLength = arguments.takeFirst().toInt();
        settings.setLocalPhoneNumberLength(numberMatchLength);
    }
    else if (QctSettings::NameOrderKey == settingKey) {
        const QString nameOrder = arguments.takeFirst();
        settings.setNameOrder(nameOrder);
    }
    else if (QctSettings::SparqlBackendsKey == settingKey) {
        const QStringList sparqlBackends = arguments;
        settings.setSparqlBackends(sparqlBackends);
    }
    // TestSettings settings
    else if (TestSettings::UT__SomeIntSettingKey == settingKey) {
        const int someIntSetting = arguments.takeFirst().toInt();
        settings.setUT__SomeIntSetting(someIntSetting);
    }
    else if (TestSettings::UT__SomeStringSettingKey == settingKey) {
        const QString someStringSetting = arguments.takeFirst();
        settings.setUT__SomeStringSetting(someStringSetting);
    }
    else if (TestSettings::UT__SomeStringListSettingKey == settingKey) {
        const QStringList someStringListSetting = arguments;
        settings.setUT__SomeStringListSetting(someStringListSetting);
    }
    // something broken
    else {
        return UnknownSettingsKeyError;
    }

    return NoError;
}
