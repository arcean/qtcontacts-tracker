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

#ifndef QCTTHREADLOCALDATA_H
#define QCTTHREADLOCALDATA_H

////////////////////////////////////////////////////////////////////////////////////////////////////

#include <QtCore>

#include "libqtcontacts_extensions_tracker_global.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QSparqlConnection;
class QctSettings;
class QctTrackerChangeListener;

////////////////////////////////////////////////////////////////////////////////////////////////////

class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctThreadLocalData
{
    friend class QThreadStorage<QctThreadLocalData *>;

private:
    QctThreadLocalData();
    ~QctThreadLocalData();

public: // singleton style accessor
    static QctThreadLocalData * instance();

public: // attributes
    void setSparqlConnection(QSparqlConnection *connection);
    QSparqlConnection * sparqlConnection() const { return m_sparqlConnection; }

    void setTrackerChangeListener(const QString &managerUri, QctTrackerChangeListener *listener);
    QctTrackerChangeListener * trackerChangeListener(const QString &managerUri) const { return m_trackerChangeListeners.value(managerUri); }

    QctSettings * settings() const { return m_settings; }

private: // fields
    QSparqlConnection * m_sparqlConnection;
    QHash<QString, QctTrackerChangeListener *> m_trackerChangeListeners;
    QctSettings * m_settings;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QCTTHREADLOCALDATA_H
