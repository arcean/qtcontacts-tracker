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

#include <QtCore>

#include <qtcontacts.h>

QTM_USE_NAMESPACE

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // load plugin from build directory
    const QDir appdir = QDir(app.applicationDirPath());
    const QDir topdir = QDir(appdir.relativeFilePath(QLatin1String("..")));
    app.setLibraryPaths(QStringList(topdir.absolutePath()) + app.libraryPaths());

    // process command line arguments
    QStringList args = app.arguments();

    int iterations = 0;
    QString searchTerm;

    if (args.count() > 1) {
        bool success = false;
        iterations = args[1].toInt(&success);

        if (not success) {
            iterations = 0;
        }
    }

    if (args.count() > 2) {
        searchTerm = QStringList(args.mid(2)).join(QLatin1String(" "));
    }

    // normalize parameters
    if (searchTerm.isEmpty()) {
        searchTerm = QLatin1String("Emilia Galotta");
    }

    if (iterations < 1) {
        iterations = searchTerm.length();
    }

    qDebug() << "iterations:" << iterations;
    qDebug() << "search term:" << searchTerm;

    // run benchmark iterations
    QContactManager cm(QLatin1String("tracker"));

    for(int i = 0; i < iterations; ++i) {
        QTime time;
        time.start();

        QContactDetailFilter filter;
        filter.setDetailDefinitionName(QContactName::DefinitionName);
        filter.setValue(searchTerm.left(i % (searchTerm.length()) + 1));
        filter.setMatchFlags(QContactFilter::MatchStartsWith);

        QList<QContact> contacts = cm.contacts(filter);

        qDebug("%d: %d contact(s) found for \"%s\", %.3fs ellapsed",
               i + 1, contacts.count(), qPrintable(filter.value().toString()),
               time.elapsed() / 1000.0);
    }

    return 0;
}
