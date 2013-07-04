/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
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

// this program
#include "schemalister.h"
#include "tableofcontents.h"
// qtcontacts-tracker
#include <engine/engine.h>
// Qt
#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
// stdlib
#include <iostream>

static const char defaultFileNamePrefix[] = "schema";
static const char defaultFileNameSuffix[] = ".html";

static void
printHelp()
{
    std::cout
        << "schemalister writes an overview how QContactDetails are mapped to RDF classes and properties." << std::endl
        << "The output is in html format and written to one file per contact type in the working directory," <<std::endl
        << "named with the pattern\""<<defaultFileNamePrefix<<"*"<<defaultFileNameSuffix<<"\"." << std::endl
        << "Usage: schemalister" << std::endl;
}

int
main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    // check arguments
    QStringList arguments = application.arguments();
    arguments.removeFirst(); // first was program name
    foreach (const QString &argument, arguments) {
        if (argument == QLatin1String("-h") || argument == QLatin1String("--help")) {
            printHelp();
            return 0;
        }
    }

    // work
    // need a QContactTrackerEngine to extract all the info
    QMap<QString, QString> engineParameters;
    QContactTrackerEngine engine(engineParameters);
    TableOfContents toc(QLatin1String("QtContacts Tracker Schema"));

    foreach(const QString &contactType, engine.supportedContactTypes()) {
        const QTrackerContactDetailSchema& schema = engine.schema(contactType);

        const QString fileName =
                QLatin1String(defaultFileNamePrefix) +
                QLatin1Char('-') + contactType.toLower() +
                QLatin1String(defaultFileNameSuffix);

        SchemaLister().writeDetailSchema(schema, contactType, fileName, toc.title());
        toc.append(fileName, SchemaLister::descriptiveNameForContactType(contactType));
    }

    toc.write(QLatin1String(defaultFileNamePrefix) +
              QLatin1String(defaultFileNameSuffix));

    return 0;
}
