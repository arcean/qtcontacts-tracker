/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "phoneutils.h"

#include "settings.h"
#include "threadlocaldata.h"

#include <cubi.h>

CUBI_USE_NAMESPACE

const QRegExp
qctPhoneNumberDTMFChars()
{
    static const QRegExp chars(QLatin1String("[pwx]"), Qt::CaseInsensitive);
    return chars;
}

QString
qctNormalizePhoneNumber(const QString& value)
{
    return qctNormalizePhoneNumber(value, Qct::RemoveUnicodeFormatters | Qct::RemoveFormatters);
}

QString
qctNormalizePhoneNumber(const QString& value, Qct::NumberNormalizationOptions options)
{
    static const QString visualSeparators = QLatin1String("[() .-]");

    QString result;

    result.reserve(value.length());

    foreach(const QChar ch, value) {
        if (options.testFlag(Qct::ConvertToLatin) && ch.isDigit()) {
            // convert eastern arabic numerals
            // http://en.wikipedia.org/wiki/Eastern_Arabic_numerals
            result += QChar::fromLatin1('0' + ch.digitValue());
            continue;
        }

        if (options.testFlag(Qct::RemoveNumberFormatters) && visualSeparators.contains(ch)) {
            // skip number formatters according to RFC 3966
            continue;
        }

        if (options.testFlag(Qct::RemoveUnicodeFormatters) && ch.category() == QChar::Other_Format) {
            // skip formatting characters like RLE, LRE, and PDF
            continue;
        }

        result += ch;
    }

    return result;
}

QString
qctMakeLocalPhoneNumber(const QString& value)
{
    const QString number = qctNormalizePhoneNumber(value, Qct::RemoveUnicodeFormatters | Qct::RemoveFormatters | Qct::ConvertToLatin);
    const int dtmfIndex = number.indexOf(qctPhoneNumberDTMFChars());
    const int localPhoneNumberLength(QctThreadLocalData::instance()->settings()->localPhoneNumberLength());

    const int last = (dtmfIndex == -1 ? number.length() : dtmfIndex);
    const int first = qMax(0, last - localPhoneNumberLength);

    return number.mid(first);
}

QString
qctMakePhoneNumberIri(const QString &number, const QStringList &subtypes, bool escape)
{
    static const QString simpleTemplate = QString::fromLatin1("urn:x-maemo-phone:%1");
    static const QString subtypeTemplate = QString::fromLatin1("urn:x-maemo-phone:%1:%2");
    static const QString subtypeSeparator = QString::fromLatin1(",");

    const QString escapedNumber = escape ? Utils::escapeIri(number) : number;

    if (subtypes.empty()) {
        return simpleTemplate.arg(escapedNumber);
    }

    QStringList lowercasedSubtypes;

    foreach (const QString &type, subtypes) {
        lowercasedSubtypes.append(escape ? Utils::escapeIri(type.toLower()) : type.toLower());
    }

    lowercasedSubtypes.sort();

    return subtypeTemplate.arg(lowercasedSubtypes.join(subtypeSeparator), escapedNumber);
}

QString
qctMakePhoneNumberIri(const QContactPhoneNumber &numberDetail, bool escape)
{
    return qctMakePhoneNumberIri(numberDetail.number(), numberDetail.subTypes(), escape);
}

Cubi::ResourceValue
qctMakePhoneNumberResource(const QContactPhoneNumber &numberDetail)
{
    return ResourceValue(qctMakePhoneNumberIri(numberDetail, false));
}

Cubi::ResourceValue
qctMakePhoneNumberResource(const QString &number, const QStringList &subtypes)
{
    return ResourceValue(qctMakePhoneNumberIri(number, subtypes, false));
}
