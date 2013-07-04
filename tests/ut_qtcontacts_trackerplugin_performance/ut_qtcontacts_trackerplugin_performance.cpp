/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2009-2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "ut_qtcontacts_trackerplugin_performance.h"
#include "resourcecleanser.h"

#include <lib/sparqlresolver.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

ut_qtcontacts_trackerplugin_performance::ut_qtcontacts_trackerplugin_performance(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static QStringList
pickIris(const QStringList &classIris, int count)
{
    const int last = classIris.count() - 1;
    QStringList iris;
    qsrand(42);

    for(int i = 0; i < count; ++i) {
        iris += classIris[last * double(qrand()) / RAND_MAX];
    }

    return iris;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void
ut_qtcontacts_trackerplugin_performance::initTestCase()
{
    static const QString query = QLatin1String("SELECT ?c { ?c a rdfs:Class }");
    QScopedPointer<QSparqlResult> result(executeQuery(query, QSparqlQuery::SelectStatement));

    QVERIFY(not result.isNull());

    while(result->next()) {
        m_classIris += result->current().value(0).toString();
    }

    QVERIFY(not m_classIris.isEmpty());
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdResolver_data()
{
    QTest::addColumn<int>("iriCount");

    QTest::newRow("1") << 1;
    QTest::newRow("5") << 5;
    QTest::newRow("15") << 15;
    QTest::newRow("100") << 100;
    QTest::newRow("500") << 500;
    QTest::newRow("all classes") << m_classIris.count();
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdResolver()
{
    QFETCH(int, iriCount);

    const QStringList &samples = pickIris(m_classIris, iriCount);

    QBENCHMARK {
        QctTrackerIdResolver resolver(samples);
        QVERIFY(resolver.lookupAndWait());

        QVERIFY(resolver.isFinished());
        QCOMPARE(resolver.trackerIds().count(), iriCount);
    }
}

template<class TableType, class IriType> static void
testTrackerIdTable(const QStringList &iris)
{
    QList<IriType> samples;
    TableType idCache;
    uint lastId = 1;

    foreach(const QString &iri, pickIris(iris, iris.count())) {
        idCache.insert(IriType(iri), lastId++);
        samples += IriType(iri);
    }

    QBENCHMARK {
        foreach(const IriType &iri, samples) {
            QVERIFY(0 != idCache[iri]);
        }
    }
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdStringMap()
{
    typedef QMap<QString, uint> TrackerIdTable;
    testTrackerIdTable<TrackerIdTable, QString>(m_classIris);
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdStringHash()
{
    typedef QHash<QString, uint> TrackerIdTable;
    testTrackerIdTable<TrackerIdTable, QString>(m_classIris);
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdUrlMap()
{
    typedef QMap<QUrl, uint> TrackerIdTable;
    testTrackerIdTable<TrackerIdTable, QUrl>(m_classIris);
}

void
ut_qtcontacts_trackerplugin_performance::testTrackerIdUrlHash()
{
    typedef QHash<QUrl, uint> TrackerIdTable;
    testTrackerIdTable<TrackerIdTable, QUrl>(m_classIris);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_performance)

