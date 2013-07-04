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

#define QTC_NO_GLOBAL_LOGGER 1
#include "logger.h"
#undef QTC_NO_GLOBAL_LOGGER

////////////////////////////////////////////////////////////////////////////////////////////////////

QctLogger::QctLogger(const QString &prefix)
    : m_prefix(qPrintable(makePrefix(prefix)))
    , m_showLocation(true)
{
}

void
QctLogger::warn(const QString &message, const char *file, int line) const
{
    warn(qPrintable(message), file, line);
}

void
QctLogger::warn(const char *message, const char *file, int line) const
{
    if (m_showLocation) {
        qWarning("%s: %s:%d: %s", m_prefix.constData(), file, line, message);
    } else {
        qWarning("%s", message);
    }
}

void
QctLogger::fail(const QString &message, const char *file, int line) const
{
    fail(qPrintable(message), file, line);
}

void
QctLogger::fail(const char *message, const char *file, int line) const
{
    if (m_showLocation) {
        qFatal("%s: %s:%d: %s", m_prefix.constData(), file, line, message);
    } else {
        qFatal("%s: %s", m_prefix.constData(), message);
    }
}

QString
QctLogger::makePrefix(const QString &prefix)
{
    QString result = QLatin1String(PACKAGE);

    if (not prefix.isEmpty()) {
        result += QLatin1Char('(');
        result += prefix;
        result += QLatin1Char(')');
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QString
qctTruncate(const QString &message, int limit)
{
    if (message.length() > limit) {
        return message.left(limit) + QLatin1String("[...]");
    }

    return message;
}

QctLogger &
qctLogger()
{
    static QctLogger logger;
    return logger;
}
