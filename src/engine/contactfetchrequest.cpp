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

#include "contactfetchrequest.h"

#include "engine.h"

#include <lib/requestextensions.h>


///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactFetchRequest::QTrackerContactFetchRequest(QContactAbstractRequest *request,
                                                         QContactTrackerEngine *engine,
                                                         QObject *parent)
    : QTrackerBaseContactFetchRequest<QContactFetchRequest>(request,
                                                            staticCast(request)->filter(),
                                                            staticCast(request)->fetchHint(),
                                                            staticCast(request)->sorting(),
                                                            engine,
                                                            parent)
    , m_nameOrder(QctRequestExtensions::get(request)->nameOrder())
{
}

QTrackerContactFetchRequest::~QTrackerContactFetchRequest()
{
}

void
QTrackerContactFetchRequest::mergeContactLists(QList<QContact> &l1, const QList<QContact> &l2)
{
    if (l1.isEmpty()) {
        l1.append(l2);
        return;
    }

    const QList<QContactSortOrder> sortOrders = sorting();
    QMutableListIterator<QContact> it1(l1);
    QList<QContact>::ConstIterator it2 = l2.constBegin();

    for (; it2 != l2.constEnd(); ++it2) {
        const QContact c2 = *it2;

        while (it1.hasNext() && engine()->compareContact(it1.peekNext(), c2, sortOrders) <= 0) {
            it1.next();
        }

        it1.insert(c2);
    }
}

void
QTrackerContactFetchRequest::processResults(const ContactCache &results)
{
    m_contacts.reserve(results.size());

    // Sort the contacts
    const QHash<QString, QList<QContactLocalId> > idsByType = sortedIds();

    // TODO: we could walk the ID lists by decreasing size here, so that for
    // the biggest one (most likely type == contact) we need no merge at
    // all
    for (QHash<QString, QList<QContactLocalId> >::ConstIterator it = idsByType.constBegin();
         it != idsByType.constEnd(); ++it) {
        const QList<QContact> contacts = getContacts(results, it.value());
        mergeContactLists(m_contacts, contacts);
    }
}

void
QTrackerContactFetchRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateContactFetchRequest(staticCast(engine()->request(this).data()),
                                        m_contacts, error, QContactAbstractRequest::FinishedState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_contactfetchrequest.cpp"
