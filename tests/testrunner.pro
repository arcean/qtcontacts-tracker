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

TARGET = $(null)
INSTALLS += install_tests
OTHER_FILES = mktests.sh

QMAKE_EXTRA_TARGETS += tests_packaged tests_local all tests_memcheck all
QMAKE_DISTCLEAN += $$tests_packaged.target $$tests_local.target $$tests_memcheck.target

testdirs=$$files($${PWD}/ut_*)

for(d,testdirs) {
    t=$$basename(d)

    !exists($${d}/$${t}.skip) {
        fixtures += $${t}/$${t}
    }
}

tests_packaged.target = tests.xml
tests_packaged.commands = $$PWD/mktests.sh -v $$VERSION -s $$TOP_SOURCEDIR -b $$TOP_BUILDDIR -t 50:300:1800 $$files(ut_*) > $@ || rm -f $@
tests_packaged.depends = $$fixtures $$PWD/mktests.sh

tests_local.target = tests-local.xml
tests_local.commands = $$PWD/mktests.sh -v $$VERSION -s $$TOP_SOURCEDIR -b $$TOP_BUILDDIR -l -t 5:30:180 $$files(ut_*) > $@ || rm -f $@
tests_local.depends = $$fixtures $$PWD/mktests.sh

tests_memcheck.target = tests-memcheck.xml
tests_memcheck.commands = $$PWD/mktests.sh -v $$VERSION -s $$TOP_SOURCEDIR -b $$TOP_BUILDDIR -l -m -t 50:300:1800 $$files(ut_*) > $@ || rm -f $@
tests_memcheck.depends = $$fixtures $$PWD/mktests.sh

install_tests.files = $$tests_packaged.target
install_tests.path = $$PREFIX/share/$$PACKAGE-tests
install_tests.depends = $$tests_packaged.target
install_tests.CONFIG = no_check_exist

all.depends = tests_packaged tests_local tests_memcheck
all.CONFIG = phony
