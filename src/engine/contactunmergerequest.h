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

#ifndef QTRACKERCONTACTUNMERGEREQUEST_H
#define QTRACKERCONTACTUNMERGEREQUEST_H

#include "contactsaverequest.h"

#include <cubi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctUnmergeIMContactsRequest;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactSaveOrUnmergeRequest : public QTrackerBaseRequest<QctUnmergeIMContactsRequest>
{
    Q_DISABLE_COPY(QTrackerContactSaveOrUnmergeRequest)
    Q_OBJECT

public:
    explicit QTrackerContactSaveOrUnmergeRequest(QContactAbstractRequest *request,
                                                 QContactTrackerEngine *engine,
                                                 QObject *parent = 0);
    virtual ~QTrackerContactSaveOrUnmergeRequest();

protected: // QTrackerAbstractRequest API
    void run();
    void updateRequest(QContactManager::Error error);

private:
   QString buildQuery();

private: // methods
    bool resolveSourceContact();
    bool resolveUnmergedContactIds();

    bool fetchPredicates();
    bool unmergeContacts();

    Cubi::Insert insertContactQuery(const QString &contactIri, const QContactOnlineAccount &account);
    QList<Cubi::Delete> cleanupQueries(const QContactOnlineAccount &account);
    Cubi::Insert insertTelepathyGeneratorFallbackQuery(const QString &contactIri);

private: // fields
    const QList<QContactOnlineAccount> m_unmergeOnlineAccounts;
    const QContact m_sourceContact;
    QString m_sourceContactIri;
    QList<QContactLocalId> m_unmergedContactIds;
    QStringList m_unmergedContactIris;
    // Maps predicate graph to predicate iris
    QMultiHash<QString, QString> m_predicates;
    // Maps IMAddress iri to (affiliation graph, affiliation iri)
    QHash<QString, QPair<QString, QString> > m_onlineAccounts;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTUNMERGEREQUEST_H
