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

#include "customdetails.h"

QTM_BEGIN_NAMESPACE

Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityTextChat, "TextChat");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityMediaCalls, "MediaCalls");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityAudioCalls, "AudioCalls");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityVideoCalls, "VideoCalls");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityUpgradingCalls, "UpgradingCalls");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityFileTransfers, "FileTransfers");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityStreamTubes, "StreamTubes");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__CapabilityDBusTubes, "DBusTubes");

Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__FieldAccountPath, "AccountPath");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAccount__FieldProtocol, "Protocol");

Q_DEFINE_LATIN1_CONSTANT(QContactPresence__FieldAuthStatusFrom, "AuthStatusFrom");
Q_DEFINE_LATIN1_CONSTANT(QContactPresence__FieldAuthStatusTo, "AuthStatusTo");
Q_DEFINE_LATIN1_CONSTANT(QContactPresence__AuthStatusYes, "Yes");
Q_DEFINE_LATIN1_CONSTANT(QContactPresence__AuthStatusRequested, "Requested");
Q_DEFINE_LATIN1_CONSTANT(QContactPresence__AuthStatusNo, "No");

#ifndef QCONTACTRELEVANCE_H

Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactRelevance, "Relevance");
Q_DEFINE_LATIN1_CONSTANT(QContactRelevance::FieldRelevance, "Relevance");

#endif // QCONTACTRELEVANCE_H

Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactPersonalAvatar, "PersonalAvatar");
Q_DEFINE_LATIN1_CONSTANT(QContactPersonalAvatar::FieldImageUrl, "ImageUrl");
Q_DEFINE_LATIN1_CONSTANT(QContactPersonalAvatar::FieldVideoUrl, "VideoUrl");

QContactAvatar
QContactPersonalAvatar::toAvatar() const
{
    QContactAvatar avatar;

    if (this->hasValue(FieldImageUrl)) {
        avatar.setImageUrl(this->imageUrl());
    }

    if (this->hasValue(FieldVideoUrl)) {
        avatar.setVideoUrl(this->videoUrl());
    }

    avatar.setValue(QContactAvatar__FieldSubType, QContactAvatar__SubTypePersonal);

    return avatar;
}

Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactOnlineAvatar, "OnlineAvatar");
Q_IMPLEMENT_CUSTOM_CONTACT_DETAIL(QContactSocialAvatar, "SocialAvatar");

Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAvatar::FieldImageUrl, "ImageUrl");
Q_DEFINE_LATIN1_CONSTANT(QContactOnlineAvatar::FieldSubType, "SubType");

Q_DEFINE_LATIN1_CONSTANT(QContactSocialAvatar::FieldImageUrl, "ImageUrl");
Q_DEFINE_LATIN1_CONSTANT(QContactSocialAvatar::FieldSubType, "SubType");

Q_DEFINE_LATIN1_CONSTANT(QContactAvatar__FieldSubType, "SubType");
Q_DEFINE_LATIN1_CONSTANT(QContactAvatar__SubTypePersonal, "Personal");
Q_DEFINE_LATIN1_CONSTANT(QContactAvatar__SubTypeOnline, "Online");
Q_DEFINE_LATIN1_CONSTANT(QContactAvatar__SubTypeLarge, "Large");

Q_DEFINE_LATIN1_CONSTANT(QContactTag__FieldDescription, "Description");

Q_DEFINE_LATIN1_CONSTANT(QContactTimestamp__FieldAccessedTimestamp, "AccessedTimestamp");

Q_DEFINE_LATIN1_CONSTANT(QContactUrl__SubTypeBlog, "Blog");

Q_DEFINE_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderFirstName, "first-last");
Q_DEFINE_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderLastName, "last-first");
Q_DEFINE_LATIN1_CONSTANT(QContactDisplayLabel__FieldOrderNone, "none");

Q_DEFINE_LATIN1_CONSTANT(QContactDisplayLabel__FirstNameLastNameOrder, "first-last");
Q_DEFINE_LATIN1_CONSTANT(QContactDisplayLabel__LastNameFirstNameOrder, "last-first");

Q_DEFINE_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetAddressBook, "addressbook");
Q_DEFINE_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetTelepathy, "telepathy");
Q_DEFINE_LATIN1_CONSTANT(QContactSyncTarget__SyncTargetMfe, "mfe");

QTM_END_NAMESPACE
