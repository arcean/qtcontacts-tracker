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

#include "support.h"

#include <QtGui/QDesktopServices>

bool
qctEquals(const QStringList &a, const QStringList &b, Qt::CaseSensitivity cs)
{
    if (a.length() != b.length()) {
        return false;
    }

    for(int i = 0; i < a.length(); ++i) {
        if (not qctEquals(a[i], b[i], cs)) {
            return false;
        }
    }

    return true;
}

bool
qctEquals(const QVariant &a, const QVariant &b, Qt::CaseSensitivity cs)
{
    if (a.type() != b.type()) {
        return false;
    }

    switch(a.type()) {
    case QVariant::Double:
        return qctEquals(a.toDouble(), b.toDouble());
    case QVariant::String:
        return qctEquals(a.toString(), b.toString(), cs);
    case QVariant::StringList:
        return qctEquals(a.toStringList(), b.toStringList(), cs);
    default:
        break;
    }

    return a == b;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QString
qctUuidString(const QUuid &uuid)
{
    const QString uuidString = uuid.toString();
    return uuidString.mid(1, uuidString.length() - 2);
}

QString
qctCamelCase(const QString &text)
{
    const QString trimmedText = text.trimmed();

    for(int i = 0; i < trimmedText.length(); ++i) {
        if (trimmedText[i].isSpace()) {
            return trimmedText; // don't convert text with inline whitespace
        }
    }

    if (trimmedText.isEmpty()) {
        return trimmedText;
    }

    QString camelCaseString;
    camelCaseString.reserve(trimmedText.length());
    camelCaseString += trimmedText[0].toUpper();
    bool previousWasLower = false;

    for(int i = 1; i < trimmedText.length(); ++i) {
        const QChar ch = trimmedText[i];

        if (not ch.isUpper()) {
            camelCaseString += ch;
            previousWasLower = true;
        } else if (previousWasLower) {
            camelCaseString += ch;
            previousWasLower = false;
        } else {
            camelCaseString += ch.toLower();
            previousWasLower = false;
        }
    }

    return camelCaseString;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctIriAlias
{
public:
    QctIriAlias(const char *prefix, const char *alias)
        : m_prefix(QLatin1String(prefix))
        , m_alias(QLatin1String(alias) + QLatin1Char(':'))
        , m_queryPattern(QLatin1Char('<') + m_prefix + QLatin1String("([^>]+)>"))
        , m_queryReplacement(m_alias + QLatin1String("\\1"))
    {
    }

public: // methods
    QString & reduce(QString &query) const
    {
        return query.replace(m_queryPattern, m_queryReplacement);
    }

    bool replace(const QString &iri, QString &alias) const
    {
        if (iri.startsWith(m_prefix)) {
            alias = m_alias + iri.mid(m_prefix.length());
            return true;
        }

        return false;
    }

public: // fields
    static const QList<QctIriAlias> Registry;

private: // fields
    QString m_prefix;
    QString m_alias;
    QRegExp m_queryPattern;
    QString m_queryReplacement;
};


const QList<QctIriAlias>
QctIriAlias::Registry = QList<QctIriAlias>()
        << QctIriAlias("http://maemo.org/ontologies/tracker#", "maemo")
        << QctIriAlias("http://www.semanticdesktop.org/ontologies/2007/08/15/nao#", "nao")
        << QctIriAlias("http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#", "ncal")
        << QctIriAlias("http://www.semanticdesktop.org/ontologies/2007/01/19/nie#", "nie")
        << QctIriAlias("http://www.semanticdesktop.org/ontologies/2007/03/22/nco#", "nco")
        << QctIriAlias("http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#", "nfo")
        << QctIriAlias("http://www.tracker-project.org/ontologies/tracker#", "tracker")
        << QctIriAlias("http://www.tracker-project.org/temp/mlo#", "mlo")
        << QctIriAlias("http://www.tracker-project.org/temp/slo#", "slo")
        << QctIriAlias("http://www.w3.org/1999/02/22-rdf-syntax-ns#", "rdf")
        << QctIriAlias("http://www.w3.org/2000/01/rdf-schema#", "rdfs")
        << QctIriAlias("http://www.w3.org/2001/XMLSchema#", "xsd")
        << QctIriAlias("http://www.w3.org/2005/xpath-functions#", "fn");


QStringList
qctReduceIris(const QStringList &iriList)
{
    QStringList result;

    result.reserve(iriList.length());

    foreach(const QString &iri, iriList) {
        result += qctIriAlias(iri);
    }

    return result;
}

QString
qctReduceIris(const QString &query)
{
    QString reducedQuery = query;

    foreach(const QctIriAlias &alias, QctIriAlias::Registry) {
        reducedQuery = alias.reduce(reducedQuery);
    }

    return reducedQuery;
}

QString
qctIriAlias(const QString &iri)
{
    foreach(const QctIriAlias &alias, QctIriAlias::Registry) {
        QString result;

        if (alias.replace(iri, result)) {
            return result;
        }
    }

    return iri;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Cubi::Value
qctMakeCubiValue(const QVariant &variant)
{
    if (variant.type() == QVariant::Url) {
        return Cubi::ResourceValue(variant.toUrl());
    }

    if (variant.userType() == qMetaTypeId<Cubi::ResourceValue>()) {
        return variant.value<Cubi::ResourceValue>();
    }

    return Cubi::LiteralValue(variant);
}
