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

include(../../src/common.pri)

TARGET = ut_qtcontacts_trackerplugin_settings_helper
TEMPLATE = app
QT += core

INCLUDEPATH += ../../src
DEPENDPATH += ../../src

INSTALLS += target
target.path = $$PREFIX/bin

SOURCES += \
    testsettings.cpp \
    ut_qtcontacts_trackerplugin_settings_helper.cpp

HEADERS += \
    testsettings.h

include(../../src/lib/lib.pri)

# add support for running the test with (latest) build without having to install anything:
# add rpath to libqtcontacts_extensions_tracker build dir to linker flags
QMAKE_LFLAGS = -Wl,-rpath,$$LIBQCTEXTENSIONS_DIR $$QMAKE_LFLAGS
