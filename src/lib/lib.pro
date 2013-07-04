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

TEMPLATE = lib
CONFIG += mobility lib create_prl qtsparql qtsparql-tracker-extensions
MOBILITY += contacts

QT += dbus
# no need for sysinfo, libcreds and cellular-qt in this library (dragged in
# from common.pri)
PKGCONFIG -= sysinfo
CONFIG -= mssf-qt cellular-qt gui

TARGET = qtcontacts_extensions_tracker

DEFINES += LIBQTCONTACTS_EXTENSIONS_TRACKER_LIBRARY

QCONTACTS_EXTENSIONS_TRACKER_PUBLIC_HEADERS = \
    avatarutils.h \
    constants.h \
    contactlocalidfetchrequest.h \
    contactmergerequest.h \
    customdetails.h \
    fileutils.h \
    garbagecollector.h \
    libqtcontacts_extensions_tracker_global.h \
    phoneutils.h \
    presenceutils.h \
    requestextensions.h \
    resourcecache.h \
    settings.h \
    sparqlconnectionmanager.h \
    sparqlresolver.h \
    threadutils.h \
    trackerchangelistener.h \
    unmergeimcontactsrequest.h

QCONTACTS_EXTENSIONS_TRACKER_PRIVATE_HEADERS = \
    logger.h \
    metatypedcontactdetail_p.h \
    queue.h \
    resolvertask.h \
    settings_p.h \
    threadlocaldata.h

HEADERS += \
    $$QCONTACTS_EXTENSIONS_TRACKER_PUBLIC_HEADERS \
    $$QCONTACTS_EXTENSIONS_TRACKER_PRIVATE_HEADERS

SOURCES += \
    avatarutils.cpp \
    constants.cpp \
    contactlocalidfetchrequest.cpp \
    contactmergerequest.cpp \
    customdetails.cpp \
    fileutils.cpp \
    garbagecollector.cpp \
    logger.cpp \
    phoneutils.cpp \
    presenceutils.cpp \
    queue.cpp \
    requestextensions.cpp \
    resolvertask.cpp \
    resourcecache.cpp \
    settings.cpp \
    sparqlconnectionmanager.cpp \
    sparqlresolver.cpp \
    threadlocaldata.cpp \
    trackerchangelistener.cpp \
    unmergeimcontactsrequest.cpp

OTHER_FILES += \
    lib.pri

QCT_INSTALLPATH_HEADERS = $$INCLUDEDIR/qtcontacts-tracker
QCT_INSTALLPATH_LIBS = $$LIBDIR

target.path = $$QCT_INSTALLPATH_LIBS

qcextt_public_headers.path = $$QCT_INSTALLPATH_HEADERS
qcextt_public_headers.files = $$QCONTACTS_EXTENSIONS_TRACKER_PUBLIC_HEADERS

install_prf.path = $$[QT_INSTALL_DATA]/mkspecs/features
install_prf.files = qtcontacts_extensions_tracker.prf

INSTALLS += \
    target \
    qcextt_public_headers \
    install_prf

include(../cubi.pri)

# Prepends the PWD to all filenames in the passed list and returns the modified list.
defineReplace(addprefix) {
    return($$join(1, " $$2/", "$$2/"))
}

check_public_headers.target = check_public_headers.stamp
check_public_headers.depends = $$addprefix($$QCONTACTS_EXTENSIONS_TRACKER_PUBLIC_HEADERS, $$PWD)
check_public_headers.commands = \
    $$PWD/check_public_headers.py \
    --srcdir=$$PWD --stamp=$$check_public_headers.target \
    $$QCONTACTS_EXTENSIONS_TRACKER_PRIVATE_HEADERS -- \
    $$QCONTACTS_EXTENSIONS_TRACKER_PUBLIC_HEADERS

PRE_TARGETDEPS += $$check_public_headers.target
QMAKE_EXTRA_TARGETS += check_public_headers
OTHER_FILES += check_public_headers.py
