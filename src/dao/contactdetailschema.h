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

#ifndef QTRACKERCONTACTDETAILSCHEMA_H
#define QTRACKERCONTACTDETAILSCHEMA_H

#include <qtcontacts.h>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetail;

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef QMap<QString, QContactDetailDefinition>         QContactDetailDefinitionMap;
typedef QMap<QString, QContactDetailFieldDefinition>    QContactDetailFieldDefinitionMap;
typedef QMap<QString, QTrackerContactDetail>            QTrackerContactDetailMap;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailSchemaData;
class QTrackerContactDetailSchema
{
public: // constants
    enum AvatarType {
        PersonalAvatar  = (1 << 0),
        OnlineAvatar    = (1 << 1),
        SocialAvatar    = (1 << 2)
    };

    Q_DECLARE_FLAGS(AvatarTypes, AvatarType)

    static const int Version = 2;

protected: // constructors
    explicit QTrackerContactDetailSchema(QTrackerContactDetailSchemaData *data);

public: // destructor
    virtual ~QTrackerContactDetailSchema();

public: // operators
    QTrackerContactDetailSchema(const QTrackerContactDetailSchema &other);
    QTrackerContactDetailSchema & operator=(const QTrackerContactDetailSchema &other);

public: // attributes
    const QString & contactType() const;

    const QTrackerContactDetailMap & details() const;
    const QTrackerContactDetail * detail(const QString &name) const;

    template<class Detail>
    const QTrackerContactDetail * detail() const
    {
        return detail(Detail::DefinitionName);
    }

    /// The RDF classes this contact type needs.
    const QStringList & contactClassIris() const;
    QSet<QString> requiredResourceIris() const;

    const QContactDetailDefinitionMap & detailDefinitions() const;
    QSet<QVariant::Type> supportedDataTypes() const;

    void setWriteBackPresence(bool setting);
    bool writeBackPresence() const;

    void setConvertNumbersToLatin(bool setting);
    bool convertNumbersToLatin() const;

    AvatarTypes avatarTypes() const;

    static bool isSyntheticDetail(const QString &name);
    static const QSet<QString> & syntheticDetails();

public: // methods
    QContactDetailDefinition describe(const QString &name) const;

private: // fields
    QExplicitlySharedDataPointer<QTrackerContactDetailSchemaData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerPersonContactDetailSchema : public QTrackerContactDetailSchema
{
public:
    explicit QTrackerPersonContactDetailSchema(AvatarTypes avatarTypes);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactGroupDetailSchema : public QTrackerContactDetailSchema
{
public:
    explicit QTrackerContactGroupDetailSchema(AvatarTypes avatarTypes);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_DECLARE_OPERATORS_FOR_FLAGS(QTrackerContactDetailSchema::AvatarTypes)

#endif // QTRACKERCONTACTDETAILSCHEMA_H
