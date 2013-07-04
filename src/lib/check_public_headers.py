#!/usr/bin/python

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

from getopt import GetoptError, getopt
import os, re, sys

srcdir = '.'
stamp = None
private_headers = []
public_headers = []

# Parse command line options
##################################################################################################

try:
    opts, args = getopt(sys.argv[1:], "d:s:h", ["srcdir=", "stamp=", "help"])

    for key, value in opts:
        if key in ['-d', '--srcdir']:
            srcdir = value
            continue

        if key in ['-s', '--stamp']:
            stamp = value
            continue

        if key in ['-h', '--help']:
            print """\
Usage: %s [OPTIONS] PRIVATE-HEADERS -- PUBLIC HEADERS

Options:
  -d, --srcdir=PATH   Path of source code folder
  -s, --stamp=PATH    Path of stamp file
  -h, --help          Print usage information
""" % sys.argv[0]

            raise SystemExit

except GetoptError, error:
    print >>sys.stderr, "%s: %s" % (sys.argv[0], error)
    raise SystemExit(2)

try:
    i = args.index('--')
    private_headers = args[:i]
    public_headers = args[i+1:]

except ValueError:
    print >>sys.stderr, "%s: -- separator not found in argument list" % sys.argv[0]
    raise SystemExit(2)

# Check for private headers referenced by public header files.
##################################################################################################

re_private_include = re.compile(r'#\s*include\s+"(.*)"')

def check_private_header_references(filename, text):
    for ref in re.findall(re_private_include, text):
        if ref in private_headers:
            fmtargs = sys.argv[0], filename, ref
            print >>sys.stderr, "%s: %s includes private header file %s" % fmtargs
            raise SystemExit(1)

# Check for QT keywords
##################################################################################################

re_qt_keywords = re.compile(r'\b(signals|slots):')

def check_qt_keywords(filename, text):
        if re.search(re_qt_keywords, text) != None:
            fmtargs = sys.argv[0], filename
            print >>sys.stderr, "%s: %s uses Qt keywords (can't compile with QT_NO_KEYWORDS)" % fmtargs
            raise SystemExit(1)

# Process the files
##################################################################################################

for filename in public_headers:
    path = os.path.join(srcdir, filename)

    try:
        text = file(path, 'r').read()

        check_private_header_references(filename, text)
        check_qt_keywords(filename, text)

    except IOError, error:
        print >>sys.stderr, "%s: %s" % (sys.argv[0], error)
        raise SystemExit(1)

# Add any other useful check here (eg. check symbol names and export declarations)
##################################################################################################

# Create stamp file on success
##################################################################################################
if stamp is not None:
    file(stamp, 'w').close()
