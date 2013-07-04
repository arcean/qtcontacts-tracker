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

class Version(object):
    def __init__(self, text):
        from re import sub, split

        self.__text = text

        text = text.replace('~', '_0_')
        text = text.replace('-', '_1_')
        text = text.replace('+', '_2_') + '_1_'

        self.__numbers = map(int, filter(None, split(r'[^0-9]+', text)))

    def __cmp__(self, other):
        return cmp(self.numbers, other.numbers)
    def __repr__(self):
        return '[Version: %r - %r]' % (self.text, self.numbers)
    def __str__(self):
            return self.text

    text = property(fget=lambda self: self.__text)
    numbers = property(fget=lambda self: self.__numbers)

class TestSuite(object):
    '''Model object describing a test suite'''

    def __init__(self, name, description, timestamp, hostinfo, results):
        from hashlib import sha1

        self.__name = name
        self.__description = description
        self.__timestamp = timestamp
        self.__package = hostinfo.get('package')
        self.__version = Version(hostinfo.get('version', '0'))
        self.__hostname = hostinfo.get('hostname')
        self.__username = hostinfo.get('username')
        self.__cpumodel = hostinfo.get('cpumodel')
        self.__results = results

        hostkey = self.username, self.hostname, self.cpumodel
        self.__hostkey = sha1('\0'.join(map(str, hostkey))).hexdigest()
        self.__fullkey = '-'.join([self.name, self.version.text, self.hostkey,
                                   self.timestamp.strftime('%s')])

    name = property(fget=lambda self: self.__name)
    description = property(fget=lambda self: self.__description)
    timestamp = property(fget=lambda self: self.__timestamp)
    package = property(fget=lambda self: self.__package)
    version = property(fget=lambda self: self.__version)
    hostname = property(fget=lambda self: self.__hostname)
    username = property(fget=lambda self: self.__username)
    cpumodel = property(fget=lambda self: self.__cpumodel)
    results = property(fget=lambda self: self.__results)
    hostkey = property(fget=lambda self: self.__hostkey)
    fullkey = property(fget=lambda self: self.__fullkey)

class TestResult(object):
    '''Model object describing a test result'''

    def __init__(self, fixture, case, start, end):
        self.__fixture = fixture
        self.__case = case
        self.__duration = end - start

    def __cmp__(self, other):
        return cmp(self.duration, other.duration)

    fixture = property(fget=lambda self: self.__fixture)
    case = property(fget=lambda self: self.__case)
    duration = property(fget=lambda self: self.__duration)
    name = property(fget=lambda self: self.fixture + '::' + self.case)

class Category(object):
    '''Model object describing performance categories'''

    def __init__(self, name, timeout):
        self.__name = name
        self.__timeout = timeout
        self.__results = []

    name = property(fget=lambda self: self.__name)
    timeout = property(fget=lambda self: self.__timeout)
    results = property(fget=lambda self: self.__results)

class ResultCache(object):
    '''Cache for previous test results'''

    def __init__(self):
        from os.path import expanduser, join

        self.__cachedir = expanduser('~/.cache/contacts')
        self.__filename = join(self.__cachedir, 'performance.dat')
        self.__suites = {}

    def add(self, suite):
        self.__suites[suite.fullkey] = suite

    def load(self):
        from cPickle import load

        try: self.__suites = load(file(self.__filename, 'rb'))
        except IOError: return False

        return True

    def save(self):
        from cPickle import HIGHEST_PROTOCOL, dump
        from tempfile import NamedTemporaryFile
        from os import rename

        tmp = NamedTemporaryFile(delete=False, dir=self.__cachedir, prefix='performance.')
        dump(self.__suites, tmp, HIGHEST_PROTOCOL)
        rename(tmp.name, self.filename)

    def findcurr(self, suite):
        return [s for s in self.__suites.values() if (s.fullkey != suite.fullkey and
                                                      s.name == suite.name and
                                                      s.hostkey == suite.hostkey and
                                                      s.version == suite.version)]

    def findprev(self, suite):
        matches, pv = [], None

        for s in self.__suites.values():
            if s.name == suite.name and s.hostkey == suite.hostkey:
                if s.version < suite.version:
                    if not pv or s.version > pv:
                        pv = s.version
                        matches = []

                    if s.version == pv:
                        matches.append(s)

                    continue

        return matches

    filename = property(fget=lambda self: self.__filename)
    suites = property(fget=lambda self: self.__suites)

def parselog(source):
    '''Parses a test results log generated by Harmattan's testrunner'''

    from datetime import datetime
    from xml.etree import ElementTree
    from re import MULTILINE, compile

    re_hostinfo = compile(r'^(\w+)=(.*)$', MULTILINE)

    suites, results = [], {}
    fixture, case = None, None
    start, end = None, None
    timestamp = None
    hostinfo = {}

    for event, element in ElementTree.iterparse(source, events=('start', 'end')):
        if 'start' == event:
            if 'case' == element.tag:
                case = element.attrib['name']
                continue

            if 'set' == element.tag:
                fixture, case = element.attrib['name'], None
                continue

            continue

        if 'end' == event:
            if 'suite' == element.tag:
                suites.append(TestSuite(element.attrib['name'],
                                        element.find('description').text,
                                        timestamp, hostinfo, results))
                timestamp, hostinfo, results = None, {}, {}
                continue

            if 'case' == element.tag:
                r = TestResult(fixture, case, start, end)
                results[r.name] = r
                start, end = None, None
                continue

            if 'stdout' == element.tag:
                if 'testHostInfo' == case:
                    hostinfo = dict(re_hostinfo.findall(element.text or ''))

                continue

            if 'start' == element.tag:
                start = datetime.strptime(element.text, '%Y-%m-%d %H:%M:%S')
                timestamp = timestamp and min(timestamp, start) or start
                continue

            if 'end' == element.tag:
                end = datetime.strptime(element.text, '%Y-%m-%d %H:%M:%S')
                continue

            continue

    return suites

def checkperf(suite, reference):
    from os import isatty

    fmtfail = isatty(1) and '\033[31mFAILURE: %s\033[0m' or 'FAILURE: %s'
    fmtwarn = isatty(1) and '\033[33mWARNING: %s\033[0m' or 'WARNING: %s'

    def fail(msg, *args): print fmtfail % msg % args
    def warn(msg, *args): print fmtwarn % msg % args

    for r in suite.results.values():
        durations = []

        for rs in reference:
            rr = rs.results.get(r.name)
            if rr: durations.append(float(rr.duration.seconds))

        if len(durations) == 0:
            continue

        dmax = max(durations)
        davg = sum(durations)/len(durations)

        if r.duration.seconds > (dmax * 1.01 + 1):
            fail('%s %ds needed, but maximum duration was %ds in %s',
                 r.name, r.duration.seconds, dmax, rs.version)
            continue

        if r.duration.seconds > (davg * 1.05 + 1):
            warn('%s run for %ds, but average duration was %ds in %s',
                 r.name, r.duration.seconds, davg, rs.version)
            continue

def main(source):
    '''Main routine of the script'''

    from datetime import timedelta

    # load the result cache
    cache = ResultCache()
    cache.load()

    # read test log and update the result cache
    suites = parselog(source)

    for s in suites:
        cache.add(s)

    cache.save()

    # compare current test performance with performance from previous runs
    for s in suites:
        checkperf(s, cache.findcurr(s))
        checkperf(s, cache.findprev(s))

    # declare performance categories
    categories = [Category(name, timedelta(seconds=timeout)) for name, timeout
                  in zip(['fast', 'slow', 'glacial'], [0, 5, 30])]

    # put results into categories
    for s in suites:
        for r in s.results.values():
            for c in reversed(categories):
                if r.duration >= c.timeout:
                    c.results.append(r)
                    break

    # print general performance stats
    print '# number of %s tests: %s' % ('/'.join([c.name for c in categories]),
                                        '/'.join([str(len(c.results)) for c in categories]))
    print

    # print all tests not qualifying for the 'fast' category
    for c in [c for c in categories if c.name != 'fast']:
        for r in c.results:
            print '%s: %s (%s seconds)' % (r.name, c.name, r.duration.seconds)

    print

def test_versions():
    assert Version('1.0') == Version('1.0')

    assert Version('1.0') < Version('2.0')
    assert Version('1.0') < Version('1.1')
    assert Version('1.0') < Version('1.0-1')
    assert Version('1.0') < Version('1.0+1')
    assert Version('1.0') > Version('1.0~git1')
    assert Version('1.0~git1') < Version('1.0~git2')

    assert Version('2.0') > Version('1.0')

if '__main__' == __name__:
    from sys import argv

    if len(argv) != 2:
        raise SystemExit('Usage: %s FILENAME' % argv[0])

    test_versions()
    main(file(argv[1]))

