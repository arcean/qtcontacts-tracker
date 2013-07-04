# This file is part of QtContacts tracker storage plugin
#
# Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
#
# Contact:  Nokia Corporation (info@qt.nokia.com)
#
# GNU Lesser General Public License Usage
# This file may be used under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation and appearing in the
# file LICENSE.LGPL included in the packaging of this file.  Please review the
# following information to ensure the GNU Lesser General Public License version
# 2.1 requirements will be met:
# http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
#
# In addition, as a special exception, Nokia gives you certain additional rights.
# These rights are described in the Nokia Qt LGPL Exception version 1.1, included
# in the file LGPL_EXCEPTION.txt in this package.
#
# Other Usage
# Alternatively, this file may be used in accordance with the terms and
# conditions contained in a signed written agreement between you and Nokia.

include(../ut_qtcontacts_trackerplugin_common/ut_qtcontacts_trackerplugin_common.pri)

QT += xml

DEFINES += SRCDIR='\\"$$PWD\\"'
SOURCES += ut_qtcontacts_trackerplugin_querybuilder.cpp
HEADERS += ut_qtcontacts_trackerplugin_querybuilder.h

testdata.files += \
    data/000-contacts.ttl \
    data/000-contacts.xml \
    data/001-minimal-contacts.ttl \
    data/100-Contact.rq \
    data/100-Group.rq \
    data/101-Contact-Address.rq \
    data/101-Group-Address.rq \
    data/102-Contact-Anniversary.rq \
    data/102-Group-Anniversary.rq \
    data/103-Contact-Birthday.rq \
    data/103-Group-Birthday.rq \
    data/104-Contact-EmailAddress.rq \
    data/104-Group-EmailAddress.rq \
    data/105-Contact-Family.rq \
    data/106-Contact-Favorite.rq \
    data/107-Contact-Gender.rq \
    data/107-Group-Gender.rq \
    data/108-Contact-GeoLocation.rq \
    data/108-Group-GeoLocation.rq \
    data/109-Contact-Guid.rq \
    data/109-Group-Guid.rq \
    data/110-Contact-Hobby.rq \
    data/110-Group-Hobby.rq \
    data/111-Contact-Name.rq \
    data/111-Group-Name.rq \
    data/112-Contact-Nickname.rq \
    data/112-Group-Nickname.rq \
    data/113-Contact-Note.rq \
    data/113-Group-Note.rq \
    data/114-Contact-OnlineAccount.rq \
    data/114-Group-OnlineAccount.rq \
    data/115-Contact-OnlineAvatar.rq \
    data/115-Group-OnlineAvatar.rq \
    data/116-Contact-Organization.rq \
    data/116-Group-Organization.rq \
    data/117-Contact-PersonalAvatar.rq \
    data/117-Group-PersonalAvatar.rq \
    data/118-Contact-PhoneNumber.rq \
    data/118-Group-PhoneNumber.rq \
    data/119-Contact-Presence.rq \
    data/119-Group-Presence.rq \
    data/120-Contact-Relevance.rq \
    data/120-Group-Relevance.rq \
    data/121-Contact-Ringtone.rq \
    data/121-Group-Ringtone.rq \
    data/122-Contact-SocialAvatar.rq \
    data/122-Group-SocialAvatar.rq \
    data/123-Contact-SyncTarget.rq \
    data/123-Group-SyncTarget.rq \
    data/124-Contact-Tag.rq \
    data/124-Group-Tag.rq \
    data/125-Contact-Timestamp.rq \
    data/125-Group-Timestamp.rq \
    data/126-Contact-Url.rq \
    data/126-Group-Url.rq \
    data/200-remove-request.rq \
    data/202-save-request-1.rq \
    data/202-save-request-1-new.rq \
    data/202-save-request-2.rq \
    data/202-save-request-2-new.rq \
    data/202-save-request-3.rq \
    data/202-save-request-4.rq \
    data/202-save-request-5.rq \
    data/202-save-request-6.rq \
    data/203-garbage-collection.rq \
    data/250-localIdFetchRequest-1.rq \
    data/250-localIdFetchRequest-2.rq \
    data/250-localIdFetchRequest-3.rq \
    data/250-localIdFetchRequest-4.rq \
    data/250-localIdFetchRequest-5.rq \
    data/250-localIdFetchRequest-6.rq \
    data/300-localContactIdFilter.rq \
    data/300-localContactIdFilter-1.xml \
    data/300-localContactIdFilter-2.xml \
    data/300-localContactIdFilter-3.xml \
    data/300-localContactIdFilter-4.xml \
    data/300-localContactIdFilter-5.xml \
    data/300-localContactIdFilter-6.xml \
    data/301-testIntersectionFilter.rq \
    data/301-testIntersectionFilter.xml \
    data/302-testUnionFilter.rq \
    data/302-testUnionFilter.xml \
    data/303-testDetailFilter-1.rq \
    data/303-testDetailFilter-2.rq \
    data/303-testDetailFilter-3.rq \
    data/303-testDetailFilter-4.rq \
    data/303-testDetailFilter-5.rq \
    data/303-testDetailFilter-6.rq \
    data/303-testDetailFilter-7.rq \
    data/303-testDetailFilter-8.rq \
    data/303-testDetailFilter-9.rq \
    data/304-testDetailRangeFilter.rq \
    data/305-testChangeLogFilter.rq \
    data/306-testRelationshipFilter-1.rq \
    data/306-testRelationshipFilter-2.rq

OTHER_FILES += $$testdata.files
