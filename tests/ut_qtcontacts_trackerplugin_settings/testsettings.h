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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_TESTSETTINGS_H
#define UT_QTCONTACTS_TRACKERPLUGIN_TESTSETTINGS_H

// subject-under-test
#include <lib/settings.h>


class TestSettings: public QctSettings
{
    Q_OBJECT

public: // key constants
    static const QString UT__SomeIntSettingKey;
    static const QString UT__SomeStringSettingKey;
    static const QString UT__SomeStringListSettingKey;
public: // default values
    static const int UT__DefaultSomeIntSetting = 7;
    static const QString UT__DefaultSomeStringSetting;
    static const QString UT__DefaultSomeStringListSetting;

public:
    TestSettings();

public: // test attributes
    void setUT__SomeIntSetting(int someIntSetting);
    int ut__someIntSetting() const;
    void setUT__SomeStringSetting(const QString &someStringSetting);
    QString ut__someStringSetting() const;
    void setUT__SomeStringListSetting(const QStringList &someStringSettingList);
    QStringList ut__someStringListSetting() const;

public: // helper
    using QctSettings::setValue;
};

#endif /* UT_QTCONTACTS_TRACKERPLUGIN_GROUPS_H */
