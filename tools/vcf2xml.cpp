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

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QVersitReader>
#include <QVersitContactImporter>

QTM_USE_NAMESPACE

static QString makeIri(const QContact &contact)
{
    QContactLocalId localId = contact.localId();
    static QContactLocalId lastLocalId = 0;

    if (0 == lastLocalId) {
        localId = ++lastLocalId;
    }

    if (QContactType::TypeContact == contact.type()) {
        return QString("contact:%1").arg(localId);
    }

    if (QContactType::TypeGroup == contact.type()) {
        return QString("contactgroup:%1").arg(localId);
    }

    return QString("invalid:%1").arg(localId);
}

static QString toString(const QVariant &value)
{
    switch(value.type()) {
    case QVariant::DateTime:
        return value.toDateTime().toString(Qt::ISODate);

    case QVariant::StringList:
        return value.toStringList().join(";");

    default:
        break;
    }

    return value.toString();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    QFile vcf, xml;

    if (args.count() > 1) {
        vcf.setFileName(args[1]);

        if (not vcf.open(QFile::ReadOnly)) {
            qWarning("Cannot open %s for reading: %s",
                     qPrintable(vcf.fileName()),
                     qPrintable(vcf.errorString()));
            return 1;
        }
    } else {
        if (not vcf.open(stdin, QFile::ReadOnly)) {
            qWarning("Cannot open STDIN for reading: %s",
                     qPrintable(vcf.errorString()));
            return 1;
        }
    }

    QVersitReader reader(&vcf);

    if (not reader.startReading() ||
        not reader.waitForFinished() ||
        QVersitReader::NoError != reader.error()) {
        qWarning("Cannot read vcard: rc=%d", reader.error());
        return 1;
    }

    QVersitContactImporter importer;

    if (not importer.importDocuments(reader.results())) {
        typedef QMap<int, QVersitContactImporter::Error> ErrorMap;
        const ErrorMap errors = importer.errors();

        for(ErrorMap::ConstIterator it = errors.constBegin(); it != errors.constEnd(); ++it) {
            qWarning("Cannot convert contact #%d: %d", it.key(), it.value());
        }

        return 1;
    }

    if (args.count() > 2) {
        xml.setFileName(args[2]);

        if (not xml.open(QFile::WriteOnly)) {
            qWarning("Cannot open %s for writing: %s",
                     qPrintable(xml.fileName()),
                     qPrintable(xml.errorString()));
            return 1;
        }
    } else {
        if (not xml.open(stdout, QFile::WriteOnly)) {
            qWarning("Cannot open STDIN for writing: %s",
                     qPrintable(xml.errorString()));
            return 1;
        }
    }

    QTextStream out(&xml);
    out << "<Contacts>\n";

    foreach(const QContact &contact, importer.contacts()) {
        out << "  <Contact id=\"" << makeIri(contact) << "\">\n";

        foreach(const QContactDetail &detail, contact.details()) {
            QVariantMap fields = detail.variantValues();

            out << "    <" << detail.definitionName();

            if (not detail.detailUri().isEmpty()) {
                out << " id=\"" << detail.detailUri() << "\"";
            }

            out << ">";

            if (fields.count() > 1 || fields.keys().first() != detail.definitionName()) {
                QVariantMap::ConstIterator it;

                for(it = fields.constBegin(); it != fields.constEnd(); ++it) {
                    out << "\n      ";
                    out << "<" << it.key() << ">";
                    out << toString(it.value());
                    out << "</" << it.key() << ">";
                }

                out << "\n    ";
            } else {
                out << toString(fields.values().first());
            }

            out << "</" << detail.definitionName() << ">\n";
        }

        out << "  </Contact>\n";
    }

    out << "</Contacts>\n";

    return 0;
}
