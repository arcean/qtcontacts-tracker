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

#include "tableofcontents.h"

// Qt
#include <QtCore/QFile>
#include <QtCore/QTextStream>

TableOfContents::TableOfContents(const QString &title)
    : mTitle(title)
{
    mDoc.appendChild(mDoc.createProcessingInstruction(QLatin1String("xml"),
                                                      QLatin1String("version=\"1.0\" encoding=\"utf-8\"")));
    mDoc.appendChild(mDoc.createElement(QLatin1String("html")));

    QDomElement head = mDoc.createElement(QLatin1String("head"));
    mDoc.lastChild().appendChild(head);

    if (not mTitle.isEmpty()) {
        head.appendChild(mDoc.createElement(QLatin1String("title")));
        head.lastChild().appendChild(mDoc.createTextNode(mTitle));
    }

    QDomElement body = mDoc.createElement(QLatin1String("body"));
    mDoc.lastChild().appendChild(body);

    if (not mTitle.isEmpty()) {
        body.appendChild(mDoc.createElement(QLatin1String("h1")));
        body.lastChild().appendChild(mDoc.createTextNode(mTitle));
    }

    body.appendChild(mDoc.createElement(QLatin1String("p")));
    body.lastChild().appendChild(mDoc.createTextNode(QString::fromLatin1("(%1 %2)").
                                                     arg(QLatin1String(PACKAGE)).
                                                     arg(QLatin1String(VERSION))));

    body.appendChild(mDoc.createElement(QLatin1String("h2")));
    body.lastChild().appendChild(mDoc.createTextNode(QLatin1String("Contents")));

    mToc = mDoc.createElement(QLatin1String("ul"));
    body.appendChild(mToc);
}

void TableOfContents::append(const QString &href, const QString &title)
{
    QDomElement link = mDoc.createElement(QLatin1String("a"));
    link.appendChild(mDoc.createTextNode(title));
    link.setAttribute(QLatin1String("href"), href);

    mToc.appendChild(mDoc.createElement(QLatin1String("li")));
    mToc.lastChild().appendChild(link);
}

void TableOfContents::write(const QString &fileName)
{
    QFile file(fileName);

    if (not file.open(QFile::WriteOnly | QFile::Truncate)) {
        qWarning("Could not open %s: %s", qPrintable(fileName), qPrintable(file.errorString()));
        return;
    }

    QTextStream(stdout) << "Writing " << file.fileName() << "\n";
    QTextStream outStream(&file);
    mDoc.save(outStream, 1);
}
