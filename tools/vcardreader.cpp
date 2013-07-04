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

#include <QBuffer>
#include <QContactManager>
#include <QCoreApplication>
#include <QFile>
#include <QVersitContactImporter>
#include <QVersitReader>

QTM_USE_NAMESPACE

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        QTextStream(stdout)
            << "This is a simple tool for importing vCard files into tracker." << endl
            << "Usage: " << argv[0] << " VCARDFILE" << endl;
        return EXIT_FAILURE;
    }

    QFile file(argv[1]);

    if (not file.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Unable to open " << file.fileName() << ": " << file.errorString() << endl;
        return EXIT_FAILURE;
    }

    QBuffer buff;
    buff.open(QBuffer::ReadWrite);

    buff.write(file.readAll());
    buff.seek(0);

    QVersitReader streamreader;
    streamreader.setDevice(&buff);

    if (not streamreader.startReading() ) {
        QTextStream(stderr) << "Unable to parse the vcard(s), bailing out." << endl;
        return EXIT_FAILURE;
    }

    streamreader.waitForFinished();

    file.close();
    buff.close();

    QVersitContactImporter importer;
    importer.importDocuments(streamreader.results());
    QList<QContact> contacts = importer.contacts();

    if (contacts.isEmpty()) {
        QTextStream(stderr) << "Contact information couldn't be read from file, bailing out." << endl;
        return EXIT_FAILURE;
    }

    QMap<int, QContactManager::Error> errorMap;
    bool success = QContactManager().saveContacts(&contacts, &errorMap);

    if (not success) {
        QTextStream(stderr)
            << "Failed to import " << contacts.count() << " contacts, "
            << errorMap.size() << " errors reported." << endl;

        return EXIT_FAILURE;
    }

    QTextStream(stdout) << "Successully imported " << contacts.count() << " contacts." << endl;

    return EXIT_SUCCESS;
}
