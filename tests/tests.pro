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

include(../src/common.pri)

testdirs = $$files($${PWD}/ut_*)

for(subdir,testdirs) {
    test = $$basename(subdir)

    !exists($${subdir}/$${test}.skip) {
        UNITTESTS += sub_$${test}
        export(UNITTESTS)

        eval(sub_$${test}.subdir += $${test})
        export(sub_$${test}.subdir)

        eval(sub_$${test}.depends += ut_qtcontacts_trackerplugin_common)
        export(sub_$${test}.depends)
    }
}

TEMPLATE = subdirs
SUBDIRS = ut_qtcontacts_trackerplugin_common testrunner $$UNITTESTS
OTHER_FILES = runtest.sh testresults.xsl testrunner.sh teststats.py gcov-summary.sh

testrunner.file = testrunner.pro
testrunner.depends = $$UNITTESTS