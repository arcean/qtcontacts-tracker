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

// THIS HEADER IS PRIVATE. IT CANNOT REASONABLE USED OUTSIDE OF QCT

#ifndef QCTLOGGER_H
#define QCTLOGGER_H

#include "libqtcontacts_extensions_tracker_global.h"

#include <QString>

////////////////////////////////////////////////////////////////////////////////////////////////////

#define qctWarn(message) qctLogger().warn(message, __FILE__, __LINE__)
#define qctFail(message) qctLogger().fail(message, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////////////////////////

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctLogger
{
public:
    QctLogger(const QString &prefix = QString());

public: // attributes
    void setShowLocation(bool value) { m_showLocation = value; }
    bool showLocation() const { return m_showLocation; }

public: // methods
    void warn(const QString &message, const char *file, int line) const;
    void warn(const char *message, const char *file, int line) const;
    void fail(const QString &message, const char *file, int line) const;
    void fail(const char *message, const char *file, int line) const;

private:
    static QString makePrefix(const QString &prefix);

private:
    QByteArray m_prefix;
    bool m_showLocation : 1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QString qctTruncate(const QString &message, int limit = 500);
LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctLogger & qctLogger();

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QCTLOGGER_H
