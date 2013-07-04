/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2009-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "displaylabelgenerator.h"

#include <qcontactdetails.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctDisplayLabelGeneratorData : public QSharedData
{
protected:
    explicit QctDisplayLabelGeneratorData(const QString &detailName);

public:
    virtual ~QctDisplayLabelGeneratorData();

public: // API to be implemented
    virtual QString createDisplayLabel(const QContact &contact) const = 0;

public:
    const QString &requiredDetailName() const;

protected:
    const QString m_detailName;
};

class SimpleDetailFieldDisplayLabelGeneratorData : public QctDisplayLabelGeneratorData
{
public:
    SimpleDetailFieldDisplayLabelGeneratorData(const QString &detailName, const QString &fieldName);

public: // AbstractDisplayLabelGeneratorData API
    virtual QString createDisplayLabel(const QContact &contact) const;

protected:
    const QString m_fieldName;
};

class FirstNameLastNameDisplayLabelGeneratorData : public QctDisplayLabelGeneratorData
{
public:
    FirstNameLastNameDisplayLabelGeneratorData();

public: // AbstractDisplayLabelGeneratorData API
    virtual QString createDisplayLabel(const QContact &contact) const;
};

class LastNameFirstNameDisplayLabelGeneratorData : public QctDisplayLabelGeneratorData
{
public:
    LastNameFirstNameDisplayLabelGeneratorData();

public: // AbstractDisplayLabelGeneratorData API
    virtual QString createDisplayLabel(const QContact &contact) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctDisplayLabelGeneratorData::QctDisplayLabelGeneratorData(const QString &detailName)
    : m_detailName(detailName)
{
}
QctDisplayLabelGeneratorData::~QctDisplayLabelGeneratorData()
{
}

inline const QString &
QctDisplayLabelGeneratorData::requiredDetailName() const
{
    return m_detailName;
}

SimpleDetailFieldDisplayLabelGeneratorData::SimpleDetailFieldDisplayLabelGeneratorData(const QString &detailName,
                                                                                       const QString &fieldName)
    : QctDisplayLabelGeneratorData(detailName)
    , m_fieldName(fieldName)
{
}

QString
SimpleDetailFieldDisplayLabelGeneratorData::createDisplayLabel(const QContact &contact) const
{
    return contact.detail(m_detailName).value(m_fieldName);
}

FirstNameLastNameDisplayLabelGeneratorData::FirstNameLastNameDisplayLabelGeneratorData()
    : QctDisplayLabelGeneratorData(QContactName::DefinitionName)
{
}

QString
FirstNameLastNameDisplayLabelGeneratorData::createDisplayLabel(const QContact &contact) const
{
    const QContactDetail nameDetail = contact.detail(QContactName::DefinitionName);

    QString result = nameDetail.value(QContactName::FieldFirstName);

    const QString lastName = nameDetail.value(QContactName::FieldLastName);
    if (not lastName.isEmpty()) {
        if (not result.isEmpty()) {
            result.append(QLatin1Char(' '));
        }

        result.append(lastName);
    }
    return result;
}

LastNameFirstNameDisplayLabelGeneratorData::LastNameFirstNameDisplayLabelGeneratorData()
    : QctDisplayLabelGeneratorData(QContactName::DefinitionName)
{
}

QString
LastNameFirstNameDisplayLabelGeneratorData::createDisplayLabel(const QContact &contact) const
{
    const QContactDetail nameDetail = contact.detail(QContactName::DefinitionName);

    QString result = nameDetail.value(QContactName::FieldLastName);

    const QString lastName = nameDetail.value(QContactName::FieldFirstName);
    if (not lastName.isEmpty()) {
        if (not result.isEmpty()) {
            result.append(QLatin1String(" "));
        }

        result.append(lastName);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QctDisplayLabelGenerator::QctDisplayLabelGenerator(QctDisplayLabelGeneratorData *data)
    : d(data)
{
}

QctDisplayLabelGenerator::QctDisplayLabelGenerator(const QctDisplayLabelGenerator &other)
    : d(other.d)
{
}

QctDisplayLabelGenerator::~QctDisplayLabelGenerator()
{
}

QctDisplayLabelGenerator & QctDisplayLabelGenerator::operator=(const QctDisplayLabelGenerator &other)
{
    d = other.d;
    return *this;
}

QString QctDisplayLabelGenerator::createDisplayLabel(const QContact& contact) const
{
    return d->createDisplayLabel(contact);
}

const QString& QctDisplayLabelGenerator::requiredDetailName() const
{
    return d->requiredDetailName();
}

QctDisplayLabelGenerator
QctDisplayLabelGenerator::simpleDetailFieldGenerator(const QString &detailName,
                                                     const QString &detailFieldName)
{
    return QctDisplayLabelGenerator(new SimpleDetailFieldDisplayLabelGeneratorData(detailName, detailFieldName));
}

QctDisplayLabelGenerator
QctDisplayLabelGenerator::firstNameLastNameGenerator()
{
    return QctDisplayLabelGenerator(new FirstNameLastNameDisplayLabelGeneratorData);
}

QctDisplayLabelGenerator
QctDisplayLabelGenerator::lastNameFirstNameGenerator()
{
    return QctDisplayLabelGenerator(new LastNameFirstNameDisplayLabelGeneratorData);
}

const QList<QctDisplayLabelGenerator> &
QctDisplayLabelGenerator::generators(ListOptions options)
{
    typedef QList<QctDisplayLabelGenerator> GeneratorList;
    typedef QHash<ListOptions, GeneratorList> GeneratorListMap;

    static GeneratorListMap generatorLists;

    GeneratorListMap::Iterator it = generatorLists.find(options);

    if (it == generatorLists.constEnd()) {
        it = generatorLists.insert(options, createGenerators(options));
    }

    return it.value();
}

QList<QctDisplayLabelGenerator>
QctDisplayLabelGenerator::createGenerators(ListOptions options)
{
    QList<QctDisplayLabelGenerator> generators;

    // Prefer nickname first/last name when requested (NB#283142, NB#287273)
    if (options & PreferNickname) {
        generators += simpleDetailFieldGenerator(QContactNickname::DefinitionName,
                                                 QContactNickname::FieldNickname);
    }

    // Choose among "FirstName LastName" and "LastName FirstName" according settings
    switch(options & RealNameFormat) {
    case FirstNameLastName:
        generators += firstNameLastNameGenerator();
        break;
    case LastNameFirstName:
        generators += lastNameFirstNameGenerator();
        break;
    }

    // If CustomLabel is set it dominates all other rules, but detailed name fields (NB#200059)
    generators += simpleDetailFieldGenerator(QContactName::DefinitionName,
                                             QContactName::FieldCustomLabel);

    // Take "any available name information" - nickname, unless configured to dominate
    if (not(options & PreferNickname)) {
        generators += simpleDetailFieldGenerator(QContactNickname::DefinitionName,
                                                 QContactNickname::FieldNickname);
    }

    // Take "any available name information" - middle name
    generators += simpleDetailFieldGenerator(QContactName::DefinitionName,
                                             QContactName::FieldMiddleName);

    // The company name, e.g. "Nokia"
    generators += simpleDetailFieldGenerator(QContactOrganization::DefinitionName,
                                             QContactOrganization::FieldName);

    // "IM username", first the most available IM nickname (e.g "The Dude")
    generators += simpleDetailFieldGenerator(QContactGlobalPresence::DefinitionName,
                                             QContactGlobalPresence::FieldNickname);

    // IM address as desperate fallback, this could be a long incomprehensible URI.
    // Usually it should be something like "thedude@ovi.com".
    generators += simpleDetailFieldGenerator(QContactOnlineAccount::DefinitionName,
                                             QContactOnlineAccount::FieldAccountUri);

    // The email address, e.g. "hans.wurst@nokia.com"
    generators += simpleDetailFieldGenerator(QContactEmailAddress::DefinitionName,
                                             QContactEmailAddress::FieldEmailAddress);

    // The phone number, e.g. "+3581122334455
    generators += simpleDetailFieldGenerator(QContactPhoneNumber::DefinitionName,
                                             QContactPhoneNumber::FieldNumber);

    // The contact's homepage URL, e.g. "http://www.nokia.com/"
    generators += simpleDetailFieldGenerator(QContactUrl::DefinitionName,
                                             QContactUrl::FieldUrl);

    return generators;
}
