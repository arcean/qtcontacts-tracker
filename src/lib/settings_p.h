/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef QCTSETTINGS_P_H
#define QCTSETTINGS_P_H

#include <QCoreApplication>
#include <QSettings>
#include <QStringList>
#include <QFileSystemWatcher>

/**
 * The basic instance which checks for updates in the storage and signals them.
 * It does so by watching the file used by the central QSettings instance to store the data
 * and by keeping a working copy of the current settings used in the process.
 * On a change of the file all settings are compared between file and working copy,
 * and those which have changed are emitted in a signal with the new values.
 * To know about changes to all settings, the default values must be known in advance.
 * Therefore they need to be registered. In the current implementation this is done in
 * each constructor of the wrapper settings class QctSettings and its subclasses.
 **/
class QctSettingsSingleton : public QObject
{
    Q_OBJECT

public:
    QctSettingsSingleton();

public:
    void registerSetting(const QString& key, const QVariant &defaultValue);
    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key) const;
    void sync();

signals:
    void valuesChanged(const QHash<QString,QVariant> &changedSettings);

private slots:
    void onStoredSettingsChanged();

private:
    /// proxy to the stored settings
    QSettings m_storedSettings;
    /// working copy of settings
    QHash<QString,QVariant> m_localSettings;
};


inline void
QctSettingsSingleton::setValue(const QString &key, const QVariant &value)
{
    // first note new setting locally...
    m_localSettings[key] = value;
    // then write to storage
    m_storedSettings.setValue(key, value);

    // signal change to all in this process
    QHash<QString,QVariant> changedSettings;
    changedSettings.insert(key, value);
    emit valuesChanged(changedSettings);
}

inline QVariant
QctSettingsSingleton::value(const QString &key) const
{
    return m_localSettings.value(key);
}

#endif // QCTSETTINGS_P_H
