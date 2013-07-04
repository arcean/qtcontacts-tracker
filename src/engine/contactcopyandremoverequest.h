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

#ifndef QTRACKERCONTACTCOPYANDREMOVEREQUEST_H
#define QTRACKERCONTACTCOPYANDREMOVEREQUEST_H

#include "contactremoverequest.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctContactMergeRequest;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactCopyAndRemoveRequest : public QTrackerBaseRequest<QctContactMergeRequest>
{
    Q_DISABLE_COPY(QTrackerContactCopyAndRemoveRequest)
    Q_OBJECT

    typedef QMultiMap<QContactLocalId, QContactLocalId> MergeIdMap;

public:
    explicit QTrackerContactCopyAndRemoveRequest(QContactAbstractRequest *request,
                                                 QContactTrackerEngine *engine,
                                                 QObject *parent = 0);
    virtual ~QTrackerContactCopyAndRemoveRequest();

protected: // QTrackerAbstractRequest API
    void run();
    void updateRequest(QContactManager::Error error);

private: // methods
    bool verifyRequest();
    bool resolveContactIds();
    bool fetchContactDetails();
    bool doMerge();

    QString pickPreferredSyncTarget(const QSet<QString> &syncTargets,
                                    const QString &destinationSyncTarget);
    QString buildMergeQuery(QContactLocalId id);
    QString mergeContacts(const QString &targetUrn, const QString &sourceUrn);

private: // fields
    const MergeIdMap m_mergeIds;

    ErrorMap m_errorMap;
    QHash<QContactLocalId, QString> m_contactIris;
    QMultiHash<QString, QString> m_contactPredicatesSingle;
    QMultiHash<QString, QString> m_contactPredicatesSingleWithGraph;
    QMultiHash<QString, QString> m_contactPredicatesMulti;
    QMultiHash<QString, QString> m_contactPredicatesMultiWithGraph;
    QHash<QString, QString> m_contactSyncTargets;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTCOPYANDREMOVEREQUEST_H
