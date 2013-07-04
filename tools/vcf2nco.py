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

import re, sys
from optparse import OptionParser

#############################################################################

def getType(params):
    for p in params:
        if p.startswith('TYPE='):
            return p[5:].split(',')

    return None

ncoAddressProperties = 'nco:pobox', 'nco:extendedAddress', 'nco:streetAddress', 'nco:locality', 'nco:region', 'nco:postalcode', 'nco:country'
ncoNameProperties = 'nco:nameFamily', 'nco:nameGiven', 'nco:nameMiddle', 'nco:namePrefix', 'nco:nameSuffix'

#############################################################################

parser = OptionParser()
parser.add_option('-c', '--count', type='int', dest='count')
parser.add_option('-m', '--mode', type='string', dest='mode')
options, args = parser.parse_args(sys.argv[1:])

input = len(args) > 0 and file(args[0]) or sys.stdin
contact, affiliation = list(), list()
contactId, addressId = 1, 1

if 'save' != options.mode:
    print '@prefix maemo: <http://maemo.org/ontologies/tracker#> .'
    print '@prefix nco: <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#> .'

for line in input:
    line = line.rstrip()

    if 'BEGIN:VCARD' == line:
        if 'save' == options.mode: print '''
DELETE {
<contact:%(contactId)d> a nco:Role .
<affiliation:%(contactId)d> a nco:Role .
<organization:%(contactId)d> a nco:Role .
}
DELETE {
<contact:%(contactId)d> nie:contentLastModified ?d .
} WHERE {
<contact:%(contactId)d> nie:contentLastModified ?d .
}
INSERT {''' % vars()

        contact = ['<contact:%(contactId)d> a nco:PersonContact' % vars(),
                   'nco:contactLocalUID "%(contactId)d"' % vars()]
        affiliation = ['<affiliation:%d> a nco:Affiliation' % contactId]
        continue

    if 'END:VCARD' == line:
        if len(affiliation) > 1:
            contact.append('nco:hasAffiliation <affiliation:%d>' % contactId)
            print ';\n    '.join(affiliation) + '.'

        print ';\n    '.join(contact) + '.'

        if 'save' == options.mode:
            print '}'

        contactId += 1

        if options.count and contactId > options.count:
            break

        continue

    key, value = line.split(':', 1)
    key = key.split(';')

    key, params = key[0], key[1:]
    type = getType(params)

    if 'BDAY' == key:
        contact.append('nco:birthDate "%sT00:00:00Z"^^xsd:dateTime' % value)
        continue
    if 'FN' == key:
        contact.append('nco:fullname "%s"' % value)
        continue
    if 'TITLE' == key:
        affiliation.append('nco:title "%s"' % value)
        continue

    if 'N' == key:
        name = zip(ncoNameProperties, value.split(';'))
        contact += ['%s "%s"' % p for p in name if p[1]]
        continue

    if 'TEL' == key:
        if 'CELL' in type:
            ncoType = 'nco:CellPhoneNumber'
        elif 'VOICE' in type:
            ncoType = 'nco:VoicePhoneNumber'
        else:
            ncoType = 'nco:PhoneNumber'

        normalized = re.sub(r'[^0-9]', '', value)

        print '<tel:%s> a %s;' % (normalized, ncoType)
        print '    maemo:localPhoneNumber "%s";' % normalized[-7:]
        print '    nco:phoneNumber "%s".' % value

        if 'HOME' in type or not 'WORK' in type:
            contact.append('nco:hasPhoneNumber <tel:%s>' % normalized)
        if 'WORK' in type:
            affiliation.append('nco:hasPhoneNumber <tel:%s>' % normalized)

        continue

    if 'EMAIL' == key:
        print '<mailto:%s> a nco:EmailAddress;' % value
        print '    nco:emailAddress "%s".' % value

        if 'HOME' in type or not 'WORK' in type:
            contact.append('nco:hasEmailAddress <mailto:%s>' % value)
        if 'WORK' in type:
            affiliation.append('nco:hasEmailAddress <mailto:%s>' % value)

        continue

    if 'X-JABBER' == key:
        url = 'telepathy:/fake/cake!%s' % value

        print '<%s> a nco:IMAddress;' % url
        print '    nco:imNickname "%s".' % value

        if 'HOME' in type or not 'WORK' in type:
            contact.append('nco:hasIMAddress <%s>' % url)
        if 'WORK' in type:
            affiliation.append('nco:hasIMAddress <%s>' % url)

        continue

    if 'ADR' == key:
        url = 'address:%d' % addressId
        addressId += 1

        address = zip(ncoAddressProperties, value.split(';'))
        address = ['<%s> a nco:PostalAddress' % url] + ['%s "%s"' % p for p in address if p[1]]

        print ';\n    '.join(address) + '.'

        if 'HOME' in type or not 'WORK' in type:
            contact.append('nco:hasPostalAddress <%s>' % url)
        if 'WORK' in type:
            affiliation.append('nco:hasPostalAddress <%s>' % url)

        continue

    if key in ('VERSION', 'UID', 'FN'):
        continue

    raise 'Unsupported vCard attribute: ' + line

