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

#ifndef QCTGARBAGECOLLECTOR_H
#define QCTGARBAGECOLLECTOR_H

#include <QtCore>
#include <QtDBus>

#include "libqtcontacts_extensions_tracker_global.h"

/*!
 * \brief Simple interface to contactsd's garbage collection plugin
 *
 * The \c QctGarbageCollector class allows you to register and run garbage
 * collection queries with contactsd's gargage collection plugin. The GC
 * mechanism consists of two steps: first, the query must be registered with
 * a unique identifier using the registerQuery() method, and then the "load"
 * of the query can be augmented using the trigger() method. Queries are
 * initially registered with a load of 0, and their load go back to 0 when the
 * garbage collection query is ran. The garbage collection query is run when
 * the load of a query reaches 1 (there is a small delay between the moment when
 * the load reaches 1 and the actual garbage collection, to ease the load on the
 * device).
 */
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctGarbageCollector
{
public: // methods
    /*!
     * Registers a new \p query againt the garbage collector
     *
     * The \p id of the query should be a name unique accross all applications,
     * one possible solution is to use the DBus name of the application.
     * More than one query per application can be registered, but they all need
     * to have a unique id. Registering a new query with an existing id will
     * overwrite the old one.
     */
    static bool registerQuery(const QString &id, const QString &query);

    /*!
     * Increases the load of a query
     *
     * When the load of the query reaches 1, garbage collection will be
     * triggered.
     */
    static bool trigger(const QString &id, double load);

private: // methods
    static QDBusMessage createCall(const QString &method);
};

#endif // QCTGARBAGECOLLECTOR_H
