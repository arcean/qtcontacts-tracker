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

#include "slots.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactLocalIdFetchRequest>

Slots::Slots(int idEntryCount)
    : QObject()
    , idEntryCount(idEntryCount)
{
}

void Slots::notifyIds(const QList<QContactLocalId> &ids)
{
    this->ids << ids;
    if (idEntryCount > 0 && this->ids.size() >= idEntryCount) {
        loop.quit();
    }
}

void Slots::idResultsAvailable()
{
    ids << qobject_cast<QContactLocalIdFetchRequest*>(sender())->ids();
}

void Slots::resultsAvailable()
{
    contacts = qobject_cast<QContactFetchRequest*>(sender())->contacts();
}

void Slots::wait(int ms)
{
    QTimer::singleShot(ms, &loop, SLOT(quit()));
    loop.exec();
}

void Slots::clear()
{
    ids.clear();
    contacts.clear();
}
