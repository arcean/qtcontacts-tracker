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

#include "contactfetchbyidrequest.h"

#include "engine.h"

#include <lib/requestextensions.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

static QContactLocalIdFilter
localIdFilter(const QList<QContactLocalId> &localIds)
{
    QContactLocalIdFilter localIdFilter;
    localIdFilter.setIds(localIds);
    return localIdFilter;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactFetchByIdRequest::QTrackerContactFetchByIdRequest(QContactAbstractRequest *request,
                                                                 QContactTrackerEngine *engine,
                                                                 QObject *parent)
    : QTrackerBaseContactFetchRequest<QContactFetchByIdRequest>(request,
                                                                localIdFilter(staticCast(request)->localIds()),
                                                                staticCast(request)->fetchHint(),
                                                                QList<QContactSortOrder>(),
                                                                engine,
                                                                parent)
    , m_localIds(staticCast(request)->localIds())
{
}

QTrackerContactFetchByIdRequest::~QTrackerContactFetchByIdRequest()
{
}

void
QTrackerContactFetchByIdRequest::processResults(const ContactCache &results)
{
    // Build up the results and errors
    for (int i = 0; i < m_localIds.count(); i++) {
        const QContactLocalId id = m_localIds[i];
        ContactCache::ConstIterator it = results.find(id);

        if (it != results.constEnd()) {
            m_contacts.append(it.value());
        } else {
            m_errorMap.insert(i, QContactManager::DoesNotExistError);
            // The entries in the resulting list of contacts must match the entries in the id list by position.
            // An invalid ID should correspond to an empty QContact.
            m_contacts.append(QContact());
        }
    }

    if (not m_errorMap.isEmpty()) {
        setLastError((m_errorMap.constEnd() - 1).value());
    }
}

void
QTrackerContactFetchByIdRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateContactFetchByIdRequest(staticCast(engine()->request(this).data()),
                                            m_contacts, error, m_errorMap, QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactfetchbyidrequest.cpp"
