#!/bin/bash

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

# due to the cleverness of the coverage data creation, all coverage
# runs are accumulated in the .gcda files in _common

LCOV_EXCLUDE_PATTERNS="
tests/*
moc_*
*.moc
qt4/*
QtSparql/*
c++/*
"

topdir=`dirname "$0"`
topdir=$(cd "$topdir/.." && pwd)

rm gcov.analysis.summary &> /dev/null

function gcov_summary() {
  find "$topdir/src" -maxdepth 1 -mindepth 1 -type d | while read dir
  do
      find $dir -name "gcov.analysis" -exec cat {} \; | ./gcov.py >> gcov.analysis.summary
  done

  echo -e "\n"
  echo -e "Coverage summary for dirs: "$COVERAGE_TEST_DIRS ": \n"
  cat gcov.analysis.summary
  echo;
}

function lcov_generation() {
    if [ -z $1 ]; then
        LCOV_OUTPUT_DIR="/tmp/coverage"
    else
        test -d "$1" || mkdir -p "$1"
        LCOV_OUTPUT_DIR=$(cd "$1" && pwd)
    fi

    find "$topdir/src" -maxdepth 1 -mindepth 1 -type d | while read dir
    do
        info_dir_short=$(basename "$dir")
        info_file=/tmp/qct_tests-$info_dir_short.info
        addinfo_file=$(echo $info_file | sed -e s,\.info,.addinfo,)
        lcov --directory $dir --capture --output-file "$info_file" -b $dir
        lcov -a "$info_file" -o "$addinfo_file"

        [ -f "$addinfo_file" ] || continue;

        # remove system includes etc.
        for pattern in $LCOV_EXCLUDE_PATTERNS
        do
            lcov -q -r "$addinfo_file" $pattern -o "$addinfo_file"
        done

        # check if the file is empty; genhtml fails on empty files
        lcov -l "$addinfo_file" >/dev/null 2>&1
        if [ $? -eq 255 ]
        then
            rm $addinfo_file
        fi
    done;

    genhtml -o $LCOV_OUTPUT_DIR $(find /tmp -name 'qct_tests-*.addinfo');
    rm /tmp/qct_tests-*info

    echo -e "\n\nNow point your browser to "file://$LCOV_OUTPUT_DIR/index.html"\n\n"
}

if [ "$1" = "lcov" ]; then
    lcov_generation $2
else
    gcov_summary
fi
