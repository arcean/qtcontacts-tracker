/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 *********************************************************************************/

#include <QtCore>

int
main()
{
    // The following call to qrand() will cause QUuid::CreateUuid to not propertly
    // seet the random number generator, resulting in an identical sequence of uuids
    // across process instances.

    qrand();

    // Calling qrand() results in thread-local seed-storage being create
    // *without* seeding the random number generator.  QUuid::createUuid calls
    // qsrand(), which assumes that if thread-local seed-storage exists, then
    // the random number generator has already been seeded.  This assumption is
    // incorrect in this case.

    QTextStream out(stdout);

    out << QUuid::createUuid() << endl;
    out << QUuid::createUuid() << endl;
    out << QUuid::createUuid() << endl;

    return 0;
}

