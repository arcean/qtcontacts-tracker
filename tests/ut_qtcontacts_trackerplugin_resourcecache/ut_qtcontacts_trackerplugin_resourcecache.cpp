/*********************************************************************************
 ** This file is part of QtContacts tracker storage plugin
 **
 ** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#include "ut_qtcontacts_trackerplugin_resourcecache.h"

#include <lib/resourcecache.h>
#include <lib/sparqlresolver.h>
#include <ontologies.h>

CUBI_USE_NAMESPACE_RESOURCES

ut_qtcontacts_trackerplugin_resourcecache::ut_qtcontacts_trackerplugin_resourcecache(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(QLatin1String(DATADIR)),
                                         QDir(QLatin1String(SRCDIR)), parent)
{
}

void
ut_qtcontacts_trackerplugin_resourcecache::testTrackerIdResolver_data()
{
    QTest::addColumn<QStringList>("iriList");

    QTest::newRow("empty") << (QStringList());
    QTest::newRow("one") << (QStringList() << nco::default_contact_me::iri());
}

void
ut_qtcontacts_trackerplugin_resourcecache::testTrackerIdResolver()
{
    QFETCH(QStringList, iriList);

    QctResourceCache::instance().clear();

    foreach(const QString &iri, iriList) {
        QVERIFY2(0 == QctResourceCache::instance().trackerId(iri), qPrintable(iri));
    }

    QEventLoop loop;

    QctTrackerIdResolver resolver(iriList);
    connect(&resolver, SIGNAL(finished()), &loop, SLOT(quit()));
    QSignalSpy spy(&resolver, SIGNAL(finished()));

    QCOMPARE(spy.count(), 0);
    QVERIFY(resolver.lookup());
    QCOMPARE(spy.count(), 0);

    loop.exec();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(resolver.trackerIds().count(), iriList.count());

    foreach(uint id, resolver.trackerIds()) {
        QVERIFY(0 != id);
    }

    for(int i = 0; i < iriList.count(); ++i) {
        QCOMPARE(QctResourceCache::instance().trackerId(iriList.at(i)),
                 resolver.trackerIds().at(i));
    }
}

void
ut_qtcontacts_trackerplugin_resourcecache::testResourceIdResolver()
{
    QctResourceCache::instance().clear();
    QCOMPARE(QctResourceCache::instance().resourceIri(1), QString());

    QEventLoop loop;
    QctResourceIriResolver resolver(QList<uint>() << 1);
    connect(&resolver, SIGNAL(finished()), &loop, SLOT(quit()));
    QSignalSpy spy(&resolver, SIGNAL(finished()));

    QCOMPARE(spy.count(), 0);
    QVERIFY(resolver.lookup());
    QCOMPARE(spy.count(), 0);

    loop.exec();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(resolver.resourceIris().count(), 1);
    QVERIFY(not resolver.resourceIris().first().isEmpty());

    QCOMPARE(QctResourceCache::instance().resourceIri(1),
             resolver.resourceIris().first());
}

void
ut_qtcontacts_trackerplugin_resourcecache::testSchemaIds_data()
{
    QTest::addColumn<QString>("iri");
    QTest::newRow("nco:CellPhoneNumber") << nco::CellPhoneNumber::iri();
    QTest::newRow("nco:default_contact_me") << nco::default_contact_me::iri();
}

void
ut_qtcontacts_trackerplugin_resourcecache::testSchemaIds()
{
    QFETCH(QString, iri);

    // run fake task to be sure the resolver task has finished
    QVERIFY(0 != engine());
    engine()->contactImpl(1, NoFetchHint, 0);

    QVERIFY(0 != QctResourceCache::instance().trackerId(iri));
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin_resourcecache)
