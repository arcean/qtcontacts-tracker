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

#include "conversion.h"

#include "transform.h"

#include <lib/logger.h>
#include <lib/phoneutils.h>
#include <lib/settings.h>

#include <cubi.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE

bool
Conversion::makeValue(const QVariant &from, QVariant &to) const
{
    if (&to != &from) {
        to.setValue(from);
    }

    return true;
}

bool
Conversion::parseValue(const QVariant &from, QVariant &to) const
{
    if (&to != &from) {
        to.setValue(from);
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
TelepathyIriConversion::makeValue(const QVariant &from, QVariant &to) const
{
    const QUrl iri = makeTelepathyIri(from.toString());
    // check isEmpty() instead of isValid() which is about encoding, not content
    return to.setValue(iri), not iri.isEmpty();
}

bool
TelepathyIriConversion::parseValue(const QVariant &from, QVariant &to) const
{
    bool ok = false;
    // Because IRI unescaping is only done for detailUri details in the fetch
    // request, we need to unescape here
    to = parseTelepathyIri(Utils::unescapeIri(from.toString()), &ok);
    return ok;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
LocalPhoneNumberConversion::makeValue(const QVariant &from, QVariant &to) const
{
    return to.setValue(qctMakeLocalPhoneNumber(from.toString())), true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
LatinPhoneNumberConversion::makeValue(const QVariant &from, QVariant &to) const
{
    return to.setValue(qctNormalizePhoneNumber(from.toString(), Qct::RemoveUnicodeFormatters | Qct::ConvertToLatin)), true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
UriAsForeignKeyConversion::makeValue(const QVariant &from, QVariant &to) const
{
    if (&to != &from) {
        to.setValue(from);
    }

    return to.convert(QVariant::String);
}

bool
UriAsForeignKeyConversion::parseValue(const QVariant &from, QVariant &to) const
{
    if (&to != &from) {
        to.setValue(from);
    }

    return to.convert(QVariant::Url);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool
DateTimeOffsetConversion::makeValue(const QVariant &from, QVariant &to) const
{
    // Some dates can't be represented by ISO8601, in this case fail
    if (from.toDateTime().toString(Qt::ISODate).isEmpty()) {
        return false;
    }

    to = from;

    return true;
}

bool
DateTimeOffsetConversion::parseValue(const QVariant &from, QVariant &to) const
{
    const QString timeWithZone = from.toString();
    const QStringList components = timeWithZone.split(DateTimeTransform::separator());

    if (components.length() > 1) {
        QDateTime dt = QDateTime::fromString(components.at(0), Qt::ISODate).
                                    addSecs(components.at(1).toLong());
        dt.setUtcOffset(components.at(1).toLong());
        to.setValue(dt);
    } else {
        QDateTime dt = QDateTime::fromString(components.at(0), Qt::ISODate);
        to.setValue(dt);
    }

    return true;
}
