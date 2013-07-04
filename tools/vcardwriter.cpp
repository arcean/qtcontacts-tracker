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
#include <QVersitContactExporter>
#include <QVersitWriter>

QTM_USE_NAMESPACE

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        QTextStream(stdout)
            << "This is a simple tool for exporting vCard files from tracker." << endl
            << "Usage: " << argv[0] << " CONTACTID" << endl;
        return EXIT_FAILURE;
    }

    bool isLocalId = false;
    QContactLocalId localId = QString(argv[1]).toUInt(&isLocalId);

    if (not isLocalId) {
        QTextStream(stderr) << "Numeric local contact id expected: " << argv[1] << endl;
        return EXIT_FAILURE;
    }

    QList<QContact> contacts = QContactManager().contacts(QList<QContactLocalId>() << localId);

    if (contacts.isEmpty()) {
        QTextStream(stderr) << "Cannot find contact with local id " << localId << endl;
        return EXIT_FAILURE;
    }

    QVersitContactExporter exporter;
    exporter.exportContacts(contacts);

    QByteArray buff;
    QVersitWriter streamWriter(&buff);

    if (not streamWriter.startWriting(exporter.documents())) {
        QTextStream(stderr) << "Unable to write the vcard(s), bailing out." << endl;
        return EXIT_FAILURE;
    }

    streamWriter.waitForFinished();

    QTextStream(stdout) << buff;

    return EXIT_SUCCESS;
}
