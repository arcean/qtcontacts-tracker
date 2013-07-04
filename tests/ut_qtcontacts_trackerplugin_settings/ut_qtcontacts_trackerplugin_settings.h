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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_SETTINGS_H
#define UT_QTCONTACTS_TRACKERPLUGIN_SETTINGS_H

#include "ut_qtcontacts_trackerplugin_common.h"

class TestSettings;
class QctSettings;

/// structure to backup the current settings data
struct SettingsData {
    QSize avatarSize;
    int concurrencyLevel;
    QString guidAlgorithmName;
    QString lastMSISDN;
    int localPhoneNumberLength;
    QString nameOrder;
    QStringList sparqlBackends;
    int ut__someIntSetting;
    QString ut__someStringSetting;
    QStringList ut__someStringListSetting;
};

class ut_qtcontacts_trackerplugin_settings : public ut_qtcontacts_trackerplugin_common
{
    Q_OBJECT

public:
    ut_qtcontacts_trackerplugin_settings(QObject *parent = 0);

private slots:
    void init();
    void cleanup();

    void testSaveFetchSettings();
    void testSaveFetchCustomSettings();
    void testUpdateOfOtherSettingsInstance();
    void testUpdateOfOtherCustomSettingsInstance();
    void testSettingsChangeSignalling();
    void testChangeSignallingOfOtherSettingsInstance();
    void testCustomSettingsChangeSignalling();
    void testChangeSignallingOfOtherCustomSettingsInstance();
    void testSignallingOfSettingsChangeInOtherProcess();
    void testSignallingOfSettingsChangeInOtherProcess_data();

private:
    void setQctSettingsNeutralValues(QctSettings &settings) const;
    void setQctSettingsTestValues(QctSettings &settings) const;
    void setTestSettingsNeutralValues(TestSettings &settings) const;
    void setTestSettingsTestValues(TestSettings &settings) const;

    void checkQctSettingsNeutralValues(const QctSettings &settings) const;
    void checkQctSettingsTestValues(const QctSettings &settings) const;
    void checkTestSettingsNeutralValues(const TestSettings &settings) const;
    void checkTestSettingsTestValues(const TestSettings &settings) const;

    void checkValuesChangedSignal(QSignalSpy &valuesChangedSpy,
                                  const QString &key, const QVariant &referenceValue) const;

    void checkQctSettingsSignalling(QctSettings &settings,
                                    QSignalSpy &valuesChangedSpy) const;
    void checkTestSettingsSignalling(TestSettings &settings,
                                     QSignalSpy &valuesChangedSpy) const;


private:
    SettingsData mSettingsDataBackup;
};

#endif /* UT_QTCONTACTS_TRACKERPLUGIN_GROUPS_H */
