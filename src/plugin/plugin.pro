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

# link with libengine.pri
include(../engine/engine.pri)

TEMPLATE = lib
CONFIG += plugin
TARGET = $$qtLibraryTarget(qtcontacts_tracker)
PLUGIN_TYPE = contacts

HEADERS += factory.h
SOURCES += factory.cpp
OTHER_FILES += plugin.pri

target.path = $$[QT_INSTALL_PLUGINS]/contacts
INSTALLS += target

# add support for running the benchmarks with (latest) build without having to install anything:
# add rpath to libqtcontacts_extensions_tracker build dir to linker flags
equals(ENABLE_BUILDDIRS_RPATH,yes):QMAKE_LFLAGS = -Wl,-rpath,$$LIBQCTEXTENSIONS_DIR $$QMAKE_LFLAGS

QMAKE_CXXFLAGS -= -Wmissing-declarations
