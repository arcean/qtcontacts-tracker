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

#ifndef QTRACKERSCALARCONTACTQUERYBUILDER_H
#define QTRACKERSCALARCONTACTQUERYBUILDER_H

#include "contactdetail.h"

#include <cubi.h>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactDetail;
class QTrackerContactDetailField;
class QTrackerContactDetailSchema;
class PropertyInfoList;

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef QHash<QString, Cubi::Variable> VariablesByName;
typedef QHash<PropertyInfoList, Cubi::Variable> VariablesByChain;
typedef QHash<QString, class QctSplitPropertyChains> QctSplitFieldChains;

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerScalarContactQueryBuilder
{
    Q_DISABLE_COPY(QTrackerScalarContactQueryBuilder)

public: // constructors
    explicit QTrackerScalarContactQueryBuilder(const QTrackerContactDetailSchema &schema,
                                               const QString &managerUri);
    virtual ~QTrackerScalarContactQueryBuilder();

public: // attributes
    const QTrackerContactDetailSchema & schema() const { return m_schema; }

    static const Cubi::Variable & contact();
    static const Cubi::Variable & context();

    static const QChar graphSeparator();
    static const QChar detailSeparator();
    static const QChar fieldSeparator();
    static const QChar listSeparator();

public: // methods
    QContactManager::Error bindFields(const QTrackerContactDetail &detail,
                                      Cubi::Select &query,
                                      const QSet<QString> &fieldFilter = QSet<QString>());
    void bindCustomDetails(Cubi::Select &query,
                           const QSet<QString>& detailFilter = QSet<QString>());

    void bindHasMemberRelationships(Cubi::Select &query);

    static bool isFilterSupported(const QContactFilter &filter);
    QContactManager::Error bindFilter(const QContactFilter &filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(QContactDetailFilter filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(QContactDetailRangeFilter filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(const QContactLocalIdFilter &filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(const QContactIntersectionFilter &filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(const QContactUnionFilter &filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(const QContactChangeLogFilter &filter, Cubi::Filter &result);
    QContactManager::Error bindFilter(const QContactRelationshipFilter &filter, Cubi::Filter &result);

    QContactManager::Error bindSortOrders(const QList<QContactSortOrder> &orders,
                                          QList<Cubi::OrderComparator> &result);

private:
    QContactManager::Error bindUniqueDetailField(const QTrackerContactDetailField &field,
                                                 Cubi::Select &query);
    QContactManager::Error bindUniqueDetail(const QTrackerContactDetail &detail,
                                            Cubi::Select &query,
                                            const QSet<QString> &fieldFilter = QSet<QString>());
    Cubi::Value bindRestrictedValuesField(const QTrackerContactDetailField &field,
                                          const PropertyInfoList &chain,
                                          const Cubi::Variable &subject);
    QContactManager::Error bindCustomValues(const QTrackerContactDetailField &field,
                                            const Cubi::Variable &subject,
                                            PropertyInfoList chain,
                                            Cubi::Value &result);
    Cubi::Value bindInverseFields(const QList<QTrackerContactDetailField> &fields,
                                  const QctSplitFieldChains &fieldChains,
                                  const Cubi::Variable &subject,
                                  const Cubi::Pattern &prefixPattern);
    QContactManager::Error bindMultiDetail(const QTrackerContactDetail &detail,
                                           Cubi::Select &query,
                                           const Cubi::Variable &subject,
                                           const QSet<QString> &fieldFilter = QSet<QString>());

    const QTrackerContactDetailField * findField(const QTrackerContactDetail &detail,
                                                 const QString &fieldName);
    Cubi::Function matchFunction(QContactDetailFilter::MatchFlags flags,
                                 Cubi::Value variable, QVariant value);
    QContactManager::Error bindTypeFilter(const QContactDetailFilter &filter,
                                          Cubi::Filter &result);
    void bindInvalidFilter(Cubi::Filter &result);
    void bindCustomDetailFilter(const QContactDetailFilter &filter,
                                Cubi::Exists &exists,
                                Cubi::Variable &subject);
    Cubi::PatternGroup bindWithoutMappingFilter(const QTrackerContactDetailField *field,
                                                const Cubi::Variable &subject,
                                                Cubi::Variable &value);
    void bindDTMFNumberFilter(const QContactDetailFilter &filter,
                              Cubi::Exists &exists,
                              const Cubi::Variable &subject);
    template<class T> QContactManager::Error bindFilterDetailField(const T &filter,
                                                                   Cubi::Exists &exists,
                                                                   Cubi::Variable &subject,
                                                                   bool hasCustomSubType = false,
                                                                   const PropertyInfoBase &propertySubtype = PropertyInfoBase());
    template<class T> QContactManager::Error bindFilterDetail(const T &filter,
                                                              Cubi::Exists &exists,
                                                              Cubi::Variable &subject,
                                                              bool hasCustomSubType = false,
                                                              const PropertyInfoBase &propertySubtype = PropertyInfoBase());
    template<class T> QContactManager::Error bindDetailFilterForAnyField(const T &filter,
                                                                         const QTrackerContactDetail *const detail,
                                                                         Cubi::Filter &result);
    template<class T> QContactManager::Error bindDetailContextFilter(const T &filter,
                                                                     const QTrackerContactDetail * const detail,
                                                                     Cubi::Filter &result);

    QVariant normalizeFilterValue(const QTrackerContactDetailField *field,
                                  const QContactDetailFilter::MatchFlags &flags,
                                  const QVariant &value,
                                  QContactManager::Error &error);
    QContactFilter::MatchFlags normalizeFilterMatchFlags(const QTrackerContactDetailField *field,
                                                         const QContactFilter::MatchFlags &matchFlags,
                                                         const QVariant &value = QVariant());
private:
    const QTrackerContactDetailSchema  &m_schema;
    VariablesByName                     m_variables;
    QString                             m_managerUri;
};

#endif // QTRACKERSCALARCONTACTQUERYBUILDER_H
