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

#ifndef QTRACKERABSTRACTCONTACTFETCHREQUEST_H
#define QTRACKERABSTRACTCONTACTFETCHREQUEST_H

#include "abstractrequest.h"

#include <cubi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetailBuilder;
class QTrackerContactDetail;
class QTrackerContactDetailField;
class QTrackerScalarContactQueryBuilder;

class QSparqlResult;
class QSparqlResultRow;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerAbstractContactFetchRequest : public QTrackerAbstractRequest
{
    Q_DISABLE_COPY(QTrackerAbstractContactFetchRequest)
    Q_OBJECT

    class DetailContext;
    class QueryContext;

public:
    typedef QHash<QContactLocalId, QContact> ContactCache;

    explicit QTrackerAbstractContactFetchRequest(QContactAbstractRequest *request,
                                                 const QContactFilter &filter,
                                                 const QContactFetchHint &fetchHint,
                                                 const QList<QContactSortOrder> &sorting,
                                                 QContactTrackerEngine *engine,
                                                 QObject *parent = 0);
    virtual ~QTrackerAbstractContactFetchRequest();

    Cubi::Select query(const QString &contactType, QContactManager::Error &error) const;

protected: // API to be implemented
    virtual void processResults(const ContactCache &results) = 0;

protected: // QTrackerAbstractRequest API
    void run();

protected:
    const QList<QContactSortOrder> &sorting() const { return m_sorting; }
    QHash<QString, QList<QContactLocalId> > sortedIds() const { return m_sortedIds; }
    static QList<QContact> getContacts(const ContactCache &cache,
                                       const QList<QContactLocalId> &ids);

private:
    QContactManager::Error bindDetails(QueryContext &context) const;
    QContactManager::Error buildQuery(QueryContext &context) const;

    void bindSorting(QTrackerScalarContactQueryBuilder &queryBuilder,
                     QTrackerAbstractContactFetchRequest::QueryContext &context) const;

    QContactManager::Error runPreliminaryIdFetchRequest(QList<QContactLocalId> &ids);

    Cubi::Select baseQuery(const QTrackerScalarContactQueryBuilder &queryBuilder) const;
    void fetchUniqueDetail(QList<QContactDetail> &details,
                           const QueryContext &queryContext,
                           const DetailContext &context,
                           const QString &affiliation);
    void fetchMultiDetails(QList<QContactDetail> &details,
                           const QueryContext &queryContext,
                           const DetailContext &context);
    void fetchMultiDetail(QContactDetail &detail,
                          const DetailContext &context,
                          const QString &rawValueString);
    void fetchResults(ContactCache &results, QueryContext &context);
    QContactDetail fetchCustomDetail(const QString &rawValue,
                                     const QString &contactType);
    void fetchCustomDetails(const QueryContext &queryContext,
                            ContactCache::Iterator contact);
    void fetchHasMemberRelationships(const QueryContext &queryContext,
                                     ContactCache::Iterator contact);

    QVariant fetchInstances(const QTrackerContactDetailField &field,
                            const QString &rawValueString,
                            QSet<QString> &graphIris) const;
    QVariant fetchField(const QTrackerContactDetailField &field,
                        const QString &rawValueString,
                        QSet<QString> &graphIris) const;
    void fetchCustomValues(const QTrackerContactDetailField &field,
                           QVariant &fieldValue,
                           const QString &rawValueString,
                           QSet<QString> &graphIris) const;
    bool saveDetail(ContactCache::iterator contact, QContactDetail &detail,
                    const QTrackerContactDetail &definition);

    enum ListExtractionMode {
        KeepListAsIs, ///< Take the raw list.
        TrimList ///< Remove empty or duplicated items
    };

    QString fieldStringWithStrippedGraph(const QTrackerContactDetailField &field,
                                         const QString &rawValueString, QSet<QString> &graphIris) const;
    QStringList fieldStringListWithStrippedGraph(const QTrackerContactDetailField &field,
                                                 const QString &rawValueString, QSet<QString> &graphIris,
                                                 ListExtractionMode mode = KeepListAsIs) const;

private: // fields
    QContactFilter                      m_filter;
    const QContactFetchHint             m_fetchHint;
    const QString                       m_nameOrder;
    QList<QContactSortOrder>            m_sorting;
    QHash<QString, QList<QContactLocalId> > m_sortedIds;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTFETCHREQUEST_H
