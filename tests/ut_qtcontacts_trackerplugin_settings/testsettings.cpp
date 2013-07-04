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

#include "testsettings.h"


const QString TestSettings::UT__DefaultSomeStringSetting = QLatin1String("DefaultSomeString");
const QString TestSettings::UT__DefaultSomeStringListSetting = QLatin1String("DefaultSomeStringList");

const QString TestSettings::UT__SomeIntSettingKey = QLatin1String("UT__SomeIntSetting");
const QString TestSettings::UT__SomeStringSettingKey = QLatin1String("UT__SomeStringSetting");
const QString TestSettings::UT__SomeStringListSettingKey = QLatin1String("UT__SomeStringListSetting");


TestSettings::TestSettings()
{
    static bool isSettingsRegistered = false;

    if (not isSettingsRegistered) {
        registerSetting(UT__SomeIntSettingKey, UT__DefaultSomeIntSetting);
        registerSetting(UT__SomeStringSettingKey, UT__DefaultSomeIntSetting);
        registerSetting(UT__SomeStringListSettingKey, UT__DefaultSomeIntSetting);

        isSettingsRegistered = true;
    }
}


void TestSettings::setUT__SomeIntSetting(int someIntSetting)
{
    setValue(UT__SomeIntSettingKey, someIntSetting);
}
int TestSettings::ut__someIntSetting() const
{
    bool valid;
    int someIntSetting = value(UT__SomeIntSettingKey).toInt(&valid);

    if (not valid) {
        someIntSetting = UT__DefaultSomeIntSetting;
    }

    return someIntSetting;
}

void TestSettings::setUT__SomeStringSetting(const QString& someStringSetting)
{
    setValue(UT__SomeStringSettingKey, someStringSetting);
}
QString TestSettings::ut__someStringSetting() const
{
    return value(UT__SomeStringSettingKey).toString();
}


void TestSettings::setUT__SomeStringListSetting(const QStringList& someStringSettingList)
{
    setValue(UT__SomeStringListSettingKey, someStringSettingList);
}
QStringList TestSettings::ut__someStringListSetting() const
{
    return value(UT__SomeStringListSettingKey).toStringList();
}
