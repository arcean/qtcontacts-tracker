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

#include "presenceutils.h"

#include <QtCore>

#include <QContactGlobalPresence>

template <class T> static void
transfer(const T &key, const QContactDetail &source, QContactDetail &target)
{
    QVariant value(source.variantValue(key));

    if (not value.isNull()) {
        target.setValue(key, value);
    }
}

int
qctAvailabilityRank(QContactPresence::PresenceState state)
{
    switch(state) {
    case QContactPresence::PresenceUnknown:
        return 0;
    case QContactPresence::PresenceHidden:
    case QContactPresence::PresenceOffline:
        return 1;
    case QContactPresence::PresenceAway:
    case QContactPresence::PresenceExtendedAway:
        return 2;
    case QContactPresence::PresenceBusy:
        return 3;
    case QContactPresence::PresenceAvailable:
        return 4;
    }

    return qctAvailabilityRank(QContactPresence::PresenceUnknown);
}

void
qctUpdateGlobalPresence(QContact &contact)
{
    const QList<QContactPresence> presenceDetails = contact.details<QContactPresence>();
    const QContactPresence *mostAvailable = 0;
    const QContactPresence *mostRecent = 0;

    if (not presenceDetails.isEmpty()) {
        mostAvailable = mostRecent = &presenceDetails.first();

        foreach(const QContactPresence &detail, presenceDetails) {
            if (qctAvailabilityRank(detail.presenceState()) > qctAvailabilityRank(mostAvailable->presenceState())) {
                if (detail.timestamp() == mostRecent->timestamp()) {
                    mostRecent = &detail;
                }

                mostAvailable = &detail;
            }

            if (detail.timestamp() > mostRecent->timestamp()) {
                mostRecent = &detail;
            }
        }
    }

    QContactGlobalPresence global = contact.detail<QContactGlobalPresence>();

    if (mostRecent && mostAvailable) {
        QSet<QString> linkedDetails;
        linkedDetails += mostRecent->detailUri();
        linkedDetails += mostAvailable->detailUri();
        global.setLinkedDetailUris(linkedDetails.toList());

        transfer(QContactGlobalPresence::FieldNickname, *mostRecent, global);
        transfer(QContactGlobalPresence::FieldTimestamp, *mostRecent, global);
        transfer(QContactGlobalPresence::FieldCustomMessage, *mostRecent, global);

        transfer(QContactGlobalPresence::FieldPresenceState, *mostAvailable, global);
        transfer(QContactGlobalPresence::FieldPresenceStateText, *mostAvailable, global);
        transfer(QContactGlobalPresence::FieldPresenceStateImageUrl, *mostAvailable, global);

        contact.saveDetail(&global);
    } else {
        contact.removeDetail(&global);
    }
}
