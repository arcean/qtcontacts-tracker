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

#include "schemalister.h"

// qtcontacts-tracker
#include <dao/contactdetailschema.h>
#include <dao/contactdetail.h>
#include <dao/support.h>
// Qt
#include <QtXml/QDomDocument>


// TODO for the schema:
// why is setOptional() applied to last property and not the detailfield itself?!

/// creates a string with all ClassInfo items in the list, using the text property, separated by a comma
static QString stringFromClassInfoListByText(const ClassInfoList &classInfoList)
{
    QString string;
    foreach (const ClassInfoBase &classInfo, classInfoList) {
        if (not string.isEmpty()) {
            string.append(QLatin1String(", "));
        }
        string.append(classInfo.text());
    }
    return string;
}

/// creates a string with all ClassInfo items in the list, using the text property, separated by a comma
static QString stringFromPropertyInfoListByText(const PropertyInfoList &propertyInfos)
{
    QString string;
    foreach (const PropertyInfoBase &propertyInfo, propertyInfos) {
        if (not string.isEmpty()) {
            string.append(QLatin1String(", "));
        }
        string.append(propertyInfo.text());
    }
    return string;
}

/// creates a string with all PropertyInfo items in the list, using the iri, separated with an arrow to right
static QString stringChainFromPropertyInfoList(const PropertyInfoList &propertyInfos)
{
    QString chainString;
    foreach (const PropertyInfoBase &propertyInfo, propertyInfos) {
        if (not chainString.isEmpty()) {
            //TODO: see how to make this an arrow, ">" appears unreplaced! But & in e.g. &#187;/&#8594; gets, what?
            chainString.append(QLatin1String(" [ "));
        }
        chainString.append(qctIriAlias(propertyInfo.iri()));
    }
    chainString.append(QLatin1String(" ?value"));
    const int propertyInfosCount = propertyInfos.count();
    if (propertyInfosCount > 1 ) {
        chainString.append(QString::fromLatin1(" ]").repeated(propertyInfosCount-1));
    }
    return chainString;
}

/// creates a descriptive name of the contact type
QString SchemaLister::descriptiveNameForContactType(const QString &contactType)
{
    return
        (contactType == QContactType::TypeContact) ? QLatin1String("Person Contact") :
        (contactType == QContactType::TypeGroup)   ? QLatin1String("Contact Group") :
        /*else*/                                     QLatin1String("Unknown contact type, this tool needs an update.");
}

static QString classNameOfDetail(const QTrackerContactDetail &detail)
{
    // FIXME: get QTM detail class name not by guessing from the detail name, also see dependencies
    return QLatin1String("QContact")+detail.name();
}

SchemaLister::SchemaLister()
{
    // document to contain the schema description, in HTML (at least somenthing close)
    mDomDocument = new QDomDocument();
    QDomProcessingInstruction processingInstruction =
        mDomDocument->createProcessingInstruction(QLatin1String("xml"),
                                                  QLatin1String("version=\"1.0\" encoding=\"utf-8\""));
    mDomDocument->appendChild(processingInstruction);
}

SchemaLister::~SchemaLister()
{
    delete mDomDocument;
}


void SchemaLister::writeDetailSchema(const QTrackerContactDetailSchema &schema,
                                     const QString &contactType, const QString &fileName,
                                     const QString &title)
{
    const QString typeName = descriptiveNameForContactType(contactType);

    QDomElement html = mDomDocument->createElement(QLatin1String("html"));
    mDomDocument->appendChild(html);

    QDomElement head = mDomDocument->createElement(QLatin1String("head"));
    createTextElement(head, QLatin1String("title"), title + QLatin1String(" - ") + typeName);
    html.appendChild(head);

    QDomElement body = mDomDocument->createElement(QLatin1String("body"));
    createTextElement(body, QLatin1String("h1"), title);
    createTextElement(body, QLatin1String("p"),
                      QString::fromLatin1("(%1 %2)").arg(QLatin1String(PACKAGE), QLatin1String(VERSION)));
    createTextElement(body, QLatin1String("h2"), typeName);
    html.appendChild(body);

    // add toc
    createTOC(body, schema);

    // list schema
    listDetailSchema(body, schema);

    // save
    const int indent = 4;
    QFile file(fileName);

    if (not file.open(QFile::WriteOnly | QFile::Truncate)) {
        qWarning("Could not open %s: %s", qPrintable(fileName), qPrintable(file.errorString()));
        return;
    }

    QTextStream(stdout) << "Writing " << file.fileName() << "\n";

    QTextStream outStream(&file);
    mDomDocument->save(outStream, indent);

    file.close();
}

void SchemaLister::createTOC(QDomElement &parentElement, const QTrackerContactDetailSchema &schema)
{
    QDomElement tocTitelElement =
        createTextElement(parentElement, QLatin1String("h4"), QLatin1String("Details"));

    QDomElement tocListingElement = mDomDocument->createElement(QLatin1String("ul"));
    parentElement.appendChild(tocListingElement);
    foreach (const QTrackerContactDetail &detail, schema.details()) {
        QDomElement tocListEntryElement = mDomDocument->createElement(QLatin1String("li"));
        tocListingElement.appendChild(tocListEntryElement);
        // link to section about detail
        const QString detailClassName = classNameOfDetail(detail);
        QDomElement tocLinkElement =
            createTextElement(tocListEntryElement, QLatin1String("a"), detailClassName);
        const QString detailLink = QLatin1Char('#') + detailClassName;
        tocLinkElement.setAttribute(QLatin1String("href"), detailLink);
    }
}

void SchemaLister::listDetailSchema(QDomElement &parentElement, const QTrackerContactDetailSchema &schema)
{
    foreach (const QTrackerContactDetail &detail, schema.details()) {
        // name
        const QString detailClassName = classNameOfDetail(detail);
        QDomElement detailTitelElement =
            createTextElement(parentElement, QLatin1String("h3"), detailClassName+QLatin1String(" detail"));

        const QString &anchorName = detailClassName;
        detailTitelElement.setAttribute(QLatin1String("id"), anchorName);

        listDetail(parentElement, detail);
    }
}

void SchemaLister::listDetail(QDomElement &parentElement, const QTrackerContactDetail &detail)
{
    // contacts types
    // FIXME: really also wanted here? after all we have the separate schemas...

    // contexts
    // FIXME: will fail once there are more contexts then just Home and Work
    const QString contextNameList = detail.hasContext() ? QLatin1String("Home, Work") : QLatin1String("-");
    createTextElement(parentElement, QLatin1String("p"), QLatin1String("Contexts: ")+contextNameList);

    // dependencies
    if (detail.isSynthesized()) {
        // FIXME: get QTM detail class name not by guessing from the detail name, also see classNameOfDetail()
        const QString dependencies = QStringList(detail.dependencies().toList()).join(QLatin1String(", QContact"));
        createTextElement(parentElement, QLatin1String("p"), QLatin1String("Is synthesized by: QContact")+dependencies);
    }

    // dependencies
    if (detail.isUnique()) {
        createTextElement(parentElement, QLatin1String("p"), QLatin1String("Unique"));
    }

    // fields
    createTextElement(parentElement, QLatin1String("h4"), QLatin1String("Fields"));
    QDomElement fieldListingElement = mDomDocument->createElement(QLatin1String("dl"));
    parentElement.appendChild(fieldListingElement);
    // TODO: sort by name
    foreach (const QTrackerContactDetailField &detailField, detail.fields()) {
        listDetailField(fieldListingElement, detailField);
    }

    // sub types
    // collect subtype info, currently defined per detail
    ClassInfoList subTypeClassInfoList;
    foreach (const QTrackerContactDetailField &detailField, detail.fields()) {
        subTypeClassInfoList += detailField.subTypeClasses();
    }
    PropertyInfoList subTypePropertyInfoList;
    foreach (const QTrackerContactDetailField &detailField, detail.fields()) {
        subTypePropertyInfoList += detailField.subTypeProperties();
    }
    if (not subTypeClassInfoList.isEmpty() || not subTypePropertyInfoList.isEmpty()) {
        createTextElement(parentElement, QLatin1String("h4"), QLatin1String("SubTypes"));
    }
    if (not subTypeClassInfoList.isEmpty()) {
        QDomElement subTypeListingElement = mDomDocument->createElement(QLatin1String("dl"));
        parentElement.appendChild(subTypeListingElement);
        foreach (const ClassInfoBase &subTypeClassInfo, subTypeClassInfoList) {
            listSubType(subTypeListingElement, subTypeClassInfo);
        }
    }
    if (not subTypePropertyInfoList.isEmpty()) {
        QDomElement subTypeListingElement = mDomDocument->createElement(QLatin1String("dl"));
        parentElement.appendChild(subTypeListingElement);
        foreach (const PropertyInfoBase &subTypePropertyInfo, subTypePropertyInfoList) {
            listSubType(subTypeListingElement, subTypePropertyInfo);
        }
    }
}

void SchemaLister::listDetailField(QDomElement &parentElement, const QTrackerContactDetailField &detailField)
{
    // name and code datatype
    createTextElement(parentElement, QLatin1String("dt"),
                      detailField.name() + QLatin1String(": ") +
                      QLatin1String(QVariant::typeToName(detailField.dataType())));

    // rdf mapping
    QDomElement detailFieldMappingDescriptionElement = mDomDocument->createElement(QLatin1String("dd"));
    parentElement.appendChild(detailFieldMappingDescriptionElement);

    const PropertyInfoList &properties = detailField.propertyChain();
    // is not synthesized?
    if (not properties.isEmpty()) {
        QDomElement propertyChainTableElement = mDomDocument->createElement(QLatin1String("table"));
        detailFieldMappingDescriptionElement.appendChild(propertyChainTableElement);

        // RDF domain
        createTableRow(propertyChainTableElement,
                       QStringList() << QLatin1String("RDF domain:")
                                     << qctIriAlias(properties.first().domainIri()));
        // RDF property chain
        createTableRow(propertyChainTableElement,
                       QStringList() << QLatin1String("RDF property chain:")
                                     << stringChainFromPropertyInfoList(properties));
        // RDF range
        createTableRow(propertyChainTableElement,
                       QStringList() << QLatin1String("RDF range:")
                                     << qctIriAlias(properties.last().rangeIri()));
    }

    const PropertyInfoList &computedProperties = detailField.computedProperties();
    if (not computedProperties.isEmpty()) {
        QDomElement computedPropertyTextElement = 
            createTextElement(detailFieldMappingDescriptionElement,QLatin1String("p"),
                              QLatin1String("Computed properties:"));

        QDomElement computedPropertyListingElement = mDomDocument->createElement(QLatin1String("table"));
        computedPropertyTextElement.appendChild(computedPropertyListingElement);

        // RDF domain
        createTableRow(computedPropertyListingElement,
                       QStringList() << QLatin1String("RDF domain:")
                                     << qctIriAlias(computedProperties.first().domainIri()));
        // RDF property chain
        createTableRow(computedPropertyListingElement,
                       QStringList() << QLatin1String("RDF property:")
                                     << stringChainFromPropertyInfoList(computedProperties));
        // RDF range
        createTableRow(computedPropertyListingElement,
                       QStringList() << QLatin1String("RDF range:")
                                     << qctIriAlias(computedProperties.last().rangeIri()));
    }

    // sub type field?
    if (detailField.hasSubTypes()) {
        if (detailField.hasSubTypeProperties()) {
            const QString subTypePropertyChainString = stringFromPropertyInfoListByText(detailField.subTypeProperties());
            createTextElement(detailFieldMappingDescriptionElement, QLatin1String("p"),
                            QLatin1String("SubTypes (by property): ")+subTypePropertyChainString);
        }
        if (detailField.hasSubTypeClasses()) {
            const QString subTypeListString = stringFromClassInfoListByText(detailField.subTypeClasses());
            createTextElement(detailFieldMappingDescriptionElement, QLatin1String("p"),
                            QLatin1String("SubTypes (by RDF class): ")+subTypeListString);
        }
    }

    // custom detail field?
    if (detailField.isWithoutMapping()) {
        createTextElement(detailFieldMappingDescriptionElement, QLatin1String("p"),
                          QLatin1String("Without explicit RDF mapping. Stored via nao:Property."));
    }

    // custom detail field?
    const PropertyInfoList::ConstIterator foreignKeyProperty = detailField.foreignKeyProperty();

    if (detailField.propertyChain().constEnd() != foreignKeyProperty) {
        createTextElement(detailFieldMappingDescriptionElement, QLatin1String("p"),
                          QLatin1String("Foreign key (in RDF property):") +
                          qctIriAlias(foreignKeyProperty->iri()));
    }

    if (not detailFieldMappingDescriptionElement.hasChildNodes()) {
        // ensure html compatibel <X></X>, not short <X/>
        const QDomText emptyTextNode = mDomDocument->createTextNode(QString());
        detailFieldMappingDescriptionElement.appendChild(emptyTextNode);
    }
}

void SchemaLister::listSubType(QDomElement &parentElement, const ClassInfoBase &subTypeClassInfo)
{
    // name
    createTextElement(parentElement, QLatin1String("dt"), subTypeClassInfo.text());

    // RDF domain
    createTextElement(parentElement,QLatin1String("dd"),
                      QLatin1String("RDF class: ")+qctIriAlias(subTypeClassInfo.iri()));
}

void SchemaLister::listSubType(QDomElement &parentElement, const PropertyInfoBase &subTypePropertyInfo)
{
    // name
    createTextElement(parentElement, QLatin1String("dt"), subTypePropertyInfo.text());

    QDomElement subTypeNameElement = mDomDocument->createElement(QLatin1String("dd"));
    parentElement.appendChild(subTypeNameElement);
    QDomElement subTypePropertyListingElement = mDomDocument->createElement(QLatin1String("table"));
    subTypePropertyListingElement.setAttribute(QLatin1String("border"), 0);
    subTypeNameElement.appendChild(subTypePropertyListingElement);

    // RDF domain
    createTableRow(subTypePropertyListingElement,
                   QStringList() << QLatin1String("RDF domain:")
                                 << qctIriAlias(subTypePropertyInfo.domainIri()));
    // RDF property chain
    createTableRow(subTypePropertyListingElement,
                   QStringList() << QLatin1String("RDF property:")
                                 << qctIriAlias(subTypePropertyInfo.iri()));
    // RDF range
    createTableRow(subTypePropertyListingElement,
                   QStringList() << QLatin1String("RDF range:")
                                 << qctIriAlias(subTypePropertyInfo.rangeIri()));
}

QDomElement SchemaLister::createTextElement(QDomElement &parentElement, const QString &tagName, const QString &text)
{
    QDomElement textElement = mDomDocument->createElement(tagName);
    QDomText textNode = mDomDocument->createTextNode(text);
    textElement.appendChild(textNode);
    parentElement.appendChild(textElement);
    return textElement;
}

QDomElement SchemaLister::createTableRow(QDomElement &tableElement, const QStringList &columnTexts)
{
    QDomElement domainTableRowElement = mDomDocument->createElement(QLatin1String("tr"));
    tableElement.appendChild(domainTableRowElement);
    foreach (const QString &columnText, columnTexts) {
        createTextElement(domainTableRowElement,QLatin1String("td"), columnText);
    }
    return domainTableRowElement;
}
