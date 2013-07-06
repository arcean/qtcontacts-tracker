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

#include "abstractcontactfetchrequest.h"

#include "engine.h"

#include <dao/contactdetail.h>
#include <dao/contactdetailschema.h>
#include <dao/scalarquerybuilder.h>
#include <dao/subject.h>
#include <dao/support.h>
#include <lib/constants.h>
#include <lib/logger.h>
#include <lib/presenceutils.h>
#include <lib/contactlocalidfetchrequest.h>
#include <lib/requestextensions.h>
#include <lib/resourcecache.h>
#include <lib/sparqlconnectionmanager.h>

#include <QtSparql>

///////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

///////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerAbstractContactFetchRequest::DetailContext
{
public:
    explicit DetailContext(const QTrackerContactDetail &definition,
                           int firstColumn, int lastColumn)
        : m_definition(definition)
        , m_firstColumn(firstColumn)
        , m_lastColumn(lastColumn)
    {
    }

    const QTrackerContactDetail & definition() const { return m_definition; }
    int firstColumn() const { return m_firstColumn; }
    int lastColumn() const { return m_lastColumn; }

private:
    QTrackerContactDetail   m_definition;
    int                     m_firstColumn;
    int                     m_lastColumn;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerAbstractContactFetchRequest::QueryContext
{
public:
    explicit QueryContext(const QTrackerContactDetailSchema &schema)
        : result(0)
        , customDetailColumn(-1)
        , hasMemberRelationshipColumn(-1)
        , fetchAllDetails(false)
        , sorted(false)
        , m_schema(schema)
    {
    }

public: // attributes
    const QTrackerContactDetailSchema & schema() const { return m_schema; }
    const QString & contactType() const { return m_schema.contactType(); }

public: // fields
    Select query;
    QSparqlResult *result;

    QList<DetailContext> details;
    QSet<QString> definitionHints;
    QSet<QString> customDetailHints;
    QList<QContactLocalId> contactIds;
    int customDetailColumn;
    int hasMemberRelationshipColumn;

    bool fetchAllDetails : 1;
    bool sorted : 1;

private: // fields
    QTrackerContactDetailSchema m_schema;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerAbstractContactFetchRequest::QTrackerAbstractContactFetchRequest(QContactAbstractRequest *request,
                                                                         const QContactFilter &filter,
                                                                         const QContactFetchHint &fetchHint,
                                                                         const QList<QContactSortOrder> &sorting,
                                                                         QContactTrackerEngine *engine,
                                                                         QObject *parent)
    : QTrackerAbstractRequest(engine, parent)
    , m_filter(filter)
    , m_fetchHint(engine->normalizedFetchHint(fetchHint,
                                              QctRequestExtensions::get(request)->nameOrder()))
    , m_nameOrder(QctRequestExtensions::get(request)->nameOrder())
    , m_sorting(sorting)
{
}

QTrackerAbstractContactFetchRequest::~QTrackerAbstractContactFetchRequest()
{
}

/// returns a query for the contact iri, the nco::contactLocalUID, the context iri, the rdfs::label of the context
Select
QTrackerAbstractContactFetchRequest::baseQuery(const QTrackerScalarContactQueryBuilder &queryBuilder) const
{
    Variable contact(queryBuilder.contact());
    Variable context(queryBuilder.context());

    Select query;

    query.addProjection(contact);
    query.addProjection(Functions::trackerId.apply(contact));
    query.addProjection(context);
    query.addProjection(rdfs::label::function().apply(context));

    foreach(const QString &classIri, queryBuilder.schema().contactClassIris()) {
        query.addRestriction(contact, rdf::type::resource(), ResourceValue(classIri));
    }

    PatternGroup contextPattern;
    contextPattern.setOptional(true);
    contextPattern.addPattern(contact, nco::hasAffiliation::resource(), context);
    query.addRestriction(contextPattern);

    return query;
}

static bool
isForeignGraph(const QString &graphIri)
{
    return (not graphIri.isEmpty() &&
            QtContactsTrackerDefaultGraphIri != graphIri);
}

static QContactManager::Error
bindFilters(QTrackerScalarContactQueryBuilder &queryBuilder,
            const QContactFilter &filter, Select &select)
{
    Filter result;

    const QContactManager::Error error = queryBuilder.bindFilter(filter, result);
    select.setFilter(result);

    return error;
}

/// Returns the @p rawValueString with the graphIri removed if present.
/// Sets @p isOtherGraph to @c true if the graphIri is not the one of qct and not emoty, @c false otherwise.
QString
QTrackerAbstractContactFetchRequest::fieldStringWithStrippedGraph(const QTrackerContactDetailField &field,
                                                                  const QString &rawValueString,
                                                                  QSet<QString> &graphIris) const
{
    if (not field.hasOwner()) {
        return rawValueString;
    }

    QString string = rawValueString;
    const int s = string.indexOf(QTrackerScalarContactQueryBuilder::graphSeparator());
    if (s < 0) {
        qctWarn(QString::fromLatin1("Could not find graphIri added for field %1: %2").
                arg(field.name(), string));
    } else {
        const QString graphIri = string.mid(s+1);
        string.truncate(s);

        graphIris.insert(graphIri);

        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
            if (isForeignGraph(graphIri)) {
                qDebug() << "Read field from other graph:" << field.name() << string << graphIri;
            }
        }
    }
    return string;
}

/// Returns the @p rawValueString splitted into stringlist with the graphIris removed if present.
/// Sets @p isOtherGraph to @c true if any graphIri is not the one of qct and not emoty, @c false otherwise.
QStringList
QTrackerAbstractContactFetchRequest::fieldStringListWithStrippedGraph(const QTrackerContactDetailField &field,
                                                                      const QString &rawValueString,
                                                                      QSet<QString> &graphIris,
                                                                      ListExtractionMode mode) const
{
    QStringList list =  rawValueString.split(QTrackerScalarContactQueryBuilder::listSeparator(),
                                             mode == TrimList ? QString::SkipEmptyParts
                                                              : QString::KeepEmptyParts);

    if (mode == TrimList) {
        list.removeDuplicates();
    }

    if (not field.hasOwner()) {
        return list;
    }

    for (int i = 0; i<list.size(); ++i) {
        QString &string = list[i];
        const int s = string.indexOf(QTrackerScalarContactQueryBuilder::graphSeparator());

        if (s < 0) {
            qctWarn(QString::fromLatin1("Could not find graphIri added for field %1: %2").
                    arg(field.name(), string));
        } else {
            const QString itemGraphIri = string.mid(s+1);
            string.truncate(s);

            graphIris.insert(itemGraphIri);

            if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
                if ((not itemGraphIri.isEmpty()) &&
                    (QtContactsTrackerDefaultGraphIri != itemGraphIri)) {
                    qDebug() << "Read field item from other graph:" << field.name() << string << itemGraphIri;
                }
            }
        }
    }

    return list;
}


QContactManager::Error
QTrackerAbstractContactFetchRequest::bindDetails(QueryContext &context) const
{
    QTrackerScalarContactQueryBuilder queryBuilder(context.schema(), engine()->managerUri());

    context.query = baseQuery(queryBuilder);

    foreach(const QTrackerContactDetail &detail, context.schema().details()) {
        if (detail.isSynthesized()) {
            continue;
        }

        // skip details which are not needed according to the fetch hints
        if (not context.definitionHints.contains(detail.name()) &&
                not context.definitionHints.isEmpty()) {
            continue;
        }

        if (detail.fields().isEmpty()) {
            if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
                qctWarn(QString::fromLatin1("Not implemented yet, skipping %1 detail").
                        arg(detail.name()));
            }

            continue;
        }

        // bind this detail to the proper query
        const int firstColumn = context.query.projections().count();
        const QContactManager::Error error = queryBuilder.bindFields(detail, context.query);

        // verify results
        if (error != QContactManager::NoError) {
            context.query = Select();
            return error;
        }

        if (context.query.isEmpty()) {
            qctWarn("No queries built");
            return QContactManager::UnspecifiedError;
        }

        // store this detail's context
        const int lastColumn = context.query.projections().count();
        context.details += DetailContext(detail, firstColumn, lastColumn);
    }

    // bind the filters
    const QContactManager::Error filterError = bindFilters(queryBuilder, m_filter, context.query);
    if (filterError != QContactManager::NoError) {
        return filterError;
    }

    // check if we can sort results in Tracker
    // return value is ignored, if sorting fails we'll fallback to in-memory sorting
    bindSorting(queryBuilder, context);

    // Create almost empty base query if actually no unique details where requested by the
    // fetch hints. This is needed to let fetchBaseModel() populate the contact cache, and more
    // importantly it is needed to avoid bogus "DoesNotExistError" errors if existing contacts
    // are requested via local id filter, but those contacts don't have any of the requested
    // details.
    if (context.query.isEmpty()) {
        QTrackerScalarContactQueryBuilder queryBuilder(context.schema(), engine()->managerUri());

        context.query = baseQuery(queryBuilder);
        const QContactManager::Error error = bindFilters(queryBuilder, m_filter, context.query);

        if (QContactManager::NoError != error) {
            context.query = Select();
            return error;
        }

        bindSorting(queryBuilder, context);
    }

    return QContactManager::NoError;
}

static bool
checkRelationshipTypesHint(const QStringList &relationshipTypes)
{
    if (relationshipTypes.isEmpty()) {
        return true; // no hint specified -> fetch all relationships
    }

    QSet<QString> unsupportedTypes;
    bool fetchRelationships = false;

    foreach (const QString &type, relationshipTypes) {
        if (type == QContactRelationship::HasMember) {
            // Do not abort, but check the other types as well for complete feedback.
            fetchRelationships = true;
        } else {
            // Unsupported types are ignored, do not result in error
            unsupportedTypes += type;
        }
    }

    if (not unsupportedTypes.isEmpty()) {
        qctWarn(QString::fromLatin1("Only HasMember relationship is supported, "
                                    "but got %1 in contact fetch hint.").
                arg(QStringList(unsupportedTypes.toList()).join(QLatin1String(", "))));
    }

    return fetchRelationships;
}

QContactManager::Error
QTrackerAbstractContactFetchRequest::buildQuery(QueryContext &context) const
{
    if (not m_fetchHint.detailDefinitionsHint().isEmpty()) {
        context.definitionHints = m_fetchHint.detailDefinitionsHint().toSet();

        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
            qDebug() << "explicit definition hints:" << m_fetchHint.detailDefinitionsHint();
        }

        // make sure we can synthesized all requested details
        foreach(const QString &name, m_fetchHint.detailDefinitionsHint()) {
            const QTrackerContactDetail *const detail = context.schema().detail(name);

            if (0 != detail) {
                context.definitionHints += detail->dependencies();
            } else if (not context.schema().isSyntheticDetail(name)) {
                context.customDetailHints += name;
            }
        }

        // make sure we fetch everything needed for sorting
        foreach(const QContactSortOrder &order, m_sorting) {
            const QString name = order.detailDefinitionName();
            const QTrackerContactDetail *const detail = context.schema().detail(name);

            if (0 != detail) {
                context.definitionHints += name;
            } else if (not context.schema().isSyntheticDetail(name)) {
                context.customDetailHints += name;
            }
        }

        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
            qDebug() << "final definition hints:" << context.definitionHints;
            qDebug() << "custom detail hints:" << context.customDetailHints;
        }
    } else {
        if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
            qDebug() << "fetching all details";
        }

        context.fetchAllDetails = true;
        context.definitionHints.clear();
    }

    const QContactManager::Error error = bindDetails(context);

    if (QContactManager::NoError != error) {
        return error;
    }

    // Build custom detail query when needed
    if (not context.customDetailHints.isEmpty() || context.fetchAllDetails) {
        QTrackerScalarContactQueryBuilder queryBuilder(context.schema(), engine()->managerUri());
        context.customDetailColumn = context.query.projections().count();
        queryBuilder.bindCustomDetails(context.query, context.customDetailHints);
    }

    // Build relationships query if asked to (only HasMember supported)
    if (not m_fetchHint.optimizationHints().testFlag(QContactFetchHint::NoRelationships)) {
        if (checkRelationshipTypesHint(m_fetchHint.relationshipTypesHint())) {
            context.hasMemberRelationshipColumn = context.query.projections().count();

            if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
                qDebug() << "fetching HasMember relationships";
            }

            QTrackerScalarContactQueryBuilder queryBuilder(context.schema(), engine()->managerUri());
            queryBuilder.bindHasMemberRelationships(context.query);
        }
    }

    if (context.query.isEmpty()) {
        qctWarn("No queries built");
        return QContactManager::UnspecifiedError;
    }

    return QContactManager::NoError;
}

void
QTrackerAbstractContactFetchRequest::bindSorting(QTrackerScalarContactQueryBuilder &queryBuilder,
                                                 QTrackerAbstractContactFetchRequest::QueryContext &context) const
{
    QList<OrderComparator> orderBy;
    const QContactManager::Error error = queryBuilder.bindSortOrders(m_sorting, orderBy);

    // No error forwarding needed, context.sorted is enough information
    if (error != QContactManager::NoError) {
        return;
    }

    context.query.setOrderBy(orderBy);
    context.sorted = true;
}

QContactManager::Error
QTrackerAbstractContactFetchRequest::runPreliminaryIdFetchRequest(QList<QContactLocalId> &ids)
{
    if (engine()->hasDebugFlag(QContactTrackerEngine::ShowNotes)) {
        qctWarn(QString::fromLatin1("Running a preliminary ID fetch request before fetching contacts"));
    }

    QctContactLocalIdFetchRequest request;
    request.setFilter(m_filter);
    request.setSorting(m_sorting);
    // We force a native request, there's no point in running a preliminary fetch request
    // if it runs a QContactFetchRequest behind the scenes
    request.setForceNative(true);
    request.setLimit(m_fetchHint.maxCountHint());

    QScopedPointer<QTrackerAbstractRequest>(engine()->createRequestWorker(&request))->exec();

    if (request.error() != QContactManager::NoError) {
        return request.error();
    }

    ids = request.ids();

    return QContactManager::NoError;
}

Select
QTrackerAbstractContactFetchRequest::query(const QString &contactType,
                                           QContactManager::Error &error) const
{
    QTrackerContactDetailSchemaMap::ConstIterator schema = engine()->schemas().find(contactType);

    if (schema == engine()->schemas().constEnd()) {
        error = QContactManager::BadArgumentError;
        return Select();
    }

    QueryContext context(engine()->schema(contactType));
    error = buildQuery(context);
    return context.query;
}

/// Returns a list of integers created from the strings in @p stringList.
/// The integers are in the order of the string, if a string could not be converted,
/// the corresponding integer is @c 0.
static QList<int>
toIntList(const QStringList &stringList)
{
    QList<int> intList;

    foreach(const QString &element, stringList) {
        intList.append(element.toInt());
    }

    return intList;
}

/// Returns a list of integers created from the content of @p string.
/// The string is separated using @p separator (defaults to QTrackerScalarContactQueryBuilder::listSeparator()),
/// the integers are in the order of the separated substrings.
/// If a substring could not be converted, the corresponding integer is @c 0.
static QList<int>
toIntList(const QString &string,
          const QChar separator = QTrackerScalarContactQueryBuilder::listSeparator())
{
    return toIntList(string.split(separator));
}

/// returns the subtype(s) for the given field as a QVariant.
/// Uses the default subtype(s) if the passed @p subTypes is empty.
static QVariant
fetchSubTypes(const QTrackerContactDetailField &field,
              QSet<QString> subTypes)
{
    // apply default value if no subtypes could be read
    if (subTypes.isEmpty() && field.hasDefaultValue()) {
        switch(field.defaultValue().type()) {
        case QVariant::String:
            subTypes.insert(field.defaultValue().toString());
            break;
        case QVariant::StringList:
            subTypes = field.defaultValue().toStringList().toSet();
            break;
        default:
            qctWarn(QString::fromLatin1("Invalid type %1 for subtype field %2").
                    arg(QLatin1String(field.defaultValue().typeName()), field.name()));
            break;
        }
    }

    // set field if any subtypes could be identified
    if (not subTypes.isEmpty()) {
        switch(field.dataType()) {
        case QVariant::String:
            // TODO: clone this detail multiple times when |subTypes| > 1
            return *subTypes.begin();
            break;

        case QVariant::StringList:
            return QStringList(subTypes.toList());
            break;

        default:
            qctWarn(QString::fromLatin1("Invalid type %1 for subtype field %2").
                    arg(QLatin1String(QVariant::typeToName(field.dataType())), field.name()));
            break;
        }
    }

    return QVariant();
}

static int
trackerId(const ResourceInfo &resource)
{
    return QctResourceCache::instance().trackerId(resource.iri());
}

/// Returns the subtypes encoded in @p rawValueString as string or stringlist in a QVariant.
/// Returns the default subtype(s) if there is no known subtype in @p rawValueString.
static QVariant
fetchSubTypesClasses(const QTrackerContactDetailField &field,
                     const QString &rawValueString)
{
    const QList<int> fetchedSubTypes = toIntList(rawValueString);
    QSet<QString> subTypes;

    foreach(const ClassInfoBase &ci, field.subTypeClasses()) {
        if (fetchedSubTypes.contains(trackerId(ci))) {
            subTypes.insert(ci.text());
        }
    }

    return fetchSubTypes(field, subTypes);
}

/// Returns the list of details, with each detail having all subtypes collected and set.
/// If there was no subtype for a detail, it was set the default subtype(s).
static QList<QContactDetail>
unifyPropertySubTypes(const QMultiHash<QString, QContactDetail> &details,
                      const QTrackerContactDetailField &subTypeField)
{
    QList<QContactDetail> results;
    QStringList subTypes = details.uniqueKeys();
    subTypes.removeOne(QString());

    foreach(QContactDetail detail, details.values(QString())) {
        QSet<QString> detailSubTypes;

        foreach(const QString &subType, subTypes) {
            if(details.values(subType).contains(detail)) {
                detailSubTypes.insert(subType);
            }
        }

        detail.setValue(subTypeField.name(), fetchSubTypes(subTypeField, detailSubTypes));

        results.append(detail);
    }

    return results;
}

QVariant
QTrackerAbstractContactFetchRequest::fetchInstances(const QTrackerContactDetailField &field,
                                                    const QString &rawValueString, QSet<QString> &graphIris) const
{
    if (rawValueString.isEmpty()) {
        return QVariant();
    }

    // apply string list when requested
    if (QVariant::StringList == field.dataType()) {
        const QStringList rawValueList = fieldStringListWithStrippedGraph(field, rawValueString, graphIris);
        const QList<int> fetchedInstances = toIntList(rawValueList);

        QStringList instances;

        foreach(const InstanceInfoBase &ii, field.allowableInstances()) {
            if (fetchedInstances.contains(trackerId(ii))) {
                instances.append(ii.text());
            }
        }

        // append default value if instances could not be identified
        if (instances.isEmpty() && field.hasDefaultValue()) {
            instances.append(field.defaultValue().toString());
        }

        return instances;
    }

    // otherwise apply single value
    bool hasInstanceId = false;
    const int instanceId = fieldStringWithStrippedGraph(field, rawValueString, graphIris).toInt(&hasInstanceId);

    if (hasInstanceId) {
        foreach(const InstanceInfoBase &instance, field.allowableInstances()) {
            if (trackerId(instance) == instanceId) {
                return instance.value();
            }
        }
    }

    qctWarn(QString::fromLatin1("Unknown instance id for field %1").
            arg(field.name()));

    // apply default value if instances could not be identified
    if (field.hasDefaultValue()) {
        return field.defaultValue();
    } else {
        return QVariant();
    }
}

/// Returns @c true if @p variant is an empty string or an empty stringlist.
static bool
isEmptyStringOrStringList(const QVariant &variant)
{
    return((QVariant::String == variant.type() && variant.toString().isEmpty()) ||
           (QVariant::StringList == variant.type() && variant.toStringList().isEmpty()));
}

QVariant
QTrackerAbstractContactFetchRequest::fetchField(const QTrackerContactDetailField &field,
                                                const QString &rawValueString,
                                                QSet<QString> &graphIris) const
{
    if (field.hasSubTypeClasses()) {
        return fetchSubTypesClasses(field, rawValueString);
    }

    if (rawValueString.isEmpty()) {
        return QVariant();
    }

    if (field.hasSubTypeProperties()) {
        return QVariant(fieldStringWithStrippedGraph(field, rawValueString, graphIris));
    }

    if (field.restrictsValues()) {
        if (not field.allowableInstances().isEmpty()) {
            return fetchInstances(field, rawValueString, graphIris);
        }

        if (not field.allowableValues().isEmpty()) {
            if (QVariant::StringList == field.dataType()) {
                return fieldStringListWithStrippedGraph(field, rawValueString, graphIris);
            } else {
                return fieldStringWithStrippedGraph(field, rawValueString, graphIris);
            }
        }
    }

    const QString valueString = fieldStringWithStrippedGraph(field, rawValueString, graphIris);

    QVariant parsedValue;
    if (not field.parseValue(valueString, parsedValue)) {
        qctWarn(QString::fromLatin1("Cannot convert value to %1 for field %2: %3").
                arg(QLatin1String(QVariant::typeToName(field.dataType())),
                    field.name(), valueString));

        return QVariant();
    }

    if (isEmptyStringOrStringList(parsedValue)) {
        return QVariant();
    }

    return parsedValue;
}

void
QTrackerAbstractContactFetchRequest::fetchCustomValues(const QTrackerContactDetailField &field,
                                                       QVariant &fieldValue,
                                                       const QString &rawValueString,
                                                       QSet<QString> &graphIris) const
{
    switch (field.dataType()) {
    case QVariant::StringList: {
        QStringList values =
                fieldValue.toStringList() +
                fieldStringListWithStrippedGraph(field, rawValueString, graphIris, TrimList);

        if (not values.isEmpty()) {
            fieldValue = values;
        } else {
            fieldValue = QVariant();
        }

        break;
    }

    case QVariant::String: {
        if (not rawValueString.isEmpty()) {
            fieldValue = fieldStringWithStrippedGraph(field, rawValueString, graphIris);
        }

        break;
    }

    default:
        qctWarn(QString::fromLatin1("Cannot fetch custom values for field %2: "
                                    "Data type %3 is not supported yet.").
                arg(field.name(), QLatin1String(QVariant::typeToName(field.dataType()))));
        break;
    }
}

bool
QTrackerAbstractContactFetchRequest::saveDetail(ContactCache::iterator contact,
                                                QContactDetail &detail,
                                                const QTrackerContactDetail &definition)
{
    if (not detail.detailUri().isEmpty()) {
        QString detailUri;

        // Detail URI scheme is different from resource IRI scheme
        // for details like QContactPresence or online avatars.
        if (definition.detailUriScheme() != definition.resourceIriScheme()) {
            bool valid = false;
            QVariant value;

            value = QTrackerContactSubject::parseIri(definition.resourceIriScheme(),
                                                     detail.detailUri(), &valid);

            if (valid) {
                detailUri = QTrackerContactSubject::makeIri(definition.detailUriScheme(),
                                                            QVariantList() << value);
            }
        }

        if (not detailUri.isEmpty()) {
            detailUri = Utils::unescapeIri(detailUri);
            detail.setLinkedDetailUris(detail.detailUri());
            detail.setDetailUri(detailUri);
        }
    }

    return contact->saveDetail(&detail);
}

static void
updateOriginAndAccessConstraints(QContactDetail &detail,
                                 const QSet<QString> &detailGraphIris)
{
    QStringList telepathyGraphIris;
    bool isReadOnlyDetail = false;

    foreach(const QString &graphIri, detailGraphIris) {
        // could happen that one detail has fields from different services if merged,
        // e.g. firstname and lastname of a contact
        if (graphIri.startsWith(QLatin1String("telepathy:"))) {
            telepathyGraphIris += graphIri;
        }

        // if any field value is not owned by qct, set whole detail readonly (can't do that on field level)
        isReadOnlyDetail |= isForeignGraph(graphIri);
    }

    // Extend linked detail uris by any graph which looks like to be from the tp plugin
    if (not telepathyGraphIris.isEmpty()) {
        QSet<QString> linkedDetailUris = detail.linkedDetailUris().toSet();

        foreach(const QString &telepathyGraphIri, telepathyGraphIris) {
            // Do not add if it is the detailUri of the detail itself (QContactOnlineAccount)
            if (telepathyGraphIri != detail.detailUri()) {
                linkedDetailUris += telepathyGraphIri;
            }
        }

        detail.setLinkedDetailUris(linkedDetailUris.toList());
    }

    if (isReadOnlyDetail) {
        QContactManagerEngine::setDetailAccessConstraints(&detail, detail.accessConstraints()|QContactDetail::ReadOnly);
    }
}

void
QTrackerAbstractContactFetchRequest::fetchUniqueDetail(QList<QContactDetail> &details,
                                                       const QueryContext &queryContext,
                                                       const DetailContext &context,
                                                       const QString &affiliation)
{
    QContactDetail detail(context.definition().name());

    int lastColumn = context.firstColumn();

    // FIXME: we don't support subtypes by class for unique details

    bool empty = true;
    QSet<QString> detailGraphIris;

    foreach(const QTrackerContactDetailField &field, context.definition().fields()) {
        if (not field.hasPropertyChain()) {
            continue;
        }

        const QString rawValueString = queryContext.result->stringValue(lastColumn++);

        if (rawValueString.isEmpty()) {
            continue;
        }

        const QVariant fieldValue = fetchField(field, rawValueString, detailGraphIris);

        if (fieldValue.isNull()) {
            continue;
        }

        if (not field.hasSubTypes()) {
            detail.setValue(field.name(), fieldValue);
            empty = false;
        } else {
            if (field.hasSubTypeProperties()) {
                // collect subtypes by those property subtype columns which have some string set
                QSet<QString> subTypes;

                foreach(const PropertyInfoBase &pi, field.subTypeProperties()) {
                    if (lastColumn == queryContext.query.projections().count()) {
                        qctWarn(QString::fromLatin1("Trying to fetch more detail fields than we have "
                                                    "columns for field %1 subtypes").
                                arg(field.name()));
                        return;
                    }

                    if (not queryContext.result->stringValue(lastColumn++).isEmpty()) {
                        subTypes.insert(pi.value().toString());
                    }
                }

                detail.setValue(field.name(), fetchSubTypes(field, subTypes));
            }
        }
    }

    // if detail is empty, return as it is ignored - no need to update uri
    if (empty) {
        return;
    }

    // update uri
    if (context.definition().hasDetailUri()) {
        // If the detailUri was on nco:hasAffiliation, we don't have anything to
        // do since we already know the IRI of the affiliation. If it was on
        // another property, it is always stored in the first column
        const QTrackerContactDetailField *detailUriField = context.definition().detailUriField();

        foreach (const PropertyInfoBase &pi, detailUriField->propertyChain()) {
            if (pi.hasDetailUri()) {
                if (pi.iri() == nco::hasAffiliation::iri()) {
                    detail.setDetailUri(Utils::unescapeIri(affiliation));
                } else {
                    detail.setDetailUri(Utils::unescapeIri(queryContext.result->stringValue(lastColumn++)));
                }
                break;
            }
        }
    }

    updateOriginAndAccessConstraints(detail, detailGraphIris);

    details.append(detail);
}

static void
fetchMultiDetailUri(QContactDetail &detail,
                    const QTrackerContactDetail& definition,
                    QStringList &fieldsData)
{
    if (definition.hasDetailUri()) {
        // If the detailUri was on nco:hasAffiliation, we don't have anything to
        // do since we already know the IRI of the affiliation. If it was on
        // another property, it is always stored in the first column
        const QTrackerContactDetailField *detailUriField = definition.detailUriField();

        PropertyInfoList::ConstIterator pi = detailUriField->propertyChain().constBegin();

        for(; pi != detailUriField->propertyChain().constEnd(); ++pi) {
            if (pi->hasDetailUri()) {
                detail.setDetailUri(Utils::unescapeIri(fieldsData.takeFirst()));
                break;
            }
        }
    }
}

/// creates a copy of @p _fields with all fields with an inverted property in the chain at the end
static QList<QTrackerContactDetailField>
moveWithInversePropertyAtEnd(const QList<QTrackerContactDetailField>& fields)
{
    QList<QTrackerContactDetailField> sortedFields;
    QList<QTrackerContactDetailField> inverseFields;

    foreach(const QTrackerContactDetailField &field, fields) {
        if (not field.hasPropertyChain()) {
            continue;
        }

        if (field.propertyChain().hasInverseProperty()) {
            inverseFields.append(field);
        } else {
            sortedFields.append(field);
        }
    }

    sortedFields += inverseFields;
    return sortedFields;
}

void
QTrackerAbstractContactFetchRequest::fetchMultiDetail(QContactDetail &detail,
                                                      const DetailContext &context,
                                                      const QString &rawValueString)
{
    QStringList fieldsData = rawValueString.split(QTrackerScalarContactQueryBuilder::fieldSeparator());

    fetchMultiDetailUri(detail, context.definition(), fieldsData);

    // Fields that have inverse properties are always stored last, so respect
    // this order when fetching them
    const QList<QTrackerContactDetailField> fields =
        moveWithInversePropertyAtEnd(context.definition().fields());

    QSet<QString> detailGraphIris;

    foreach(const QTrackerContactDetailField &field, fields) {
        // Property subtypes are done in unifyPropertySubTypes
        if (field.hasSubTypeProperties()) {
            continue;
        }

        if (fieldsData.empty()) {
            qctWarn(QString::fromLatin1("Trying to fetch more detail fields than we have "
                                        "columns for detail %1").arg(context.definition().name()));
            return;
        }

        QVariant fieldValue;

        if (not field.isWithoutMapping()) {
            fieldValue = fetchField(field, fieldsData.takeFirst(), detailGraphIris);
        }

        if (field.permitsCustomValues()) {
            if (fieldsData.empty()) {
                qctWarn(QString::fromLatin1("Missing custom values for field %1 of detail %2").
                                            arg(field.name(), context.definition().name()));
            }

            fetchCustomValues(field, fieldValue, fieldsData.takeFirst(), detailGraphIris);
        }


        if (not fieldValue.isNull()) {
            detail.setValue(field.name(), fieldValue);
        }
    }

    updateOriginAndAccessConstraints(detail, detailGraphIris);
}

/* Enhances \sa QContactDetail::isEmpty by ignoring context and detail uri - stored also as values */
static bool
areContactDetailDataValuesEmpty(const QContactDetail &detail)
{
    const QVariantMap variantValues = detail.variantValues();
    if (variantValues.empty()) {
        return true;
    }
    if (variantValues.size() <= 2) {
        QStringList keys(variantValues.keys());
        keys.removeOne(QContactDetail::FieldDetailUri);
        keys.removeOne(QContactDetail::FieldLinkedDetailUris);
        return (keys.size() == 0);
    }
    return false;
}

void
QTrackerAbstractContactFetchRequest::fetchMultiDetails(QList<QContactDetail> &details,
                                                       const QueryContext &queryContext,
                                                       const DetailContext &detailContext)
{
    int lastColumn = detailContext.firstColumn();

    const QTrackerContactDetailField *subTypeField = detailContext.definition().subTypeField();

    // First demarshall the raw data
    const QString rawValueString = queryContext.result->stringValue(lastColumn++);

    if (rawValueString.isEmpty()) {
        return;
    }

    QHash<QString, QString> subTypesDetailsData;
    subTypesDetailsData.insert(QString(), rawValueString);

    // Subtype properties are spread over various details (one for
    // each subtype)
    // If we have subtype properties, fetch as many details as we
    // have subtypes
    if (subTypeField && subTypeField->hasSubTypeProperties()) {
        // Only used for debug message
        const QString &definitionName = detailContext.definition().name();

        foreach(const PropertyInfoBase &pi, subTypeField->subTypeProperties()) {
            if (lastColumn > detailContext.lastColumn()) {
                qctWarn(QString::fromLatin1("Trying to fetch more subtype details "
                                            "than we have columns for detail %1").
                        arg(definitionName));
                return;
            }

            const QString rawSubTypeData = queryContext.result->stringValue(lastColumn++);

            if (rawSubTypeData.isEmpty()) {
                continue;
            }

            subTypesDetailsData.insert(pi.value().toString(), rawSubTypeData);
        }
    }

    // Now parse each fetched detail into a proper QContactDetail,
    // keeping the info about its subtype
    QMultiHash<QString, QContactDetail> subTypeDetails;
    for (QHash<QString,QString>::ConstIterator it = subTypesDetailsData.constBegin();
         it != subTypesDetailsData.constEnd(); ++it ) {
        const QString &subType = it.key();
        const QString &rawDetailsData = it.value();
        const QStringList detailsData = rawDetailsData.split(QTrackerScalarContactQueryBuilder::detailSeparator());

        foreach(const QString &detailData, detailsData) {
            if (detailData.isEmpty()) {
                continue;
            }

            QContactDetail detail(detailContext.definition().name());

            fetchMultiDetail(detail, detailContext, detailData);
            subTypeDetails.insert(subType,detail);
        }
    }

    // And finally unify the fetched details

    // There is at least one key, the one of the default type
    if (subTypeField && subTypeField->hasSubTypeProperties()) {
        details.append(unifyPropertySubTypes(subTypeDetails, *subTypeField));
    } else {
        details.append(subTypeDetails.values());
    }
}

void
QTrackerAbstractContactFetchRequest::fetchResults(ContactCache &results, QueryContext &queryContext)
{
    QSparqlResult *const result = queryContext.result;

    const int limit = m_fetchHint.maxCountHint();

    for(bool hasRow = result->first(); not isCanceled() && hasRow; hasRow = result->next()) {
        // identify the contact
        const QString contactIri = result->stringValue(0);
        const QContactLocalId localId = result->value(1).toUInt();
        const QString affiliation = result->stringValue(2);
        const QString affiliationContext = qctCamelCase(result->stringValue(3));

        // create contact if not already existing
        ContactCache::Iterator contact(results.find(localId));

        if(contact == results.end()) {
            // For the case where we have a limit set, but no sorting (if we
            // have sorting, the limit was applied to the preliminary ID fetch
            // request). Breaking the for here is not "fair" in the sense that
            // the first schema that gets filled will get more contacts in. On
            // the other hand, what do you expect with a limit and no sorting?
            if (limit >= 0 && (results.size()) >= limit) {
                break;
            }

            QContact c;
            QContactId id;
            id.setLocalId(localId);
            id.setManagerUri(engine()->managerUri());
            c.setId(id);
            c.setType(queryContext.contactType());

            contact = results.insert(localId, c);
            queryContext.contactIds.append(localId);
        }

        // read details
        for(QList<DetailContext>::ConstIterator detailContext = queryContext.details.constBegin();
            detailContext != queryContext.details.constEnd();
            ++detailContext) {
            QList<QContactDetail> details;

            if (detailContext->definition().isUnique()) {
                fetchUniqueDetail(details, queryContext, *detailContext, affiliation);
            } else {
                fetchMultiDetails(details, queryContext, *detailContext);
            }

            foreach(QContactDetail detail, details) {
                // Ignore the detail if we fetched no fields for it (or just detailUri)
                if (areContactDetailDataValuesEmpty(detail)) {
                    continue;
                }

                // No context is added for empty context string.
                if (detailContext->definition().hasContext() && not affiliationContext.isEmpty()) {
                    detail.setContexts(affiliationContext);
                }

                if (not saveDetail(contact, detail, detailContext->definition())) {
                    qctWarn(QString::fromLatin1("Could not save detail %1 on contact %2").
                            arg (detail.definitionName(), contactIri));
                }
            }
        }

        fetchCustomDetails(queryContext, contact);

        fetchHasMemberRelationships(queryContext, contact);
    }
}

QContactDetail
QTrackerAbstractContactFetchRequest::fetchCustomDetail(const QString &rawValue, const QString &contactType)
{
    QStringList tokens = rawValue.split(QTrackerScalarContactQueryBuilder::fieldSeparator());

    // Minimum number of tokens is detail name + 1 field name/value tuple
    if (tokens.size() < 3) {
        return QContactDetail();
    }

    QMultiHash<QString, QString> detailValues;
    QString detailName = tokens.takeFirst();

    QContactDetail detail(detailName);

    const QContactDetailDefinitionMap detailDefs = engine()->detailDefinitions(contactType, 0);


    while(tokens.size() >= 2) {
        QString fieldName = tokens.takeFirst();
        QStringList fieldValues = tokens.takeFirst().split(QTrackerScalarContactQueryBuilder::listSeparator());
        foreach(const QString &f, fieldValues) {
            detailValues.insert(fieldName, f);
        }
    }

    foreach(const QString &fieldName, detailValues.uniqueKeys()) {
        QMap<uint, QString> orderedValues;

        // Values are retrieved as "tracker-id:value" pairs.
        // Order values by tracker-id and extract the value.
        foreach(const QString &s, detailValues.values(fieldName)) {
            const int i = s.indexOf(QLatin1Char(':'));
            orderedValues.insert(s.left(i).toUInt(), s.mid(i + 1));
        }

        // QVariant cannot deal with QList<QString> :-/
        const QStringList fieldValues = orderedValues.values();
        QVariant fieldValue;

        if (fieldValues.size() == 1) {
            fieldValue = fieldValues.first();
        } else {
            fieldValue = fieldValues;
        }

        detail.setValue(fieldName, fieldValue);

        const QContactDetailFieldDefinitionMap fieldDefs = detailDefs[detail.definitionName()].fields();
        const QContactDetailFieldDefinition fieldDef = fieldDefs.value(fieldName);

        if (QVariant::Invalid != fieldDef.dataType() && fieldValue.convert(fieldDef.dataType())) {
            detail.setValue(fieldName, fieldValue);
        }
    }

    return detail;
}

void
QTrackerAbstractContactFetchRequest::fetchCustomDetails(const QueryContext &queryContext,
                                                        ContactCache::Iterator contact)
{
    if (queryContext.customDetailColumn < 0 ||
        queryContext.customDetailColumn >= queryContext.query.projections().count()) {
        return;
    }

    const QString rawValue = queryContext.result->stringValue(queryContext.customDetailColumn);

    foreach(const QString &rawDetailValue, rawValue.split(QTrackerScalarContactQueryBuilder::detailSeparator())) {
        QContactDetail detail = fetchCustomDetail(rawDetailValue, contact->type());

        if (not areContactDetailDataValuesEmpty(detail)) {
            contact->saveDetail(&detail);
        }
    }
}

void
QTrackerAbstractContactFetchRequest::fetchHasMemberRelationships(const QueryContext &queryContext,
                                                                 ContactCache::Iterator contact)
{
    if (queryContext.hasMemberRelationshipColumn < 0 ||
        queryContext.hasMemberRelationshipColumn >= queryContext.query.projections().count()) {
        return;
    }

    QList<QContactRelationship> hasMemberRelationships;

    QContactRelationship relationship;
    relationship.setRelationshipType(QContactRelationship::HasMember);

    QContactId contactId;
    contactId.setManagerUri(engine()->managerUri());
    contactId.setLocalId(contact->localId());

    QContactId otherContactId;
    otherContactId.setManagerUri(engine()->managerUri());

    // HasMember with contact in Second role, valid for both group and normal contacts
    int currentColumn = queryContext.hasMemberRelationshipColumn;
    const QString rawValue = queryContext.result->stringValue(currentColumn++);

    if (not rawValue.isEmpty()) {
        relationship.setSecond(contactId);

        const QStringList localIdList = rawValue.split(QTrackerScalarContactQueryBuilder::listSeparator());
        foreach(const QString &localIdString, localIdList) {
            bool isValidId = false;
            const QContactLocalId localId = localIdString.toUInt(&isValidId);

            if (isValidId) {
                otherContactId.setLocalId(localId);
                relationship.setFirst(otherContactId);
                hasMemberRelationships << relationship;
            }
        }
    }

    if (queryContext.contactType() == QContactType::TypeGroup) {
        // HasMember with contact in First role, only applyable for group contacts
        const QString rawValue = queryContext.result->stringValue(currentColumn++);

        if (not rawValue.isEmpty()) {
            relationship.setFirst(contactId);

            const QStringList localIdList = rawValue.split(QTrackerScalarContactQueryBuilder::listSeparator());

            foreach(const QString &localIdString, localIdList) {
                bool isValidId = false;
                const QContactLocalId localId = localIdString.toUInt(&isValidId);

                if (isValidId) {
                    otherContactId.setLocalId(localId);
                    relationship.setSecond(otherContactId);
                    hasMemberRelationships << relationship;
                }
            }
        }
    }

    QContactManagerEngine::setContactRelationships(&contact.value(), hasMemberRelationships);
}

static void
updateDetailLinks(QContact &contact)
{
    typedef QHash<QString, QContactDetail> DetailHash;

    DetailHash detailHash;

    foreach(const QContactDetail &detail, contact.details()) {
        if (not detail.detailUri().isEmpty()) {
            detailHash.insert(detail.detailUri(), detail);
        }
    }

    for(DetailHash::ConstIterator detail = detailHash.constBegin(); detail != detailHash.constEnd(); ++detail) {
        foreach(const QString &detailUri, detail->linkedDetailUris()) {
            DetailHash::Iterator target = detailHash.find(detailUri);

            if (target == detailHash.constEnd()) {
                continue;
            }

            QStringList linkedDetailUris = target->linkedDetailUris();

            if (linkedDetailUris.contains(detail->detailUri())) {
                continue;
            }

            linkedDetailUris.append(detail->detailUri());
            target->setLinkedDetailUris(linkedDetailUris);
            contact.saveDetail(&target.value());
        }
    }
}

static void
removeDuplicateDetails(QContact &contact)
{
    QSet<QContactDetail> knownDetails;

    foreach(QContactDetail detail, contact.details()) {
        if(knownDetails.contains(detail)) {
            contact.removeDetail(&detail);
        } else {
            knownDetails.insert(detail);
        }
    }
}

void
QTrackerAbstractContactFetchRequest::run()
{
    if (isCanceled()) {
        return;
    }

    const QStringList &detailHint = m_fetchHint.detailDefinitionsHint();
    const bool calculateGlobalPresence(detailHint.contains(QContactGlobalPresence::DefinitionName) || detailHint.isEmpty());
    const bool calculateDisplayLabel(detailHint.contains(QContactDisplayLabel::DefinitionName) || detailHint.isEmpty());
    const bool calculateAvatar(detailHint.contains(QContactAvatar::DefinitionName) || detailHint.isEmpty());

    // Results are already sorted if we run a preliminary ID fetch
    bool isSortedAlready = false;

    // If we just have a limit and no sorting, we just stop the contact fetching
    // when we reach the specified number of contacts, no need to do an ID fetch
    // first
    if (m_fetchHint.maxCountHint() >= 0 && not m_sorting.isEmpty()) {
        QList<QContactLocalId> sortedIds;

        const QContactManager::Error error = runPreliminaryIdFetchRequest(sortedIds);

        if (error == QContactManager::NoError) {
            QContactLocalIdFilter filter;
            filter.setIds(sortedIds);

            // We can reset the filtering and sorting since the ID fetch request took care of it
            m_filter = filter;
            m_sorting.clear();

            isSortedAlready = true;

            // We can write now the IDs in m_sortedIds. The key (schema) is not important,
            // since the whole result set will be sorted (vs. contacts and groups being
            // sorted each on their side)
            m_sortedIds.insert(QString(), sortedIds);
        }
        // else, it means we could not retrieve the local IDs sorted (maybe sorting was done
        // on a synthetic detail, or another not supported way). In that case, we revert to
        // good old fetch request (which would have been called by the ID fetch request
        // anyway in emulated mode)
    }

    ContactCache results;

    foreach(const QTrackerContactDetailSchema &schema, engine()->schemas())  {
        // build RDF query
        QueryContext context(schema);

        const QContactManager::Error error = buildQuery(context);

        if (QContactManager::NoError != error) {
            setLastError(error);
            return;
        }

        // run the query
        const QSparqlQuery query(context.query.sparql(engine()->selectQueryOptions()));
        QScopedPointer<QSparqlResult> result(runQuery(query, SyncQueryOptions));

        if (result.isNull()) {
            return; // runQuery() called reportError()
        }

        context.result = result.data();
        fetchResults(results, context);

        // Update synthetic details and detail links
        // That needs to be done before sorting
        foreach (QContactLocalId id, context.contactIds) {
            QContact &c = results[id];

            removeDuplicateDetails(c);

            if (calculateGlobalPresence) {
                qctUpdateGlobalPresence(c);
            }
            if (calculateDisplayLabel) {
                engine()->updateDisplayLabel(c, m_nameOrder);
            }
            if (calculateAvatar) {
                engine()->updateAvatar(c);
            }

            updateDetailLinks(c);
        }

        if (not isSortedAlready) {
            if (context.sorted || m_sorting.isEmpty()) {
                m_sortedIds[context.contactType()] = context.contactIds;
            } else {
                qctWarn(QString::fromLatin1("Could not sort results for contact type %1, reverting "
                                            "to in memory sorting.").arg(schema.contactType()));
                const QList<QContact> contacts = getContacts(results, context.contactIds);
                m_sortedIds[context.contactType()] = QContactManagerEngine::sortContacts(contacts, m_sorting);
            }
        }
    }

    processResults(results);
}

QList<QContact>
QTrackerAbstractContactFetchRequest::getContacts(const ContactCache &cache,
                                                 const QList<QContactLocalId> &ids)
{
    QList<QContact> contacts;
    contacts.reserve(ids.size());

    foreach (QContactLocalId id, ids) {
        contacts.append(cache.value(id));
    }

    return contacts;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "moc_abstractcontactfetchrequest.cpp"
