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

#ifndef QCTAVATARUTILS_H
#define QCTAVATARUTILS_H

#include <QImage>
#include <QFile>

#include "libqtcontacts_extensions_tracker_global.h"

/*!
 * \brief Thumbnails the given \p image in place, and returns its full path
 * \return the full path if the thumbnailing succeeds, a null QString else
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString
qctWriteThumbnail(QImage &image);

/*!
 * \brief Thumbnails the given \p image in place, and returns its full path.
 * Possible errors are reported in \p unless null is passed.
 * \return the full path if the thumbnailing succeeds, a null QString else
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString
qctWriteThumbnail(QImage &image, QFile::FileError *error);

#endif // QCTAVATARUTILS_H
