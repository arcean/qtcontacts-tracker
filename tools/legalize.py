#!/usr/bin/python

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

import datetime, mimetypes, os, re, sys

license = """
This file is part of QtContacts tracker storage plugin

Copyright (c) %(copyyears)s Nokia Corporation and/or its subsidiary(-ies).

Contact:  Nokia Corporation (info@qt.nokia.com)

GNU Lesser General Public License Usage
This file may be used under the terms of the GNU Lesser General Public License
version 2.1 as published by the Free Software Foundation and appearing in the
file LICENSE.LGPL included in the packaging of this file.  Please review the
following information to ensure the GNU Lesser General Public License version
2.1 requirements will be met:
http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.

In addition, as a special exception, Nokia gives you certain additional rights.
These rights are described in the Nokia Qt LGPL Exception version 1.1, included
in the file LGPL_EXCEPTION.txt in this package.

Other Usage
Alternatively, this file may be used in accordance with the terms and
conditions contained in a signed written agreement between you and Nokia.
""".strip()

thisyear = datetime.date.today().year

srcdirs = [
    os.path.join(os.path.dirname(__file__), '..', 'src'),
    os.path.join(os.path.dirname(__file__), '..', 'tests'),
    os.path.join(os.path.dirname(__file__), '..', 'tools'),
    os.path.join(os.path.dirname(__file__), '..', 'benchmarks'),
]

srcdirs = map(os.path.relpath, sys.argv[1:] or srcdirs)

mimetypes.add_type('application/octet-stream', '.gcda')
mimetypes.add_type('application/octet-stream', '.gcno')
mimetypes.add_type('application/vnd.nokia.qt.qmakeprofile', '.prf')
mimetypes.add_type('application/vnd.nokia.qt.qmakeprofile', '.pri')
mimetypes.add_type('application/vnd.nokia.qt.qmakeprofile', '.pro')
mimetypes.add_type('text/x-sparql', '.rq')
mimetypes.add_type('text/x-turtle', '.ttl')

mimemagics = (
    (re.compile(r'.ELF.*'), 'application/x-executable'),
    (re.compile(r'#!\s*/bin/bash\b.*'), 'application/x-shellscript'),
)

reignore = re.compile(r'Makefile(\..*)*|build-stamp\..*|.*\.prl|README|moc_.*\.cpp|.*\.skip|\.gitignore')
recopyright = re.compile(r'Copyright\s+\(C\)\s+(\d+)\b(.*)', re.IGNORECASE)
retrailingspace = re.compile(r'\s+\n')


def fixheader(header, separator, prefix=None, suffix=None):
    copyright = recopyright.search(header)

    if not copyright:
        print '%s: No copyright information' % filepath
        return

    if copyright.group(2).find('Nokia') < 0:
        print '%s: Nokia doesn\'t own copyright' % filepath
        return

    firstyear = int(copyright.group(1))
    copyyears = sorted(set([firstyear, thisyear]))
    copyyears = '-'.join(map(str, copyyears))

    prefix = (prefix or '') + separator
    suffix = (suffix or '')

    separator = '\n' + separator

    header = separator.join(license.split('\n')) % vars()
    header = prefix + header + suffix
    header = retrailingspace.sub('\n', header) + '\n\n'

    return header


class legalize:
    @staticmethod
    def cxx(filepath, text):
        header = text.find('*/')

        if header < 0:
            print '%s: No license header' % filepath
            return

        code = text[header+2:].lstrip()
        header = text[:header].lstrip()

        if not header.startswith('/*'):
            print '%s: No license header' % filepath
            return

        hline = '*' * max(len(l) for l in license.split('\n'))
        header = fixheader(header, ' ** ', '/**' + hline + '\n', '\n ' + hline + '**/')

        if not header:
            return

        return header + code

    @staticmethod
    def shell(filepath, text):
        lines = text.split('\n')
        shebang = ''
        header = ''

        if lines[0].startswith('#!'):
            shebang = lines.pop(0) + '\n\n'

            while lines:
                if lines[0].strip():
                    break

                lines.pop(0)

        while lines:
            if not lines[0].startswith('#'):
                break

            header += lines.pop(0)
            header += '\n'

        if not header:
            print '%s: No license header' % filepath
            return

        code = '\n'.join(lines).lstrip()
        header = fixheader(header, '# ')

        if not header:
            return

        return shebang + header + code

    @staticmethod
    def skip(filepath, text):
        pass

handlers = {
    'text/x-chdr': legalize.cxx,
    'text/x-c++src': legalize.cxx,

    'application/vnd.nokia.qt.qmakeprofile': legalize.shell,
    'application/x-shellscript': legalize.shell,
    'text/x-sh': legalize.shell,
    'text/x-python': legalize.shell,

    'application/octet-stream': legalize.skip,
    'application/x-executable': legalize.skip,
    'application/x-object': legalize.skip,
    'application/xml': legalize.skip,
    'text/html': legalize.skip,
    'text/x-sparql': legalize.skip,
    'text/x-turtle': legalize.skip,
    'text/x-moc': legalize.skip,
    'text/x-vcard': legalize.skip,
}

for src in srcdirs:
    for dirpath, subdirs, files in os.walk(src):
        for filename in files:
            if reignore.match(filename):
                continue

            filepath = os.path.join(dirpath, filename)
            mimetype, encoding = mimetypes.guess_type(filepath)

            if not mimetype:
                magic = open(filepath).read(20)

                for r, t in mimemagics:
                    if r.match(magic):
                        mimetype = t
                        break

            if not mimetype:
                print '%s: Skipping file of unknown type' % filepath
                continue

            legalize = handlers.get(mimetype)

            if not legalize:
                print '%s: No legalize handler for %s MIME type' % (filepath, mimetype)
                continue

            oldtext = open(filepath).read()
            newtext = legalize(filepath, oldtext)

            if newtext and oldtext != newtext:
                print '%s: Updating license header' % filepath
                file(filepath, 'w').write(newtext)
