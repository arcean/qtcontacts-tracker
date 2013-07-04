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

include(../../src/common.pri)

TESTDATADIR = $$PREFIX/share/$$PACKAGE-tests/$$TARGET

CONFIG += mobility testcase
MOBILITY += contacts versit
QT += testlib

DEFINES += DATADIR='\\"$$TESTDATADIR\\"'
INSTALLS += target testdata

LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_BUILDPATH = $$TOP_BUILDDIR/tests/ut_qtcontacts_trackerplugin_common
LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_SOURCEPATH = $$TOP_SOURCEDIR/tests/ut_qtcontacts_trackerplugin_common
LIBS += $$LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_BUILDPATH/libut_qtcontacts_trackerplugin_common.a

DEPENDPATH *= $$LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_BUILDPATH
INCLUDEPATH *= $$LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_SOURCEPATH

libut_qtcontacts_trackerplugin_common.target = build-stamp.libut_qtcontacts_trackerplugin_common
libut_qtcontacts_trackerplugin_common.commands = $$BUILD_OTHER($$LIBUT_QTCONTACTS_TRACKERPLUGIN_COMMON_BUILDPATH,$$PWD)
libut_qtcontacts_trackerplugin_common.depends = $$PWD/*.cpp $$PWD/*.h build-stamp.libplugin

target.path = $$PREFIX/bin

testdata.files =
testdata.path = $$TESTDATADIR

QMAKE_EXTRA_TARGETS += libut_qtcontacts_trackerplugin_common
PRE_TARGETDEPS += build-stamp.libut_qtcontacts_trackerplugin_common

include(../../src/plugin/plugin.pri)

# add support for running the tests with (latest) build without having to install anything:
# add rpath to libqtcontacts_extensions_tracker build dir to linker flags
equals(ENABLE_BUILDDIRS_RPATH,yes):QMAKE_LFLAGS = -Wl,-rpath,$$LIBQCTEXTENSIONS_DIR $$QMAKE_LFLAGS
