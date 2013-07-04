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

#ifndef QCTPHONEUTILS_H
#define QCTPHONEUTILS_H

#include <QContactPhoneNumber>

#include <cubi.h>

#include "libqtcontacts_extensions_tracker_global.h"

namespace Qct
{
    /*!
     * \enum Qct::NumberNormalizationOption
     *
     * This enum lists the possible normalizations to apply to a phone number.
     */
    enum NumberNormalizationOption {
        NoNormalization         = 0,        /*!< Don't normalize the number at all */
        RemoveNumberFormatters  = 1 << 0,   /*!< Remove number formatters, as defined in RFC 3966 */
        ConvertToLatin          = 1 << 1,   /*!< Ensure all numbers are latin numbers (as opposed to
                                                 Eastern Arabic scripts for instance) */
        RemoveUnicodeFormatters = 1 << 2,   /*!< Remove Unicode formatting characters like RLE, LRE, or PDF */

        RemoveFormatters = RemoveNumberFormatters /*!< \deprecated Old name of RemoveNumberFormatters */
    };

    Q_DECLARE_FLAGS(NumberNormalizationOptions, NumberNormalizationOption)
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Qct::NumberNormalizationOptions)

QTM_USE_NAMESPACE

/*!
 * \brief Returns a regexp to match DTMF codes in a phone number
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT const QRegExp qctPhoneNumberDTMFChars();

/*!
 * \overload QString qctNormalizePhoneNumber(const QString &value, Qct::NumberNormalizationOptions options)
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctNormalizePhoneNumber(const QString& value);

/*!
 * \brief Returns the normalized version of a phone number
 *
 * The \p options parameters determines what transformations are applied to the
 * number during the normalization.
 *
 * \sa Qct::NumberNormalizationOption
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctNormalizePhoneNumber(const QString &value,
                                                                        Qct::NumberNormalizationOptions options);
/*!
 * \brief Returns the "lookup value" of a phone number
 *
 * This function returns the version of a phone number suitable to be stored
 * in the maemo:localPhoneNumber property of a nco:PhoneNumber. It is basically
 * the normalized version, additionally trimmed to the local phone number
 * length.
 *
 * \sa QctSettings::localPhoneNumberLength
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctMakeLocalPhoneNumber(const QString& value);

/*!
 * \brief Generates a normalized IRI from a phone number and its subtypes
 *
 * Returned IRI is of the form urn:x-maemo-phone:escapedNumber, or
 * urn:x-maemo-phone:subtype1,subtype2:escapedNumber
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctMakePhoneNumberIri(const QString &number,
                                                                      const QStringList &subtypes = QStringList(),
                                                                      bool escape = true);

/*!
 * \brief Generates a normalized IRI for a QContactPhoneNumber detail
 *
 * \sa qctMakePhoneNumberIri(const QString &number)
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctMakePhoneNumberIri(const QContactPhoneNumber &number,
                                                                      bool escape = true);

/*!
 * \brief Generates a Cubi::Resource with a normalized IRI for a QContactPhoneNumber detail
 *
 * \sa qctMakePhoneNumberIri(const QString &number)
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT Cubi::ResourceValue qctMakePhoneNumberResource(const QContactPhoneNumber &number);

/*!
 * \brief Generates a Cubi::Resource with a normalized IRI from a phone number and its subtypes
 *
 * \sa qctMakePhoneNumberIri(const QString &number)
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT Cubi::ResourceValue qctMakePhoneNumberResource(const QString &number,
                                                                                       const QStringList &subtypes = QStringList());

#endif // QCTPHONEUTILS_H
