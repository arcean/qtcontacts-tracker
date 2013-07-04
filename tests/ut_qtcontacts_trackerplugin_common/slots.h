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

#ifndef SLOTS_H
#define SLOTS_H

#include <QtContacts/QContact>
#include <QtCore/QEventLoop>

QTM_USE_NAMESPACE

class Slots: public QObject
{
    Q_OBJECT

public:
    Slots(int idEntryCount = 0);

public slots:
    void notifyIds(const QList<QContactLocalId> &ids);
    void idResultsAvailable();
    void resultsAvailable();

    void wait(int ms);
    void clear();

public:
    QList<QContactLocalId> ids;
    QList<QContact> contacts;
    QEventLoop loop;
    int idEntryCount;
};

#endif // SLOTS_H
