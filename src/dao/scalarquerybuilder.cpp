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

#include "scalarquerybuilder.h"

#include "contactdetailschema.h"
#include "resourceinfo.h"
#include "support.h"

#include <lib/logger.h>
#include <lib/settings.h>
#include <lib/phoneutils.h>
#include <lib/threadlocaldata.h>

#include <ontologies/maemo.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////////////////////////

static const QContactFilter::MatchFlags MatchFunctionFlags
        (QContactFilter::MatchExactly | QContactFilter::MatchContains |
         QContactFilter::MatchStartsWith | QContactFilter::MatchEndsWith);
static const QContactFilter::MatchFlags StringCompareFlags
        (QContactFilter::MatchFixedString | MatchFunctionFlags);

////////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerScalarContactQueryBuilder::QTrackerScalarContactQueryBuilder(const QTrackerContactDetailSchema &schema,
                                                                     const QString &managerUri)
    : m_schema(schema)
    , m_managerUri(managerUri)
{
}

QTrackerScalarContactQueryBuilder::~QTrackerScalarContactQueryBuilder()
{
}

const Variable &
QTrackerScalarContactQueryBuilder::contact()
{
    static const Variable s(QLatin1String("contact"));
    return s;
}

const Variable &
QTrackerScalarContactQueryBuilder::context()
{
    static const Variable c(QLatin1String("context"));
    return c;
}

const QChar
QTrackerScalarContactQueryBuilder::graphSeparator()
{
    static const QChar separator(0x1C);
    return separator;
}

const QChar
QTrackerScalarContactQueryBuilder::detailSeparator()
{
    static const QChar separator(0x1E);
    return separator;
}

const QChar
QTrackerScalarContactQueryBuilder::fieldSeparator()
{
    static const QChar separator(0x1F);
    return separator;
}

const QChar
QTrackerScalarContactQueryBuilder::listSeparator()
{
    static const QChar separator(0x1D);
    return separator;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Returns the first part of the @p propertyChain including the property giving the detail uri.
/// If there is no property giving the detail uri, the result is empty.
static PropertyInfoList
chainToDetailUri(const PropertyInfoList &propertyChain)
{
    PropertyInfoList result;
    PropertyInfoList::ConstIterator it = propertyChain.constBegin();
    PropertyInfoList::ConstIterator end = propertyChain.constEnd();
    for(; it != end; ++it) {
        result.append(*it);

        if (it->hasDetailUri()) {
            break;
        }
    }
    if (it == end) {
        result.clear();
    }
    return result;
}

/// Returns a ValueChain from the @p propertyInfoList
static ValueChain
valueChain(const PropertyInfoList &propertyInfoList)
{
    ValueChain result;
    foreach(const PropertyInfoBase &propertyInfo, propertyInfoList) {
        result.append(propertyInfo.resource());
    }
    return result;
}

/// Returns a chain of predicate functions from the property @p chain on @p subject.
static Function
predicateFunctionChain(const PropertyInfoList& chain, const Value &subject, bool withGraph = false)
{
    if (chain.empty()) {
        return Function();
    }

    PropertyInfoList::ConstIterator it = chain.constBegin();

    PredicateFunction current = it->predicateFunction().apply(subject);

    for(it++; it != chain.constEnd(); it++) {
        current = it->predicateFunction().apply(current);
    }

    if (not chain.last().singleValued()) {
        current.setValueSeparator(QTrackerScalarContactQueryBuilder::listSeparator());
    }
    if (withGraph) {
        current.setGraphSeparator(QTrackerScalarContactQueryBuilder::graphSeparator());
    }
    return current;
}

static PatternGroup
createPatternForPredicateChain(const Variable &subject,
                               const PropertyInfoList &properties,
                               const Variable &object)
{
    Variable last = subject;
    PatternGroup patterns;

    for(PropertyInfoList::ConstIterator it = properties.constBegin();
        it != properties.constEnd();
        ++it) {
        Variable o;

        if (it == properties.constEnd() - 1) {
            o = object;
        }

        if (it->isInverse()) {
            patterns.addPattern(o, it->resource(), last);
        } else {
            patterns.addPattern(last, it->resource(), o);
        }

        last = o;
    }

    return patterns;
}

static PatternGroup
createPatternForCustomField(const Variable &subject,
                            const QString &fieldName,
                            const Variable &object)
{
    PatternGroup patterns;

    Variable field;

    patterns.addPattern(subject, nao::hasProperty::resource(), field);

    if (not fieldName.isEmpty()) {
        patterns.addPattern(field, nao::propertyName::resource(), LiteralValue(fieldName));
    }

    patterns.addPattern(field, nao::propertyValue::resource(), object);

    return patterns;
}

static PatternGroup
createPatternForCustomDetail(const Variable &subject,
                             const QString &detailName,
                             const QString &fieldName,
                             const Variable &object)
{
    PatternGroup patterns;

    Variable detail;

    patterns.addPattern(subject, nao::hasProperty::resource(), detail);
    patterns.addPattern(detail, nao::propertyName::resource(), LiteralValue(detailName));
    patterns.addPattern(createPatternForCustomField(detail, fieldName, object));

    return patterns;
}

/// Returns a PrefixFunction which appends the iri of the graph of @p predicate to the @p value,
/// using the default graph separator.
/// Also works if @p predicate is in the default graph.
static PrefixFunction
applyConcatWithGraph(const Value &value, const PatternBase &predicate)
{
    // to get the graph for predicates in a named graph as well as in the default graph
    // a subselect like this is needed, with ?foo ?bar ?baz being the final predicate to the value:
    // "tracker:coalesce((SELECT ?g WHERE { GRAPH ?g { ?foo ?bar ?baz }), "")
    const Variable graph;
    Select graphSelect;
    graphSelect.addProjection(graph);
    Graph graphPattern(graph);
    graphPattern.addPattern(predicate);
    graphSelect.addRestriction(graphPattern);

    const PrefixFunction graphCoalesce = Functions::coalesce.apply(Filter(graphSelect), LiteralValue(QString()));

    ValueChain valueChain;
    valueChain
        << value
        << LiteralValue(QTrackerScalarContactQueryBuilder::graphSeparator())
        << graphCoalesce;
    return Functions::concat.apply(valueChain);
}

/// Inserts a copy of the value @p inter between all values of @p list.
static void
intercalate(ValueChain &list, const Value &inter)
{
    for (int i = list.size() - 1; i > 0; --i) {
        list.insert(i, inter);
    }
}

class QctSplitPropertyChains
{
public:
    QctSplitPropertyChains(const PropertyInfoList &chain);

    bool isEmpty() const { return fullChain().isEmpty(); }
    const PropertyInfoList & fullChain() const { return m_fullChain; }
    const PropertyInfoList & projectionChain() const { return m_projectionChain; }
    const PropertyInfoList & restrictionChain() const { return m_restrictionChain; }

private:
    PropertyInfoList m_fullChain;
    PropertyInfoList m_projectionChain;
    PropertyInfoList m_restrictionChain;
};

/// Walks the @p chain backwards to find the part that can be bound with with predicate functions,
/// then returns that as @p projectionChain, the rest as @p restrictionPredicates.
/// If there is a predicate which defines ownership and it is not the last one in the chain, this and all those before
/// will be the added to the @p restrictionPredicates, even if they could be expressed by predicate functions.
/// This is done to be able to bind the subject and the object of that predicate to variables in the restriction part,
/// so the graph for that triple can be queried.
/// The last property in the chain is always set to be used as predicate function.
QctSplitPropertyChains::QctSplitPropertyChains(const PropertyInfoList &chain)
    : m_fullChain(chain)
{
    PropertyInfoList::ConstIterator it = chain.constEnd();

    // always add the last property
    if (it != chain.constBegin()) {
        m_projectionChain += *(--it);
    }

    while(it != chain.constBegin()) {
        it--;

        if (not it->singleValued() || it->definesOwnership()) {
            it++;
            break;
        }

        m_projectionChain += *it;
    }

    while(it != chain.constBegin()) {
        m_restrictionChain.prepend(*(--it));
    }
}

/// Adds a projection to @p query for the detail uri of @p field, using the @p chain on the @p subject.
/// If there is no such property, no projection will be added.
static void
bindDetailUri(const QTrackerContactDetailField &field,
              const Variable &subject,
              const PropertyInfoList &chain,
              Select &query)
{
    const PropertyInfoList detailUriChain = chainToDetailUri(chain);

    if (not detailUriChain.isEmpty()) {
        if (detailUriChain.isSingleValued()) {
            query.addProjection(predicateFunctionChain(detailUriChain, subject, field.hasOwner()));
        } else {
            qctWarn(QString::fromLatin1("DetailUri for field %1 on non single"
                                        "valued property is not supported").
                    arg(field.name()));
            query.addProjection(LiteralValue(QString()));
        }
    }
}

static Select
bindUniqueFieldWithSubquery(const QTrackerContactDetailField& field,
                            const Variable &subject,
                            const PropertyInfoList &chain,
                            const SparqlTransform *transform)
{
    Select subquery;
    const Variable instance;
    const PatternGroup propertyPattern = createPatternForPredicateChain(subject, chain, instance);

    subquery.addRestriction(propertyPattern);

    Function valueFunction = transform->apply(instance);

    if (field.hasOwner()) {
        valueFunction = applyConcatWithGraph(valueFunction, propertyPattern);
    }
    if (not chain.first().singleValued()) {
        valueFunction = Functions::groupConcat.apply(valueFunction, LiteralValue(QTrackerScalarContactQueryBuilder::listSeparator()));
    }

    subquery.addProjection(valueFunction);

    return subquery;
}

static QContactManager::Error
bindInstanceValues(const QTrackerContactDetailField &field,
                   const Variable &subject,
                   const PropertyInfoList &chain,
                   Select &query)
{
    if (field.hasOwner()) {
        Select select;
        if (chain.count() != 1) {
            qctWarn(QString::fromLatin1("Values-by-instances with a property chain "
                                        "longer than 1 are not supported for field %1").
                    arg(field.name()));
            return QContactManager::NotSupportedError;
        }
        query.addProjection(bindUniqueFieldWithSubquery(field, subject, chain, IdTransform::instance()));
    } else {
        Function function = predicateFunctionChain(chain, subject);

        function = Functions::trackerId.apply(function);
        if (not chain.last().singleValued()) {
            function = Functions::groupConcat.apply(function, LiteralValue(QTrackerScalarContactQueryBuilder::listSeparator()));
        }
        query.addProjection(function);
    }

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindUniqueDetailField(const QTrackerContactDetailField &field,
                                                         Select &query)
{
    Variable subject = contact();

    PropertyInfoList chain = field.propertyChain();

    // if bound to an affiliation, switch subject to the query-wide context value
    if (chain.first().iri() == nco::hasAffiliation::iri()) {
        chain.removeFirst();
        subject = context();
    }

    // will also not add a projection if the detailUri is bound by the nco:hasAffiliation
    // which was removed from the chain just before. That affiliation is already fetched in the base query.
    if (field.hasDetailUri()) {
        bindDetailUri(field, subject, chain, query);
    }

    if (field.hasSubTypeClasses()) {
        qctWarn(QString::fromLatin1("Subtypes by class not supported for field %1").
                arg(field.name()));
        return QContactManager::NotSupportedError;
    }

    if (field.hasSubTypeProperties()) {
        qctWarn(QString::fromLatin1("Subtypes by property not supported for field %1").
                arg(field.name()));
        return QContactManager::NotSupportedError;
    }

    // FIXME: we don't handle inverse properties nor detailUri here
    if (field.isInverse()) {
        qctWarn(QString::fromLatin1("Inverse properties not supported for field %1").
                arg(field.name()));
        return QContactManager::NotSupportedError;
    }

    const bool isRestrictedToInstances = (not field.allowableInstances().isEmpty());
    const bool withGraph = (field.hasOwner() && not isRestrictedToInstances);
    const bool definesOwnership = field.definesOwnership();

    if (chain.isSingleValued(false) && not field.isWithoutMapping() && not definesOwnership) {
        const SparqlTransform *transform = field.sparqlTransform();
        if (isRestrictedToInstances) {
            const QContactManager::Error error = bindInstanceValues(field, subject, chain, query);

            if (error != QContactManager::NoError) {
                return error;
            }
        } else if (transform != 0) {
            // field has a sparql-level transform function so we can't use the
            // extended property function syntax to get the graph
            query.addProjection (bindUniqueFieldWithSubquery(field, subject, chain, transform));
        } else {
            // get value(s) by predicate function chain
            Function p = predicateFunctionChain(chain, subject, withGraph);

            query.addProjection(p);
        }
    } else {
        // get value(s) by subquery
        Select s;
        Variable last;
        Variable lastForNormalRestrictions = last;
        Pattern patternForGraph;

        // split property chain into predicate function chain for projection and rest as restrictions
        PropertyInfoList projectionChain;
        ValueChain restrictionPredicates;

        if (not field.isWithoutMapping()) {
            const QctSplitPropertyChains splitChains(chain);
            restrictionPredicates = valueChain(splitChains.restrictionChain());
            projectionChain = splitChains.projectionChain();

            if (definesOwnership) {
                // insert the last restriction predicate, which is the ownership defining one,
                // manually into the restriction part and by this define patternForGraph
                Value ownershipPredicate = restrictionPredicates.takeLast();
                // for restrictions with more than one predicate define a new variable for the pivot resource
                lastForNormalRestrictions = restrictionPredicates.isEmpty() ? subject : Variable();
                patternForGraph = Pattern(lastForNormalRestrictions, ownershipPredicate, last);
                s.addRestriction(patternForGraph);
            }
        } else {
            restrictionPredicates = valueChain(chain);
            restrictionPredicates.append(nao::hasProperty::resource());
            projectionChain.append(PropertyInfo<nao::propertyValue>());
            s.addRestriction(last, nao::propertyName::resource(), LiteralValue(field.name()));
        }

        // add restrictions to innerSelect
        s.addRestriction(subject, restrictionPredicates, lastForNormalRestrictions);

        // add predicate function chain as projection to subselect
        Function projection;
        if (definesOwnership) {
            projection = applyConcatWithGraph(predicateFunctionChain(projectionChain, last), patternForGraph);
        } else {
            projection = predicateFunctionChain(projectionChain, last, withGraph);
        }

        if (projection.isValid()) {
            if (isRestrictedToInstances) {
                if (field.hasOwner()) {
                    qctWarn(QString::fromLatin1("Values-by-instance not supported for field %1").
                            arg(field.name()));
                    return QContactManager::NotSupportedError;
                } else {
                    projection = Functions::trackerId.apply(projection);
                    if (not projectionChain.last().singleValued()) {
                        projection = Functions::groupConcat.apply(projection, LiteralValue(listSeparator()));
                    }
                }
            }

            s.addProjection(projection);
        } else {
            s.addProjection(last);
        }

        query.addProjection(s);
    }

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindUniqueDetail(const QTrackerContactDetail &detail,
                                                    Select &query,
                                                    const QSet<QString> &fieldFilter)
{
    if (not detail.isUnique()) {
        qctWarn(QString::fromLatin1("Calling bindUniqueDetail on a non unique detail: %1").
                arg(detail.name()));
        return QContactManager::UnspecifiedError;
    }

    foreach(const QTrackerContactDetailField &field, detail.fields()) {
        if (not fieldFilter.empty() && not fieldFilter.contains(field.name())) {
            continue;
        }

        if (not field.hasPropertyChain()) {
            continue;
        }

        const QContactManager::Error error = bindUniqueDetailField(field, query);

        if (error != QContactManager::NoError) {
            return error;
        }

        // For subtype properties we bind the field once with each property, and
        // we'll check what we have when we fetch
        if (field.hasSubTypeProperties()) {
            foreach(const PropertyInfoBase &pi, field.subTypeProperties()) {
                QTrackerContactDetailField f = field;

                if (f.propertyChain().empty()) {
                    qctWarn(QString::fromLatin1("Can't apply property subtype without property "
                                                "chain for field %1").arg(field.name()));
                    continue;
                }

                PropertyInfoList chain = f.propertyChain();
                chain.last() = pi;
                f.setPropertyChain(chain);

                // TODO: replace this with some few projections and restrictions as needed if possible
                const QContactManager::Error error = bindUniqueDetailField(f, query);

                if (error != QContactManager::NoError) {
                    return error;
                }
            }
        }
    }

    return QContactManager::NoError;
}

static Function
bindMultivaluePropertyChainWithSubquery(const PropertyInfoList &chain,
                                        const Variable &subject,
                                        Select &innerSelect,
                                        const SparqlTransform *transform)
{
    const Variable instance;
    Value value;

    PropertyInfoList baseChain = chain;
    const PropertyInfoBase postfix = baseChain.takeLast();

    Variable last;
    if (baseChain.isEmpty()) {
        last = subject;
    } else {
        innerSelect.addRestriction(subject, valueChain(baseChain), last);
    }
    const Pattern postfixPattern(last, postfix.resource(), instance);
    innerSelect.addRestriction(postfixPattern);

    return applyConcatWithGraph(transform->apply(instance), postfixPattern);
}


Value
QTrackerScalarContactQueryBuilder::bindRestrictedValuesField(const QTrackerContactDetailField &field,
                                                             const PropertyInfoList &chain,
                                                             const Variable &subject)
{
    if (chain.isSingleValued(false) && field.allowableInstances().isEmpty()) {
        // get value(s) by predicate function chain
        Function projection = predicateFunctionChain(chain, subject, field.hasOwner());

        return Filter(projection);
    }
    Select innerSelect;
    Value value;

    if (field.hasOwner()) {
        value = bindMultivaluePropertyChainWithSubquery(chain, subject, innerSelect, IdTransform::instance());
    } else {
        const Variable instance;

        // For instances (=resources) we use tracker:id, but for
        // literal we just retrieve them
        if (not field.allowableInstances().empty()) {
            value = Functions::trackerId.apply(instance);
        } else {
            value = instance;
        }
        innerSelect.addRestriction(subject, valueChain(chain), instance);
    }

    innerSelect.addProjection(Functions::groupConcat.apply(value, LiteralValue(listSeparator())));
    return Filter(innerSelect);
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindCustomValues(const QTrackerContactDetailField &field,
                                                    const Cubi::Variable &subject,
                                                    PropertyInfoList chain,
                                                    Value &result)
{
    // If the field has a mapping but allows custom values, then the property is attached
    // to the resource which is the subject of the last property in the chain, so the object
    // of the penultimate. That is of course only valid if the value of the field is expressed
    // by a literal, not if it's determined by a subclass of subproperty type (in which case
    // the value depends on the class or the property)
    if (not field.isWithoutMapping() && not field.hasSubTypeClasses() && not field.hasSubTypeProperties()) {
        if (chain.isEmpty()) {
            qctWarn(QString::fromLatin1("Empty prefix chain is not permitted for custom value field %1").
                    arg(field.name()));
            return QContactManager::UnspecifiedError;
        }

        if (chain.last().isInverse()) {
            qctWarn(QString::fromLatin1("Custom values for fields with the last property inverse are"
                                        "not supported for field %1").arg(field.name()));
            return QContactManager::UnspecifiedError;
        }

        chain.removeLast();
    }

    if (not chain.isEmpty() && not chain.isSingleValued()) {
        qctWarn(QString::fromLatin1("Custom fields on resources with multi valued chains not supported"
                                    "for field %1").arg(field.name()));
        return QContactManager::UnspecifiedError;
    }

    const Variable property(QLatin1String("p"));
    chain += piHasProperty;
    const PatternGroup propertyPatterns = createPatternForPredicateChain(subject, chain, property);

    Value value = nao::propertyValue::function().apply(property);
    if (field.hasOwner()) {
        value = applyConcatWithGraph(value, propertyPatterns);
    }

    Select innerSelect;
    innerSelect.addRestriction(propertyPatterns);
    innerSelect.addRestriction(property,
                               nao::propertyName::resource(),
                               LiteralValue(field.name()));
    innerSelect.addProjection(Functions::groupConcat.apply(value, LiteralValue(listSeparator())));

    result = Filter(innerSelect);
    return QContactManager::NoError;
}

Value
QTrackerScalarContactQueryBuilder::bindInverseFields(const QList<QTrackerContactDetailField> &fields,
                                                     const QctSplitFieldChains &fieldChains,
                                                     const Variable &subject,
                                                     const Pattern &prefixPattern)
{
    ValueChain innerFields;
    Select innerSelect;
    QSet<PropertyInfoList> generatedRestrictions;
    QHash<QString, Variable> inverseObjects;
    // Maps the IRI of a PropertyInfoBase and the Pattern which is binding it
    QHash<QString, Pattern> ownershipPatterns;

    QList<QTrackerContactDetailField>::ConstIterator field = fields.constEnd();
    while(field != fields.constBegin()) {
        field--;
        PropertyInfoList restrictionChain, projectionChain;
        const PropertyInfoList chain = fieldChains.find(field->name())->fullChain();
        PropertyInfoList::ConstIterator pi = chain.constBegin();
        Pattern ownershipPattern = (field->hasOwner() ? prefixPattern : Pattern());

        // FIXME: this mess somehow replicates the stuff found in SplitPropertyChains
        for(; pi != chain.constEnd(); ++pi) {
            if (pi->isInverse()) {
                break;
            }

            restrictionChain.append(*pi);
        }

        if (pi == chain.constEnd()) {
            // What? hasInverseProperty returned true and we didn't find one?
            continue;
        }

        Variable inverseObject;

        if (not generatedRestrictions.contains(restrictionChain)) {
            Variable lastSubject = subject;
            if (not restrictionChain.isEmpty()) {
                Variable inverseSubject;

                // We need to build the statement list manually here to store the
                // ownership statement (if any)
                const PropertyInfoList::ConstIterator lastProperty = restrictionChain.constEnd() - 1;
                for (PropertyInfoList::ConstIterator base = restrictionChain.constBegin();
                     base != restrictionChain.constEnd(); ++base) {
                    const Variable object = (base == lastProperty ? inverseSubject : Variable());
                    const Pattern p(lastSubject, base->resource(), object);

                    if (base->definesOwnership()) {
                        ownershipPattern = p;
                    }

                    innerSelect.addRestriction(p);
                    lastSubject = object;
                }
            }

            const Pattern lastRestriction(inverseObject, pi->resource(), lastSubject);

            if (pi->definesOwnership()) {
                ownershipPattern = lastRestriction;
            }

            innerSelect.addRestriction(lastRestriction);

            generatedRestrictions.insert(restrictionChain);
            inverseObjects.insert(pi->iri(), inverseObject);
            ownershipPatterns.insert(pi->iri(), ownershipPattern);
        } else {
            inverseObject = inverseObjects.value(pi->iri());
            ownershipPattern = ownershipPatterns.value(pi->iri());
        }

        pi++;

        if (pi == chain.constEnd()) {
            if (field->hasOwner()) {
                innerFields.append(applyConcatWithGraph(inverseObject,
                                                        ownershipPattern));
            } else {
                innerFields.append(inverseObject);
            }
        } else {
            PropertyInfoList ownershipChain;

            for(; pi != chain.constEnd(); ++pi) {
                projectionChain.append(*pi);
                ownershipChain.append(*pi);

                if (pi->definesOwnership()) {
                    break;
                }
            }

            for(; pi != chain.constEnd(); ++pi) {
                projectionChain.append(*pi);
            }

            Value v;

            // FIXME: SubTypes, refactor

            if ((field->hasOwner() && projectionChain.last().definesOwnership()) ||
                not field->hasOwner()) {
                // If ownership was on last property in the chain, we can fetch it
                // using extended function properties
                v = predicateFunctionChain(projectionChain, inverseObject, field->hasOwner());
            } else if (projectionChain.length() == ownershipChain.length() &&
                       field->hasOwner() && ownershipPattern != Pattern()) {
                // In that case the ownership statement belongs to the "straight" part, which
                // is in the "WHERE" of the scalar select for that inverse chain. We can
                // thus reuse the Pattern to fetch the graph.
                Function f = predicateFunctionChain(projectionChain, inverseObject, false);
                v = applyConcatWithGraph(f, ownershipPattern);
            } else {
                // Here, it gets complicated, because the statement determining the ownership
                // is in the chain after the inverse property. So we'd probably need another
                // scalar select but that gets a bit insane, and the schema has no such cases
                // so far, leaving unimplemented
                qctWarn(QString::fromLatin1("Unsupported ownership for inverse field %1").
                        arg(field->name()));
                continue;
            }

            innerFields.append(Functions::coalesce.apply(v, LiteralValue(QString())));
        }
    }

    const QString coalesceString = QString(innerFields.size()-1, fieldSeparator());

    if (innerFields.size() > 1) {
        intercalate(innerFields, LiteralValue(fieldSeparator()));
        innerSelect.addProjection(Functions::concat.apply(innerFields));
    } else if (innerFields.size() == 1) {
        const Function &f = static_cast<const Function&>(innerFields.first());
        innerSelect.addProjection(f);
    }

    return Functions::coalesce.apply(Filter(innerSelect),
                                     LiteralValue(coalesceString));
}

static Value
applyFieldRestrictions(Select &query, Value subject,
                       const PropertyInfoList &chain,
                       Pattern *ownershipPattern = 0)
{
    foreach(const PropertyInfoBase &pi, chain) {
        const Pattern pattern(subject, pi.resource(), Variable());

        if (ownershipPattern && pi.definesOwnership()) {
            *ownershipPattern = pattern;
        }

        query.addRestriction(pattern);
        subject = pattern.object();
    }

    return subject;
}

// FIXME: cut this monster into smaller pieces!
QContactManager::Error
QTrackerScalarContactQueryBuilder::bindMultiDetail(const QTrackerContactDetail &detail,
                                                   Select &query,
                                                   const Cubi::Variable &subject,
                                                   const QSet<QString> &fieldFilter)
{
    // A non-unique detail can contain fields attached to one or more non-unique
    // resources (ie a detail can aggregate properties from various RDF resources).
    // We first list those different resources, and then for each of them we fetch
    // all the properties' values grouped together using fn:concat.

    PropertyInfoList prefixes;
    QMultiHash<PropertyInfoBase, QTrackerContactDetailField> fields;
    // This hash stores the modified property chains for each field (key = field name)
    QctSplitFieldChains fieldChains;

    bool hasDetailUri = false;
    PropertyInfoList detailUriChain;
    PropertyInfoBase detailUriPrefix;
    Variable detailUriSubject;

    QMultiMap<PropertyInfoBase, QTrackerContactDetailField> inverseFields;

    foreach (const QTrackerContactDetailField &field, detail.fields()) {
        if (not fieldFilter.empty() && not fieldFilter.contains(field.name())) {
            continue;
        }

        if (not field.hasPropertyChain()) {
            continue;
        }

        PropertyInfoList chain = field.propertyChain();
        const PropertyInfoBase prefix = chain.first();

        // If needed, store information on how to retrieve the detailUri later.
        // It will be prepended to the normal properties
        if (not hasDetailUri && field.hasDetailUri()) {
            hasDetailUri = true;
            detailUriSubject = subject;
            detailUriChain = chainToDetailUri(chain);
            detailUriPrefix = prefix;
        }

        chain.removeFirst(); //TODO: rather add a baseChain to detailDefinition

        // FIXME: This probably breaks if chain.first is an inverse property
        fieldChains.insert(field.name(), QctSplitPropertyChains(chain));
        if (chain.hasInverseProperty()) {
            inverseFields.insert(prefix, field);
            continue;
        }

        if (field.hasSubTypeProperties()) {
            if (field.propertyChain().size() != 1) {
                qctWarn(QString::fromLatin1("Subtype properties with a property chain "
                                            "longer than 1 are not supported for field %1").
                        arg(field.name()));
                return QContactManager::NotSupportedError;
            } else {
                foreach(const PropertyInfoBase &pi, field.subTypeProperties()) {
                    prefixes.append(pi);
                    fields.insert(pi, field);
                }
            }
        }

        if (not prefixes.contains(prefix)) {
            prefixes.append(prefix);
        }
        fields.insert(prefix, field);
    }

    foreach(const PropertyInfoBase &prefix, prefixes) {
        ValueChain tokens; // TODO: the class name confuses, is a list and not a chain!
        // This "hack" is needed, else for details which are on affiliations but don't
        // have context, we fetch the complete list for each affiliation → we end up
        // with as many duplicates as we have affiliations
        const Variable object = (prefix == piHasAffiliation) ? context() : Variable();
        const Pattern prefixPattern(subject, prefix.resource(), object);

        if (hasDetailUri && detailUriPrefix == prefix) {
            if (not detailUriChain.empty() && prefix == detailUriChain.first()) {
                detailUriChain.removeFirst();
                detailUriSubject = object;
            }

            // no graph added for detail uris
            if (detailUriChain.empty()) {
                tokens.append(detailUriSubject);
            } else {
                tokens.append(predicateFunctionChain(detailUriChain, detailUriSubject));
            }
        }

        // We walk the list in reverse order to preserve field order
        // Fields containing inverse properties are bound in the end anyway
        // since they use a nested scalar select
        const QList<QTrackerContactDetailField> prefixFields = fields.values(prefix);

        // express fields with unshared prefix property and no other property in chain by predicate function
        if (prefixFields.size() == 1) {
            const QTrackerContactDetailField &field = prefixFields.first();
            const QctSplitFieldChains::ConstIterator it = fieldChains.find(field.name());

            if (it != fieldChains.constEnd() && it->isEmpty()) {
                PredicateFunction predicateFunction = prefix.predicateFunction().apply(subject);
                predicateFunction.setValueSeparator(QTrackerScalarContactQueryBuilder::detailSeparator());
                if (field.hasOwner()) {
                    predicateFunction.setGraphSeparator(QTrackerScalarContactQueryBuilder::graphSeparator());
                }
                query.addProjection(predicateFunction);
                continue;
            }
        }
        // normal algorithm
        Select groupSelect;
        PropertyInfoList sharedRestrictions;
        bool haveSharedRestrictions = false;
        Value sharedRestrictionsTarget;

        for(QctSplitFieldChains::ConstIterator it = fieldChains.constBegin(); it != fieldChains.constEnd(); ++it) {
            if (it->restrictionChain() != sharedRestrictions) {
                if (sharedRestrictions.isEmpty()) {
                    sharedRestrictions = it->restrictionChain();
                    haveSharedRestrictions = (sharedRestrictions.count() > 1);
                } else if (not it->restrictionChain().isEmpty()) {
                    haveSharedRestrictions = false;
                }
            }
        }

        if (haveSharedRestrictions) {
            sharedRestrictionsTarget = applyFieldRestrictions(groupSelect, object, sharedRestrictions);
        }

        for(QList<QTrackerContactDetailField>::ConstIterator
            field = prefixFields.constEnd(); field-- != prefixFields.constBegin(); ) {
            const QctSplitFieldChains::ConstIterator splitChains = fieldChains.find(field->name());
            Value v;

            if (not field->isWithoutMapping()) {
                if (splitChains != fieldChains.constEnd() && not splitChains->isEmpty()) {
                    // flag whether the an empy result (null) needs to be replaced with an empty
                    // string as it can happen with optional fields. not initializing by purpose
                    // to let the compiler help us if we forget considering it in one of the many
                    // branches following here
                    bool needsEmptyStringCoalesce;

                    // FIXME: What happens if we have instances but no property chain?
                    // We don't use IRIs but trackerId for instances
                    // In this case, we have to generate a scalar select, because we
                    // can't apply tracker:id if we have various instances (v would not be
                    // an IRI but a comma separated list of IRIs)
                    if (field->restrictsValues()) {
                        v = bindRestrictedValuesField(*field, splitChains->fullChain(), object);
                        needsEmptyStringCoalesce = true;
                    } else {
                        // This call will return the parts of the chain that can be bound
                        // with predicate functions, however, in the part that is in the
                        // scalar select restrictions, we still have to differentiate between
                        // the part defining ownership, and the part that is here because of
                        // multi-valued properties. The part that defines ownership will be
                        // stored in patternForGraph, and the complete chain will go in the
                        // scalar select restrictions.
                        //
                        // We also need a sub-select if there's a transform on
                        // SPARQL-level; some of the functions can't operate
                        // on predicate functions, only on sparql variables.

                        const SparqlTransform *const transform = field->sparqlTransform();
                        if (splitChains->restrictionChain().isEmpty() && transform == 0) {
                            const bool ownershipFetchWithPredicate = (not field->propertyChain().isEmpty()) &&
                                    (field->propertyChain().last().definesOwnership());
                            if (field->hasOwner() && field->definesOwnership() && not ownershipFetchWithPredicate) {
                                v = applyConcatWithGraph(predicateFunctionChain(splitChains->projectionChain(), object),
                                                         prefixPattern);
                                needsEmptyStringCoalesce = false;
                            } else {
                                v = predicateFunctionChain(splitChains->fullChain(), object, field->hasOwner());
                                needsEmptyStringCoalesce = true;
                            }
                        } else {
                            // might need to do subselect
                            PropertyInfoList restrictionChain = splitChains->restrictionChain();
                            bool usesSharedRestrictions = haveSharedRestrictions && not restrictionChain.isEmpty();
                            Pattern ownershipPattern;
                            Select subselect;

                            Value last = applyFieldRestrictions(subselect, object,
                                                                splitChains->restrictionChain(),
                                                                &ownershipPattern);
                            if (usesSharedRestrictions) {
                               last = sharedRestrictionsTarget;
                            }

                            Function f;
                            if (field->hasOwner() && field->definesOwnership()) {
                                f = applyConcatWithGraph(predicateFunctionChain(splitChains->projectionChain(), last),
                                                         ownershipPattern);
                            } else if (transform == 0) {
                                f = predicateFunctionChain(splitChains->projectionChain(),
                                                           last, field->hasOwner());
                            } else {
                                f = bindMultivaluePropertyChainWithSubquery(splitChains->fullChain(),
                                                                            object, subselect, transform);
                            }

                            if (haveSharedRestrictions) {
                                needsEmptyStringCoalesce = false;
                                v = f;
                            } else {
                                needsEmptyStringCoalesce = true;
                                subselect.addProjection(f);
                                v = Filter(subselect);
                            }
                        }
                    }

                    if (needsEmptyStringCoalesce) {
                        v = Functions::coalesce.apply(v, LiteralValue(QString()));
                    }
                } else {
                    v = object;
                }

                if (field->hasSubTypes()) {
                    if (field->hasSubTypeClasses()) {
                        // no graph added to subtypes by class
                        Select innerSelect;
                        Variable type(QLatin1String("t"));
                        innerSelect.addProjection(Functions::groupConcat.
                                                  apply(Functions::trackerId.apply(type),
                                                        LiteralValue(listSeparator())));
                        innerSelect.addRestriction(v, rdf::type::resource(), type);
                        tokens.append(Functions::coalesce.apply(Filter(innerSelect),
                                                                LiteralValue(QString())));
                    }
                } else {
                    if (v == object) {
                        if (field->hasOwner()) {
                            v = applyConcatWithGraph(v, prefixPattern);
                        }
                    }
                    tokens.append(v);
                }
            }

            if (field->permitsCustomValues()) {
                Value customFieldSelect;
                const QContactManager::Error error = bindCustomValues(*field, object,
                                                                      splitChains->fullChain(),
                                                                      customFieldSelect);

                if (error != QContactManager::NoError) {
                    return error;
                }

                tokens.append(Functions::coalesce.apply(customFieldSelect, LiteralValue(QString())));
            }

        }

        // FIXME: this can probably be factorized better
        // FIXME: we don't handle DetailUri there
        if (inverseFields.contains(prefix)) {
            QList<QTrackerContactDetailField> inversePrefixFields = inverseFields.values(prefix);
            tokens.append(bindInverseFields(inversePrefixFields, fieldChains, object, prefixPattern));
        }


        Value jointTokens;
        if (tokens.size() > 1) {
            intercalate(tokens, LiteralValue(fieldSeparator()));
            jointTokens = Functions::concat.apply(tokens);
        } else if (tokens.size() == 1 ){
            jointTokens = tokens.first();
        } else {
            // We can have no fields if there was only one field, and it was pointing
            // to a value and not a resource
            jointTokens = object;
        }

        if (prefix != piHasAffiliation) {
            groupSelect.addRestriction(prefixPattern);
        }
        groupSelect.addProjection(Functions::groupConcat.apply(jointTokens, LiteralValue(detailSeparator())));

        query.addProjection(groupSelect);
    }

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFields(const QTrackerContactDetail &detail, Select &query,
                                              const QSet<QString> &fieldFilter)
{
    if (detail.isUnique()) {
        return bindUniqueDetail(detail, query, fieldFilter);
    } else {
        const Variable &subject = detail.hasContext() ? context() : contact();
        return bindMultiDetail(detail, query, subject, fieldFilter);
    }
}

void
QTrackerScalarContactQueryBuilder::bindCustomDetails(Cubi::Select &query,
                                                     const QSet<QString> &detailFilter)
{
    Select customDetailsQuery;

    Variable detail(QLatin1String("customDetail"));
    Function detailName(nao::propertyName::function().apply(detail));

    Variable field(QLatin1String("customField"));
    Function fieldName(nao::propertyName::function().apply(field));


    ValueChain valueConcatParams;
    Variable fieldValue(QLatin1String("value"));
    valueConcatParams.append(Functions::trackerId.apply(field));
    valueConcatParams.append(LiteralValue(QLatin1String(":")));
    valueConcatParams.append(fieldValue);

    Select fieldValuesQuery;
    fieldValuesQuery.addProjection(Functions::groupConcat.
                                   apply(Functions::concat.apply(valueConcatParams),
                                         LiteralValue(listSeparator())));
    fieldValuesQuery.addRestriction(field, nao::propertyValue::resource(), fieldValue);

    Select propertySelect;
    propertySelect.addProjection(Functions::groupConcat.
                                 apply(Functions::concat.apply(fieldName,
                                                               LiteralValue(fieldSeparator()),
                                                               Filter(fieldValuesQuery)),
                                       LiteralValue(fieldSeparator())));
    propertySelect.addRestriction(detail, nao::hasProperty::resource(), field);

    ValueChain groupConcatParams;
    groupConcatParams.append(detailName);
    groupConcatParams.append(LiteralValue(fieldSeparator()));
    groupConcatParams.append(Filter(propertySelect));
    customDetailsQuery.addProjection(Functions::groupConcat.
                                     apply(Functions::concat.apply(groupConcatParams),
                                           LiteralValue(detailSeparator())));
    customDetailsQuery.addRestriction(contact(), nao::hasProperty::resource(), detail);

    if (not detailFilter.empty()) {
        ValueList detailsToFetch;

        foreach(const QString &filter, detailFilter) {
            LiteralValue name(qVariantFromValue(filter));
            detailsToFetch.addValue(name);
        }

        customDetailsQuery.setFilter(Functions::in.apply(detailName, detailsToFetch));
    }

    query.addProjection(customDetailsQuery);
}


void QTrackerScalarContactQueryBuilder::bindHasMemberRelationships(Select& query)
{
    // HasMember with contact in Second role, valid for both group and normal contacts
    Variable groupContact;
    const PrefixFunction groupContactGroupConcat =
        Functions::groupConcat.apply(Functions::trackerId.apply(groupContact),
                                     LiteralValue(QTrackerScalarContactQueryBuilder::listSeparator()));
    Select groupContactsSelect;
    groupContactsSelect.addProjection(groupContactGroupConcat);
    groupContactsSelect.addRestriction(contact(), nco::belongsToGroup::resource(), groupContact);

    query.addProjection(groupContactsSelect);

    if (m_schema.contactType() == QContactType::TypeGroup) {
        // HasMember with contact in First role, only applyable for group contacts
        Variable memberContact;
        const PrefixFunction memberContactGroupConcat =
            Functions::groupConcat.apply(Functions::trackerId.apply(memberContact),
                                        LiteralValue(QTrackerScalarContactQueryBuilder::listSeparator()));
        Select memberContactsSelect;
        memberContactsSelect.addProjection(memberContactGroupConcat);
        memberContactsSelect.addRestriction(memberContact, nco::belongsToGroup::resource(), contact());

        query.addProjection(memberContactsSelect);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#define DO_ENUM_VALUE(Type) \
    case Type: return QLatin1String(#Type)

static QString
filterName(QContactFilter::FilterType type)
{
    switch(type) {
        DO_ENUM_VALUE(QContactFilter::InvalidFilter);
        DO_ENUM_VALUE(QContactFilter::ContactDetailFilter);
        DO_ENUM_VALUE(QContactFilter::ContactDetailRangeFilter);
        DO_ENUM_VALUE(QContactFilter::ChangeLogFilter);
        DO_ENUM_VALUE(QContactFilter::ActionFilter);
        DO_ENUM_VALUE(QContactFilter::RelationshipFilter);
        DO_ENUM_VALUE(QContactFilter::IntersectionFilter);
        DO_ENUM_VALUE(QContactFilter::UnionFilter);
        DO_ENUM_VALUE(QContactFilter::LocalIdFilter);
        DO_ENUM_VALUE(QContactFilter::DefaultFilter);
    }

    return QString::fromLatin1("QContactFilter::FilterType(%1)").arg(type);
}

static QString
eventName(QContactChangeLogFilter::EventType type)
{
    switch(type) {
        DO_ENUM_VALUE(QContactChangeLogFilter::EventAdded);
        DO_ENUM_VALUE(QContactChangeLogFilter::EventChanged);
        DO_ENUM_VALUE(QContactChangeLogFilter::EventRemoved);
    }

    return QString::fromLatin1("QContactChangeLogFilter::EventType(%1)").arg(type);
}

#undef DO_ENUM_VALUE

////////////////////////////////////////////////////////////////////////////////////////////////////

static QContactFilter::MatchFlags
matchFunctionFlags(QContactFilter::MatchFlags flags)
{
    return flags & MatchFunctionFlags;
}

static QContactFilter::MatchFlags
stringCompareFlags(QContactFilter::MatchFlags flags)
{
    return flags & StringCompareFlags;
}

Function
QTrackerScalarContactQueryBuilder::matchFunction(QContactDetailFilter::MatchFlags flags,
                                                 Value variable, QVariant value)
{
    Value param = variable;

    // Convert variable and search value into lower-case for case-insensitive matching.
    // Must convert the value locally, instead of also using fn:lower-case because
    // fn:starts-with only takes literals as second argument, but not return values.
    if (QVariant::String == value.type() && 0 == (flags & QContactFilter::MatchCaseSensitive)) {
        param = Functions::lowerCase.apply(param);
        value.setValue(value.toString().toLower());
    }

    Value valueObject = qctMakeCubiValue(value);

    // If exact matching is requested for non-string literal still do case sensitive matching,
    // as case insensitive matching of numbers or URLs doesn't make much sense.
    if (QVariant::String != value.type() && 0 == stringCompareFlags(flags)) {
        return Functions::equal.apply(param, valueObject);
    }

    // for case sensitive matching there are specialized functions...
    switch(matchFunctionFlags(flags)) {
    case QContactFilter::MatchContains:
        return Functions::contains.apply(param, valueObject);
    case QContactFilter::MatchStartsWith:
        return Functions::startsWith.apply(param, valueObject);
    case QContactFilter::MatchEndsWith:
        return Functions::endsWith.apply(param, valueObject);
    }

    return Functions::equal.apply(param, valueObject);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T> static bool
isCompoundFilterSupported(const QContactFilter &filter)
{
    foreach(const QContactFilter &childFilter, static_cast<const T&>(filter).filters()) {
        if (not QTrackerScalarContactQueryBuilder::isFilterSupported(childFilter)) {
            return false;
        }
    }

    return true;
}

static bool
isCanonicalFilterSupported(const QContactFilter &filter)
{
    switch(filter.type()) {
    case QContactFilter::DefaultFilter:
    case QContactFilter::LocalIdFilter:
    case QContactFilter::ContactDetailFilter:
    case QContactFilter::ContactDetailRangeFilter:
    case QContactFilter::ChangeLogFilter:
    case QContactFilter::RelationshipFilter:
    case QContactFilter::InvalidFilter:
        return true;

    case QContactFilter::IntersectionFilter:
        return isCompoundFilterSupported<QContactIntersectionFilter>(filter);
    case QContactFilter::UnionFilter:
        return isCompoundFilterSupported<QContactUnionFilter>(filter);

    case QContactFilter::ActionFilter:
        break;
    }

    return false;
}

static inline ClassInfoList::ConstIterator
findClassInfoWithText(const ClassInfoList &list, const QString &text)
{
    ClassInfoList::ConstIterator it;
    for (it = list.constBegin(); it != list.constEnd(); ++it) {
        if (it->text() == text) {
            break;
        }
    }
    return it;
}

static inline PropertyInfoList::ConstIterator
findPropertyInfoWithText(const PropertyInfoList &list, const QString &text)
{
    PropertyInfoList::ConstIterator it;
    for (it = list.constBegin(); it != list.constEnd(); ++it) {
        if (it->text() == text) {
            break;
        }
    }
    return it;
}

static QString
findClassSubTypeIri(const QTrackerContactDetailField *field, const QVariant &value)
{
        // lookup class for given subtype
        const QString subType = value.toString();
        const ClassInfoList &subTypeClasses = field->subTypeClasses();
        ClassInfoList::ConstIterator it = findClassInfoWithText(subTypeClasses, subType);

        // standard subtype found?
        if (it != subTypeClasses.constEnd()) {
            return it->iri();
        }

        return QString();
}

bool
QTrackerScalarContactQueryBuilder::isFilterSupported(const QContactFilter &filter)
{
    return isCanonicalFilterSupported(QContactManagerEngine::canonicalizedFilter(filter));
}

static bool
applyTypeCast(QContactFilter::MatchFlags flags,
              const QTrackerContactDetailField *field, QVariant &value)
{
    if (0 != stringCompareFlags(flags)) {
        return value.convert(QVariant::String);
    }

    return field->makeValue(value, value);
}

static Filter
createSubTypePropertyFilter(const Variable &subject,
                            const Variable &object,
                            const PropertyInfoList &subtypes,
                            const PropertyInfoBase &type)
{
    ValueChain operands;

    // Filtering is only needed if the filtered value corresponds to a RDF
    // property that has sub-properties. Because we don't really have this
    // information in the schema, we assume the default value is the base
    // property (that's at least how it works for QContactUrl).

    foreach(const PropertyInfoBase &subtype, subtypes) {
        if (subtype.iri() == type.iri()) {
            return Filter();
        }
    }

    foreach(const PropertyInfoBase &subtype, subtypes) {
        Exists exists;
        exists.addPattern(subject, subtype.resource(), object);
        operands.append(Functions::not_.apply(Filter(exists)));
    }

    return Filter(Functions::and_.apply(operands));
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindTypeFilter(const QContactDetailFilter &filter,
                                                  Filter &result)
{
    const QString contactType(filter.value().toString());

    // We don't need to repeat a filter with rdf:type here, since there is one base
    // query per contact type, and that base query already filters on the type:
    // ?_contact a rdf:PersonContact ...
    result = LiteralValue(schema().contactType() == contactType);

    return QContactManager::NoError;
}

void
QTrackerScalarContactQueryBuilder::bindCustomDetailFilter(const QContactDetailFilter &filter,
                                                          Exists &exists,
                                                          Variable &subject)
{
        Variable fieldValue(QString::fromLatin1("fieldValue"));

        exists.addPattern(createPatternForCustomDetail(contact(),
                                                       filter.detailDefinitionName(),
                                                       filter.detailFieldName(),
                                                       fieldValue));

        subject = fieldValue;
}

PatternGroup
QTrackerScalarContactQueryBuilder::bindWithoutMappingFilter(const QTrackerContactDetailField *field,
                                                            const Variable &subject,
                                                            Variable &value)
{
        value = Variable(QString::fromLatin1("fieldValue"));

        return createPatternForCustomField(subject, field->name(), value);
}

static bool
phoneNumberHasDTMFCodes(const QString &number)
{
    return number.contains(qctPhoneNumberDTMFChars());
}

void
QTrackerScalarContactQueryBuilder::bindDTMFNumberFilter(const QContactDetailFilter &filter,
                                                        Exists &exists,
                                                        const Variable &subject)
{
    static const ValueChain phoneNumberChain = ValueChain() << nco::hasAffiliation::resource()
                                                            << nco::hasPhoneNumber::resource();
    Select mainSelect;
    QStringList numbers;
    ValueChain numberSelects;

    const QString &value = filter.value().toString();
    numbers.append(value);

    const int dtmfIndex = value.indexOf(qctPhoneNumberDTMFChars());

    // If there was no DTMF code, or the DTMF code was the first character, then we just
    // do the exact match. That would be an unfortunate/silly case though.
    if (dtmfIndex > 0) {
        numbers.append(value.left(dtmfIndex));
    }

    Variable phoneNumberResource;

    foreach (const QString &number, numbers) {
        Select select;
        Variable contact;

        select.addProjection(maemo::localPhoneNumber::function().apply(phoneNumberResource));
        select.addRestriction(contact, rdf::type::resource(), nco::PersonContact::resource());
        select.addRestriction(contact, phoneNumberChain, phoneNumberResource);
        select.addRestriction(phoneNumberResource, maemo::localPhoneNumber::resource(), LiteralValue(number));

        numberSelects.append(Filter(select));
    }

    if (numberSelects.size() > 1) {
        mainSelect.addProjection(Functions::coalesce.apply(numberSelects), subject);
    } else {
        const Select &select = static_cast<const Filter&>(numberSelects.first()).select();
        mainSelect.addProjection(select, subject);
    }

    exists.addPattern(CompositionalSelect(mainSelect));
}

template <class T> QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilterDetail(const T &filter,
                                                    Exists &exists,
                                                    Variable &subject,
                                                    bool hasCustomSubType,
                                                    const PropertyInfoBase &propertySubtype)
{
    static const uint unsupportedMatchFlags =
            // FIXME: add support for those flags
            QContactDetailFilter::MatchKeypadCollation;

    if (filter.matchFlags() & unsupportedMatchFlags) {
        qctWarn(QString::fromLatin1("%1: Unsupported match flags: %2").
                arg(filterName(filter.type())).
                arg(filter.matchFlags() & unsupportedMatchFlags));
    }

    // find detail and field definition
    const QTrackerContactDetail *const detail = m_schema.detail(filter.detailDefinitionName());

    if (0 == detail) {
        bindCustomDetailFilter(filter, exists, subject);
        return QContactManager::NoError;
    }

    if (filter.detailFieldName().isEmpty()) {
        qctWarn("field name must not be empty");
        return QContactManager::NotSupportedError;
    }

    return bindFilterDetailField(filter, exists, subject, hasCustomSubType, propertySubtype);
}

template <class T> QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilterDetailField(const T &filter,
                                                         Exists &exists,
                                                         Variable &subject,
                                                         bool hasCustomSubType,
                                                         const PropertyInfoBase &propertySubtype)
{

    // find detail and field definition
    const QTrackerContactDetail *const detail = m_schema.detail(filter.detailDefinitionName());
    const QTrackerContactDetailField *const field = findField(*detail, filter.detailFieldName());

    if (0 == field) {
        qctWarn(QString::fromLatin1("Could not find schema for field %1 in detail %2").
                arg(filter.detailFieldName(), filter.detailDefinitionName()));
        return QContactManager::NotSupportedError;
    }

    PropertyInfoList properties = field->propertyChain();

    // Custom subtypes are stored in nao:Property objects
    if (field->hasSubTypeClasses() && not hasCustomSubType) {
        properties.append(PropertyInfo<rdf::type>());
    }

    if (not propertySubtype.iri().isEmpty()) {
        properties.removeLast();
    }

    // do special processing for phone numbers
    if (not properties.empty() && properties.last().iri() == nco::phoneNumber::iri()) {
        if (filter.matchFlags().testFlag(QContactFilter::MatchPhoneNumber)) {
            // use the normalized phone number, stored in maemo:localPhoneNumber resource
            properties.removeLast();
            properties.append(field->computedProperties().first());

            // this hack only works for maemo:localPhoneNumber
            Q_ASSERT(properties.last().iri() == maemo::localPhoneNumber::iri());
        }
    }

    Variable filterValue;

    if (detail->hasContext()) {
        properties.prepend(piHasAffiliation);
    }

    exists.addPattern(createPatternForPredicateChain(contact(), properties, filterValue));

    if (not propertySubtype.iri().isEmpty()) {
        Variable newValue;
        exists.addPattern(filterValue, propertySubtype.resource(), newValue);

        exists.setFilter(createSubTypePropertyFilter(filterValue,
                                                     newValue,
                                                     field->subTypeProperties(),
                                                     propertySubtype));
        filterValue = newValue;
    }

    if (field->isWithoutMapping() ||
        hasCustomSubType ||
        (field->hasSubTypeProperties() && propertySubtype == 0)) {
        Variable newValue;
        exists.addPattern(bindWithoutMappingFilter(field, filterValue, newValue));
        filterValue = newValue;
    }

    subject = filterValue;

    return QContactManager::NoError;
}

QVariant
QTrackerScalarContactQueryBuilder::normalizeFilterValue(const QTrackerContactDetailField *field,
                                                        const QContactDetailFilter::MatchFlags &flags,
                                                        const QVariant &value,
                                                        QContactManager::Error &error)
{
    error = QContactManager::NoError;

    if (value.isNull()) {
        return value;
    }

    QVariant v = value;

    // try casting the filter value
    if (not applyTypeCast(flags, field, v)) {
        qctWarn(QString::fromLatin1("Cannot apply required casts to filter value "
                                    "for field %1").arg(field->name()));
        error = QContactManager::BadArgumentError;
        return v;
    }

    // do special processing for phone numbers
    // FIXME: find a more generic pattern for doing this modifications
    if (not field->propertyChain().empty() &&
            field->propertyChain().last().iri() == nco::phoneNumber::iri()) {
        // Phone numbers are almost impossible to parse (TODO: reference?)
        // So phone number handling is done based on heuristic:
        // it turned out that matching the last 7 digits works good enough (tm).
        // The actual number of digits to use is stored in QctSettings().localPhoneNumberLength().
        // The phone number's digits are stored in tracker using a maemo:localPhoneNumber property.
        if (QContactFilter::MatchEndsWith == matchFunctionFlags(flags)) {
            return qctMakeLocalPhoneNumber(v.toString());
        }
    }

    if (field->hasSubTypeClasses()) {
        // FIXME: keep QUrl here. LiteralValue needs that to work properly,
        // need to find proper solution for this.
        const QUrl subTypeIri = findClassSubTypeIri(field, v);

        // standard subtype found?
        if (subTypeIri.isValid()) {
            return subTypeIri;
        } else {
            if (not field->permitsCustomValues()) {
                qctWarn(QString::fromLatin1("Unknown subtype %2 for field %1").
                        arg(field->name(), v.toString()));
                error = QContactManager::BadArgumentError;
                return QVariant();
            }

            return v;
        }
    }

    return v;
}

QContactFilter::MatchFlags
QTrackerScalarContactQueryBuilder::normalizeFilterMatchFlags(const QTrackerContactDetailField *field,
                                                             const QContactFilter::MatchFlags &matchFlags,
                                                             const QVariant &value)
{
    QContactFilter::MatchFlags flags = matchFlags;

    // do special processing for phone numbers
    // FIXME: find a more generic pattern for doing this modifications
    if (not field->propertyChain().empty() &&
            field->propertyChain().last().iri() == nco::phoneNumber::iri()) {
        // Phone numbers are almost impossible to parse (TODO: reference?)
        // So phone number handling is done based on heuristic:
        // it turned out that matching the last 7 digits works good enough (tm).
        // The actual number of digits to use is stored in QctSettings().localPhoneNumberLength().
        // The phone number's digits are stored in tracker using a maemo:localPhoneNumber property.

        if (flags.testFlag(QContactFilter::MatchPhoneNumber)) {
            const int localPhoneNumberLength = QctThreadLocalData::instance()->settings()->localPhoneNumberLength();
            const QString phoneNumberReference = value.toString();

            // remove existing function flag and replace with normal string oriented one
            flags &= ~MatchFunctionFlags;
            if (phoneNumberReference.length() >= localPhoneNumberLength) {
                flags |= QContactFilter::MatchEndsWith;
            } else {
                flags |= QContactFilter::MatchExactly;
            }
        }
    }

    return flags;
}

template<class T> QContactManager::Error
QTrackerScalarContactQueryBuilder::bindDetailFilterForAnyField(const T &filter,
                                                               const QTrackerContactDetail *const detail,
                                                               Filter &result)
{
    // Rewrite filter into union filter. This avoids bugs by reusing existing code.
    // Without such habit it is hard if not impossible to consider all special cases
    // in each code path.
    QContactUnionFilter unionFilter;

    foreach(const QTrackerContactDetailField &field, detail->fields()) {
        T subFilter = filter;
        subFilter.setDetailDefinitionName(detail->name(), field.name());
        unionFilter.append(subFilter);
    }

    return bindFilter(unionFilter, result);
}

template<class T> QContactManager::Error
QTrackerScalarContactQueryBuilder::bindDetailContextFilter(const T &filter,
                                                           const QTrackerContactDetail * const detail,
                                                           Filter &result)
{
    // A detail can be composed of many fields, and they don't necessarily have
    // to be properties of a same resource. So what we do here is to check each
    // property of the detail without checking its value, and we check the label
    // on the affiliation.
    // Result is of the form: FILTER(rdfs:label(?affiliation) == "Foobar" && (EXISTS { prop1 } ||
    //                                                                        EXISTS { prop2 } ||
    //                                                                        ...
    //                                                                        EXISTS { propN }))

    if (not detail->hasContext()) {
        // We return a false filter rather than an error here, since the group
        // schema has no details with a context.
        result = Filter(QVariant(false));
        return QContactManager::NoError;
    }

    ValueChain andOperands;
    ValueChain orOperands;

    Exists mainExists;
    Variable affiliation;

    mainExists.addPattern(contact(), nco::hasAffiliation::resource(), affiliation);

    if (not filter.value().isNull()) {
        andOperands.append(matchFunction(filter.matchFlags(),
                                         rdfs::label::function().apply(affiliation),
                                         filter.value()));
    }

    foreach (const QTrackerContactDetailField &field, detail->fields()) {
        if (field.isWithoutMapping()) {
            continue;
        }

        Exists e;

        e.addPattern(createPatternForPredicateChain(affiliation, field.propertyChain(), Variable()));

        orOperands.append(Filter(e));
    }

    if (orOperands.isEmpty()) {
        qctWarn(QString::fromLatin1("Filtering on context is not supported for detail without mapping %1").
                arg(detail->name()));
        return QContactManager::NotSupportedError;
    }

    andOperands.append(Functions::or_.apply(orOperands));
    mainExists.setFilter(Functions::and_.apply(andOperands));
    result = Filter(mainExists);

    return QContactManager::NoError;
}

static bool
qctMatches(const QString &sample, const QString &pattern, QContactFilter::MatchFlags flags)
{
    if (not flags.testFlag(QContactFilter::MatchCaseSensitive)) {
        return qctMatches(sample.toLower(), pattern.toLower(),
                          flags | QContactFilter::MatchCaseSensitive);
    }

    switch(matchFunctionFlags(flags)) {
    case QContactFilter::MatchContains:
        return sample.contains(pattern);
    case QContactFilter::MatchStartsWith:
        return sample.startsWith(pattern);
    case QContactFilter::MatchEndsWith:
        return sample.endsWith(pattern);
    }

    return sample == pattern;
}

static bool
qctMatches(const QVariant &sample, const QVariant &pattern, QContactFilter::MatchFlags flags)
{
    return qctMatches(sample.toString(), pattern.toString(), flags);
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(QContactDetailFilter filter, Filter &result)
{
    if (filter.detailDefinitionName() == QContactType::DefinitionName &&
        filter.detailFieldName() == QContactType::FieldType) {
        return bindTypeFilter(filter, result);
    }

    bool hasSubTypeClasses = false;
    bool hasCustomSubTypeClasses = false;
    PropertyInfoList subTypeProperties;

    const QTrackerContactDetail *const detail = m_schema.detail(filter.detailDefinitionName());
    PropertyInfoBase propertySubtype;

    ValueList instanceValues;
    bool hasInstances = false;

    if (detail != 0) {
        if (filter.detailFieldName().isEmpty()) {
            return bindDetailFilterForAnyField(filter, detail, result);
        }

        if (filter.detailFieldName() == QContactDetail::FieldContext) {
            return bindDetailContextFilter(filter, detail, result);
        }

        const QTrackerContactDetailField *const field = findField(*detail, filter.detailFieldName());

        if (field != 0) {
            hasSubTypeClasses = field->hasSubTypeClasses();
            hasCustomSubTypeClasses = (hasSubTypeClasses &&
                                       findClassSubTypeIri(field, filter.value()).isEmpty());

            filter.setMatchFlags(normalizeFilterMatchFlags(field, filter.matchFlags(), filter.value()));

            if (not field->allowableInstances().isEmpty()) {
                static const QContactFilter::MatchFlags supportedFlags =
                        QContactFilter::MatchCaseSensitive | StringCompareFlags;

                if (0 != (filter.matchFlags() & ~supportedFlags)) {
                    qctWarn("Unsupported match flags for instance field");
                    return QContactManager::NotSupportedError;
                }

                foreach(const InstanceInfoBase &instance, field->allowableInstances()) {
                    if (qctMatches(instance.value(), filter.value(), filter.matchFlags())) {
                        instanceValues.addValue(instance.resource());
                    }
                }

                hasInstances = true;
            } else {
                QContactManager::Error error = QContactManager::UnspecifiedError;
                filter.setValue(normalizeFilterValue(field, filter.matchFlags(), filter.value(), error));

                if (error != QContactManager::NoError) {
                    return error;
                }
            }

            subTypeProperties = field->subTypeProperties();

            if (field->hasSubTypeProperties()) {
                const QString subType = filter.value().toString();

                if (not field->propertyChain().empty() && field->defaultValue() == subType) {
                    propertySubtype = field->propertyChain().last();
                } else {
                    PropertyInfoList::ConstIterator it = findPropertyInfoWithText(subTypeProperties, subType);

                    if (it != subTypeProperties.constEnd()) {
                        propertySubtype = *it;
                    }
                }
            }

        }
    }

    Exists exists;
    Variable subject;

    const QContactManager::Error error =
            bindFilterDetail<QContactDetailFilter>(filter, exists, subject,
                                                   hasCustomSubTypeClasses, propertySubtype);

    if (error != QContactManager::NoError) {
        return error;
    }

    // This if block works for both the wildcard filter (a field was bound in
    // an EXISTS block, but null value = wildcard filter) and "exists" filter
    // (all detail fields were bound in bindFilterDetail, but no check on value
    // since we just check for detail presence).
    // We also don't set a filter for subtype properties, since filtering is
    // done on the predicate (and not the object), in bindFilterDetail.
    if (filter.value().isNull() ||
        (not subTypeProperties.empty() && not propertySubtype.iri().isEmpty())) {
        result = Filter(exists);
        return QContactManager::NoError;
    }

    // Phone numbers with DTMF code need a special query for the fallback to the non-DTMF version
    const bool hasDTMFCodes =
            (filter.matchFlags().testFlag(QContactDetailFilter::MatchPhoneNumber) &&
             filter.detailDefinitionName() == QContactPhoneNumber::DefinitionName &&
             filter.detailFieldName() == QContactPhoneNumber::FieldNumber &&
             phoneNumberHasDTMFCodes(filter.value().toString()));

    if (hasSubTypeClasses) {
        exists.setFilter(Functions::equal.apply(subject, qctMakeCubiValue(filter.value())));
    } else if (hasDTMFCodes) {
        bindDTMFNumberFilter(filter, exists, subject);
    } else if (hasInstances) {
        const QList<Value> valueList = instanceValues.values();
        switch(valueList.count()) {
        case 0:
            exists.setFilter(LiteralValue(false));
            break;
        case 1:
            exists.setFilter(Functions::equal.apply(subject, valueList.first()));
            break;
        default:
            exists.setFilter(Functions::in.apply(subject, instanceValues));
            break;
        }
    } else {
        exists.setFilter(matchFunction(filter.matchFlags(), subject, filter.value()));
    }

    result = Filter(exists);
    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(QContactDetailRangeFilter filter, Filter &result)
{
    const QTrackerContactDetail *const detail = m_schema.detail(filter.detailDefinitionName());

    if (detail != 0) {
        if (filter.detailFieldName().isEmpty()) {
            return bindDetailFilterForAnyField(filter, detail, result);
        }

        const QTrackerContactDetailField *const field = findField(*detail, filter.detailFieldName());

        if (field != 0) {
            filter.setMatchFlags(normalizeFilterMatchFlags(field, filter.matchFlags()));
            QContactManager::Error error = QContactManager::UnspecifiedError;
            const QVariant value1 = normalizeFilterValue(field, filter.matchFlags(),
                                                         filter.minValue(), error);

            if (error != QContactManager::NoError) {
                return error;
            }

            error = QContactManager::UnspecifiedError;
            const QVariant value2 = normalizeFilterValue(field, filter.matchFlags(),
                                                         filter.maxValue(), error);

            if (error != QContactManager::NoError) {
                return error;
            }

            filter.setRange(value1, value2, filter.rangeFlags());
        }
    }

    Exists exists;
    Variable subject;

    const QContactManager::Error error =
            bindFilterDetail<QContactDetailRangeFilter>(filter, exists, subject);

    if (error != QContactManager::NoError) {
        return error;
    }

    ValueChain operands;

    if (not filter.minValue().isNull()) {
        // IncludeLower = 0, ExcludeLower = 2
        if (not filter.rangeFlags().testFlag(QContactDetailRangeFilter::ExcludeLower)) {
            operands.append(Functions::lessThanOrEqual.apply(qctMakeCubiValue(filter.minValue()), subject));
        } else {
            operands.append(Functions::lessThan.apply(qctMakeCubiValue(filter.minValue()), subject));
        }
    }

    if (not filter.maxValue().isNull()) {
        // IncludeUpper = 1, ExcludeUpper = 0
        if (filter.rangeFlags().testFlag(QContactDetailRangeFilter::IncludeUpper)) {
            operands.append(Functions::lessThanOrEqual.apply(subject, qctMakeCubiValue(filter.maxValue())));
        } else {
            operands.append(Functions::lessThan.apply(subject, qctMakeCubiValue(filter.maxValue())));
        }
    }

    exists.setFilter(Functions::and_.apply(operands));
    result = Filter(exists);

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactLocalIdFilter &filter,
                                              Cubi::Filter &result)
{
    if (filter.ids().isEmpty()) {
        qctWarn(QString::fromLatin1("%1: Local contact id list cannot be empty").
                arg(filterName(filter.type())));
        return QContactManager::BadArgumentError;
    }

    ValueList ids;

    foreach(QContactLocalId id, filter.ids()) {
        ids.addValue(LiteralValue(qVariantFromValue(id)));
    }

    result = Functions::in.apply(Functions::trackerId.apply(contact()), ids);
    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactIntersectionFilter &filter,
                                              Cubi::Filter &result)
{
    ValueChain filters;

    foreach(const QContactFilter &childFilter, filter.filters()) {
        Filter filter;
        const QContactManager::Error error = bindFilter(childFilter, filter);

        if (QContactManager::NoError != error) {
            return error;
        }

        filters.append(filter);
    }

    result = Functions::and_.apply(filters);

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactUnionFilter &filter,
                                              Cubi::Filter &result)
{
    ValueChain filters;

    foreach(const QContactFilter &childFilter, filter.filters()) {
        Filter filter;
        const QContactManager::Error error = bindFilter(childFilter, filter);

        if (QContactManager::NoError != error) {
            return error;
        }

        filters.append(filter);
    }

    result = Functions::or_.apply(filters);

    return QContactManager::NoError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactChangeLogFilter &filter,
                                              Filter &result)
{
    Exists exists;
    Variable contentCreated;
    Variable contentLastModified;

    switch(filter.eventType()) {
    case QContactChangeLogFilter::EventAdded:
        exists.addPattern(contact(), nie::contentCreated::resource(), contentCreated);
        exists.setFilter(Functions::greaterThanOrEqual.apply(contentCreated,
                                                             LiteralValue(qVariantFromValue(filter.since()))));
        result = exists;
        return QContactManager::NoError;

    case QContactChangeLogFilter::EventChanged:
        exists.addPattern(contact(), nie::contentLastModified::resource(), contentLastModified);
        exists.setFilter(Functions::greaterThanOrEqual.apply(contentLastModified,
                                                             LiteralValue(qVariantFromValue(filter.since()))));
        result = exists;
        return QContactManager::NoError;

    case QContactChangeLogFilter::EventRemoved:
        break;
    }

    qctWarn(QString::fromLatin1("%1: Unsupported event type: %2").
            arg(filterName(filter.type()), eventName(filter.eventType())));

    return QContactManager::NotSupportedError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactRelationshipFilter &filter,
                                              Filter &result)
{
    if (filter.relationshipType() == QContactRelationship::HasMember) {
        const QContactId relatedContactId = filter.relatedContactId();

        // only HasMember relationships between local groups and local contacts are stored ATM
        if (relatedContactId.managerUri() == m_managerUri) {
            LiteralValue rdfLocalId(qVariantFromValue(relatedContactId.localId()));

            const QContactRelationship::Role relatedContactRole = filter.relatedContactRole();
            ValueChain restrictions;

            // filter for all contacts in a given group?
            if (relatedContactRole == QContactRelationship::First
                || relatedContactRole == QContactRelationship::Either) {
                const Variable rdfGroupContact(QString::fromLatin1("group"));
                const Value rdfGroupContactId = Functions::trackerId.apply(rdfGroupContact);

                Exists exists;
                exists.addPattern(rdfGroupContact, rdf::type::resource(), nco::Contact::resource());
                exists.addPattern(rdfGroupContact, rdf::type::resource(), nco::ContactGroup::resource());
                exists.addPattern(contact(), nco::belongsToGroup::resource(), rdfGroupContact);
                exists.setFilter(Functions::equal.apply(rdfGroupContactId, rdfLocalId));
                restrictions += Filter(exists);
            }

            // filter for all groups a given contact is in?
            if (relatedContactRole == QContactRelationship::Second
                || relatedContactRole == QContactRelationship::Either) {
                const Variable rdfContact(QString::fromLatin1("member"));
                const Value rdfContactId = Functions::trackerId.apply(rdfContact);

                Exists exists;
                // not PersonContact, groups could also be part of other groups
                exists.addPattern(rdfContact, rdf::type::resource(), nco::Contact::resource());
                exists.addPattern(rdfContact, nco::belongsToGroup::resource(), contact());
                exists.setFilter(Functions::equal.apply(rdfContactId, rdfLocalId));
                restrictions += Filter(exists);
            }

            if (not restrictions.isEmpty()) {
                result = Functions::or_.apply(restrictions);
            } else {
                result = Filter();
            }
        } else {
            qctWarn(QString::fromLatin1("Relationships to contacts of the %1 "
                                        "contact manager are not stored here (%2).").
                    arg(relatedContactId.managerUri(), m_managerUri));

            // none at all available for relations to ones from other manager ATM
            Exists exists;
            exists.setFilter(LiteralValue(QVariant(false)));
            result = exists;
        }

        return QContactManager::NoError;
    }

    qctWarn(QString::fromLatin1("%1: Unsupported relationship type: %2").
            arg(filterName(filter.type()), filter.relationshipType()));

    return QContactManager::NotSupportedError;
}

void
QTrackerScalarContactQueryBuilder::bindInvalidFilter(Cubi::Filter &result)
{
    result = Filter(LiteralValue(QVariant(false)));
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindFilter(const QContactFilter &filter, Cubi::Filter &result)
{
    QContactFilter canonicalFilter(QContactManagerEngine::canonicalizedFilter(filter));

    switch(canonicalFilter.type()) {
    case QContactFilter::LocalIdFilter:
        return bindFilter(static_cast<const QContactLocalIdFilter&>(canonicalFilter), result);
    case QContactFilter::IntersectionFilter:
        return bindFilter(static_cast<const QContactIntersectionFilter&>(canonicalFilter), result);
    case QContactFilter::UnionFilter:
        return bindFilter(static_cast<const QContactUnionFilter&>(canonicalFilter), result);
    case QContactFilter::ContactDetailFilter:
        return bindFilter(static_cast<const QContactDetailFilter&>(canonicalFilter), result);
    case QContactFilter::ContactDetailRangeFilter:
        return bindFilter(static_cast<const QContactDetailRangeFilter&>(canonicalFilter), result);
    case QContactFilter::ChangeLogFilter:
        return bindFilter(static_cast<const QContactChangeLogFilter&>(canonicalFilter), result);
    case QContactFilter::RelationshipFilter:
        return bindFilter(static_cast<const QContactRelationshipFilter&>(canonicalFilter), result);
    case QContactFilter::DefaultFilter:
        // We didn't apply any explicit restrictions, but the purpose of this filter is to
        // apply no restrictions at all. Therefore all restrictions implied by this filter
        // have been applied and we return true.
        return QContactManager::NoError;
    case QContactFilter::InvalidFilter:
        bindInvalidFilter(result);
        return QContactManager::NoError;

    case QContactFilter::ActionFilter:
        break;
    }

    qctWarn(QString::fromLatin1("%1: Unsupported filter type").arg(filterName(filter.type())));

    return QContactManager::NotSupportedError;
}

QContactManager::Error
QTrackerScalarContactQueryBuilder::bindSortOrders(const QList<QContactSortOrder> &orders,
                                                  QList<Cubi::OrderComparator> &result)
{
    foreach (const QContactSortOrder &o, orders) {
        if (o.blankPolicy() != QContactSortOrder::BlanksFirst) {
            qctWarn("Only BlanksFirst policy is supported. Falling back to in memory sorting");
            return QContactManager::NotSupportedError;
        }

        if (o.caseSensitivity() != Qt::CaseInsensitive) {
            qctWarn("Only case insensitive sorting is supported. Falling back to in memory sorting");
            return QContactManager::NotSupportedError;
        }

        Select orderSelect;

        const Variable object;

        orderSelect.addProjection(object);

        if (schema().isSyntheticDetail(o.detailDefinitionName())) {
            qctWarn(QString::fromLatin1("Sorting on synthesized detail %1 is not supported."
                                        "Falling back to in memory sorting").
                    arg(o.detailDefinitionName()));
            return QContactManager::NotSupportedError;
        }

        const QTrackerContactDetail *detail = schema().detail(o.detailDefinitionName());

        if (detail == 0) {
            orderSelect.addRestriction(createPatternForCustomDetail(contact(),
                                                                    o.detailDefinitionName(),
                                                                    o.detailFieldName(),
                                                                    object));
        } else {
            const Variable subject = detail->hasContext() ? context() : contact();
            const QTrackerContactDetailField *field = findField(*detail, o.detailFieldName());

            if (field == 0) {
                qctWarn(QString::fromLatin1("Sorting on field %1 of detail %2 is not supported: it "
                                            "is not in the schema. Falling back to in memory sorting").
                        arg(o.detailFieldName(), o.detailDefinitionName()));
                return QContactManager::NotSupportedError;
            }

            if (field->isSynthesized()) {
                qctWarn(QString::fromLatin1("Sorting on field %1 of detail %2 is not supported: it "
                                            "is synthesized. Falling back to in memory sorting").
                        arg(o.detailFieldName(), o.detailDefinitionName()));
                return QContactManager::NotSupportedError;
            }

            if (field->hasSubTypes()) {
                qctWarn(QString::fromLatin1("Sorting on field %1 of detail %2 is not supported: it "
                                            "is a subtype field. Falling back to in memory sorting").
                        arg(o.detailFieldName(), o.detailDefinitionName()));
                return QContactManager::NotSupportedError;
            }

            const PropertyInfoList &chain = field->propertyChain();
            const Variable last = (field->isWithoutMapping() ? Variable() : object);

            orderSelect.addRestriction(createPatternForPredicateChain(subject, chain, last));

            if (field->isWithoutMapping()) {
                orderSelect.addRestriction(createPatternForCustomField(last,
                                                                       o.detailFieldName(),
                                                                       object));
            }
        }

        orderSelect.setLimit(1);

        result.append(OrderComparator(Filter(orderSelect),
                                      o.direction() == Qt::AscendingOrder ? OrderComparator::Ascending
                                                                          : OrderComparator::Descending));
    }

    return QContactManager::NoError;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const QTrackerContactDetailField *
QTrackerScalarContactQueryBuilder::findField(const QTrackerContactDetail &detail,
                                             const QString &fieldName)
{
    if (fieldName.isEmpty()) {
        return 0;
    }

    const QTrackerContactDetailField *field = detail.field(fieldName);

    if (0 == field) {
        qctWarn(QString::fromLatin1("Unsupported field %2 for %1 detail").
                arg(detail.name(), fieldName));
    }

    return field;
}
