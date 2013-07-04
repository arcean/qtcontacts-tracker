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

#include "contactdetail_p.h"
#include "contactdetailfield.h"
#include "support.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetail::QTrackerContactDetail(const QString &name)
    : d(new QTrackerContactDetailData(name))
{
}

QTrackerContactDetail::QTrackerContactDetail(QTrackerContactDetailData *data)
    : d(data)
{
}

QTrackerContactDetail::~QTrackerContactDetail()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QTrackerContactDetail::QTrackerContactDetail(const QTrackerContactDetail &other)
    : d(other.d)
{
}

QTrackerContactDetail &
QTrackerContactDetail::operator=(const QTrackerContactDetail &other)
{
    return d = other.d, *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const QString &
QTrackerContactDetail::name() const
{
    return d->m_name;
}

void
QTrackerContactDetail::setHasContext(bool flag)
{
    d->m_hasContext = flag;
}

bool
QTrackerContactDetail::hasContext() const
{
    return d->m_hasContext;
}

void
QTrackerContactDetail::setUnique(bool flag)
{
    d->m_isUnique = flag;
}

bool
QTrackerContactDetail::isUnique() const
{
    return d->m_isUnique;
}

void
QTrackerContactDetail::setInternal(bool flag)
{
    d->m_isInternal = flag;
}

bool
QTrackerContactDetail::isInternal() const
{
    return d->m_isInternal;
}

void
QTrackerContactDetail::addField(const QTrackerContactDetailField &field)
{
    d->m_fields.append(field);
    d->m_chainsDirty = true;
}

const QList<QTrackerContactDetailField> &
QTrackerContactDetail::fields() const
{
    return d->m_fields;
}

bool
QTrackerContactDetailData::isPossessedChain(const PropertyInfoList &chain) const
{
    foreach(const PropertyInfoList &foreignKeyChain, m_foreignKeyChains) {
        if (chain.startsWith(foreignKeyChain)) {
            return false;
        }
    }

    return true;
}

static PropertyInfoList::ConstIterator
endOfPredicateChain(const PropertyInfoList &chain)
{
    PropertyInfoList::ConstIterator pi = chain.constBegin();

    for(; pi != chain.constEnd(); ++pi) {
        if (pi->isInverse()) {
            break;
        }
    }

    return pi - 1;
}

void
QTrackerContactDetailData::computePredicateChains()
{
    m_predicateChains.clear();
    m_foreignKeyChains.clear();
    m_possessedChains.clear();

    // Collect predicate chains from detail fields.
    // Also identify chains that lead to foreign keys.
    foreach(const QTrackerContactDetailField &field, m_fields) {
        if (not field.hasPropertyChain()) {
            continue;
        }

        PropertyInfoList fieldPredicates(field.propertyChain().constBegin(),
                                         endOfPredicateChain(field.propertyChain()));

        if (not fieldPredicates.isEmpty()) {
            if (field.isForeignKey()) {
                m_foreignKeyChains += fieldPredicates;
            }

            m_predicateChains += fieldPredicates;
        }
    }

    // Figure out which predicate chains are fully possessed,
    // e.g. don't lead to some shared resources identified by foreign keys.
    foreach(const PropertyInfoList &chain, m_predicateChains) {
        if (isPossessedChain(chain)) {
            m_possessedChains += chain;
        }
    }

    m_chainsDirty = false;
}

const QSet<PropertyInfoList> &
QTrackerContactDetail::predicateChains() const
{
    if (d->m_chainsDirty) {
        d->computePredicateChains();
    }

    return d->m_predicateChains;
}

const QSet<PropertyInfoList> &
QTrackerContactDetail::foreignKeyChains() const
{
    if (d->m_chainsDirty) {
        d->computePredicateChains();
    }

    return d->m_foreignKeyChains;
}

const QSet<PropertyInfoList> &
QTrackerContactDetail::possessedChains() const
{
    if (d->m_chainsDirty) {
        d->computePredicateChains();
    }

    return d->m_possessedChains;
}

const QSet<PropertyInfoList> &
QTrackerContactDetail::customValueChains() const
{
    if (d->m_customValueChains.isEmpty()) {
        foreach(const QTrackerContactDetailField &field, fields()) {
            if (not field.permitsCustomValues() || not field.hasPropertyChain()) {
                // skip fields that don't permit custom values
                continue;
            }

            PropertyInfoList::ConstIterator begin = field.propertyChain().constBegin();
            PropertyInfoList::ConstIterator end = field.propertyChain().constEnd();

            if (not field.hasSubTypeClasses() && not field.isWithoutMapping()) {
                // find relevant end of the property chain
                end -= 1;
            }

            // build and store predicate chain
            PropertyInfoList fieldPredicates(begin, end);

            if (not fieldPredicates.isEmpty()) {
                d->m_customValueChains += fieldPredicates;
            }
        }
    }

    return d->m_customValueChains;
}

void
QTrackerContactDetail::addDependency(const QString &detail)
{
    d->m_dependencies.insert(detail);
}

const QSet<QString> &
QTrackerContactDetail::dependencies() const
{
    return d->m_dependencies;
}

const QTrackerContactDetailField *
QTrackerContactDetail::field(const QString &name) const
{
    foreach(const QTrackerContactDetailField &f, fields()) {
        if (f.name() == name) {
            return &f;
        }
    }

    if (QContactDetail::FieldDetailUri == name) {
        return detailUriField();
    }

    return 0;
}

const QTrackerContactDetailField *
QTrackerContactDetail::resourceIriField() const
{
    foreach(const QTrackerContactDetailField &f, fields()) {
        if (f.hasDetailUri()) {
            return &f;
        }
    }

    return 0;
}

const QTrackerContactDetailField *
QTrackerContactDetail::detailUriField() const
{
    if (d->m_detailUriField.isNull()) {
        // collect RDF properties which lead to the detail's URI
        const QTrackerContactDetailField *const subject = resourceIriField();

        if (subject) {
            PropertyInfoList::ConstIterator i = subject->propertyChain().constBegin();
            PropertyInfoList::ConstIterator end = subject->propertyChain().constEnd();
            PropertyInfoList uriPropertyChain;

            if (subject->propertyChain().count() > 1) {
                --end;
            }

            while (i != end) {
                uriPropertyChain.append(*i++);
            }

            // synthesize the detail URI field
            QScopedPointer<QTrackerContactDetailField> field
                    (new QTrackerContactDetailField(QContactDetail::FieldDetailUri));

            field->setPropertyChain(uriPropertyChain);
            field->setDataType(QVariant::Url);

            d->m_detailUriField.reset(field.take());
        }
    }

    return d->m_detailUriField.data();
}

const QTrackerContactDetailField *
QTrackerContactDetail::subTypeField() const
{
    foreach(const QTrackerContactDetailField &f, fields()) {
        if (f.hasSubTypes()) {
            return &f;
        }
    }

    return 0;
}

void
QTrackerContactDetail::setDetailUriScheme(QTrackerContactSubject::Scheme scheme)
{
    d->m_detailUriScheme = scheme;
}

QTrackerContactSubject::Scheme
QTrackerContactDetail::detailUriScheme() const
{
    if (QTrackerContactSubject::None != d->m_detailUriScheme) {
        return d->m_detailUriScheme;
    }

    return resourceIriScheme();
}

QTrackerContactSubject::Scheme
QTrackerContactDetail::resourceIriScheme() const
{
    foreach(const QTrackerContactDetailField &f, fields()) {
        PropertyInfoList::ConstIterator detailUriProperty = f.detailUriProperty();

        if (detailUriProperty != f.propertyChain().constEnd()) {
            return detailUriProperty->resourceIriScheme();
        }
    }

    return QTrackerContactSubject::None;
}

bool
QTrackerContactDetail::hasDetailUri() const
{
    foreach(const QTrackerContactDetailField &f, fields()) {
        if (f.hasDetailUri()) {
            return true;
        }
    }

    return false;
}

QContactDetailDefinition
QTrackerContactDetail::describe() const
{
    QContactDetailDefinition schema;

    schema.setName(name());
    schema.setUnique(isUnique());

    if (hasContext()) {
        static const QString contextHome = QContactDetail::ContextHome;
        static const QString contextWork = QContactDetail::ContextWork;

        QContactDetailFieldDefinition f;
        f.setDataType(QVariant::StringList);
        f.setAllowableValues(QVariantList() << contextHome << contextWork);
        schema.insertField(QContactDetail::FieldContext, f);
    }

    foreach(const QTrackerContactDetailField &f, fields()) {
        schema.insertField(f.name(), f.describe());
    }

    return schema;
}

const QTrackerContactDetail *
QTrackerContactDetail::findImplementation(const QTrackerContactDetailSchema &schema,
                                          const QHash<QString, QContactDetail> &otherDetails,
                                          const QContactDetail &actualDetail) const
{
    const QTrackerContactDetail *const implementation =
            d->findImplementation(schema, otherDetails, actualDetail);
    return implementation ? implementation : this;
}

QList<const QTrackerContactDetail *>
QTrackerContactDetail::implementations(const QTrackerContactDetailSchema &schema) const
{
    QList<const QTrackerContactDetail *> implementations = d->implementations(schema);

    if (implementations.isEmpty()) {
        implementations << this;
    }

    return implementations;
}
