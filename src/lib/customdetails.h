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

#ifndef QCTCUSTOMDETAILS_H
#define QCTCUSTOMDETAILS_H

#include "libqtcontacts_extensions_tracker_global.h"

#include <qtcontacts.h>
#include <QtCore>

QTM_BEGIN_NAMESPACE

#ifndef QCONTACTRELEVANCE_H

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QContactRelevance : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(QContactRelevance, "Relevance")
    Q_DECLARE_LATIN1_CONSTANT(FieldRelevance, "Relevance");

    void setRelevance(double relevance) { setValue(FieldRelevance, relevance); }
    QString relevance() const { return value(FieldRelevance); }
};

#endif // QCONTACTRELEVANCE_H

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QContactPersonalAvatar : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(QContactPersonalAvatar, "PersonalAvatar")
    Q_DECLARE_LATIN1_CONSTANT(FieldImageUrl, "ImageUrl");
    Q_DECLARE_LATIN1_CONSTANT(FieldVideoUrl, "VideoUrl");

    void setImageUrl(const QUrl& imageUrl) {setValue(FieldImageUrl, imageUrl);}
    QUrl imageUrl() const {return value<QUrl>(FieldImageUrl);}

    void setVideoUrl(const QUrl& videoUrl) {setValue(FieldVideoUrl, videoUrl);}
    QUrl videoUrl() const {return value<QUrl>(FieldVideoUrl);}

    QContactAvatar toAvatar() const;
};

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QContactOnlineAvatar : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(QContactOnlineAvatar, "OnlineAvatar")
    Q_DECLARE_LATIN1_CONSTANT(FieldImageUrl, "ImageUrl");
    Q_DECLARE_LATIN1_CONSTANT(FieldSubType, "SubType");

    void setImageUrl(const QUrl& imageUrl) {setValue(FieldImageUrl, imageUrl);}
    QUrl imageUrl() const {return value<QUrl>(FieldImageUrl);}

    void setSubType(const QString& subType) {setValue(FieldSubType, subType);}
    QString subType() const {return value<QString>(FieldSubType);}
};

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QContactSocialAvatar : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(QContactSocialAvatar, "SocialAvatar")
    Q_DECLARE_LATIN1_CONSTANT(FieldImageUrl, "ImageUrl");
    Q_DECLARE_LATIN1_CONSTANT(FieldSubType, "SubType");

    void setImageUrl(const QUrl& imageUrl) {setValue(FieldImageUrl, imageUrl);}
    QUrl imageUrl() const {return value<QUrl>(FieldImageUrl);}

    void setSubType(const QString& subType) {setValue(FieldSubType, subType);}
    QString subType() const {return value<QString>(FieldSubType);}
};

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactAvatar__FieldSubType, "SubType");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactAvatar__SubTypePersonal, "Personal");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactAvatar__SubTypeOnline, "Online");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactAvatar__SubTypeLarge, "Large");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityTextChat, "TextChat");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityMediaCalls, "MediaCalls");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityAudioCalls, "AudioCalls");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityVideoCalls, "VideoCalls");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityUpgradingCalls, "UpgradingCalls");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityFileTransfers, "FileTransfers");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityStreamTubes, "StreamTubes");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityDBusTubes, "DBusTubes");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__FieldAccountPath, "AccountPath");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactPresence__FieldAuthStatusFrom, "AuthStatusFrom");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactPresence__FieldAuthStatusTo, "AuthStatusTo");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactPresence__AuthStatusYes, "Yes");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactPresence__AuthStatusRequested, "Requested");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactPresence__AuthStatusNo, "No");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactTag__FieldDescription, "Description");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactTimestamp__FieldAccessedTimestamp, "AccessedTimestamp");

Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderFirstName, "first-last");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderLastName, "last-first");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderNone, "none");

// this deprecated declarations will removed one day
Q_DECL_DEPRECATED Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactOnlineAccount__FieldProtocol, "Protocol");
Q_DECL_DEPRECATED Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactDisplayLabel__FirstNameLastNameOrder, "first-last");
Q_DECL_DEPRECATED Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactDisplayLabel__LastNameFirstNameOrder, "last-first");
Q_DECL_DEPRECATED Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactUrl__SubTypeBlog, "Blog");

// common sync targets
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetAddressBook, "addressbook");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetTelepathy, "telepathy");
Q_DECLARE_EXTERN_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetMfe, "mfe");

QTM_END_NAMESPACE

#endif // QCTCUSTOMDETAILS_H
