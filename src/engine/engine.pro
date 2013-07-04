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

include(../common.pri)

QMAKE_CXXFLAGS *= $$system(pkg-config --cflags-only-I tracker-sparql-0.10)

TEMPLATE = lib
CONFIG += mobility staticlib plugin create_prl qtsparql
MOBILITY = contacts

INCLUDEPATH += ..
DEPENDPATH += ..

HEADERS += \
    abstractcontactfetchrequest.h \
    abstractrequest.h \
    basecontactfetchrequest.h \
    baserequest.h \
    contactcopyandremoverequest.h \
    contactfetchrequest.h \
    contactfetchbyidrequest.h \
    contactidfetchrequest.h \
    contactremoverequest.h \
    contactsaverequest.h \
    contactunmergerequest.h \
    detaildefinitionfetchrequest.h \
    displaylabelgenerator.h \
    engine.h \
    engine_p.h \
    guidalgorithm.h \
    relationshipfetchrequest.h \
    relationshipremoverequest.h \
    relationshipsaverequest.h \
    tasks.h

SOURCES += \
    abstractcontactfetchrequest.cpp \
    abstractrequest.cpp \
    contactcopyandremoverequest.cpp \
    contactfetchrequest.cpp \
    contactfetchbyidrequest.cpp \
    contactidfetchrequest.cpp \
    contactremoverequest.cpp \
    contactsaverequest.cpp \
    contactunmergerequest.cpp \
    detaildefinitionfetchrequest.cpp \
    displaylabelgenerator.cpp \
    engine.cpp \
    guidalgorithm.cpp \
    relationshipfetchrequest.cpp \
    relationshipremoverequest.cpp \
    relationshipsaverequest.cpp \
    tasks.cpp

OTHER_FILES += \
    engine.pri

include(../cubi.pri)
include(../lib/lib.pri)
