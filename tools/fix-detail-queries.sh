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

testname=ut_qtcontacts_trackerplugin_querybuilder
srcdir=tests/$testname
project="$srcdir/$testname.pro"
offset=100

for detail in `sed -ne 's:.*test\(\w\+\)Detail().*:\1:p' $srcdir/$testname.h`
do
    offset=$[$offset + 1]
    for type in Contact Group
    do
        filename="$srcdir/data/$offset-$type-$detail.rq"
        test -f "$filename" && continue

        wildcard="$srcdir/data/*-$type-$detail.rq"

        if test -f $wildcard
        then
            git mv $wildcard "$filename"
            continue
        fi

        echo "no reference file: $filename"
    done
done

for filename in $srcdir/data/*.rq
do
    basename=`basename "$filename"`

    if ! fgrep -q "data/$basename" $project
    then
        detail=`echo $basename | sed 's/^[0-9]\+-\(.*\)\.rq/\1/'`
        sed -i -e "s:data/[0-9]\\+-$detail\.rq:data/$basename:" $project
    fi

    fgrep -q "data/$basename" $project && continue

    echo "not in project file: $filename"
done

sed -ne 's:data/\(1[0-9]\+\)-[A-Za-z]\+-\(.*\)\.rq.*:\1 \2:p' $project |
sort -u | while read offset name
do
    fgrep -q "verifyContactDetail($offset, QContact$name::DefinitionName);" $srcdir/$testname.cpp && continue
    echo "not in unit test: data/$offset-*-$name.rq"
done

