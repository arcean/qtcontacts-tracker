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

#ifndef LIBQTCONTACTS_EXTENSIONS_TRACKER_GLOBAL_H
#define LIBQTCONTACTS_EXTENSIONS_TRACKER_GLOBAL_H

#include <QtCore/qglobal.h>

#include <QtMobility/qmobilityglobal.h>
#include <QtMobility/QLatin1Constant>

#if defined(LIBQTCONTACTS_EXTENSIONS_TRACKER_LIBRARY)
#  define LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT Q_DECL_EXPORT
#else
#  define LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT Q_DECL_IMPORT
#endif

#define Q_DECLARE_EXTERN_LATIN1_CONSTANT(varname, str) \
    LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT extern const QLatin1Constant<sizeof(str)> varname

#endif // LIBQTCONTACTS_EXTENSIONS_TRACKER_GLOBAL_H
