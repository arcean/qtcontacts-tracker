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

#ifndef QTRACKERSUPPORT_H
#define QTRACKERSUPPORT_H

#include <QContactDetail>
#include <QtCore>

#include <cubi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

// Everytime a QContactDetail is created or one of its fields is accessed, a QContactStringHolder
// instance is created for the definition name and the field name. It creates a persistent cache
// to permit pointer based comparisons and conversions between QString and QLatin1Constant.
// It also allocates memory on heap since it uses a QHash behind the scenes. To avoid having
// QHash allocate nodes in random places within the heap (which would cause bad fragmentation),
// we create QContactDetails for all the strings we know in the schema, which forces
// QContactStringHolder to allocate all of its cache now. Note that this approach is not bullet
// proof: We cannot warm the cache for custom details which we don't know in advance.
static inline QString qctInternString(const QString &s)
{
    return QTM_PREPEND_NAMESPACE(QContactDetail(s).definitionName());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QString qctUuidString(const QUuid &uuid = QUuid::createUuid());
QString qctCamelCase(const QString &text);

////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
class QctSingleton
{
public: // attributes
    static T* instance() { static T instance; return &instance; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

bool qctEquals(double a, double b, double epsilon = 1e-5);
bool qctEquals(const QString &a, const QString &b, Qt::CaseSensitivity cs);
bool qctEquals(const QStringList &a, const QStringList &b, Qt::CaseSensitivity cs);
bool qctEquals(const QVariant &a, const QVariant &b, Qt::CaseSensitivity cs);

////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool
qctEquals(double a, double b, double epsilon)
{
    return (qAbs(a - b) < epsilon);
}

inline bool
qctEquals(const QString &a, const QString &b, Qt::CaseSensitivity cs)
{
    return (0 == QString::compare(a, b, cs));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> inline void
qctPropagate(const T &value, T *target)
{
    if (0 != target) {
        *target = value;
    }
}

template<typename K, typename V> inline void
qctPropagate(const V &value, QMap<K, V> *target, const K &key)
{
    if (0 != target) {
        target->insert(key, value);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

QStringList qctReduceIris(const QStringList &iriList);
QString qctReduceIris(const QString &query);
QString qctIriAlias(const QString &iri);

////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
inline uint qHash(const QList<T> &list)
{
    return not list.isEmpty() ? qHash(list.first()) : 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Cubi::Value qctMakeCubiValue(const QVariant &variant);

////////////////////////////////////////////////////////////////////////////////////////////////////

Q_DECLARE_METATYPE(Cubi::ResourceValue)

#endif // QTRACKERSUPPORT_H

