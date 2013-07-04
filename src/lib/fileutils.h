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

#ifndef QCTFILEUTILS_H
#define QCTFILEUTILS_H

#include "libqtcontacts_extensions_tracker_global.h"

#include <QDir>

/*!
 * \brief Returns the common cache directory
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctHomeCacheDir();

/*!
 * \brief Returns the cache directory for qtcontacts-tracker
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctContactsCacheDir();

/*!
 * \brief Returns the cache directory for qtcontacts-tracker's avatars
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctAvatarCacheDir();

/*!
 * \brief Returns the common local data directory
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctLocalDataDir();

/*!
 * \brief Returns the local data directory for qtcontacts-tracker
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctContactsLocalDataDir();

/*!
 * \brief Returns the local directory for qtcontacts-tracker's avatars
 */
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QDir qctAvatarLocalDataDir();

#endif // QCTFILEUTILS_H
