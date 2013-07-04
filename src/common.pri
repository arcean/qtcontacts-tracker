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

defineReplace(BUILD_OTHER) {
    isEmpty(3) {
        makefile=Makefile
        project=$$2/$$basename(2).pro
    } else {
        makefile=Makefile.$$3
        project=$$2/$${3}.pro
    }

    return((cd $$1 && { test -f $$makefile || qmake $$project;} && $(MAKE) -f $$makefile;) && touch $@)
}

# =================================================================================================
# Permit the developer to override some settings
# =================================================================================================

exists(../user.pri) {
    include(../user.pri)
}

# =================================================================================================
# Pass some configuration variables as macro
# =================================================================================================

DEFINES *= PACKAGE=\\\"$${PACKAGE}\\\"
DEFINES *= VERSION=\\\"$${VERSION_LABEL}\\\"
DEFINES *= VERSION_MAJOR=$${VERSION_MAJOR}
DEFINES *= VERSION_MINOR=$${VERSION_MINOR}
DEFINES *= VERSION_MICRO=$${VERSION_MICRO}
DEFINES *= QT_NO_CAST_FROM_ASCII=1

equals(ENABLE_CREDENTIALS,yes) {
    DEFINES *= ENABLE_CREDENTIALS=1
    CONFIG *= mssf-qt
    MSSF *= creds
}

equals(ENABLE_CELLULAR,yes) {
    DEFINES *= ENABLE_CELLULAR=1
    CONFIG *= cellular-qt link_pkgconfig
    PKGCONFIG *= sysinfo
}

# =================================================================================================
# Set custom compiler flags
# =================================================================================================

QMAKE_CXXFLAGS *= -Werror=switch -Wmissing-declarations

equals(ENABLE_RTTI,yes): QMAKE_CXXFLAGS *= -frtti
else:equals(ENABLE_RTTI,no): QMAKE_CXXFLAGS *= -fno-rtti

unix:CONFIG *= hide_symbols
