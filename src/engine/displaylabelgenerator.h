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

#ifndef DISPLAYLABELGENERATOR_H
#define DISPLAYLABELGENERATOR_H

#include <QContact>

#include <QString>
#include <QExplicitlySharedDataPointer>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctDisplayLabelGeneratorData;
class QctDisplayLabelGenerator
{
public:
    enum ListOption {
        // use "FirstName LastName" when considering real name for display label
        FirstNameLastName   = (0 << 0),
        // use "LastName FirstName" when considering real name for display label
        LastNameFirstName   = (1 << 0),
        // mask for the "various" realname usage options
        RealNameFormat      = (FirstNameLastName | LastNameFirstName),

        // prefer nickname over real name when building the display label
        PreferNickname      = (1 << 1)
    };

    Q_DECLARE_FLAGS(ListOptions, ListOption)

protected:
    explicit QctDisplayLabelGenerator(QctDisplayLabelGeneratorData *data);

public:
    ~QctDisplayLabelGenerator();

public: // operators
    QctDisplayLabelGenerator(const QctDisplayLabelGenerator &other);
    QctDisplayLabelGenerator & operator=(const QctDisplayLabelGenerator &other);

public: // methods
    QString createDisplayLabel(const QContact &contact) const;
    const QString & requiredDetailName() const;

public: // factory
    static const QList<QctDisplayLabelGenerator> & generators(ListOptions options);

private:
    static QList<QctDisplayLabelGenerator> createGenerators(ListOptions options);

    static QctDisplayLabelGenerator simpleDetailFieldGenerator(const QString &detailName,
                                                               const QString &fieldName);
    static QctDisplayLabelGenerator firstNameLastNameGenerator();
    static QctDisplayLabelGenerator lastNameFirstNameGenerator();

protected:
    QExplicitlySharedDataPointer<QctDisplayLabelGeneratorData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_DECLARE_OPERATORS_FOR_FLAGS(QctDisplayLabelGenerator::ListOptions)

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // DISPLAYLABELGENERATOR_H
