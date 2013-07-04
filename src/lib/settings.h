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

#ifndef QCTSETTINGS_H
#define QCTSETTINGS_H

#include "libqtcontacts_extensions_tracker_global.h"

#include <qtcontacts.h>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctSettingsData;
/**
 * QctSettings wraps the storage of basic settings used by the Tracker backend to QtContacts.
 * There can be multiple instances of QctSettings and its subclasses, they share the internal
 * settings engine.
 * To get informed to changes to the settings, connect to the
 * signal @code valuesChanged(QHash<QString,QVariant>) @endcode. and test if settings your code
 * is interested in are part of the changed values and read the new value
 * @code
 * void Handler::onSettingsChanged(const QHash<QString,QVariant> &changedSettings)
 * {
 *     QHash<QString,QVariant>::ConstIterator it = changedSettings.find(someSettingKey);
 *     if (it != changedSettings.constEnd()) {
 *         QVariant newValue = it.value();
 * [...]
 * @endcode
 * Subclasses of QctSettings need to register all settings in the constructor of the subclass,
 * calling @code registerSetting(key, defaultValue); @endcode for each.
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctSettings : public QObject
{
    Q_OBJECT

public: // key constants
    static const QString NumberMatchLengthKey;
    static const QString AvatarSizeKey;
    static const QString NameOrderKey;
    static const QString GuidAlgorithmNameKey;
    static const QString LastMSISDNKey;
    static const QString SparqlBackendsKey;
    static const QString PreferNicknameKey;

public: // default values
    static const int DefaultPhoneNumberLength = 7;
    static const int DefaultAvatarSize = 192; // copied from Harmattan Contacts
    static const QString DefaultNameOrder;
    static const QStringList DefaultSparqlBackends;
    static const bool DefaultPreferNickname = false;

public: // constructors
    explicit QctSettings(QObject *parent = 0);
    /// calls sync() before deconstruction.
    virtual ~QctSettings();

public: // attributes
    /// sets the length of the local part of the phone number
    void setLocalPhoneNumberLength(int length);
    int localPhoneNumberLength() const;

    /// sets the default size of the avatar image, which is square, in count of pixels
    void setAvatarSize(int avatarSize);
    QSize avatarSize() const;

    /// sets the order of firstname and lastname in the displaylabel,
    /// cmp. QContactDisplayLabel__FieldOrderFirstName and QContactDisplayLabel__FieldOrderLastName
    void setNameOrder(const QString &nameOrder);
    QString nameOrder() const;

    /// sets the algorithm to be used for Guid creation
    void setGuidAlgorithmName(const QString &algorithmName);
    QString guidAlgorithmName() const;

    /// the last MSISDN read from SIM card. used for Guid creation
    void setLastMSISDN(const QString &msisdn);
    QString lastMSISDN() const;

    /// sets the SPARQL-backends used by the Tracker backend to QtContacts
    void setSparqlBackends(const QStringList &backends);
    QStringList sparqlBackends() const;

    /// prefer nickname over first and last name when synthesizing a contact's display label.
    void setPreferNickname(bool preferNickname);
    bool preferNickname() const;

public:
    /// does immediate synchronisation with the storage system, instead of automatically only after some timeout.
    void sync();

Q_SIGNALS:
    /// emitted if some of the values changed, @p settings contains all changed settings with the new values.
    void valuesChanged(const QHash<QString,QVariant> &changedSettings);

public: // methods
    /// stores @p value for setting @p key
    void setValue(const QString &key, const QVariant &value);
    /// returns the current value of the settings @p key
    QVariant value(const QString &key) const;
    /// @deprecated: defaultvalue needs to be set with registerSetting() instead
    Q_DECL_DEPRECATED QVariant value(const QString &key, const QVariant &defaultValue) const;

protected: // methods
    /// registers @p key as used, so it is regarded if looking for changes in the storage
    void registerSetting(const QString &key, const QVariant &defaultValue = QVariant());

private: // data
    QExplicitlySharedDataPointer<QctSettingsData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QCTSETTINGS_H
