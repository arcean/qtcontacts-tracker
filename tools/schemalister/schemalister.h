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

#ifndef SCHEMALISTER_H
#define SCHEMALISTER_H

// this engine
class QTrackerContactDetailSchema;
class QTrackerContactDetail;
class QTrackerContactDetailField;
class ClassInfoBase;
class PropertyInfoBase;
// Qt
class QDomDocument;
class QDomElement;
class QString;
class QStringList;


class SchemaLister
{
public:
    SchemaLister();
    ~SchemaLister();

public:
    void writeDetailSchema(const QTrackerContactDetailSchema &schema, const QString &contactType,
                           const QString &fileName, const QString &title);

    static QString descriptiveNameForContactType(const QString &contactType);

protected:
    void listDetailSchema(QDomElement &parentElement, const QTrackerContactDetailSchema &schema);
    void listDetail(QDomElement &parentElement, const QTrackerContactDetail &detail);
    void listDetailField(QDomElement &parentElement, const QTrackerContactDetailField &detailField);
    void listSubType(QDomElement &parentElement, const ClassInfoBase &subTypeClassInfo);
    void listSubType(QDomElement &parentElement, const PropertyInfoBase &subTypePropertyInfo);

    void createTOC(QDomElement &parentElement, const QTrackerContactDetailSchema &schema);

    QDomElement createTextElement(QDomElement &parentElement, const QString &tagName, const QString &text);
    QDomElement createTableRow(QDomElement &tableElement, const QStringList &columnTexts);

protected:
    QDomDocument *mDomDocument;
};

#endif /*SCHEMALISTER_H*/
