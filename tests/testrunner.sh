#!/bin/sh

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

# figure out environment
srcdir=`dirname "$0"`
testsuite="${1:-${srcdir}/tests-local.xml}"
logfile="`basename "$testsuite" .xml`.log"
report="`basename "$logfile" .log`.html"

# run selected test suite
testrunner-lite -f "$testsuite" -o "$logfile" -v

# print test stats
"$srcdir/teststats.py" "$logfile"

# generate HTML report
xsltproc "$srcdir/testresults.xsl" "$logfile" > "$report"
printf "Check \033[1m%s\033[0m for pretty printed results.\n" "$report"

