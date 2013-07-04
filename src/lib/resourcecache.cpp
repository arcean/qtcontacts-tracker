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

#include "resourcecache.h"

#include "sparqlresolver.h"
#include "threadutils.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctResourceCacheData : public QSharedData
{
    friend class QctResourceCache;

private: // fields
    QHash<uint, QString> m_resourceIris;
    QHash<QString, uint> m_trackerIds;
    QReadWriteLock m_readWriteLock;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QctResourceCache::QctResourceCache()
    : d(new QctResourceCacheData)
{
}


QctResourceCache &
QctResourceCache::instance()
{
    static QctResourceCache instance;
    return instance;
}

bool
QctResourceCache::prefill(const QStringList &resourceIris) const
{
    return QctTrackerIdResolver(resourceIris).lookupAndWait();
}

uint
QctResourceCache::trackerId(const QString &resourceIri) const
{
    QCT_SYNCHRONIZED_READ(&d->m_readWriteLock);
    return d->m_trackerIds.value(resourceIri, 0);
}

QString
QctResourceCache::resourceIri(uint trackerId) const
{
    QCT_SYNCHRONIZED_READ(&d->m_readWriteLock);
    return d->m_resourceIris.value(trackerId);
}

void
QctResourceCache::insert(const QString &resourceIri, uint trackerId)
{
    QCT_SYNCHRONIZED_WRITE(&d->m_readWriteLock);
    d->m_resourceIris.insert(trackerId, resourceIri);
    d->m_trackerIds.insert(resourceIri, trackerId);
}

void
QctResourceCache::clear()
{
    // basically for unit tests
    QCT_SYNCHRONIZED_WRITE(&d->m_readWriteLock);
    d->m_resourceIris.clear();
    d->m_trackerIds.clear();
}
