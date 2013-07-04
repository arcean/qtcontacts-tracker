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

#ifndef QTRACKERCONTACTSAVEREQUEST_H
#define QTRACKERCONTACTSAVEREQUEST_H

#include "baserequest.h"

#include <QElapsedTimer>
#include <cubi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactSaveRequest : public QTrackerBaseRequest<QContactSaveRequest>
{
    friend class UpdateBuilder;

    Q_DISABLE_COPY(QTrackerContactSaveRequest)
    Q_OBJECT

public:
    explicit QTrackerContactSaveRequest(QContactAbstractRequest *request,
                                        QContactTrackerEngine *engine,
                                        QObject *parent = 0);
    virtual ~QTrackerContactSaveRequest();

    void setTimestamp(const QDateTime &timestamp) { m_timestamp = timestamp; }
    const QDateTime & timestamp() const { return m_timestamp; }

    // might modify local ids
    QString queryString() const;

public: // QTrackerAbstractRequest API
    void updateRequest(QContactManager::Error error);
    Dependencies dependencies() const;
    void run();

private:
    // will remove odd details
    QContactManager::Error normalizeContact(QContact &contact, QHash<QString, QContactDetail> &detailsByUri) const;
    QContactManager::Error writebackThumbnails(QContact &contact);

    bool resolveContactIris();
    bool resolveContactIds();

    static bool isNewContact(const QContact &contact) { return 0 == contact.localId(); }
    bool isFullSaveRequest(const QContact &contact) const { return isNewContact(contact) || m_detailMask.isEmpty(); }
    bool isPartialSaveRequest(const QContact &contact) const { return not isFullSaveRequest(contact); }
    bool isUnknownDetail(const QContact &contact, const QString &name) const { return isPartialSaveRequest(contact) && not m_detailMask.contains(name); }

private: // fields
    QList<QContact> m_contacts;
    QStringList m_contactIris;
    QStringList m_detailMask;

    ErrorMap m_errorMap;

    const QString m_nameOrder;
    QDateTime m_timestamp;
    int m_batchSize;
    int m_updateCount;

    QElapsedTimer m_stopWatch;

    Cubi::ValueList m_weakSyncTargets;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTSAVEREQUEST_H
