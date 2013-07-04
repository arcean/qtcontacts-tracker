# This file is part of QtContacts tracker storage plugin
#
# Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

TARGET = ut_qtcontacts_trackerplugin

DEFINES -= QT_NO_CAST_FROM_ASCII=1 # FIXME: later - 806 errors are way too much for me right now
DEFINES += SRCDIR='\\"$$PWD\\"'

HEADERS += ut_qtcontacts_trackerplugin.h
SOURCES += ut_qtcontacts_trackerplugin.cpp

testdata.files += \
    data/avatars.ttl \
    data/contacts.vcf \
    data/fullname.ttl \
    data/fullname.vcf \
    data/preserve-uid.vcf \
    data/N900/adam-adelle.vcf \
    data/N900/adrian-addy.vcf \
    data/N900/alexandra-alehta.vcf \
    data/N900/amity-anderson.vcf \
    data/N900/cecily-howard.vcf \
    data/N900/elizabeth-leas.vcf \
    data/N900/frideswide-holt.vcf \
    data/N900/fulke-mayberry.vcf \
    data/N900/isabel-allen.vcf \
    data/N900/jerome-margan.vcf \
    data/N900/joyce-engell.vcf \
    data/N900/ralph-lamgley.vcf \
    data/N900/sybil-wilbar.vcf \
    data/N900/tobias-ansley.vcf \
    data/NB153234-N900.vcf \
    data/NB153234-N900.xml \
    data/NB153234-S60.vcf \
    data/NB153234-S60.xml \
    data/NB174588.vcf \
    data/NB174588.xml \
    data/NB173388-with-thumb.vcf \
    data/NB183073.vcf \
    data/NB191670.vcf \
    data/NB193261-S60.vcf \
    data/NB193304.vcf \
    data/NB198749.vcf \
    data/NB214563.vcf \
    data/NB225606.vcf \
    data/NB229032.vcf \
    data/NB248183.vcf \
    data/test-account-1.rq

OTHER_FILES += $$testdata.files
