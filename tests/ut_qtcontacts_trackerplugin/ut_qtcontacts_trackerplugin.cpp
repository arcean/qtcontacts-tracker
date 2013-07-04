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

#include "ut_qtcontacts_trackerplugin.h"
#include "slots.h"

#include <dao/contactdetail.h>
#include <dao/contactdetailschema.h>
#include <dao/conversion.h>
#include <dao/support.h>

#include <engine/engine.h>
#include <engine/contactidfetchrequest.h>
#include <engine/relationshipsaverequest.h>
#include <engine/relationshipfetchrequest.h>

#include <lib/constants.h>
#include <lib/contactmergerequest.h>
#include <lib/customdetails.h>
#include <lib/phoneutils.h>
#include <lib/contactlocalidfetchrequest.h>
#include <lib/settings.h>
#include <lib/sparqlresolver.h>
#include <lib/trackerchangelistener.h>
#include <lib/unmergeimcontactsrequest.h>

#include <ontologies/nco.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

CUBI_USE_NAMESPACE
CUBI_USE_NAMESPACE_RESOURCES

////////////////////////////////////////////////////////////////////////////////////////////////////

#define A0 "\331\240"
#define A1 "\331\241"
#define A2 "\331\242"
#define A3 "\331\243"
#define A4 "\331\244"
#define A5 "\331\245"
#define A6 "\331\246"
#define A7 "\331\247"
#define A8 "\331\250"
#define A9 "\331\251"

#define LRE "\342\200\252"
#define RLE "\342\200\253"
#define PDF "\342\200\254"

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef QHash<QString, QSet<QString> > ImplictTypeHash;

////////////////////////////////////////////////////////////////////////////////////////////////////

static ContactDetailSample
makeDetailSample(const QContactDetail &detail, const QString &value)
{
    return qMakePair(detail, value);
}

static ContactDetailSample
makeDetailSample(const QString &detailName, const QString &fieldName, const QString &value)
{
    QContactDetail detail(detailName);
    detail.setValue(fieldName, value);
    return qMakePair(detail, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static QStringList
sortedStringList(QStringList list)
{
    return list.sort(), list;
}

static QStringList
impliedTypes(const QStringList &subTypes, const ImplictTypeHash &implicitTypes)
{
    QSet<QString> result = subTypes.toSet();

    foreach(const QString &type, subTypes) {
        ImplictTypeHash::ConstIterator it = implicitTypes.find(type);

        if (it == implicitTypes.constEnd()) {
            qWarning() << "unknown subtype:" << type;
            continue;
        }

        result += it.value();
    }

    return sortedStringList(result.toList());
}

static QStringList
impliedPhoneNumberTypes(const QStringList &subTypes)
{
    typedef QHash<QString, QSet<QString> > ImplictTypeHash;
    static ImplictTypeHash implicitTypes;

    if (implicitTypes.isEmpty()) {
        implicitTypes.insert(QContactPhoneNumber::SubTypeAssistant, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeVoice);
        implicitTypes.insert(QContactPhoneNumber::SubTypeBulletinBoardSystem, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeModem);
        implicitTypes.insert(QContactPhoneNumber::SubTypeCar, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeVoice);
        implicitTypes.insert(QContactPhoneNumber::SubTypeFax, QSet<QString>());
        implicitTypes.insert(QContactPhoneNumber::SubTypeMessagingCapable, QSet<QString>());
        implicitTypes.insert(QContactPhoneNumber::SubTypeMobile, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeMessagingCapable <<
                             QContactPhoneNumber::SubTypeVoice);
        implicitTypes.insert(QContactPhoneNumber::SubTypeModem, QSet<QString>());
        implicitTypes.insert(QContactPhoneNumber::SubTypePager, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeMessagingCapable);
        implicitTypes.insert(QContactPhoneNumber::SubTypeVideo, QSet<QString>() <<
                             QContactPhoneNumber::SubTypeVoice);
        implicitTypes.insert(QContactPhoneNumber::SubTypeVoice, QSet<QString>());
    }

    return impliedTypes(subTypes, implicitTypes);
}

static QStringList
impliedSubTypes(const QString &definitionName, const QStringList &values)
{
    if (QContactPhoneNumber::DefinitionName == definitionName) {
        return impliedPhoneNumberTypes(values);
    }

    return values;
}


static bool
detailMatches(const QContactDetail &reference, const QContactDetail &sample)
{
    const QVariantMap fields = reference.variantValues();
    QVariantMap::ConstIterator it;

    for(it = fields.constBegin(); it != fields.constEnd(); ++it) {
        Qt::CaseSensitivity cs = Qt::CaseSensitive;

        QVariant sampleValue = sample.variantValue(it.key());
        QVariant referenceValue = it.value();

        if (referenceValue.toString().isEmpty() && sampleValue.isNull()) {
            continue;
        }

        if (QVariant::Date == referenceValue.type() ||
            QVariant::DateTime == referenceValue.type()) {
            referenceValue.setValue(referenceValue.toDate().toString(Qt::ISODate));
            sampleValue.setValue(sampleValue.toDate().toString(Qt::ISODate));
        }

        if (QVariant::StringList == referenceValue.type() &&
            QVariant::String == sampleValue.type() &&
            1 == referenceValue.toStringList().length()) {
            referenceValue = referenceValue.toStringList().first();
        }

        if (QContactAddress::DefinitionName == reference.definitionName() &&
            QContactAddress::FieldSubTypes == it.key()) {
            referenceValue.setValue(sortedStringList(referenceValue.toStringList()));
            sampleValue.setValue(sortedStringList(sampleValue.toStringList()));
        }

        if (QContactGender::DefinitionName == reference.definitionName()) {
            cs = Qt::CaseInsensitive;
        }

        if (QContactUrl::DefinitionName == reference.definitionName() &&
            QContactUrl::FieldUrl == it.key()) {
            referenceValue.setValue(QUrl(referenceValue.toString()));
            sampleValue.setValue(QUrl(sampleValue.toString()));
        }

        if (QVariant::StringList == referenceValue.type() && QLatin1String("SubTypes") == it.key()) {
            // ignore value order for SubTypes lists
            QStringList referenceList = impliedSubTypes(reference.definitionName(),
                                                        referenceValue.toStringList());
            referenceValue = (qSort(referenceList), referenceList);
            QStringList sampleList = sampleValue.toStringList();
            sampleValue = (qSort(sampleList), sampleList);
        }

        if (not qctEquals(sampleValue, referenceValue, cs)) {
            return false;
        }
    }

    return true;
}

static QStringList
getFuzzableDetailDefinitionNamesForPredicate(const QContactDetailDefinitionMap &definitions,
                                             bool (*predicate)(const QContactDetailDefinition &))
{
    QStringList names;

    foreach(const QContactDetailDefinition &def, definitions) {
        if (predicate(def) == true) {
            names.append(def.name());
        }
    }

    return names;
}

static bool
isFuzzableDetail(const QContactDetailDefinition &def)
{
    return (QContactDisplayLabel::DefinitionName != def.name() &&
            QContactGlobalPresence::DefinitionName != def.name() &&
            QContactGuid::DefinitionName != def.name() &&
            QContactPresence::DefinitionName != def.name() &&
            QContactThumbnail::DefinitionName != def.name() &&
            QContactType::DefinitionName != def.name());
}

static QStringList
getFuzzableDetailDefinitionNamesForType(const QContactDetailDefinitionMap &definitions)
{
    return getFuzzableDetailDefinitionNamesForPredicate(definitions, isFuzzableDetail);
}

static bool
isSortableDetail(const QContactDetailDefinition &def)
{
    return (isFuzzableDetail(def) &&
            QContactRelevance::DefinitionName != def.name());
}

static QStringList
getSortableDetailDefinitionNamesForType(const QContactDetailDefinitionMap &definitions)
{
    return getFuzzableDetailDefinitionNamesForPredicate(definitions, isSortableDetail);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ut_qtcontacts_trackerplugin::ut_qtcontacts_trackerplugin(QObject *parent)
    : ut_qtcontacts_trackerplugin_common(QDir(DATADIR), QDir(SRCDIR), parent)
{
}

void
ut_qtcontacts_trackerplugin::initTestCase()
{
    QScopedPointer<QSparqlResult> result
            (executeQuery(loadReferenceFile("test-account-1.rq"),
                          QSparqlQuery::InsertStatement));

    QVERIFY(not result.isNull());
}

void
ut_qtcontacts_trackerplugin::testContacts()
{
    QContact c1, c2;

    QContactManager::Error error;

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c1, &error));
    QCOMPARE(error, QContactManager::NoError);

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c2, &error));
    QCOMPARE(error, QContactManager::NoError);

    error = QContactManager::UnspecifiedError;

    QContactFilter filter;
    QList<QContactLocalId> contacts(engine()->contactIds(filter, NoSortOrders, &error));
    QCOMPARE(error, QContactManager::NoError);

    QVERIFY2(contacts.contains(c1.localId()), "Previously added contact is not found");
    QVERIFY2(contacts.contains(c2.localId()), "Previously added contact is not found");
}

void
ut_qtcontacts_trackerplugin::testContact()
{
    // Test invalid contact id
    QContactManager::Error error;

    error = QContactManager::UnspecifiedError;
    QContact invalidContact = engine()->contactImpl( -1, NoFetchHint, &error);
    QVERIFY(error != QContactManager::NoError);

    // Add a contact
    QContact newContact;
    const QContactLocalId oldid = newContact.localId();
    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&newContact, &error));
    QCOMPARE(error, QContactManager::NoError);

    QContactLocalId id = newContact.localId();
    QVERIFY(id != oldid);

    // Find the added contact
    error = QContactManager::UnspecifiedError;
    QContact c = engine()->contactImpl(id, NoFetchHint, &error);
    QCOMPARE(c.localId(), newContact.localId());
    QCOMPARE(error, QContactManager::NoError);
}

void
ut_qtcontacts_trackerplugin::testSaveFullname()
{
    const QString fullname = "A Full Name";

    QList<QContact> contacts = parseVCards(referenceFileName("fullname.vcf"), 1);
    QVERIFY(not contacts.isEmpty());

    QContactName name = contacts.first().detail<QContactName>();
    QCOMPARE(name.customLabel(), fullname);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool success = engine()->saveContact(&contacts.first(), &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != contacts.first().localId());
    QVERIFY(success);

    name = contact(contacts.first().localId()).detail<QContactName>();
    QCOMPARE(name.customLabel(), fullname);

    loadRawTuples("fullname.ttl");
    name = contact("contact:fullname").detail<QContactName>();
    QCOMPARE(name.customLabel(), fullname);
}

void
ut_qtcontacts_trackerplugin::testSaveName()
{
    QContact c;
    QContactLocalId initialId = c.localId();
    int detailsAdded = 0;

    QMap<QString,QString> nameValues;
    QContactName name;

    nameValues.insert(QContactName::FieldPrefix, "Mr");
    nameValues.insert(QContactName::FieldFirstName, "John");
    nameValues.insert(QContactName::FieldMiddleName, "Rupert");
    nameValues.insert(QContactName::FieldLastName, "Doe");
    nameValues.insert(QContactName::FieldSuffix, "III");
    nameValues.insert(QContactName::FieldCustomLabel, "The Duke");

    foreach (QString field, nameValues.keys()) {
        name.setValue(field, nameValues.value(field));
    }
    c.saveDetail(&name);

    QContactNickname nick;
    nick.setValue(QLatin1String(QContactNickname::FieldNickname), "Johnny");
    c.saveDetail(&nick);

    QCOMPARE(c.detail<QContactName>().prefix(), QLatin1String("Mr"));
    QCOMPARE(c.detail<QContactName>().firstName(), QLatin1String("John"));
    QCOMPARE(c.detail<QContactName>().middleName(), QLatin1String("Rupert"));
    QCOMPARE(c.detail<QContactName>().lastName(), QLatin1String("Doe"));
    QCOMPARE(c.detail<QContactName>().suffix(), QLatin1String("III"));
    QCOMPARE(c.detail<QContactName>().customLabel(), QLatin1String("The Duke"));
    QCOMPARE(c.detail<QContactNickname>().nickname(), QLatin1String("Johnny"));

    detailsAdded++;

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(c.localId() != initialId);
    QContact contact = this->contact(c.localId());
    QList<QContactName> details = contact.details<QContactName>();
    QList<QContactNickname> details2 = contact.details<QContactNickname>();
    QCOMPARE(details.count(), detailsAdded);
    QCOMPARE(details2.count(), detailsAdded);
    // Name is unique
    foreach(QString field, nameValues.keys()) {
        QCOMPARE(details.first().value(field), nameValues.value(field));
    }
    QCOMPARE(details2.first().value(QLatin1String(QContactNickname::FieldNickname)), QString("Johnny"));

    // Try changing the name of the saved contact.
    {
        QMap<QString,QString> nameValues;
        QContactName name = c.detail<QContactName>();
        nameValues.insert(QLatin1String(QContactName::FieldPrefix), "Mr2");
        nameValues.insert(QLatin1String(QContactName::FieldFirstName), "John2");
        nameValues.insert(QLatin1String(QContactName::FieldMiddleName), "Rupert2");
        nameValues.insert(QLatin1String(QContactName::FieldLastName), "");
        //    nameValues.insert(QContactName::FieldSuffix, "III");

        foreach (QString field, nameValues.keys()) {
            name.setValue(field, nameValues.value(field));
        }
        c.saveDetail(&name);

        QContactNickname nick = c.detail<QContactNickname>();
        nick.setValue(QLatin1String(QContactNickname::FieldNickname), "Johnny2");
        c.saveDetail(&nick);


        error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(c.localId() != initialId);

        error = QContactManager::UnspecifiedError;
        QContact contact = engine()->contactImpl(c.localId(), NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QList<QContactName> details = contact.details<QContactName>();
        QList<QContactNickname> details2 = contact.details<QContactNickname>();
        QCOMPARE(details.count(), detailsAdded);
        QCOMPARE(details2.count(), detailsAdded);

        // Name is unique
        foreach(QString field, nameValues.keys()) {
            QCOMPARE(details.at(0).value(field), nameValues.value(field));
        }

        QCOMPARE(details2.at(0).value(QLatin1String(QContactNickname::FieldNickname)), QString("Johnny2"));
     }
}

void
ut_qtcontacts_trackerplugin::testSaveNameUnique()
{
    // save contact with one name
    QContactName name1;
    name1.setFirstName("Till");
    name1.setLastName("Eulenspiegel");

    QContact savedContact;
    QVERIFY(savedContact.saveDetail(&name1));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());

    // fetch the contact and compare content
    error = QContactManager::UnspecifiedError;
    QContact fetchedContact = engine()->contactImpl(savedContact.localId(), NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetchedContact.detail<QContactName>().firstName(), name1.firstName());
    QCOMPARE(fetchedContact.detail<QContactName>().lastName(), name1.lastName());
    QCOMPARE(fetchedContact.localId(), savedContact.localId());

    // save contact with second name detail which is invalid
    QContactName name2;
    name2.setFirstName("Hans");
    name2.setLastName("Wurst");
    QVERIFY(savedContact.saveDetail(&name2));

    // the engine shall drop the odd detail
    QTest::ignoreMessage(QtWarningMsg, "Dropping odd details: Name detail must be unique");

    qctLogger().setShowLocation(false);
    error = QContactManager::UnspecifiedError;
    const bool contactSaved = engine()->saveContact(&savedContact, &error);
    qctLogger().setShowLocation(true);

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactSaved);

    // again fetch the contact and compare content
    error = QContactManager::UnspecifiedError;
    fetchedContact = engine()->contactImpl(savedContact.localId(), NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetchedContact.detail<QContactName>().firstName(), name1.firstName());
    QCOMPARE(fetchedContact.detail<QContactName>().lastName(), name1.lastName());
    QCOMPARE(fetchedContact.localId(), savedContact.localId());
}

void
ut_qtcontacts_trackerplugin::testSaveNonLatin1Name_data()
{
    QTest::addColumn<QString>("givenName");
    QTest::addColumn<QString>("middleName");
    QTest::addColumn<QString>("familyName");
    QTest::addColumn<QString>("formattedName");

    QTest::newRow("german name")
            // given name
            << QString::fromUtf8("J\xC3\xBCrgen") // Jürgen
            // middle name
            << QString::fromUtf8("H\xC3\xA4nschen") // Hänschen
            // family name
            << QString::fromUtf8("K\x6C\x65hler") // Köhler
            // formatted name
            << QString::fromUtf8("J\xC3\xBCrgen K\x6C\x65hler"); // Jürgen Köhler

    QTest::newRow("chinese name")
            // given name
            << QString::fromUtf8("\xE6\x98\x8E") // 明
            // middle name
            << QString::fromUtf8("")
            // family name
            << QString::fromUtf8("\xE6\x9D\x8E") // 李
            // formatted name
            << QString::fromUtf8("\xE6\x98\x8E \xE6\x9D\x8E"); // 明 李

    QTest::newRow("russian name")
            // given name
            << QString::fromUtf8("\xD0\xAE\xD1\x80\xD0\xb8\xD0\xB9") // Юрий
            // middle name
            << QString::fromUtf8("\xD0\x90\xD0\xBB\xD0\xB5\xD0\xBA\xD1\x81\xD0\xB5\xD0\xB5\xD0\xB2\xD0\xB8\xD1\x87") // Алексеевич
            // family name
            << QString::fromUtf8("\xD0\x93\xD0\xB0\xD0\xB3\xD0\xB0\xD1\x80\xD0\xB8\xD0\xBD") // Гагарин
            // formatted name
            << QString::fromUtf8("\xD0\xAE\xD1\x80\xD0\xB8\xD0\xB9\x20\xD0\x93\xD0\xB0\xD0\xB3\xD0\xB0\xD1\x80\xD0\xB8\xD0\xBD"); // Юрий Гагарин

    // TODO: some name from RTL language, like arabic or hebrew
}

void
ut_qtcontacts_trackerplugin::testSaveNonLatin1Name()
{
    QFETCH(QString, givenName);
    QFETCH(QString, middleName);
    QFETCH(QString, familyName);
    QFETCH(QString, formattedName);

    // create contact
    QContact contact;
    QContactName nameDetail;
    nameDetail.setFirstName(givenName);
    nameDetail.setMiddleName(middleName);
    nameDetail.setLastName(familyName);
    contact.saveDetail(&nameDetail);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&contact, &error));
    registerForCleanup(contact);
    QCOMPARE(error,  QContactManager::NoError);

    // fetch contact
    error = QContactManager::UnspecifiedError;
    const QContact fetchedContact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    const QContactName fetchedContactName = fetchedContact.detail<QContactName>();
    QCOMPARE(fetchedContactName.firstName(), givenName);
    QCOMPARE(fetchedContactName.middleName(), middleName);
    QCOMPARE(fetchedContactName.lastName(), familyName);
    QCOMPARE(fetchedContact.displayLabel(), formattedName);
}

// test NB#189108
void
ut_qtcontacts_trackerplugin::testFetchAll()
{
    // create some few contacts to have something in the db for sure
    QList<QContact> contacts;

    for(int i = 1; i <= 5; ++i) {
        QContactNickname nickname;
        nickname.setNickname(QString::fromLatin1("Fetchy %1").arg(i));

        contacts.append(QContact());
        QVERIFY(contacts.last().saveDetail(&nickname));
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsSaved = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved);

    // directly asks tracker how many contacts are stored
    const QString queryString =
            "SELECT COUNT(?c) WHERE {"
            "  { ?c a nco:PersonContact } UNION"
            "  { ?c a nco:ContactGroup . ?c a nco:Contact }"
            "}";

    const QScopedPointer<QSparqlResult> result
            (executeQuery(queryString, QSparqlQuery::SelectStatement));

    QVERIFY(not result.isNull());
    QVERIFY(result->next());

    // fetch contacts via contact manager engine
    contacts = engine()->contacts(QContactFilter(), NoSortOrders,
                                  NoFetchHint, &error);

    // compare sparql and engine result
    QCOMPARE(result->value(0).toInt(), contacts.count());
}

void ut_qtcontacts_trackerplugin::testFetchById_data()
{
    QTest::addColumn<bool>("useSyncEngineCall");
    QTest::addColumn<bool>("doError");

    QTest::newRow("async (without error)") << false << false;
    QTest::newRow("sync (without error)")  << true  << false;
    QTest::newRow("async (with error)")    << false << true;
    QTest::newRow("sync (with error)")     << true  << true;
}

void ut_qtcontacts_trackerplugin::testFetchById()
{
    QFETCH(bool, useSyncEngineCall);
    QFETCH(bool, doError);

    // create some few contacts to have something in the db for sure
    QList<QContact> contacts;

    for(int i = 1; i <= 5; ++i) {
        QContact contact;

        SET_TESTNICKNAME_TO_CONTACT(contact);
        contacts << contact;
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsSaved = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved);

    QList<QContactLocalId> ids = QList<QContactLocalId>()
        << contacts.at(3).localId()
        << contacts.at(2).localId()
        << contacts.at(4).localId();

    // insert in the middle, to see if contacts before and after bad ids are fetched
    static const int posOfError = 2;
    if (doError) {
        // insert non existing id before last
        // (using id of last save + random offset should make sure, as tracker
        // uses increasing numbers for ids of new resources)
        ids.insert(posOfError, contacts.last().localId() + 23);
    }

    QList<QContact> fetchedContacts;
    QMap<int, QContactManager::Error> errorMap;
    error = QContactManager::UnspecifiedError;

    if (useSyncEngineCall) {
        fetchedContacts = engine()->contacts(ids, NoFetchHint, &errorMap, &error);
    } else {
        QContactFetchByIdRequest request;

        request.setLocalIds(ids);

        QVERIFY(engine()->startRequest(&request));
        QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

        QVERIFY(request.isFinished());

        fetchedContacts = request.contacts();
        error = request.error();
        errorMap = request.errorMap();
    }

    if (doError) {
        QCOMPARE(error, QContactManager::DoesNotExistError);
        QCOMPARE(errorMap.size(), 1);
        QCOMPARE(errorMap.value(posOfError), QContactManager::DoesNotExistError);
    } else {
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(errorMap.isEmpty());
    }

    QCOMPARE(fetchedContacts.size(), ids.size());
    QCOMPARE(fetchedContacts.at(0).localId(), ids.at(0));
    QCOMPARE(fetchedContacts.at(1).localId(), ids.at(1));
    if (doError) {
        QVERIFY(fetchedContacts.at(2).isEmpty());
        QCOMPARE(fetchedContacts.at(3).localId(), ids.at(3));
    } else {
        QCOMPARE(fetchedContacts.at(2).localId(), ids.at(2));
    }
}

void
ut_qtcontacts_trackerplugin::testTorture_data()
{
    QTest::addColumn<int>("duration");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("gentle") << 0 << false;
    QTest::newRow("painful") << 2500 << false;
    QTest::newRow("gentle-cancel") << 0 << true;
    QTest::newRow("painful-cancel") << 2500 << true;
}

void
ut_qtcontacts_trackerplugin::testTorture()
{
    QContactManager manager(QLatin1String("tracker"), makeEngineParams());
    QCOMPARE(manager.managerName(), QLatin1String("tracker"));

    QList<QContactAbstractRequest *> requests;
    QList<QContactLocalId> ids = manager.contactIds();
    static const int n_requests = 50;
    QFETCH(int, duration);
    QFETCH(bool, cancel);


    for (int i = 0; i < n_requests; i ++) {
        requests += new QContactFetchRequest;
        requests.last()->setManager(&manager);
        QVERIFY(requests.last()->start());

        requests += new QContactLocalIdFetchRequest;
        requests.last()->setManager(&manager);
        QVERIFY(requests.last()->start());

        requests += new QContactRelationshipFetchRequest;
        requests.last()->setManager(&manager);
        QVERIFY(requests.last()->start());

        QContactFetchByIdRequest *const fbir = new QContactFetchByIdRequest;
        requests += fbir;
        fbir->setLocalIds(ids);
        fbir->setManager(&manager);
        QVERIFY(fbir->start());
    }

    if (duration > 0) {
        QEventLoop loop;
        QTimer::singleShot(duration, &loop, SLOT(quit()));
        loop.exec();
    }

    if (cancel) {
        foreach (QContactAbstractRequest *request, requests) {
            request->cancel();
            delete request;
        }
    } else {
        qDeleteAll(requests);
    }
}

void
ut_qtcontacts_trackerplugin::testSaveNothing()
{
    QContactList nothing;
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactsSaved = engine()->saveContacts(&nothing, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved );
}

struct PhoneValue {
    QString number;
    QString latinNumber;
    QString context;
    QString subtype;
};

static void
add(QMap<QString, PhoneValue> &phoneValues, const QString &number,
    const QString &context = QString(), const QString &subtype = QString())
{
    const PhoneValue value = { number, number, context, subtype };
    phoneValues.insert(number, value);
}

static void
addWithLatin(QMap<QString, PhoneValue> &phoneValues, const QString &number, const QString &latinNumber,
             const QString &context = QString(), const QString &subtype = QString())
{
    const PhoneValue value = { number, latinNumber, context, subtype };
    phoneValues.insert(latinNumber, value);
}

void
ut_qtcontacts_trackerplugin::testSavePhoneNumber_data()
{
    QTest::addColumn<int>("iteration");

    QTest::newRow("1st run") << 0;
    QTest::newRow("2nd run") << 1;
    QTest::newRow("3rd run") << 2;
}

void
ut_qtcontacts_trackerplugin::testSavePhoneNumber()
{
    QFETCH(int, iteration);
    Q_UNUSED(iteration);

    // use the same values for 2 contacts
    QContact c;
    QContactLocalId initialId = c.localId();
    int detailsAdded = 0;
    QContactName name;
    name.setFirstName("I have phone numbers");
    name.setLastName("Girl");
    c.saveDetail(&name);

    QMap<QString, PhoneValue> phoneValues;

    add(phoneValues, "(704)486-6472", QContactDetail::ContextHome);
    add(phoneValues, "(765)957-1663", QContactDetail::ContextHome);
    add(phoneValues, "(999)888-1111", QContactDetail::ContextHome, QContactPhoneNumber::SubTypeMobile);
    add(phoneValues, "(999)888-4444", QContactDetail::ContextHome, QContactPhoneNumber::SubTypeLandline);

    // Also check that non-latin scripts are properly converted to latin
    addWithLatin(phoneValues,
                 QString::fromUtf8(RLE A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 PDF),
                 QString::fromLatin1("0123456789"),
                 QContactDetail::ContextHome);

    add(phoneValues, "(792)123-6113", QContactDetail::ContextWork);
    add(phoneValues, "(918)491-7361", QContactDetail::ContextWork, QContactPhoneNumber::SubTypeMobile);
    add(phoneValues, "(412)670-1514", QContactDetail::ContextWork, QContactPhoneNumber::SubTypeCar);

    foreach(const QString &number, phoneValues.keys()) {
        const PhoneValue &value(phoneValues[number]);
        QContactPhoneNumber phone;

        phone.setNumber(value.number);

        if (not value.context.isEmpty()) {
            phone.setContexts(value.context);
        }
        if (not value.subtype.isEmpty()) {
            phone.setSubTypes(value.subtype);
        }

        QVERIFY(c.saveDetail(&phone));
        detailsAdded++;
    }

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(error,  QContactManager::NoError);
    const QContactLocalId savedId(c.localId());
    QVERIFY(savedId != initialId);
    // wait for commit transaction to be done, no signals yet
    for(int i = 0; i < 100; i++) {
        usleep(10000);
        QCoreApplication::processEvents();
    }

    // verify with synchronous read too
    error = QContactManager::UnspecifiedError;
    QContact contact = engine()->contactImpl(c.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QList<QContactPhoneNumber> details = contact.details<QContactPhoneNumber>();
    QCOMPARE(details.count(), detailsAdded);

    foreach(const QContactPhoneNumber &detail, details) {
        QMap<QString, PhoneValue>::ConstIterator i = phoneValues.find(detail.number());

        // Verify that the stored values and attributes are the same as given
        QVERIFY2(i != phoneValues.constEnd(), qPrintable(detail.number()));

        QCOMPARE(detail.number(), i->latinNumber);
        QCOMPARE(detail.contexts().first(), i->context);

        if (i->subtype.isEmpty()) { // default is voice
            QVERIFY2(detail.subTypes().contains(QContactPhoneNumber::SubTypeVoice),
                     qPrintable(detail.number()));
        } else {
            QVERIFY2(detail.subTypes().contains(i->subtype),
                     qPrintable(detail.number()));
        }

        QVERIFY2(not detail.detailUri().isEmpty(), qPrintable(i->number));
    }

    // save again with normalized phone numbers
    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&contact, &error));
    QCOMPARE(error,  QContactManager::NoError);
    QCOMPARE(contact.localId(), savedId);
    // wait for commit transaction to be done, no signals yet
    for(int i = 0; i < 100; i++) {
        usleep(10000);
        QCoreApplication::processEvents();
    }

    error = QContactManager::UnspecifiedError;
    contact = engine()->contactImpl(c.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    details = contact.details<QContactPhoneNumber>();
    QCOMPARE(details.count(), detailsAdded);

    foreach(QContactPhoneNumber detail, details) {
        QMap<QString, PhoneValue>::ConstIterator i = phoneValues.find(detail.number());

        // Verify that the stored values and attributes are the same as given
        QVERIFY2(i != phoneValues.constEnd(), qPrintable(detail.number()));

        QCOMPARE(detail.number(), i->latinNumber);
        QCOMPARE(detail.contexts().first(), i->context);

        if (i->subtype.isEmpty()) { // default is voice
            QVERIFY2(detail.subTypes().contains(QContactPhoneNumber::SubTypeVoice),
                     qPrintable(detail.detailUri()));
        } else {
            QVERIFY2(detail.subTypes().contains(i->subtype),
                     qPrintable(detail.detailUri()));
        }
    }

    // edit one of numbers . values, context and subtypes and save again
    QString editedPhoneValue = "+7044866473";
    QContactPhoneNumber phone = details[0];
    phone.setNumber(editedPhoneValue);
    phone.setContexts(QContactDetail::ContextWork);
    phone.setSubTypes(QContactPhoneNumber::SubTypeMobile);
    c = contact;
    QCOMPARE(c.localId(), savedId);
    QVERIFY(c.saveDetail(&phone));
    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(c.localId(), savedId);
    QCOMPARE(error,  QContactManager::NoError);
    c = this->contact(c.localId(), QStringList()<<QContactPhoneNumber::DefinitionName);
    QCOMPARE(c.localId(), savedId);
    details = c.details<QContactPhoneNumber>();
    QCOMPARE(details.count(), detailsAdded);
    bool found = false;
    foreach (QContactPhoneNumber detail, details) {
        if(detail.number() == phone.number())
        {
            found = true;
            QVERIFY(detail.subTypes().contains(QContactPhoneNumber::SubTypeMobile));
            QVERIFY(detail.contexts().contains(QContactPhoneNumber::ContextWork));
            break;
        }
    }
    QVERIFY(found);
}

void
ut_qtcontacts_trackerplugin::testSimilarPhoneNumber()
{
    QStringList phoneNumbers;
    QContact contact;

    QContactPhoneNumber tel1;
    tel1.setNumber("112233");
    phoneNumbers += tel1.number();
    QVERIFY(contact.saveDetail(&tel1));

    QContactPhoneNumber tel2;
    tel2.setNumber("112233 ");
    phoneNumbers += tel2.number();
    QVERIFY(contact.saveDetail(&tel2));

    QContactPhoneNumber tel3;
    tel3.setNumber("11-22-33");
    phoneNumbers += tel3.number();
    QVERIFY(contact.saveDetail(&tel3));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool contactSaved = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != contact.localId());
    QVERIFY(contactSaved);

    error = QContactManager::UnspecifiedError;
    contact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != contact.localId());

    QCOMPARE(contact.details<QContactPhoneNumber>().count(), phoneNumbers.size());

    foreach(const QContactPhoneNumber &tel, contact.details<QContactPhoneNumber>()) {
        phoneNumbers.removeOne(tel.number());
    }

    QVERIFY(phoneNumbers.isEmpty());
}

void
ut_qtcontacts_trackerplugin::testEasternArabicPhoneNumber_data()
{
    QTest::addColumn<QString>("queryPhoneNumber");
    QTest::addColumn<QString>("savedPhoneNumber");
    QTest::addColumn<QString>("otherPhoneNumber");

    QTest::newRow("arabic-latin")
            << QString::fromUtf8("11223344")
            << QString::fromUtf8(RLE A1 A1 "." A2 A2 "." A3 A3 "." A4 A4 PDF)
            << QString();
    QTest::newRow("latin-arabic")
            << QString::fromUtf8(A2 A2 "." A3 A3 "." A4 A4 "." A5 A5)
            << QString::fromUtf8("22-334-455")
            << QString();
    QTest::newRow("arabic-arabic")
            << QString::fromUtf8("\331\243\331\243.\331\244\331\244.\331\245\331\245.\331\246\331\246")
            << QString::fromUtf8("\331\243\331\243-\331\244\331\244\331\245-\331\245\331\246\331\246")
            << QString();
    QTest::newRow("latin-latin")
            << QString::fromUtf8("44 556(677)")
            << QString::fromUtf8(LRE "44-556-677" PDF)
            << QString();
    QTest::newRow("latin-latin-clipboard")
            << QString::fromUtf8(LRE "55 667(788)" PDF)
            << QString::fromUtf8("55-667-788")
            << QString();
    QTest::newRow("false-friend")
            << QString::fromUtf8("+493055667788")
            << QString::fromUtf8("+49-30-5566-7788")
            << QString::fromUtf8("+49-338-566-7788");
}

void
ut_qtcontacts_trackerplugin::testEasternArabicPhoneNumber()
{
    QFETCH(QString, queryPhoneNumber);
    QFETCH(QString, savedPhoneNumber);
    QFETCH(QString, otherPhoneNumber);

    QContact savedContact;

    {
        QContactPhoneNumber tel;
        tel.setNumber(savedPhoneNumber);
        QVERIFY(savedContact.saveDetail(&tel));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&savedContact, &error);
        QCOMPARE(QContactManager::NoError, error);
        QVERIFY(0 != savedContact.localId());
        QVERIFY(contactSaved);
    }

    QContact otherContact;

    if (not otherPhoneNumber.isEmpty()) {
        QContactPhoneNumber tel;
        tel.setNumber(otherPhoneNumber);
        QVERIFY(otherContact.saveDetail(&tel));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&otherContact, &error);
        QCOMPARE(QContactManager::NoError, error);
        QVERIFY(0 != otherContact.localId());
        QVERIFY(contactSaved);
    }

    QContactDetailFilter f;
    f.setDetailDefinitionName(QContactPhoneNumber::DefinitionName,
                              QContactPhoneNumber::FieldNumber);
    f.setMatchFlags(QContactDetailFilter::MatchPhoneNumber);
    f.setValue(queryPhoneNumber);

    changeSetting(QctSettings::NumberMatchLengthKey, 7);

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactList result = engine()->contacts(f, NoSortOrders, fetchHint<QContactPhoneNumber>(), &error);
        QCOMPARE(QContactManager::NoError, error);

        int contactIndex = -1, otherIndex = -1;

        for(int i = 0; i < result.count(); ++i) {
            if (result.at(i).localId() == savedContact.localId()) {
                contactIndex = i;
                continue;
            }

            if (result.at(i).localId() == otherContact.localId()) {
                otherIndex = i;
                continue;
            }
        }

        QVERIFY(contactIndex >= 0);
        QVERIFY(otherIndex >= 0 || otherPhoneNumber.isEmpty());
    }
}

void
ut_qtcontacts_trackerplugin::testSamePhoneNumber()
{
    QContactPhoneNumber tel;
    tel.setNumber("099-8877563");

    QList<QContactLocalId> savedIds;

    // save two different contacts with identical phone number
    for(int i = 0; i < 2; ++i) {
        QContact contact;
        QVERIFY(contact.saveDetail(&tel));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);

        savedIds += contact.localId();
    }

    // check that both contacts really got saved and got that phone number
    QList<QContact> contactList = contacts(localIds());
    QCOMPARE(contactList.count(), localIds().count());
    QString detailUri;

    foreach(const QContact &c, contactList) {
        QCOMPARE(c.details<QContactPhoneNumber>().count(), 1);

        const QContactPhoneNumber detail = c.detail<QContactPhoneNumber>();

        if (detailUri.isEmpty()) {
            detailUri = detail.detailUri();
        }

        QCOMPARE(detail.number(), tel.number());
        QCOMPARE(detail.detailUri(), detailUri);

        QVERIFY(not detail.detailUri().isEmpty());
    }

    // check if we really can lookup both contacts by phone number.
    // both with exact and with fuzzy matching.
    for(int i = 0; i < 2; ++i) {
        QContactDetailFilter filter;

        filter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName,
                                       QContactPhoneNumber::FieldNumber);
        filter.setValue(tel.number());

        if (1 == i) {
            filter.setMatchFlags(QContactFilter::MatchPhoneNumber);
        }

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QList<QContactLocalId> fetchedIds = engine()->contactIds(filter, NoSortOrders, &error);
        QCOMPARE(error, QContactManager::NoError);

        foreach(QContactLocalId localId, savedIds) {
            QVERIFY(fetchedIds.contains(localId));
        }
    }
}

void
ut_qtcontacts_trackerplugin::testPhoneNumberContext()
{
    QContact c;
    QContactPhoneNumber phone;
    phone.setContexts(QContactDetail::ContextHome);
    phone.setNumber("555-888");
    phone.setSubTypes(QContactPhoneNumber::SubTypeMobile);
    c.saveDetail(&phone);
    QContact contactToSave = c;
    // Let's do this all twice, first time save new detail, and next iteration change the context
    for (int iterations = 0; iterations < 2; iterations++) {
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&contactToSave, &error));
        QCOMPARE(error, QContactManager::NoError);
        // wait for commit transaction to be done, no signals yet
        for(int i = 0; i < 100; i++) {
            usleep(10000);
            QCoreApplication::processEvents();
        }

        QContactFetchRequest request;
        request.setFilter(localIdFilter(contactToSave.localId()));
        request.setFetchHint(fetchHint<QContactPhoneNumber>());

        Slots slot;
        QObject::connect(&request, SIGNAL(resultsAvailable()),
                &slot, SLOT(resultsAvailable()));

        engine()->startRequest(&request);

        engine()->waitForRequestFinishedImpl(&request, 0);

        // if it takes more, then something is wrong
        QVERIFY(request.isFinished());
        QVERIFY(!slot.contacts.isEmpty());

        QContact contactToTest;
        foreach (QContact savedContact, slot.contacts) {
            if (savedContact.localId() == contactToSave.localId()) {
                contactToTest = savedContact;
            }
        }
        QCOMPARE(contactToTest.localId(), contactToSave.localId()); // Just to be sure we got the saved contact
        QCOMPARE(contactToTest.details<QContactPhoneNumber>().count(), 1);
        if (0 == iterations) {
            // perform context change
            QContactPhoneNumber phoneToEdit = contactToTest.detail<QContactPhoneNumber>();
            phoneToEdit.setContexts(QContactDetail::ContextWork);
            contactToTest.saveDetail(&phoneToEdit);
            contactToSave = contactToTest;
        }
        QVERIFY(contactToTest.details<QContactPhoneNumber>().count() == 1);
    }
}

void
ut_qtcontacts_trackerplugin::testWritingOnlyWorkMobile()
{
    QContact c;
    QContactPhoneNumber phone;
    phone.setContexts(QContactDetail::ContextWork);
    phone.setNumber("555999");
    phone.setSubTypes(QContactPhoneNumber::SubTypeMobile);
    c.saveDetail(&phone);
    QContact& contactToSave = c;
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&contactToSave, &error));
    QCOMPARE(error, QContactManager::NoError);
    // wait for commit transaction to be done, no signals yet
    for(int i = 0; i < 100; i++) {
        usleep(10000);
        QCoreApplication::processEvents();
    }

    QContactFetchRequest request;
    request.setFilter(localIdFilter(contactToSave.localId()));
    request.setFetchHint(fetchHint<QContactPhoneNumber>());

    Slots slot;
    QObject::connect(&request, SIGNAL(resultsAvailable()),
            &slot, SLOT(resultsAvailable()));

    engine()->startRequest(&request);

    engine()->waitForRequestFinishedImpl(&request, 0);

    // if it takes more, then something is wrong
    QVERIFY(request.isFinished());
    QVERIFY(!slot.contacts.isEmpty());

    QContact contactToTest;
    foreach (QContact savedContact, slot.contacts) {
        if (savedContact.localId() == c.localId()) {
            contactToTest = savedContact;
        }
    }

    // add implicied subtypes for new fetch request
    phone.setSubTypes(phone.subTypes() <<
                      QContactPhoneNumber::SubTypeMessagingCapable <<
                      QContactPhoneNumber::SubTypeVoice);

    QCOMPARE(contactToTest.localId(), c.localId()); // Just to be sure we got the saved contact
    QCOMPARE(contactToTest.details<QContactPhoneNumber>().count(), 1);
    QCOMPARE(contactToTest.detail<QContactPhoneNumber>().number(), phone.number());
    QCOMPARE(contactToTest.detail<QContactPhoneNumber>().subTypes().toSet(), phone.subTypes().toSet());
    QCOMPARE(contactToTest.detail<QContactPhoneNumber>().contexts(), phone.contexts());
}

void
ut_qtcontacts_trackerplugin::testSaveAddress()
{
    QContact c;
    QContactName name;
    name.setFirstName("Aruba & Barbados");
    name.setLastName("Girl");
    c.saveDetail(&name);
    QContactLocalId initialId = c.localId();
    int detailsAdded = 0;

    // List of pairs of field-value map and context
    typedef QMap<QString,QString> typeAddress;
    typedef QPair<typeAddress,QString> typeAddressWithContext;
    QList<typeAddressWithContext> addressValues;

    // TODO check status of 137174 and other libqttracker1pre6 bugs before refactoring
    typeAddress values;
    values.insert(QLatin1String(QContactAddress::FieldCountry), "Barbados");
    values.insert(QLatin1String(QContactAddress::FieldPostcode), "55555");
    values.insert(QLatin1String(QContactAddress::FieldStreet), "Martindales Rd");
    values.insert(QLatin1String(QContactAddress::FieldExtendedAddress), "In the Front Garden");
    values.insert(QLatin1String(QContactAddress::FieldRegion), "Bridgetown");
    values.insert(QLatin1String(QContactAddress::FieldLocality), "Bridgetown town");



    addressValues.append(typeAddressWithContext(values, QLatin1String(QContactDetail::ContextWork)));
    values.clear();
    values.insert(QLatin1String(QContactAddress::FieldCountry), "Aruba");
    values.insert(QLatin1String(QContactAddress::FieldPostcode), "44444");
    values.insert(QLatin1String(QContactAddress::FieldStreet), "Brazilie Straat");
    values.insert(QLatin1String(QContactAddress::FieldRegion), "Oranjestad");
    values.insert(QLatin1String(QContactAddress::FieldLocality), "Bridgetown town");
    values.insert(QLatin1String(QContactAddress::FieldPostOfficeBox), "00011102");

    addressValues.append(typeAddressWithContext(values, QLatin1String(QContactDetail::ContextHome)));
    values.clear();
    values.insert(QLatin1String(QContactAddress::FieldCountry), "ArubaWork");
    values.insert(QLatin1String(QContactAddress::FieldPostcode), "44445");
    values.insert(QLatin1String(QContactAddress::FieldStreet), "Sunset Blvd");
    values.insert(QLatin1String(QContactAddress::FieldRegion), "Oranjestad");
    values.insert(QLatin1String(QContactAddress::FieldLocality), "Bridgetown town");
    values.insert(QLatin1String(QContactAddress::FieldPostOfficeBox), "00011103");

    addressValues.append(typeAddressWithContext(values, QLatin1String(QContactDetail::ContextHome)));
    foreach (typeAddressWithContext addressWithContext, addressValues) {
        QContactAddress address;
        foreach (QString field, addressWithContext.first.keys()) {
            address.setValue(field, addressWithContext.first.value(field));
        }
        address.setContexts(addressWithContext.second);
        c.saveDetail(&address);
        detailsAdded++;
    }

    QContactManager::Error error(QContactManager::UnspecifiedError);
    engine()->saveContact(&c, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(c.localId() != initialId);
    error = QContactManager::UnspecifiedError;
    QContact contact = engine()->contactImpl(c.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QList<QContactAddress> details = contact.details<QContactAddress>();
    QCOMPARE(details.count(), detailsAdded);
    bool found = false;
    // Test if inserted values are found in some of the details
    foreach (typeAddressWithContext addressWithContext, addressValues) {
        foreach (QContactAddress detail, details) {
            foreach (QString field, addressWithContext.first.keys()) {
                found = (detail.value(field) == addressWithContext.first.value(field));
                if (!found)
                    break;
            }
            if (found)
                break;
        }
        QVERIFY2(found, "Inserted detail was not found in the fetched details");
    }
}

// Related bug: NB#248183
void
ut_qtcontacts_trackerplugin::testExtendedAddress()
{
    // Check parsing from vcard
    QList<QContact> contacts = parseVCards(referenceFileName("NB248183.vcf"));
    QCOMPARE(contacts.count(), 1);

    QContact contact = contacts.first();
    QCOMPARE(contact.details<QContactAddress>().count(), 1);

    const QString extendedAddress = contact.detail<QContactAddress>().extendedAddress();
    QCOMPARE(extendedAddress, QString::fromLatin1("Lawspet"));

    // Check saving and fetching
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactSaved);

    QVERIFY(0 != contact.localId());

    error = QContactManager::UnspecifiedError;
    QContact fetchedContact = engine()->contactImpl(contact.localId(), fetchHint<QContactAddress>(), &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetchedContact.localId(), contact.localId());
    QCOMPARE(fetchedContact.details<QContactAddress>().count(), 1);
    QCOMPARE(fetchedContact.detail<QContactAddress>().extendedAddress(), extendedAddress);
}

void
ut_qtcontacts_trackerplugin::testSaveOrganization()
{
    QList<PairOfStrings> samples;

    samples << PairOfStrings(QContactOrganization::FieldName, "Nokia");
    samples << PairOfStrings(QContactOrganization::FieldLogoUrl, "nokia.jpg");
    samples << PairOfStrings(QContactOrganization::FieldDepartment, "Meego R&D");
    samples << PairOfStrings(QContactOrganization::FieldLocation, "Helsinki");
    samples << PairOfStrings(QContactOrganization::FieldRole, "Developer");
    samples << PairOfStrings(QContactOrganization::FieldTitle, "Code Guru");

    QContact contact;

    for(int i = 0; i < samples.count(); ++i) {
        QContactOrganization org = contact.detail<QContactOrganization>();
        org.setValue(samples[i].first, samples[i].second);
        contact.saveDetail(&org);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        bool success = engine()->saveContact(&contact, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(success);

        error = QContactManager::UnspecifiedError;
        const QContactLocalId contactLocalId = contact.localId();
        contact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
        QCOMPARE(contact.localId(), contactLocalId);
        QCOMPARE(error,  QContactManager::NoError);

        org = contact.detail<QContactOrganization>();

        for(int j = 0; j <= i; ++j) {
            QCOMPARE(org.value(samples[j].first), samples[j].second);
        }
    }
}

void
ut_qtcontacts_trackerplugin::testSaveEmailAddress()
{
    QContact c;
    QContactLocalId initialId = c.localId();
    int detailsAdded = 0;

    QMap<QString,QString> values;
    values.insert("john.does@hotmail.com", QContactDetail::ContextHome);
    values.insert("john.doe@gmail.com", QContactDetail::ContextWork);
    values.insert("john.doe@nokia.com", QContactDetail::ContextWork);
    values.insert("john.doe@johndoe.com", QContactDetail::ContextHome);
    foreach(QString address, values.keys()) {
        QContactEmailAddress emailAddress;
        emailAddress.setEmailAddress(address);
        emailAddress.setContexts(values.value(address));
        c.saveDetail(&emailAddress);
        detailsAdded++;
    }
    QContactName name;
    name.setFirstName("Jo");
    name.setLastName("H N Doe");
    c.saveDetail(&name);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContact(&c, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(c.localId() != initialId);
    QVERIFY(contactSaved);

    error = QContactManager::UnspecifiedError;
    QContact contact = engine()->contactImpl(c.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QList<QContactEmailAddress> details = contact.details<QContactEmailAddress>();
    QCOMPARE(details.count(), detailsAdded);
    foreach (QContactEmailAddress detail, details) {
        QString address = detail.value(QContactEmailAddress::FieldEmailAddress);
        QVERIFY(values.contains(address));
        QCOMPARE(detail.contexts()[0], values.value(address));
    }
}

void
ut_qtcontacts_trackerplugin::testSaveCustomValues()
{
    // create contact with custom subtypes
    QContactName name;
    name.setFirstName("Teppo");
    name.setLastName("Virtanen");

    QContactPhoneNumber savedNumber;
    savedNumber.setNumber("+23234234");
    savedNumber.setSubTypes(QStringList() <<
                            QContactPhoneNumber::SubTypeVoice <<
                            "CrazyA" << "CrazyB" << "CrazyC");

    QContactOnlineAccount savedAccount;
    savedAccount.setAccountUri("jepa@account.org");
    savedAccount.setValue(QContactOnlineAccount__FieldAccountPath, "/org/freedesktop/testSaveCustomValues/account");
    savedAccount.setContexts(QContactDetail::ContextHome);
    savedAccount.setSubTypes(QStringList() << "FunkyA" << "FunkyB" << "FunkyC");

    QContact savedContact;
    savedContact.saveDetail(&name);
    savedContact.saveDetail(&savedNumber);
    savedContact.saveDetail(&savedAccount);

    // save the test contact
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContact(&savedContact, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());
    QVERIFY(contactSaved);

    // restore the contact
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(QStringList() <<
                                       QContactPhoneNumber::DefinitionName <<
                                       QContactOnlineAccount::DefinitionName);

    error = QContactManager::UnspecifiedError;
    QContact fetchedContact = engine()->contactImpl(savedContact.localId(), fetchHint, &error);

    QCOMPARE(error,  QContactManager::NoError);
    QCOMPARE(fetchedContact.localId(), savedContact.localId());

    // compare the fetched contact with saved contact
    const QList<QContactPhoneNumber> fetchedNumbers =
            fetchedContact.details<QContactPhoneNumber>();

    QCOMPARE(fetchedNumbers.count(), 1);
    QCOMPARE(fetchedNumbers.first().number(), savedNumber.number());
    QCOMPARE(fetchedNumbers.first().subTypes().count(), savedNumber.subTypes().count());
    QCOMPARE(fetchedNumbers.first().subTypes().toSet(), savedNumber.subTypes().toSet());

    const QList<QContactOnlineAccount> fetchedAccounts =
            fetchedContact.details<QContactOnlineAccount>();

    QCOMPARE(fetchedAccounts.count(), 1);
    QCOMPARE(fetchedAccounts.first().accountUri(), savedAccount.accountUri());
    QCOMPARE(fetchedAccounts.first().contexts().count(), savedAccount.contexts().count());
    QCOMPARE(fetchedAccounts.first().contexts().toSet(), savedAccount.contexts().toSet());
    QCOMPARE(fetchedAccounts.first().subTypes().count(), savedAccount.subTypes().count());
    QCOMPARE(fetchedAccounts.first().subTypes().toSet(), savedAccount.subTypes().toSet());
}

void
ut_qtcontacts_trackerplugin::testRemoveContact()
{
    QContact c;
    QContactPhoneNumber phone;
    phone.setNumber("+358501234567");
    c.saveDetail(&phone);
    QContactEmailAddress email;
    email.setEmailAddress("super.man@hotmail.com");
    c.saveDetail(&email);
    QContactName name;
    name.setFirstName("Super");
    name.setLastName("Man");
    c.saveDetail(&name);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(error,  QContactManager::NoError);

    error = QContactManager::UnspecifiedError;
    QVERIFY2(engine()->removeContact(c.localId(), &error), "Removing a contact failed");
    QCOMPARE(error, QContactManager::NoError);

    QVERIFY2(engine()->contactImpl(c.localId(), NoFetchHint, &error) == QContact(),
             "Found a contact, which should have been removed");
}

void
ut_qtcontacts_trackerplugin::testRemoveSelfContact()
{
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> idsToRemove;

    for (int i = 0; i < 3; ++i) {
        QContact contact;
        QContactName contactName;
        contactName.setFirstName(QString::fromLatin1("%1 %2").arg(Q_FUNC_INFO).arg(i));
        contact.saveDetail(&contactName);

        QVERIFY(engine()->saveContact(&contact, &error));
        QCOMPARE(error, QContactManager::NoError);
        idsToRemove.append(contact.localId());
    }

    error = QContactManager::UnspecifiedError;
    QContactLocalId selfId = engine()->selfContactId(&error);
    QCOMPARE(error, QContactManager::NoError);

    idsToRemove.append(selfId);

    QContactRemoveRequest request;
    request.setContactIds(idsToRemove);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));
    QCOMPARE(request.error(), QContactManager::PermissionsError);
    QCOMPARE(request.errorMap().size(), 1);
    QCOMPARE(request.errorMap().value(3, QContactManager::NoError), QContactManager::PermissionsError);

    idsToRemove.removeLast();

    // Rest of the contacts should have been deleted properly
    foreach (QContactLocalId id, idsToRemove) {
        error = QContactManager::UnspecifiedError;
        QContact contact(engine()->contact(id, QContactFetchHint(), &error));
        QCOMPARE(error, QContactManager::DoesNotExistError);
    }

    // test to only remove self contact
    error = QContactManager::UnspecifiedError;
    QVERIFY(not engine()->removeContact(selfId, &error));
    QCOMPARE(error, QContactManager::PermissionsError);
}

void
ut_qtcontacts_trackerplugin::testSaveContacts()
{
    QList<QContact> contacts;
    QStringList fixedGuids;

    for (int i = 0; i < 3; i++) {
        QContact c;

        QContactName name;
        name.setFirstName("John");
        name.setLastName(QString::number(i));
        QVERIFY(c.saveDetail(&name));

        // skip first contact to test GUID detail auto-creation
        fixedGuids += (i > 0 ? QUuid::createUuid().toString() : QString());

        if (not fixedGuids.last().isEmpty()) {
            QContactGuid uid;
            uid.setGuid(fixedGuids.last());
            QVERIFY(c.saveDetail(&uid));
        }

        QContactGender gender;

        switch(i % 3) {
        case 0:
            gender.setGender(QContactGender::GenderMale);
            break;
        case 1:
            gender.setGender(QContactGender::GenderFemale);
            break;
        default:
            gender.setGender(QContactGender::GenderUnspecified);
            break;
        }

        QVERIFY(c.saveDetail(&gender));
        contacts.append(c);

        QVERIFY(c.displayLabel().isEmpty());
    }

    QMap<int, QContactManager::Error> errorMap;
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QDateTime earlier = QDateTime::currentDateTime().addSecs(-1);

    changeSetting(QctSettings::NameOrderKey, QContactDisplayLabel__FieldOrderFirstName);
    const bool contactsSaved = engine()->saveContacts(&contacts, &errorMap, &error);

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactsSaved);

    for (int i = 0; i < contacts.count(); i++) {
        QVERIFY(contacts[i].localId() != 0);
        QCOMPARE(contacts[i].displayLabel(), QString::fromLatin1("John %1").arg(i));

        error = QContactManager::UnspecifiedError;

        const QContact contact = engine()->contactImpl(contacts[i].localId(), NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QCOMPARE(contact.localId(), contacts[i].localId());

        const QList<QContactName> details = contact.details<QContactName>();
        QCOMPARE(details.count(), 1);
        QCOMPARE(details.first().lastName(), QString("%1").arg(QString::number(i,10)));

        const QList<QContactGender> genders = contact.details<QContactGender>();
        QCOMPARE(genders.count(), 1);
        QCOMPARE(genders.first().gender(),contacts[i].detail<QContactGender>().gender());

        const QList<QContactGuid> guids = contact.details<QContactGuid>();
        QCOMPARE(guids.count(), 1);

        if (fixedGuids.at(i).isEmpty()) {
            QVERIFY(not guids.first().guid().isEmpty());
        } else {
            QCOMPARE(guids.first().guid(), fixedGuids.at(i));
        }

        const QList<QContactTimestamp> timestamps = contact.details<QContactTimestamp>();

        QCOMPARE(timestamps.count(), 1);
        QVERIFY(not timestamps.first().lastModified().isNull());
        QVERIFY(timestamps.first().lastModified() >= earlier);
        QVERIFY(not timestamps.first().created().isNull());
        QVERIFY(timestamps.first().created() >= earlier);
    }

    // save contacts again to check if timestamps get updated
    sleep(1);

    const QDateTime later = QDateTime::currentDateTime().addSecs(-1);
    QVERIFY(later > earlier);

    error = QContactManager::UnspecifiedError;
    engine()->saveContacts(&contacts, &errorMap, &error);
    QCOMPARE(error, QContactManager::NoError);

    for (int i = 0; i < contacts.count(); i++) {
        QVERIFY(contacts[i].localId() != 0);
        error = QContactManager::UnspecifiedError;
        QContact contact = engine()->contactImpl(contacts[i].localId(), NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QCOMPARE(contact.localId(), contacts[i].localId());

        const QList<QContactTimestamp> timestamps = contact.details<QContactTimestamp>();
        QCOMPARE(timestamps.count(), 1);

        QVERIFY(not timestamps.first().lastModified().isNull());
        QVERIFY(timestamps.first().lastModified() >= later);

        QVERIFY(not timestamps.first().created().isNull());
        QVERIFY(timestamps.first().created() >= earlier);
        QVERIFY(timestamps.first().created() < later);
    }
}

void
ut_qtcontacts_trackerplugin::testRemoveContacts()
{
    QList<QContactLocalId> addedIds;
    for (int i = 0; i < 5; i++) {
        QContact c;
        QContactName name;
        name.setFirstName(QString("John%1").arg(QString::number(i,10)));
        c.saveDetail(&name);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);

        addedIds.append(c.localId());
    }
    QList<QContactLocalId> toApiRemove;
    toApiRemove.append(addedIds.takeLast());
    toApiRemove.append(addedIds.takeLast());
    QList<QContactLocalId> toPluginRemove(addedIds);

    // Remove all, but last of the added contacts
    QMap<int, QContactManager::Error> errorMap;
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->removeContacts(toPluginRemove, &errorMap, &error));
    QCOMPARE(error,  QContactManager::NoError);
    for (int i = 0; i < errorMap.count(); i++) {
        QCOMPARE(toPluginRemove[i], 0U);
    }

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->removeContacts(toApiRemove, &errorMap, &error));
    QCOMPARE(error, QContactManager::NoError);
    for (int i = 0; i < errorMap.count(); i++) {
        QCOMPARE(toApiRemove[i], 0U);
    }
}

static bool
haveAvatarPath(const QList<QContactAvatar> &avatars, const QString &filename)
{
    foreach(const QContactAvatar &a, avatars) {
        if (a.imageUrl().toLocalFile() == filename) {
            return true;
        }
    }

    return false;
}

void
ut_qtcontacts_trackerplugin::testAvatar()
{
    const uint baseId = qAbs(qrand());

    const QString accountId1 = QString::fromLatin1("test-%1@ovi.com").arg(baseId);
    const QString accountId2 = QString::fromLatin1("test-%1@ovi.com").arg(baseId + 1);
    const QString accountId3 = QString::fromLatin1("test-%1@ovi.com").arg(baseId + 2);
    const QString accountPath1 = QString::fromLatin1("/org/freedesktop/testAvatar/account/%1").arg(baseId);
    const QString accountPath2 = QString::fromLatin1("/org/freedesktop/testAvatar/account/%1").arg(baseId + 1);
    const QString accountPath3 = QString::fromLatin1("/org/freedesktop/testAvatar/account/%1").arg(baseId + 2);
    const QString contactIri = QString::fromLatin1("contact:testAvatar:%1").arg(baseId);

    const QString serviceProvider = QLatin1String("ovi.com");
    const QString protocol = QLatin1String("jabber");

    const QContactLocalId contactId1 = insertIMContact(contactIri, accountId1,
                                                       QLatin1String("nco:presence-status-available"),
                                                       QLatin1String("In Helsinki"), accountPath1,
                                                       protocol, serviceProvider);
    const QContactLocalId contactId2 = insertIMContact(contactIri, accountId2,
                                                       QLatin1String("nco:presence-status-busy"),
                                                       QLatin1String("In Brisbane"), accountPath2,
                                                       protocol, serviceProvider);
    const QContactLocalId contactId3 = insertIMContact(contactIri, accountId3,
                                                       QLatin1String("nco:presence-status-available"),
                                                       QLatin1String("In Berlin"), accountPath3,
                                                       protocol, serviceProvider);

    QVERIFY(contactId1 != 0);
    QCOMPARE(contactId2, contactId1);
    QCOMPARE(contactId3, contactId1);

    QContactAvatar personalAvatar;
    QList<QContactAvatar> avatars;
    QContact contactWithAvatar = contact(contactId1, QStringList());
    avatars = contactWithAvatar.details<QContactAvatar>();
    QCOMPARE(avatars.size(), 3);

    QContactName nameDetail = contactWithAvatar.detail<QContactName>();
    nameDetail.setCustomLabel(__func__);

    QVERIFY(contactWithAvatar.saveDetail(&nameDetail));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&contactWithAvatar, &error));
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(0 != contactWithAvatar.localId());

    contactWithAvatar = contact(contactId1, QStringList());
    avatars = contactWithAvatar.details<QContactAvatar>();

    QCOMPARE(avatars.size(), 3);

    QVERIFY(haveAvatarPath(avatars, onlineAvatarPath(accountPath1)));
    QVERIFY(haveAvatarPath(avatars, onlineAvatarPath(accountPath2)));

    QCOMPARE(avatars[2].imageUrl().toLocalFile(), onlineAvatarPath(accountPath2));

    personalAvatar.setImageUrl(QUrl("file:///home/user/.contacts/avatars/default_avatar.png"));

    contactWithAvatar.saveDetail(&personalAvatar);

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact( &contactWithAvatar, &error));
    QCOMPARE(error,  QContactManager::NoError);

    error = QContactManager::UnspecifiedError;
    QContact c = engine()->contactImpl(contactWithAvatar.localId(), NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);

    avatars = c.details<QContactAvatar>();

    QCOMPARE(avatars.size(), 4);

    QCOMPARE(avatars[0].imageUrl(), personalAvatar.imageUrl());
    QCOMPARE(avatars[0].linkedDetailUris(), QStringList());

    QVERIFY(haveAvatarPath(avatars, onlineAvatarPath(accountPath1)));
    QVERIFY(haveAvatarPath(avatars, onlineAvatarPath(accountPath2)));

    QCOMPARE(avatars[3].imageUrl().toLocalFile(), onlineAvatarPath(accountPath2));
}

void
ut_qtcontacts_trackerplugin::testOnlineAvatar_data()
{
    QTest::addColumn<QString>("context");

    QTest::newRow("none") << QString();
    QTest::newRow("home") << QContactDetail::ContextHome.latin1();
    QTest::newRow("work") << QContactDetail::ContextWork.latin1();
    QTest::newRow("other") << QContactDetail::ContextOther.latin1();
}

void
ut_qtcontacts_trackerplugin::testOnlineAvatar()
{
    QFETCH(QString, context);

    const QString avatarUri = "file:///home/user/.cache/avatars/a888d5a6-2434-480a-8798-23875437bcf3";
    const QString accountPath = "/org/freedesktop/fake/account";
    const QString accountUri = "first.last@talk.com";

    QContactOnlineAccount account;
    account.setValue(QContactOnlineAccount::FieldAccountUri, accountUri);
    account.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
    account.setDetailUri(makeTelepathyIri(accountPath, accountUri));

    if (not context.isEmpty()) {
        account.setContexts(context);
    }

    QContact savedContact;
    QVERIFY(savedContact.saveDetail(&account));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());

    error = QContactManager::UnspecifiedError;
    QContact fetchedContact = engine()->contactImpl(savedContact.localId(),
                                                    NoFetchHint, &error);
    QCOMPARE(fetchedContact.localId(), savedContact.localId());
    QCOMPARE(error, QContactManager::NoError);

    QStringList expectedContexts = account.contexts();

    const QList<QContactOnlineAccount> fetchedAccounts = fetchedContact.details<QContactOnlineAccount>();
    const QList<QContactAvatar> fetchedAvatars = fetchedContact.details<QContactAvatar>();

    QCOMPARE(fetchedAccounts.count(), 1);
    QCOMPARE(fetchedAccounts.first().detailUri(), account.detailUri());
    QCOMPARE(fetchedAccounts.first().contexts(), expectedContexts);

    QCOMPARE(fetchedAvatars.count(), 1);
    QCOMPARE(fetchedAvatars.first().contexts(), QStringList());
    QCOMPARE(fetchedAvatars.first().imageUrl().toString(), avatarUri);
    QCOMPARE(fetchedAvatars.first().linkedDetailUris(), QStringList(account.detailUri()));
}

void
ut_qtcontacts_trackerplugin::testAvatarTypes_data()
{
    const QString testContactIri = QLatin1String("contact:avatar-types");
    const QStringList contactIris = loadRawContacts(QLatin1String("avatars.ttl"));
    QVERIFY(contactIris.contains(testContactIri));

    QctTrackerIdResolver resolver(QStringList() << testContactIri);
    QVERIFY(resolver.lookupAndWait());

    QContactLocalId testContactId = resolver.trackerIds().first();
    QVERIFY(testContactId != 0);

    QTest::addColumn<QString>("avatarTypes");
    QTest::addColumn<QString>("personalAvatarIri");
    QTest::addColumn<QString>("onlineAvatarIri");
    QTest::addColumn<QString>("socialAvatarIri");
    QTest::addColumn<uint>("contactLocalId");

    const QString personalAvatarIri = QLatin1String("file://home/user/.cache/avatars/default/sunshine.jpg");
    const QString onlineAvatarIri = QLatin1String("file://home/user/.cache/avatars/square/snowball.jpg");
    const QString socialAvatarIri = QLatin1String("file://home/user/.cache/avatars/large/snowball.jpg");

    QTest::newRow("default")
            << QString::fromLatin1("")
            << personalAvatarIri << onlineAvatarIri << QString()
            << testContactId;

    QTest::newRow("personal")
            << QString::fromLatin1("personal")
            << personalAvatarIri << QString() << QString()
            << testContactId;
    QTest::newRow("online")
            << QString::fromLatin1("online")
            << QString() << onlineAvatarIri << QString()
            << testContactId;
    QTest::newRow("social")
            << QString::fromLatin1("social")
            << QString() << QString() << socialAvatarIri
            << testContactId;

    QTest::newRow("personal,online,social")
            << QString::fromLatin1("personal,online")
            << personalAvatarIri << onlineAvatarIri << QString()
            << testContactId;
    QTest::newRow("online,social")
            << QString::fromLatin1("online,social")
            << QString() << onlineAvatarIri << socialAvatarIri
            << testContactId;

    QTest::newRow("all")
            << QString::fromLatin1("all")
            << personalAvatarIri << onlineAvatarIri << socialAvatarIri
            << testContactId;
}

void
ut_qtcontacts_trackerplugin::testAvatarTypes()
{
    QFETCH(QString, avatarTypes);
    QFETCH(QString, personalAvatarIri);
    QFETCH(QString, onlineAvatarIri);
    QFETCH(QString, socialAvatarIri);
    QFETCH(uint, contactLocalId);

    QMap<QString, QString> params = makeEngineParams();

    if (avatarTypes.isEmpty()) {
        params.remove(QLatin1String("avatar-types"));
    } else {
        params.insert(QLatin1String("avatar-types"), avatarTypes);
    }

    QScopedPointer<QContactManager> cm(new QContactManager(QLatin1String("tracker"), params));
    const QContact contact = cm->contact(contactLocalId);
    QCOMPARE(QContactManager::NoError, cm->error());
    QCOMPARE(contact.localId(), contactLocalId);

    bool personalAvatarFound = false;
    bool onlineAvatarFound = false;
    bool socialAvatarFound = false;

    foreach(const QContactAvatar &avatar, contact.details<QContactAvatar>()) {
        if (avatar.imageUrl() == personalAvatarIri) {
            QVERIFY(not personalAvatarFound);
            personalAvatarFound = true;
            continue;
        }

        if (avatar.imageUrl() == onlineAvatarIri) {
            QVERIFY(not onlineAvatarFound);
            onlineAvatarFound = true;
            continue;
        }

        if (avatar.imageUrl() == socialAvatarIri) {
            QVERIFY(not socialAvatarFound);
            socialAvatarFound = true;
            continue;
        }

        QFAIL(QLatin1String("Unexpected avatar: ") + avatar.imageUrl().toString());
    }

    QVERIFY(personalAvatarIri.isEmpty() || personalAvatarFound);
    QVERIFY(onlineAvatarIri.isEmpty() || onlineAvatarFound);
    QVERIFY(socialAvatarIri.isEmpty() || socialAvatarFound);
}

void
ut_qtcontacts_trackerplugin::testDateDetail_data()
{
    QTest::addColumn<QString>("definitionName");
    QTest::addColumn<QString>("fieldName");

    QTest::newRow("QContactBirthday::Birthday") << QString(QContactBirthday::DefinitionName.latin1())
                                                << QString(QContactBirthday::FieldBirthday.latin1());
    QTest::newRow("QContactAnniversary::OriginalDate") << QString(QContactAnniversary::DefinitionName.latin1())
                                                       << QString(QContactAnniversary::FieldOriginalDate.latin1());
    QTest::newRow("Made up detail") << QString("MadeUpDetail")
                                    << QString("Date");
}

void
ut_qtcontacts_trackerplugin::testDateDetail()
{
    static QStringList foreignTimeZoneDates;

    foreignTimeZoneDates
        << QLatin1String("1917-02-12T00:00:00+05:54")
        << QLatin1String("2010-01-01T00:00:00+05:30")
        << QLatin1String("2010-01-01T00:00:00-05:00");

    QFETCH(QString, definitionName);
    QFETCH(QString, fieldName);

    QContact c;
    QDate date;
    QDateTime dateTime;

    QContactName name;
    name.setFirstName("test");
    name.setLastName(definitionName);
    c.saveDetail(&name);

    date = QDate::fromString("1960-03-24", Qt::ISODate);
    dateTime.setDate(date);
    QContactDetail detail = QContactDetail(definitionName);
    detail.setValue(fieldName, date);
    c.saveDetail(&detail);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(error, QContactManager::NoError);

    QContactDetail savedDetail = contact(c.localId()).detail(definitionName);
    QVERIFY(not savedDetail.isEmpty());

    QCOMPARE(savedDetail.variantValue(fieldName).toDate(), date);
    QCOMPARE(savedDetail.variantValue(fieldName).toDateTime().toLocalTime(), dateTime);

    foreach (const QString date, foreignTimeZoneDates) {
        QString query;

        if (definitionName == QContactBirthday::DefinitionName) {
            static const QString queryTemplate = QLatin1String
                    ("INSERT OR REPLACE { ?c nco:birthDate '%1' }"
                     "WHERE { ?c a nco:PersonContact . FILTER (tracker:id(?c)=%2) }");
            query = queryTemplate.arg(date).arg(c.localId());
        } else if (definitionName == QContactAnniversary::DefinitionName) {
            static const QString queryTemplate = QLatin1String
                    ("INSERT OR REPLACE { ?e ncal:dateTime \"%1\" }"
                     "WHERE { ?c a nco:PersonContact; ncal:anniversary [ ncal:dtstart ?e ] . "
                     "        FILTER(tracker:id(?c)=%2) }");
            query = queryTemplate.arg(date).arg(c.localId());
        } else if (definitionName == QLatin1String("MadeUpDetail")) {
            static const QString queryTemplate = QLatin1String
                    ("INSERT OR REPLACE { ?p nao:propertyValue \"%1\" }"
                     "WHERE { ?c a nco:PersonContact; "
                     "           nao:hasProperty [ nao:propertyName \"%3\"; nao:hasProperty ?p ] ."
                     "        ?p nao:propertyName \"Date\" . "
                     "        FILTER(tracker:id(?c)=%2) }");
            query = queryTemplate.arg(date).arg(c.localId()).arg(definitionName);
        } else {
            QFAIL("Unknown detail definition");
        }

        QScopedPointer<QSparqlResult> result(executeQuery(query, QSparqlQuery::InsertStatement));

        QVERIFY(not result.isNull());

        savedDetail = contact(c.localId()).detail(definitionName);
        QVERIFY(not savedDetail.isEmpty());
        QCOMPARE(savedDetail.value(fieldName), date);
    }
}

template<class T> static bool
listContainsDetail(const QList<T> &list, const T &detail)
{
    foreach (const T &d, list) {
        if (compareFuzzedDetail(detail, d)) {
            return true;
        }
    }

    return false;
}

void
ut_qtcontacts_trackerplugin::testOrganization()
{
    // Company information
    QContact contactWithCompany1, contactWithoutCompany;
    QContactOrganization company1;
    company1.setName("Nokia");
    company1.setDepartment(QStringList() << "Mobile");
    company1.setTitle("First Bug Hunter");
    company1.setRole("Developer");
    QContactOrganization company2;
    company2.setName("ANPE");
    company2.setDepartment(QStringList() << "Jobless");
    company2.setTitle("The hopeless case");
    company2.setRole("Jobseeker");
    QContactName name1, name2;
    name1.setFirstName("John");
    name1.setLastName("TestCompany1");
    name2.setFirstName("Frankie");
    name2.setLastName("Flowers");
    contactWithCompany1.saveDetail(&name1);
    contactWithCompany1.saveDetail(&company1);
    contactWithCompany1.saveDetail(&company2);
    contactWithoutCompany.saveDetail(&name2);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&contactWithCompany1, &error));
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(engine()->saveContact(&contactWithoutCompany, &error));
    QCOMPARE(error,  QContactManager::NoError);

    QContactLocalId id1 = contactWithCompany1.localId();
    const QList<QContactOrganization> organizations = contact(id1).details<QContactOrganization>();
    QCOMPARE(organizations.size(), 2);
    QVERIFY(listContainsDetail<QContactOrganization>(organizations, company1));
    QVERIFY(listContainsDetail<QContactOrganization>(organizations, company2));

    // NB#192947: Ensure there is no organization in that contact
    QContactLocalId id2 = contactWithoutCompany.localId();
    QCOMPARE(contact(id2).details<QContactOrganization>().size(), 0);
}

void
ut_qtcontacts_trackerplugin::testUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("subtype");

    QTest::newRow("homepage/home")
            << "http://home.homepage/"
            << QContactDetail::ContextHome.latin1()
            << QContactUrl::SubTypeHomePage.latin1();
    QTest::newRow("homepage/work")
            << "http://work.homepage/"
            << QContactDetail::ContextWork.latin1()
            << QContactUrl::SubTypeHomePage.latin1();
    QTest::newRow("homepage/other")
            << "http://other.homepage/"
            << QContactDetail::ContextOther.latin1()
            << QContactUrl::SubTypeHomePage.latin1();

    QTest::newRow("favourite/home")
            << "http://home.favourite/"
            << QContactDetail::ContextHome.latin1()
            << QContactUrl::SubTypeFavourite.latin1();
    QTest::newRow("favourite/work")
            << "http://work.favourite/"
            << QContactDetail::ContextWork.latin1()
            << QContactUrl::SubTypeFavourite.latin1();
    QTest::newRow("favourite/other")
            << "http://other.favourite/"
            << QContactDetail::ContextOther.latin1()
            << QContactUrl::SubTypeFavourite.latin1();

    QTest::newRow("blog/home")
            << "http://home.blog/"
            << QContactDetail::ContextHome.latin1()
            << QContactUrl::SubTypeBlog.latin1();
    QTest::newRow("blog/work")
            << "http://work.blog/"
            << QContactDetail::ContextWork.latin1()
            << QContactUrl::SubTypeBlog.latin1();
    QTest::newRow("blog/other")
            << "http://other.blog/"
            << QContactDetail::ContextOther.latin1()
            << QContactUrl::SubTypeBlog.latin1();

    QTest::newRow("default/home")
            << "http://home.default/"
            << QContactDetail::ContextHome.latin1()
            << QString();
    QTest::newRow("default/work")
            << "http://work.default/"
            << QContactDetail::ContextWork.latin1()
            << QString();
    QTest::newRow("default/other")
            << "http://other.default/"
            << QContactDetail::ContextOther.latin1()
            << QString();
}

void
ut_qtcontacts_trackerplugin::testUrl()
{
    QFETCH(QString, url);
    QFETCH(QString, context);
    QFETCH(QString, subtype);

    // construct and save contact
    QContact savedContact;

    QContactUrl savedUrl;
    savedUrl.setUrl(url);
    savedUrl.setContexts(context);

    if (not subtype.isEmpty()) {
        savedUrl.setSubType(subtype);
    }

    QVERIFY(savedContact.saveDetail(&savedUrl));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error,  QContactManager::NoError);

    // check fetched contact
    const QContactUrl fetchedUrl = contact(savedContact.localId()).detail<QContactUrl>();

    if (subtype.isEmpty()) {
        // test implicit default when needed
        subtype = QContactUrl::SubTypeFavourite.latin1();
    }

    QVERIFY(not fetchedUrl.isEmpty());
    QCOMPARE(fetchedUrl.url(), url);
    QCOMPARE(fetchedUrl.contexts(), QStringList(context));
    QCOMPARE(fetchedUrl.subType(), subtype);

    // edit type
    QContact c1 = contact(savedContact.localId());
    QCOMPARE(c1.details<QContactUrl>().size(), 1);
    QContactUrl detail = c1.detail<QContactUrl>();
    detail.setContexts(QContactDetail::ContextWork);

    c1.saveDetail(&detail);
    QVERIFY(c1.detail<QContactUrl>().contexts().size() == 1);
    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c1, &error));
    QCOMPARE(error,  QContactManager::NoError);
    c1 = contact(savedContact.localId());
    QVERIFY(c1.details<QContactUrl>().size() == 1);


    // add additional URL (NB#178354)
    detail = QContactUrl();
    detail.setUrl("http://meego.com/");
    QVERIFY(c1.saveDetail(&detail));

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&c1, &error));
    QCOMPARE(error,  QContactManager::NoError);

    QSet<QString> expectedUrls = QSet<QString>() << savedUrl.url() << detail.url();
    QSet<QString> fetchedUrls;

    foreach(const QContactUrl &u, contact(savedContact.localId()).details<QContactUrl>()) {
        fetchedUrls.insert(u.url());
    }

    QCOMPARE(fetchedUrls, expectedUrls);
}

static void
compareUrlDetails(const QList<QContactUrl> &referenceUrls,
                  QList<QContactUrl> actualUrls)
{
    int notFoundReferenceUrlsCount = 0;

    // go through all reference urls and see if there is matching one in the actual urls
    // if there is one remove it from the actual urls, so it won't be matched a second time
    foreach(const QContactUrl &referenceUrl, referenceUrls) {
        QList<QContactUrl>::Iterator actualUrlsIt = actualUrls.begin();
        for (; actualUrlsIt != actualUrls.end(); ++actualUrlsIt) {
            if (detailMatches(referenceUrl, *actualUrlsIt)) {
                break;
            }
        }
        if (actualUrlsIt != actualUrls.end()) {
            actualUrls.erase(actualUrlsIt);
        } else {
            qWarning() << "reference url not found:" << referenceUrl;
            notFoundReferenceUrlsCount += 1;
        }
    }
    // report all actual urls that were not matched by the reference urls
    foreach(const QContactUrl &actualUrl, actualUrls) {
            qWarning() << "fetched url not in references:" << actualUrl;
    }
    QCOMPARE(actualUrls.count(), 0);
    QCOMPARE(notFoundReferenceUrlsCount, 0);
}

void
ut_qtcontacts_trackerplugin::testMultipleUrls()
{
    const struct ContextData
    {
        QString definition;
        QString name;
    }
    contexts[] =
    {
        {QContactDetail::ContextHome.latin1(),  QLatin1String("home")},
        // omitting ContextWork, is implemented like ContextHome
        {QContactDetail::ContextOther.latin1(), QLatin1String("other")}
    };
    static const int contextsCount = sizeof(contexts)/sizeof(contexts[0]);

    const struct SubTypeData
    {
        QString id;
        QString name;
    }
    subtypes[] =
    {
        {QContactUrl::SubTypeFavourite.latin1(), QLatin1String("favourite")},
        {QContactUrl::SubTypeHomePage.latin1(),  QLatin1String("homepage")},
        // omitting SubTypeBlog, is implemented like SubTypeHomePage
        {QString(),                              QLatin1String("default")}
    };
    static const int subtypesCount = sizeof(subtypes)/sizeof(subtypes[0]);

    const QString urlTemplate = QLatin1String("http://%1.%2/%3");

    // create combinations of all possible subtypes and contexts, with both orders
    // (so using the full table of combinations, not just the half)
    // All contacts used are stored and fetched in one go,
    // instead of using a _data method, to speed-up the test.
    QList<QContact> referenceContacts;
    for(int c1 = 0; c1 < contextsCount; ++c1) {
        const ContextData &context1 = contexts[c1];
        for(int s1 = 0; s1 < subtypesCount; ++s1) {
            const SubTypeData &subtype1 = subtypes[s1];
            for(int c2 = 0; c2 < contextsCount; ++c2) {
                const ContextData &context2 = contexts[c2];
                for(int s2 = 0; s2 < subtypesCount; ++s2) {
                    const SubTypeData &subtype2 = subtypes[s2];

                    // construct reference contact
                    QContact contact;

                    QContactUrl urlDetail1;
                    urlDetail1.setUrl(urlTemplate.arg(context1.name, subtype1.name, QLatin1String("1")));
                    urlDetail1.setContexts(context1.definition);
                    if (not subtype1.id.isEmpty()) {
                        urlDetail1.setSubType(subtype1.id);
                    }
                    QVERIFY(contact.saveDetail(&urlDetail1));

                    QContactUrl urlDetail2;
                    urlDetail2.setUrl(urlTemplate.arg(context2.name, subtype2.name, QLatin1String("2")));
                    urlDetail2.setContexts(context2.definition);
                    if (not subtype2.id.isEmpty()) {
                        urlDetail2.setSubType(subtype2.id);
                    }
                    QVERIFY(contact.saveDetail(&urlDetail2));

                    referenceContacts << contact;
                }
            }
        }
    }

    // save reference contacts
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const bool saveSuccess = engine()->saveContacts(&referenceContacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(saveSuccess);

    // extract look-up list of reference url pairs from reference contacts
    QHash<QContactLocalId, QList<QContactUrl> > referenceUrlsByLocalId;
    foreach(const QContact &referenceContact, referenceContacts) {
        QList<QContactUrl> referenceUrls = referenceContact.details<QContactUrl>();
        // set default values, if needed
        for(int i = 0; i < referenceUrls.count(); ++i) {
            QContactUrl &url = referenceUrls[i];
            if (url.subType().isEmpty()) {
                url.setSubType(QContactUrl::SubTypeFavourite);
            }
        }
        referenceUrlsByLocalId.insert(referenceContact.localId(), referenceUrls);
    }

    // fetch all contacts
    const QContactLocalIdList localIds = referenceUrlsByLocalId.keys();
    const QContactList fetchedContacts = contacts(localIds);

    // compare url details of fetched contacts with reference details
    foreach(const QContact &fetchedContact, fetchedContacts) {
        QHash<QContactLocalId, QList<QContactUrl> >::Iterator referenceUrlsIt =
            referenceUrlsByLocalId.find(fetchedContact.localId());
        QVERIFY2(referenceUrlsIt != referenceUrlsByLocalId.end(), "Contact fetched which has unknown localId.");

        // check fetched urls
        const QList<QContactUrl> fetchedUrlDetails = fetchedContact.details<QContactUrl>();
        compareUrlDetails(*referenceUrlsIt, fetchedUrlDetails);
        CHECK_CURRENT_TEST_FAILED;

        // remove used reference url detail pair
        referenceUrlsByLocalId.erase(referenceUrlsIt);
    }

    // check if there were contacts fetched for all reference urls
    foreach (const QList<QContactUrl> &referenceUrls, referenceUrlsByLocalId) {
            qWarning() << "No contact fetched for reference url pair:" << referenceUrls;
    }
    QCOMPARE(referenceUrlsByLocalId.count(), 0);
}

void ut_qtcontacts_trackerplugin::testUniqueDetails_data()
{
    QTest::addColumn<QString>("definitionName");
    QTest::addColumn<QString>("fieldName");
    QTest::addColumn<QVariant>("firstValue");
    QTest::addColumn<QVariant>("secondValue");

    QTest::newRow("gender")
            << QString(QContactGender::DefinitionName.latin1())
            << QString(QContactGender::FieldGender.latin1())
            << QVariant(QContactGender::GenderFemale.latin1())
            << QVariant(QContactGender::GenderMale.latin1());

    QTest::newRow("gender2")
            << QString(QContactGender::DefinitionName.latin1())
            << QString(QContactGender::FieldGender.latin1())
            << QVariant(QContactGender::GenderFemale.latin1())
            << QVariant(QContactGender::GenderUnspecified.latin1());
}

void ut_qtcontacts_trackerplugin::testUniqueDetails()
{
    QFETCH(QString, definitionName);
    QFETCH(QString, fieldName);
    QFETCH(QVariant, firstValue);
    QFETCH(QVariant, secondValue);

    QContact savedContact;

    QContactDetail first(definitionName);
    first.setValue(fieldName, firstValue);
    QVERIFY(savedContact.saveDetail(&first));

    QContactDetail second(definitionName);
    second.setValue(fieldName, secondValue);
    QVERIFY(savedContact.saveDetail(&second));

    const QString warning = "Dropping odd details: %1 detail must be unique";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning.arg(definitionName)));

    qctLogger().setShowLocation(false);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    bool success(engine()->saveContact(&savedContact, &error));
    qctLogger().setShowLocation(true);

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());
    QCOMPARE(success, true);

    error = QContactManager::UnspecifiedError;
    QContact fetchedContact(engine()->contactImpl(savedContact.localId(),
                                                  NoFetchHint, &error));
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(fetchedContact.localId(), savedContact.localId());

    QCOMPARE(fetchedContact.details(definitionName).count(), 1);
    QCOMPARE(fetchedContact.detail(definitionName).variantValue(fieldName), firstValue);
}

template<class K, class V> static QHash<K, V>
toHash(const QMap<K,V> &map)
{
    QHash<K,V> hash;

    for(typename QMap<K,V>::ConstIterator i = map.constBegin();
    i != map.constEnd(); ++i) {
        hash.insert(i.key(), i.value());
    }

    return hash;
}

void
ut_qtcontacts_trackerplugin::testCustomDetails()
{
    QContact savedContact;

    QContactDetail simple("FavoriteColor");
    simple.setValue(simple.definitionName(), "Blue");
    QVERIFY(savedContact.saveDetail(&simple));

    QContactDetail complex("AllTimeFavorites");
    complex.setValue("Song", "Underworld - Born Slippy");
    complex.setValue("Phone", "Nokia N900");
    complex.setValue("Place", "Home");
    QVERIFY(savedContact.saveDetail(&complex));

    QContactDetail multi("CreditCardNumber");
    multi.setValue(multi.definitionName(), "1122334455");
    QVERIFY(savedContact.saveDetail(&multi));

    QContactDetail multi2(multi.definitionName());
    multi2.setValue(multi2.definitionName(), "5544332211");
    QVERIFY(savedContact.saveDetail(&multi2));

    QContactDetail list("Things");
    list.setValue(list.definitionName(), QStringList() << "Foo" << "Bar" << "Blub");
    QVERIFY(savedContact.saveDetail(&list));

    QContactManager::Error error(QContactManager::UnspecifiedError);
    bool success(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());
    QCOMPARE(success, true);

    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(QStringList() <<
                                       simple.definitionName() <<
                                       complex.definitionName() <<
                                       multi.definitionName() <<
                                       list.definitionName());

    error = QContactManager::UnspecifiedError;
    QContact fetchedContact(engine()->contactImpl(savedContact.localId(), fetchHint, &error));
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(fetchedContact.localId(), savedContact.localId());

    QCOMPARE(fetchedContact.details(simple.definitionName()).count(), 1);
    QCOMPARE(fetchedContact.detail(simple.definitionName()).variantValues(),
             simple.variantValues());

    QCOMPARE(fetchedContact.details(complex.definitionName()).count(), 1);
    QCOMPARE(toHash(fetchedContact.detail(complex.definitionName()).variantValues()),
             toHash(complex.variantValues()));

    QCOMPARE(fetchedContact.details(multi.definitionName()).count(), 2);

    bool multiFound = false;
    bool multi2Found = false;

    foreach(const QContactDetail &detail, fetchedContact.details(multi.definitionName())) {
        if (detail == multi) {
            multiFound = true;
            continue;
        }

        if (detail == multi2) {
            multi2Found = true;
            continue;
        }

        // yes this leaks, but this is ok when a test fails
        qDebug() << detail << multi << multi2;
        QFAIL(QTest::toString(detail.variantValues()));
    }

    QCOMPARE(fetchedContact.details(list.definitionName()).count(), 1);
    QCOMPARE(fetchedContact.detail(list.definitionName()).variantValues(),
             list.variantValues());

    QVERIFY(multiFound);
    QVERIFY(multi2Found);
}

typedef QPair<QStringList, QStringList> SubTypeSample;
typedef QList<SubTypeSample> SubTypeSampleList;

typedef QPair<QString, QVariant> FieldNameAndValue;
typedef QList<FieldNameAndValue> FieldNameAndValueList;

Q_DECLARE_METATYPE(FieldNameAndValueList)
Q_DECLARE_METATYPE(QContactAbstractRequest::State)
Q_DECLARE_METATYPE(Qt::ConnectionType)
Q_DECLARE_METATYPE(SubTypeSampleList)

void
ut_qtcontacts_trackerplugin::testRemoveSubType_data()
{
    // NOTE: When extending the data set please order the subtypes such that more
    // and more (implicit and  real) subtypes get remove with each iteration.
    // When possible and reasonable, of course.

    QTest::addColumn<QString>("detailName");
    QTest::addColumn<FieldNameAndValueList>("fieldNameAndValueList");
    QTest::addColumn<QString>("subTypesField");
    QTest::addColumn<SubTypeSampleList>("samples");

    // by class
    QTest::newRow("phone number")
            << (QString::fromLatin1(QContactPhoneNumber::DefinitionName.latin1()))
            << (FieldNameAndValueList() <<
                qMakePair(QString::fromLatin1(QContactPhoneNumber::FieldNumber.latin1()),
                          QVariant(QLatin1String("33445566"))))
            << (QString::fromLatin1(QContactPhoneNumber::FieldSubTypes.latin1()))
            << (SubTypeSampleList() <<
                qMakePair(QStringList() <<
                          QContactPhoneNumber::SubTypeFax <<
                          QContactPhoneNumber::SubTypeMobile,
                          QStringList() <<
                          QContactPhoneNumber::SubTypeFax <<
                          QContactPhoneNumber::SubTypeMessagingCapable <<
                          QContactPhoneNumber::SubTypeMobile <<
                          QContactPhoneNumber::SubTypeVoice) <<
                qMakePair(QStringList() <<
                          QContactPhoneNumber::SubTypeMobile,
                          QStringList() <<
                          QContactPhoneNumber::SubTypeMessagingCapable <<
                          QContactPhoneNumber::SubTypeMobile <<
                          QContactPhoneNumber::SubTypeVoice) <<
                qMakePair(QStringList() <<
                          QContactPhoneNumber::SubTypeLandline,
                          QStringList() <<
                          QContactPhoneNumber::SubTypeLandline <<
                          QContactPhoneNumber::SubTypeVoice) <<
                qMakePair(QStringList() <<
                          QContactPhoneNumber::SubTypeVoice,
                          QStringList() <<
                          QContactPhoneNumber::SubTypeVoice));

    // by class
    QTest::newRow("street address")
            << (QString::fromLatin1(QContactAddress::DefinitionName.latin1()))
            << (FieldNameAndValueList() <<
                qMakePair(QString::fromLatin1(QContactAddress::FieldCountry.latin1()),
                          QVariant(QLatin1String("Finnland"))))
            << (QString::fromLatin1(QContactAddress::FieldSubTypes.latin1()))
            << (SubTypeSampleList() <<
                qMakePair(QStringList() <<
                          QContactAddress::SubTypePostal <<
                          QContactAddress::SubTypeInternational <<
                          QContactAddress::SubTypeParcel,
                          QStringList() <<
                          QContactAddress::SubTypeInternational <<
                          QContactAddress::SubTypeParcel <<
                          QContactAddress::SubTypePostal) <<
                qMakePair(QStringList() <<
                          QContactAddress::SubTypeInternational <<
                          QContactAddress::SubTypeParcel,
                          QStringList() <<
                          QContactAddress::SubTypeInternational <<
                          QContactAddress::SubTypeParcel) <<
                qMakePair(QStringList() <<
                          QContactAddress::SubTypeInternational,
                          QStringList() <<
                          QContactAddress::SubTypeInternational));

    // by property
    QTest::newRow("url")
            << (QString::fromLatin1(QContactUrl::DefinitionName.latin1()))
            << (FieldNameAndValueList() <<
                qMakePair(QString::fromLatin1(QContactUrl::FieldUrl.latin1()),
                          QVariant(QLatin1String("http://openismus.com/"))))
            << (QString::fromLatin1(QContactUrl::FieldSubType.latin1()))
            << (SubTypeSampleList() <<
                qMakePair(QStringList() <<
                          QContactUrl::SubTypeHomePage,
                          QStringList() <<
                          QContactUrl::SubTypeHomePage) <<
                qMakePair(QStringList() <<
                          QContactUrl::SubTypeBlog,
                          QStringList() <<
                          QContactUrl::SubTypeBlog) <<
                qMakePair(QStringList() <<
                          QContactUrl::SubTypeFavourite,
                          QStringList() <<
                          QContactUrl::SubTypeFavourite));

    // without explicit RDF mapping. Stored via nao:Property
    QTest::newRow("online account")
            << (QString::fromLatin1(QContactOnlineAccount::DefinitionName.latin1()))
            << (FieldNameAndValueList() <<
                qMakePair(QString::fromLatin1(QContactOnlineAccount::FieldAccountUri.latin1()),
                          QVariant(QLatin1String("knut@Yeti"))) <<
                qMakePair(QString::fromLatin1(QContactOnlineAccount__FieldAccountPath.latin1()),
                          QVariant(QLatin1String("/org/Yeti/knut"))))
            << (QString::fromLatin1(QContactOnlineAccount::FieldSubTypes.latin1()))
            << (SubTypeSampleList() <<
                qMakePair(QStringList() <<
                          QContactOnlineAccount::SubTypeImpp <<
                          QContactOnlineAccount::SubTypeSip <<
                          QContactOnlineAccount::SubTypeSipVoip <<
                          QContactOnlineAccount::SubTypeVideoShare,
                          QStringList() <<
                          QContactOnlineAccount::SubTypeImpp <<
                          QContactOnlineAccount::SubTypeSip <<
                          QContactOnlineAccount::SubTypeSipVoip <<
                          QContactOnlineAccount::SubTypeVideoShare) <<
                qMakePair(QStringList() <<
                          QContactOnlineAccount::SubTypeImpp,
                          QStringList() <<
                          QContactOnlineAccount::SubTypeImpp) <<
                qMakePair(QStringList() <<
                          QContactOnlineAccount::SubTypeSip,
                          QStringList() <<
                          QContactOnlineAccount::SubTypeSip) <<
                qMakePair(QStringList() <<
                          QContactOnlineAccount::SubTypeSipVoip,
                          QStringList() <<
                          QContactOnlineAccount::SubTypeSipVoip) <<
                qMakePair(QStringList() <<
                          QContactOnlineAccount::SubTypeVideoShare,
                          QStringList() <<
                          QContactOnlineAccount::SubTypeVideoShare));
}

void
ut_qtcontacts_trackerplugin::testRemoveSubType()
{
    QFETCH(QString, detailName);
    QFETCH(FieldNameAndValueList, fieldNameAndValueList);
    QFETCH(QString, subTypesField);
    QFETCH(SubTypeSampleList, samples);

    foreach(const SubTypeSample &sample, samples) {
        // create contact with given detail
        QContactDetail detail(detailName);
        foreach (const FieldNameAndValue &fieldNameAndValue, fieldNameAndValueList) {
            detail.setValue(fieldNameAndValue.first, fieldNameAndValue.second);
        }
        detail.setValue(subTypesField, sample.first);

        QContact savedContact;
        QVERIFY(savedContact.saveDetail(&detail));

        // store the contact
        QContactManager::Error error(QContactManager::NoError);
        QVERIFY(engine()->saveContact(&savedContact, &error));
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != savedContact.localId());

        // refetch
        QContact fetchedContact(engine()->contactImpl(savedContact.localId(),
                                                      NoFetchHint, &error));

        // check
        QCOMPARE(error, QContactManager::NoError);
        QCOMPARE(fetchedContact.localId(), savedContact.localId());

        detail = fetchedContact.detail(detailName);
        QCOMPARE(detail.value<QStringList>(subTypesField).toSet(), sample.second.toSet());
        foreach (const FieldNameAndValue &fieldNameAndValue, fieldNameAndValueList) {
            QCOMPARE(detail.variantValue(fieldNameAndValue.first), fieldNameAndValue.second);
        }
    }
}

/// Returns the labels of all the @param tags as stringlist, in order of the tags.
static QStringList
tagLabels(const QList<QContactTag> &tags)
{
    QStringList result;
    foreach(const QContactTag &tag, tags) {
        result << tag.tag();
    }
    return result;
}

void
ut_qtcontacts_trackerplugin::testTags_data()
{
    QTest::addColumn<bool>("useConvenience");

    QTest::newRow("as details") << false;
    QTest::newRow("as property") << true;
}

void
ut_qtcontacts_trackerplugin::testTags()
{
    QFETCH(bool, useConvenience);

    static const QString tag1Label = QLatin1String("Test1");
    static const QString tag2Label = QLatin1String("Test2");

    // create contact
    QContact contact;
    SET_TESTNICKNAME_TO_CONTACT(contact);

    // add tags to contact and save
    QContactTag tag1;
    tag1.setTag(tag1Label);
    QContactTag tag2;
    tag2.setTag(tag2Label);

    if (useConvenience) {
        contact.setTags(QStringList() << tag1Label << tag2Label);
    } else {
        QVERIFY(contact.saveDetail(&tag1));
        QVERIFY(contact.saveDetail(&tag2));
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool saveSuccess = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(saveSuccess);

    // fetch contact with tags
    QContact fetchedContact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
    QCOMPARE(fetchedContact.localId(), contact.localId());
    QCOMPARE(error, QContactManager::NoError);

    // check for tags by detail and by convenience method
    QList<QContactTag> fetchedTags = fetchedContact.details<QContactTag>();
    QCOMPARE(fetchedTags.count(), 2);
    QStringList fetchedLabels = tagLabels(fetchedTags);
    QVERIFY(fetchedLabels.contains(tag1Label));
    QVERIFY(fetchedLabels.contains(tag2Label));
    QStringList fetchedContactTags = fetchedContact.tags();
    QCOMPARE(fetchedContactTags.count(), 2);
    QVERIFY(fetchedContactTags.contains(tag1Label));
    QVERIFY(fetchedContactTags.contains(tag2Label));

    // remove tags from contact and save
    if (useConvenience) {
        contact.setTags(QStringList());
    } else {
        QVERIFY(contact.removeDetail(&tag1));
        QVERIFY(contact.removeDetail(&tag2));
    }
    QCOMPARE(contact.details<QContactTag>().count(), 0);
    QCOMPARE(contact.tags().count(), 0);

    error = QContactManager::UnspecifiedError;
    saveSuccess = engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(saveSuccess);

    // fetch again
    fetchedContact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
    QCOMPARE(fetchedContact.localId(), contact.localId());
    QCOMPARE(error, QContactManager::NoError);

    // check for no tags by detail and by convenience method
    fetchedTags = fetchedContact.details<QContactTag>();
    QCOMPARE(fetchedTags.count(), 0);
    QCOMPARE(fetchedContact.tags().count(), 0);
}

void
ut_qtcontacts_trackerplugin::testFavouriteTag()
{
    static const QLatin1String favorite("favourite");

    QContactName name;
    name.setFirstName("Tuck");
    name.setLastName("Sherwood");
    name.resetKey(); // XXX workaround for qtcontacts bug

    QContactTag tag;
    tag.setTag(favorite);
    tag.resetKey(); // XXX workaround for qtcontacts bug

    // save contact with favorite tag
    QContact savedContact;
    QVERIFY(savedContact.saveDetail(&name));
    QVERIFY(savedContact.saveDetail(&tag));

    QList<QContactTag> favoriteTags;
    favoriteTags = savedContact.details<QContactTag>(QContactTag::FieldTag, favorite);
    QCOMPARE(favoriteTags.count(), 1);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());

    // fetch contact with favorite tag
    QContact fetchedContact(engine()->contactImpl(savedContact.localId(),
                                                  NoFetchHint, &error));
    QCOMPARE(fetchedContact.localId(), savedContact.localId());
    QCOMPARE(error, QContactManager::NoError);

    favoriteTags = fetchedContact.details<QContactTag>(QContactTag::FieldTag, favorite);
    QCOMPARE(favoriteTags.count(), 1);

    // save same contact without favorite tag
    QVERIFY(savedContact.removeDetail(&tag));
    favoriteTags = savedContact.details<QContactTag>(QContactTag::FieldTag, favorite);
    QCOMPARE(favoriteTags.count(), 0);

    QVERIFY(engine()->saveContact(&savedContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != savedContact.localId());

    // fetch contact without favorite tag
    fetchedContact = engine()->contactImpl(savedContact.localId(),
                                           NoFetchHint, &error);
    QCOMPARE(fetchedContact.localId(), savedContact.localId());
    QCOMPARE(error, QContactManager::NoError);

    favoriteTags = fetchedContact.details<QContactTag>(QContactTag::FieldTag, favorite);
    QCOMPARE(favoriteTags.count(), 0);
}

void
ut_qtcontacts_trackerplugin::testSyncContactManagerContactsAddedSince()
{
    QDateTime start;
    QList<QContactLocalId> addedIds;
    syncContactsAddedSinceHelper(start, addedIds);

    QContactChangeLogFilter filter(QContactChangeLogFilter::EventAdded);
    filter.setSince(start);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> contactIds = engine()->contactIds(filter, NoSortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(contactIds.size(), addedIds.size());
}

void
ut_qtcontacts_trackerplugin::testSyncTrackerEngineContactsIdsAddedSince()
{
    QDateTime start;
    QList<QContactLocalId> addedIds;
    syncContactsAddedSinceHelper(start, addedIds);

    QContactChangeLogFilter filter(QContactChangeLogFilter::EventAdded);
    filter.setSince(start);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> contactIds = engine()->contactIds(filter, NoSortOrders, &error);
    QCOMPARE(contactIds.size(), addedIds.size());
}

void
ut_qtcontacts_trackerplugin::testSyncContactManagerContactIdsAddedSince()
{
    QDateTime start;
    QList<QContactLocalId> addedIds;
    syncContactsAddedSinceHelper(start, addedIds);
    QContactChangeLogFilter filter(QContactChangeLogFilter::EventAdded);
    filter.setSince(start);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> contactIds = engine()->contactIds(filter, NoSortOrders, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QCOMPARE(contactIds.size(), addedIds.size());
}


void
ut_qtcontacts_trackerplugin::syncContactsAddedSinceHelper(QDateTime& start,
                                                          QList<QContactLocalId>& addedIds)
{
    for (int i = 0; i < 3; i++) {
        QContact c;
        QContactName name;
        name.setFirstName("A"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
    }

    QTest::qWait(1000);
    start = QDateTime::currentDateTime();

    for (int i = 0; i < 3; i++) {
        QContact c;
        QContactName name;
        name.setFirstName("B"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
        addedIds.append(c.localId());
    }
}

void
ut_qtcontacts_trackerplugin::testContactsAddedSince()
{
    QList<QContactLocalId> addedIds;
    QDateTime start;
    for (int i = 0; i < 3; i++) {
        QContact c;
        QContactName name;
        name.setFirstName("A"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
    }

    QTest::qWait(2000);
    start = QDateTime::currentDateTime().addSecs(-1);

    for (int i = 0; i < 3; i++) {
        QContact c;
        QContactName name;
        name.setFirstName("B"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
        addedIds.append(c.localId());
    }
    QTest::qWait(2000);

    for(int i = 0; i < 100; i++)
    {
        usleep(20000);
        QCoreApplication::processEvents();
    }


    // now one asynchronous request to read all the
    QContactFetchRequest request;
    QContactChangeLogFilter filter(QContactChangeLogFilter::EventAdded);
    filter.setSince(start);
    request.setFilter(filter);

    // here You specify which details are of interest
    QStringList details;
    details << QContactAvatar::DefinitionName
            << QContactBirthday::DefinitionName
            << QContactAddress::DefinitionName
            << QContactEmailAddress::DefinitionName
            << QContactDisplayLabel::DefinitionName
            << QContactGender::DefinitionName
            << QContactAnniversary::DefinitionName
            << QContactName::DefinitionName
            << QContactOnlineAccount::DefinitionName
            << QContactOrganization::DefinitionName
            << QContactPhoneNumber::DefinitionName;
    request.setFetchHint(fetchHint(details));


    Slots slot;
    QObject::connect(&request, SIGNAL(resultsAvailable()),
            &slot, SLOT(resultsAvailable()));

    // start. clients should, instead of following use
    // request.setManager(trackermanagerinstance);
    // request.start();
    engine()->startRequest(&request);
    engine()->waitForRequestFinishedImpl(&request, 0);
    // if it takes more, then something is wrong
    QVERIFY(request.isFinished());
    QCOMPARE(request.error(), QContactManager::NoError);
    QCOMPARE(slot.contacts.count(), addedIds.count());

    foreach(QContact cont, slot.contacts) {
        QVERIFY2(addedIds.contains(cont.localId()), "One of the added contacts was not reported as added");
    }

    QContactLocalIdFetchRequest idreq;
    filter.setSince(start);
    idreq.setFilter(filter);

    Slots slot2;
    QObject::connect(&idreq, SIGNAL(resultsAvailable()),
            &slot2, SLOT(idResultsAvailable()));
    engine()->startRequest(&idreq);
    engine()->waitForRequestFinishedImpl(&idreq, 0);
    QVERIFY(idreq.isFinished());
    QCOMPARE(slot2.ids.count(), addedIds.count());
    foreach(QContactLocalId id, slot2.ids) {
        QVERIFY2(addedIds.contains(id), "One of the added contacts was not reported as added");
    }

}

void
ut_qtcontacts_trackerplugin::testContactsModifiedSince()
{
    QDateTime start;
    QList<QContactLocalId> addedIds;
    QList<QContactLocalId> modified;

    const int contactsToAdd = 5;
    const int contactsToModify = 3;
    QVERIFY2(contactsToAdd >= contactsToModify, "Cannot modify more contacts than this test has added");
    QVERIFY2(contactsToModify+1 <= contactsToAdd, "Cannot modify more contacts than this test has added");

    // Add contacts with only first name and store them to list of added
    for (int i = 0; i < contactsToAdd; i++) {
        QContact c;
        QContactName name;
        name.setFirstName("A"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
        addedIds.append(c.localId());
    }

    QTest::qWait(2000);
    start = QDateTime::currentDateTime();

   // Modify and save rest of the contacts
    for (int i = 0; i < contactsToModify; i++) {
        QContact c = contact(addedIds[i]);
        QContactName name = c.detail<QContactName>();
        // Modify name
        name.setFirstName("B"+QString::number(i));
        QVERIFY2(c.saveDetail(&name), qPrintable(name.firstName()));
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY2(engine()->saveContact(&c, &error), qPrintable(name.firstName()));
        QCOMPARE(error,  QContactManager::NoError);
        modified.append(c.localId());
    }
    // Set filter
    QContactChangeLogFilter filter(QContactChangeLogFilter::EventChanged);
    filter.setSince(start);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QList<QContactLocalId> actuallyModifiedIds = engine()->contactIds(filter, NoSortOrders, &error);
    QCOMPARE(error,  QContactManager::NoError);

    error = QContactManager::UnspecifiedError;
    QList<QContact> actuallyModified = engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);

    // Num of actually modified should be same as supposedly modified
    QCOMPARE(actuallyModifiedIds.count(), modified.count());
    QCOMPARE(actuallyModified.count(), modified.count());

    // All the ids of the modified contacts should be found in the result list
    foreach (QContactLocalId id, modified) {
        QVERIFY2(actuallyModifiedIds.contains(id), "One the modified contacts was not reported as modified");
    }
}

void
ut_qtcontacts_trackerplugin::testContactsRemovedSince()
{
    QDateTime start = QDateTime::currentDateTime();
    QContactChangeLogFilter filter(QContactChangeLogFilter::EventRemoved);
    filter.setSince(start);

    QTest::ignoreMessage(QtWarningMsg,
                         "QContactFilter::ChangeLogFilter: Unsupported event type: "
                         "QContactChangeLogFilter::EventRemoved");

    qctLogger().setShowLocation(false);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> actuallyRemoved = engine()->contactIds(filter, NoSortOrders, &error);
    qctLogger().setShowLocation(true);

    QCOMPARE(error,  QContactManager::NotSupportedError);
    QVERIFY(actuallyRemoved.isEmpty());
}

void
ut_qtcontacts_trackerplugin::cleanupTestCase()
{
    qctLogger().setShowLocation(true);
    resetEngine();
}

void
ut_qtcontacts_trackerplugin::cleanup()
{
    QContactManager::Error error;

    foreach (QContactLocalId id, addedContacts) {
        engine()->removeContact(id, &error);
    }

    addedContacts.clear();

    ut_qtcontacts_trackerplugin_common::cleanup();
}


void
ut_qtcontacts_trackerplugin::testAsyncReadContacts()
{
    addedContacts.clear();
    // Add at least one contact to be sure that this doesn't fail because tracker is clean

    QStringList firstNames, lastNames;
    firstNames << "aa" << "ab" << "ac" << "dd" << "fe";
    lastNames << "fe" << "ab" << "dd" << "dd" << "aa";
    for (int i = 0; i < firstNames.count(); i++) {
        QContact c;
        QContactName name;
        name.setFirstName(firstNames.at(i));
        name.setLastName(lastNames.at(i));
        QContactAvatar avatar;
        avatar.setImageUrl(QUrl("default_avatar.png"));
        QVERIFY(c.saveDetail(&name));
        QVERIFY(c.saveDetail(&avatar));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        addedContacts.append(c.localId());
    }

    // Prepare the filter for the request - we really should test only the contact we add here.
    const QContactLocalIdFilter filter = localIdFilter(addedContacts);

    // this one will get complete contacts

    Slots slot;
    QContactFetchRequest request;
    QList<QContactSortOrder> sorting;
    QContactSortOrder sort, sort1;
    sort.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldLastName);
    sort1.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    sorting << sort << sort1;
    QStringList details; details << QContactName::DefinitionName << QContactAvatar::DefinitionName;
    request.setFetchHint(fetchHint(details));
    request.setSorting(sorting);
    request.setFilter(filter);

    QObject::connect(&request, SIGNAL(resultsAvailable()),
            &slot, SLOT(resultsAvailable()));

    // this one only ids
    QContactLocalIdFetchRequest request1;
    request1.setFilter(filter);
    QObject::connect(&request1, SIGNAL(resultsAvailable()),
            &slot, SLOT(idResultsAvailable()));

    // the purpose is to compare if all contacts are loaded, and
    // if optional fields are defined properly in request

    // start both at once
    engine()->startRequest(&request);
    engine()->startRequest(&request1);
    engine()->waitForRequestFinishedImpl(&request, 0);
    engine()->waitForRequestFinishedImpl(&request1, 0);


    // if it takes more, then something is wrong
    QVERIFY(request.isFinished());
    QVERIFY(request1.isFinished());

    // there need1 to be something added to be verified
    QVERIFY(!request.contacts().isEmpty());
    // now ask for one contact
    QVERIFY(!slot.contacts.isEmpty());
    // there need to be something added to be verified
    QVERIFY(!request1.ids().isEmpty());
    // now ask for one contact
    QVERIFY(!slot.ids.isEmpty());

    QCOMPARE(slot.contacts.count(), slot.ids.count());
    QVERIFY(slot.contacts.count() >= firstNames.count());
    for( int i = 0; i < slot.contacts.size() -1 ; i++)
    {
        QContact contact = slot.contacts[i];
        QContact contact1 = slot.contacts[i+1];
        QString last0 = contact.detail<QContactName>().lastName();
        QString first0 = contact.detail<QContactName>().firstName();
        QString last1 = contact1.detail<QContactName>().lastName();
        QString first1 = contact1.detail<QContactName>().firstName();
        // sorting
        qDebug() << "contacts:" << contact.localId() << first0 << last0;
        bool test = last0 < last1 || (last0 == last1 && first0 <= first1);
        if (!test) {
            qDebug() << "contacts sort failed. First: " << contact1.localId() << first0 << last1 << "lasts: " << last0 << last1;
        }
        QVERIFY2(test, "Sorting failed.");
    }

}

void
ut_qtcontacts_trackerplugin::testSortContacts()
{
    // create sample contacts
    QList<PairOfStrings> samples;
    samples << qMakePair(QString::fromLatin1("Adelhoefer"), QString::fromLatin1("9"));
    samples << qMakePair(QString::fromLatin1("Lose"), QString::fromLatin1("8"));
    samples << qMakePair(QString::fromLatin1("Zumwinkel"), QString::fromLatin1("7"));
    QList<QContact> contacts;
    QStringList lastNames;

    foreach(const PairOfStrings &sample, samples) {
        QContact c;
        QContactName name;
        QContactNickname nickname;
        name.setLastName(sample.first);
        nickname.setNickname(sample.second);
        c.saveDetail(&name);
        c.saveDetail(&nickname);
        contacts.append(c);
        lastNames.append(sample.first);
    }

    // save sample contacts
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool success = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    // build local id list
    QContactLocalIdList localIds;

    for(int i = 0; i < contacts.count(); ++i) {
        QVERIFY2(0 != contacts[i].localId(), qPrintable(QString::number(i)));
        localIds.append(contacts[i].localId());
    }

    QCOMPARE(localIds.count(), samples.count());

    // prepare fetch request
    const QContactLocalIdFilter filter = localIdFilter(localIds);

    const QContactFetchHint nameFetchHint = fetchHint<QContactName>();

    QList<QContactSortOrder> sortOrders;
    sortOrders.append(QContactSortOrder());

    sortOrders.last().setDetailDefinitionName(QContactName::DefinitionName,
                                              QContactName::FieldLastName);

    // fetch contacts in ascending order
    error = QContactManager::UnspecifiedError;
    sortOrders.last().setDirection(Qt::AscendingOrder);
    contacts = engine()->contacts(filter, sortOrders, nameFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(contacts.count(), localIds.count());

    // verify fetched contacts are in ascending order
    QContactLocalIdList fetchedIds;
    QStringList fetchedNames;

    foreach(const QContact &c, contacts) {
        fetchedNames.append(c.detail<QContactName>().lastName());
        fetchedIds.append(c.localId());
    }

    QCOMPARE(fetchedNames, lastNames);
    QCOMPARE(fetchedIds, localIds);

    // fetch contacts in descending order
    error = QContactManager::UnspecifiedError;
    sortOrders.last().setDirection(Qt::DescendingOrder);
    contacts = engine()->contacts(filter, sortOrders, nameFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(contacts.count(), localIds.count());

    // verify fetched contacts are in descending order
    fetchedNames.clear();
    fetchedIds.clear();

    foreach(const QContact &c, contacts) {
        fetchedNames.prepend(c.detail<QContactName>().lastName());
        fetchedIds.prepend(c.localId());
    }

    QCOMPARE(fetchedNames, lastNames);
    QCOMPARE(fetchedIds, localIds);

    // check that details needed for sorting are always fetched
    // for that, we don't fetch the QContactNickname, and ask for ascending sorting
    // on nickname first, and ascending sorting on name then. Those two orders
    // lead different results, so if we get the "name" one, it means nickname
    // sorting failed.
    error = QContactManager::UnspecifiedError;
    sortOrders.clear();

    sortOrders.append(QContactSortOrder());
    sortOrders.last().setDirection(Qt::AscendingOrder);
    sortOrders.last().setDetailDefinitionName(QContactNickname::DefinitionName,
                                              QContactNickname::FieldNickname);

    sortOrders.append(QContactSortOrder());
    sortOrders.last().setDirection(Qt::AscendingOrder);
    sortOrders.last().setDetailDefinitionName(QContactName::DefinitionName,
                                              QContactName::FieldLastName);

    contacts = engine()->contacts(filter, sortOrders, nameFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(contacts.count(), localIds.count());

    // verify fetched contacts are in descending order
    fetchedNames.clear();
    fetchedIds.clear();

    foreach(const QContact &c, contacts) {
        fetchedNames.prepend(c.detail<QContactName>().lastName());
        fetchedIds.prepend(c.localId());
    }

    QCOMPARE(fetchedNames, lastNames);
    QCOMPARE(fetchedIds, localIds);

    // Check sorting for LocalIdFetchRequest
    // Sorting is still ascending on nickname = inverse order of localIds
    error = QContactManager::UnspecifiedError;
    QContactLocalIdList sortedLocalIds = engine()->contactIds(filter, sortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);
    QCOMPARE(sortedLocalIds.size(), localIds.size());

    for(int i = 0; i < sortedLocalIds.size(); ++i) {
        QCOMPARE(sortedLocalIds.at(i), localIds.at(localIds.size() - i - 1));
    }
}

template<class T> QVariantList
testSparqlSorting_makeValues(int /* nValues */)
{
    qFatal("%s: Unhandled type", Q_FUNC_INFO);
    return QVariantList();
}

template<> QVariantList
testSparqlSorting_makeValues<QString>(int nValues)
{
    QVariantList values;
    const int numberSize = 1 + (int)log10(nValues);

    for (int i = 0; i < nValues; ++i) {
        values.append(QString::fromLatin1("%1").arg(i, numberSize, 10, QLatin1Char('0')));
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<QStringList>(int nValues)
{
    QVariantList values;

    foreach (const QVariant &str, testSparqlSorting_makeValues<QString>(nValues)) {
        values.append(QStringList() << str.toString());
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<QDateTime>(int nValues)
{
    QVariantList values;

    for (int i = 0; i < nValues; ++i) {
        values.append(QDateTime::currentDateTime());
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<QUrl>(int nValues)
{
    QVariantList values;

    foreach (const QVariant &str, testSparqlSorting_makeValues<QString>(nValues)) {
        values.append(QUrl(str.toString()));
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<bool>(int nValues)
{
    QVariantList values;

    for (int i = 0; i < nValues; ++i) {
        values.append(QVariant(bool(i%2)));
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<int>(int nValues)
{
    QVariantList values;

    for (int i = 0; i < nValues; ++i) {
        values.append(i);
    }

    return values;
}

template<> QVariantList
testSparqlSorting_makeValues<double>(int nValues)
{
    QVariantList values;

    for (int i = 0; i < nValues; ++i) {
        values.append((double) i);
    }

    return values;
}

static bool
testSparqlSorting_valueIsLess(const QVariant &a, const QVariant &b)
{
    if (a.type() != b.type()) {
        qFatal("Variant types must be of equal type for sorting");
        return a.constData() < b.constData();
    }

    switch(a.type()) {
    case QVariant::String:
        return a.toString() < b.toString();

    default:
        qFatal("Unsupported variant type for sorting: %s", a.typeName());
        return a.constData() < b.constData();
    }
}

static QVariantList
testSparqlSorting_makeFieldValues(const QString &detailName, const QString &fieldName,
                                  const QContactDetailFieldDefinition &field, int nValues)
{
    QVariantList allowableValues = field.allowableValues();

    if (not allowableValues.isEmpty()) {
        qSort(allowableValues.begin(), allowableValues.end(),
              testSparqlSorting_valueIsLess);

        if (field.dataType() != allowableValues.first().type()) {
            if (field.dataType() == QVariant::StringList) {
                const bool isSubTypes = (QLatin1String("SubTypes") == fieldName);

                for(int i = 0; i < allowableValues.size(); ++i) {
                    QStringList values = QStringList(allowableValues.at(i).toString());

                    if (isSubTypes) {
                        values = impliedSubTypes(detailName, values);
                    }

                    allowableValues[i] = values;
                }
            } else {
                qFatal("Unhandled type %u", field.dataType());
                return QVariantList();
            }
        }

        return allowableValues;
    }

    switch (field.dataType()) {
    case QVariant::String:
        return testSparqlSorting_makeValues<QString>(nValues);
    case QVariant::StringList:
        return testSparqlSorting_makeValues<QStringList>(nValues);
    case QVariant::DateTime:
        return testSparqlSorting_makeValues<QDateTime>(nValues);
    case QVariant::Url:
        return testSparqlSorting_makeValues<QUrl>(nValues);
    case QVariant::Bool:
        return testSparqlSorting_makeValues<bool>(nValues);
    case QVariant::Int:
        return testSparqlSorting_makeValues<int>(nValues);
    case QVariant::Double:
        return testSparqlSorting_makeValues<double>(nValues);
    default:
        qFatal("Unhandled type %u", field.dataType());
        return QVariantList();
    }
}

class FilterDetail : public QContactDetail
{
public:
    Q_DECLARE_CUSTOM_CONTACT_DETAIL(FilterDetail, "FilterDetail")
    Q_DECLARE_LATIN1_CONSTANT(FieldContactIndex, "ContactIndex");
    Q_DECLARE_LATIN1_CONSTANT(FieldTestName, "TestName");

    void setContactIndex(const QString &value) { setValue(FieldContactIndex, value); }
    QString contactIndex() const { return value(FieldContactIndex); }

    void setTestName(const QString &value) { setValue(FieldTestName, value); }
    QString testName() const { return value(FieldTestName); }
};

Q_DEFINE_LATIN1_CONSTANT(FilterDetail::DefinitionName, "FilterDetail");
Q_DEFINE_LATIN1_CONSTANT(FilterDetail::FieldContactIndex, "ContactIndex");
Q_DEFINE_LATIN1_CONSTANT(FilterDetail::FieldTestName, "TestName");

static const QContactDetailDefinition &
filterDetailDefinition()
{
    static QContactDetailDefinition detail;

    if (detail.isEmpty()) {
        QContactDetailFieldDefinition field;
        field.setDataType(QVariant::String);

        detail.setName(FilterDetail::DefinitionName);

        detail.insertField(FilterDetail::FieldContactIndex, field);
        detail.insertField(FilterDetail::FieldTestName, field);
    }

    return detail;
}

struct testSparqlSorting_FieldHints : public QSharedData
{
    QSet<QString> contactTypes;
    QString backingField;
    QVariantList values;
};

static bool
testSparqlSorting_isMetaField(const QString &fieldName)
{
    return (QLatin1String("Context") == fieldName ||
            QLatin1String("SubType") == fieldName ||
            QLatin1String("SubTypes") == fieldName);
}

void
ut_qtcontacts_trackerplugin::testSparqlSorting_data()
{
    QTest::addColumn<QString>("definitionName");
    QTest::addColumn<QString>("fieldName");
    QTest::addColumn<QStringList>("contactTypes");
    QTest::addColumn<QString>("backingField");
    QTest::addColumn<QVariantList>("values");
    QTest::addColumn<uint>("sortDirection");

    // minimum number of values to generate for each detail
    static const int nValues = 3;

    static const QList<Qt::SortOrder> sortDirections =
            QList<Qt::SortOrder>() << Qt::AscendingOrder << Qt::DescendingOrder;

    // Maps a definitionName to the contact types where it's defined
    typedef QExplicitlySharedDataPointer<testSparqlSorting_FieldHints> FieldHintsPointer;
    typedef QHash<PairOfStrings, FieldHintsPointer> FieldHintsHash;

    FieldHintsHash fieldHints;

    foreach(const QString &type, engine()->supportedContactTypes()) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(type, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(not definitions.isEmpty());

        foreach(const QString &detailName, getSortableDetailDefinitionNamesForType(definitions)) {
            const QContactDetailFieldDefinitionMap fieldDefs = definitions[detailName].fields();

            for (QContactDetailFieldDefinitionMap::ConstIterator
                 it = fieldDefs.constBegin(); it != fieldDefs.constEnd(); ++it) {
                const QString fieldName = it.key();
                const PairOfStrings detail = qMakePair(detailName, fieldName);
                FieldHintsPointer hint = fieldHints.value(detail);

                if (not hint) {
                    hint = new testSparqlSorting_FieldHints();
                    hint->values = testSparqlSorting_makeFieldValues(detailName, fieldName,
                                                                     *it, nValues);
                    fieldHints.insert(detail, hint);

                    if (testSparqlSorting_isMetaField(fieldName)) {
                        foreach(const QString &candidate, fieldDefs.keys()) {
                            if (not testSparqlSorting_isMetaField(candidate)) {
                                hint->backingField = candidate;
                                break;
                            }
                        }
                    }
                }

                hint->contactTypes.insert(type);
            }
        }
    }

    for (FieldHintsHash::ConstIterator it = fieldHints.constBegin(); it != fieldHints.constEnd(); ++it) {
        const PairOfStrings detail = it.key();
        const FieldHintsPointer hint = it.value();

        foreach (Qt::SortOrder sortDirection, sortDirections) {
            QTest::newRow(qPrintable(detail.first + QLatin1String(":") +
                                     detail.second + QLatin1String(":") +
                                     QString::number(sortDirection)))
                    << it.key().first << it.key().second
                    << QStringList(hint->contactTypes.toList())
                    << hint->backingField  << hint->values
                    << uint(sortDirection);
        }
    }
}

static QVariant::Type
detailFieldType(const QContactDetailDefinition &detailDefinition,
                const QString &fieldName)
{
    const QMap<QString, QContactDetailFieldDefinition> fieldDefinitions = detailDefinition.fields();

    QMap<QString, QContactDetailFieldDefinition>::ConstIterator field = fieldDefinitions.find(fieldName);

    if (field == fieldDefinitions.constEnd()) {
        return QVariant::Invalid;
    }

    return field.value().dataType();
}

void
ut_qtcontacts_trackerplugin::testSparqlSorting()
{
    QFETCH(QString, definitionName);
    QFETCH(QString, fieldName);
    QFETCH(QString, backingField);
    QFETCH(QStringList, contactTypes);
    QFETCH(QVariantList, values);
    QFETCH(uint, sortDirection);

    // We assume that datatypes are the same for every contact type
    const QVariant::Type fieldType = detailFieldType(engine()->detailDefinition(definitionName, QContactType::TypeContact, 0),
                                                     fieldName);
    if (fieldType == QVariant::Url) {
        QSKIP("Mobility does not know how to compare QUrl", SkipSingle);
    }

    // store filter detail definition
    foreach (const QString &contactType, contactTypes) {
        {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            const bool success = engine()->removeDetailDefinition(filterDetailDefinition().name(),
                                                                  contactType, &error);

            if (error != QContactManager::DoesNotExistError) {
                QCOMPARE(error, QContactManager::NoError);
                QVERIFY(success);
            }
        }

        {
            QContactManager::Error error = QContactManager::UnspecifiedError;
            const bool success = engine()->saveDetailDefinition(filterDetailDefinition(),
                                                                contactType, &error);
            QCOMPARE(error, QContactManager::NoError);
            QVERIFY(success);
        }
    }

    const QString testName = QString::fromLatin1("%1::%2 %3 %4").
            arg(definitionName, fieldName, QString::number(sortDirection),
                QUuid::createUuid().toString());

    QList<QContact> savedContacts;

    // We had another field with a monotonic value, so that if duplicates values were
    // generated by makeValues(), we have at least a secondary sort order we can rely on
    uint cIndex = 0;

    foreach (const QVariant &value, values) {
        foreach (const QString &contactType, contactTypes) {
            qDebug() << value << contactType;
            QContact c;

            QContactType ctDetail;
            ctDetail.setType(contactType);
            c.saveDetail(&ctDetail);

            FilterDetail filterDetail;
            filterDetail.setTestName(testName);
            filterDetail.setContactIndex(QString::fromLatin1("%1").arg(cIndex++, 2, 10, QLatin1Char('0')));
            c.saveDetail(&filterDetail);

            QContactDetail detail(definitionName);
            detail.setValue(fieldName, value);

            if (not backingField.isEmpty()) {
                detail.setValue(backingField, QLatin1String("xyz"));
            }

            c.saveDetail(&detail);

            savedContacts.append(c);
        }

        saveContacts(savedContacts);
    }

    // Refetch saved contacts, to get all the generated details
    {
        QList<QContactLocalId> ids;
        foreach (const QContact &c, savedContacts) {
            ids << c.localId();
        }

        savedContacts = contacts(ids);
    }

    QContactSortOrder primarySortOrder;
    primarySortOrder.setDetailDefinitionName(definitionName, fieldName);
    primarySortOrder.setDirection((Qt::SortOrder)sortDirection);

    QContactSortOrder secondarySortOrder;
    secondarySortOrder.setDetailDefinitionName(FilterDetail::DefinitionName,
                                               FilterDetail::FieldContactIndex);

    const QList<QContactSortOrder> sortOrders = QList<QContactSortOrder>() << primarySortOrder
                                                                           << secondarySortOrder;

    QContactDetailFilter filter;
    filter.setDetailDefinitionName(FilterDetail::DefinitionName, FilterDetail::FieldTestName);
    filter.setValue(testName);

    QContactFetchRequest fetchRequest;
    fetchRequest.setFilter(filter);
    fetchRequest.setSorting(sortOrders);

    QVERIFY(engine()->startRequest(&fetchRequest));
    QVERIFY(engine()->waitForRequestFinishedImpl(&fetchRequest, 0));
    QCOMPARE(fetchRequest.error(), QContactManager::NoError);
    QCOMPARE(fetchRequest.contacts().size(), savedContacts.size());

    const QList<QContactLocalId> sortedIds = engine()->sortContacts(savedContacts, sortOrders);

    QList<QContactLocalId> fetchedIds;
    QVariantList fetchedValues, sortedValues;

    foreach(const QContact &c, fetchRequest.contacts()) {
        fetchedIds.append(c.localId());
        fetchedValues.append(c.detail(definitionName).variantValue(fieldName));
    }

    QHash<QContactLocalId, QContact> contactsById;

    foreach(const QContact &c, savedContacts) {
        contactsById.insert(c.localId(), c);
    }

    foreach(QContactLocalId id, sortedIds) {
        sortedValues.append(contactsById.value(id).detail(definitionName).variantValue(fieldName));
    }

    QCOMPARE(fetchedIds.size(), sortedIds.size());

    // Using a "custom QCOMPARE" here to provide more information in case of failure
    if (fetchedIds != sortedIds) {
        qDebug() << "   Fetched IDs" << fetchedIds;
        qDebug() << "Fetched values" << fetchedValues;
        qDebug() << "    Sorted IDs" << sortedIds;
        qDebug() << " Sorted values" << sortedValues;
        QFAIL("fetchedIds != sortedIds");
    }
}

void
ut_qtcontacts_trackerplugin::testLimit_data()
{
    QTest::addColumn<int>("limit");
    QTest::addColumn<QString>("definitionName");
    QTest::addColumn<QString>("fieldName");

    for (int i = -5; i < 15; i += 5) {
        QString testName = QString::fromLatin1("limit == %1 (sorting on nickname)").arg(i);
        QTest::newRow(testName.toLatin1().constData()) << i
                                                       << QContactNickname::DefinitionName.operator QString()
                                                       << QContactNickname::FieldNickname.operator QString();

        testName = QString::fromLatin1("limit == %1 (sorting on DisplayLabel)").arg(i);
        QTest::newRow(testName.toLatin1().constData()) << i
                                                       << QContactDisplayLabel::DefinitionName.operator QString()
                                                       << QContactDisplayLabel::FieldLabel.operator QString();
    }
}

void
ut_qtcontacts_trackerplugin::testLimit()
{
    static const QStringList names = QStringList()
            << QLatin1String("Charles")
            << QLatin1String("Fran\xe7ois")
            << QLatin1String("Georges")
            << QLatin1String("Napoleon")
            << QLatin1String("Ren\xe9")
            << QLatin1String("Val\xe9ry");

    QFETCH(int, limit);
    QFETCH(QString, definitionName);
    QFETCH(QString, fieldName);

    QList<QContact> contacts;
    QList<QContactLocalId> contactIds;

    for (int i = 0; i < names.size(); ++i) {
        const QString name = names.at(i);

        QContact c;

        QContactNickname nameDetail;
        nameDetail.setNickname(name);
        c.saveDetail(&nameDetail);

        QContactType typeDetail;
        if (i%2 == 0) {
            typeDetail.setType(QContactType::TypeContact);
        } else {
            typeDetail.setType(QContactType::TypeGroup);
        }
        c.saveDetail(&typeDetail);

        contacts.append(c);
    }

    saveContacts(contacts);

    foreach (const QContact &contact, contacts) {
        contactIds.append(contact.localId());
    }

    QContactLocalIdFilter filter;
    filter.setIds(contactIds);

    QContactSortOrder sortOrder;
    sortOrder.setDetailDefinitionName(definitionName, fieldName);

    QContactFetchHint fetchHint;
    fetchHint.setMaxCountHint(limit);

    QContactFetchRequest fetchRequest;
    fetchRequest.setFilter(filter);
    fetchRequest.setFetchHint(fetchHint);
    fetchRequest.setSorting(QList<QContactSortOrder>() << sortOrder);

    QVERIFY(engine()->startRequest(&fetchRequest));
    QVERIFY(engine()->waitForRequestFinishedImpl(&fetchRequest, 0));
    QCOMPARE(fetchRequest.error(), QContactManager::NoError);

    const int expectedResultCount = (limit < 0 ? names.size() : qMin(limit, names.size()));
    const QList<QContact> fetchedContacts = fetchRequest.contacts();

    QCOMPARE(fetchedContacts.size(), expectedResultCount);

    for (int i = 0; i < expectedResultCount; ++i) {
        QCOMPARE(fetchedContacts.at(i).localId(), contactIds.at(i));
    }

    QctContactLocalIdFetchRequest idRequest;
    idRequest.setFilter(filter);
    idRequest.setLimit(limit);
    idRequest.setSorting(QList<QContactSortOrder>() << sortOrder);

    QVERIFY(engine()->startRequest(&idRequest));
    QVERIFY(engine()->waitForRequestFinishedImpl(&idRequest, 0));
    QCOMPARE(idRequest.error(), QContactManager::NoError);

    const QList<QContactLocalId> fetchedIds = idRequest.ids();

    QCOMPARE(fetchedIds.size(), expectedResultCount);

    for (int i = 0; i < expectedResultCount; ++i) {
        QCOMPARE(fetchedIds.at(i), contactIds.at(i));
    }
}

void
ut_qtcontacts_trackerplugin::testFilterContacts()
{
    // this one will get complete contacts
    QContact c;
    QContactName name;
    name.setFirstName("Zuba");
    name.setLastName("Zub");
    c.saveDetail(&name);
    QContactPhoneNumber phone;

    phone.setNumber("4872444");
    c.saveDetail(&phone);

    QContactBirthday birthday;
    birthday.setDate(QDate(2010, 2, 14));
    c.saveDetail(&birthday);

    QContactManager::Error error(QContactManager::UnspecifiedError);
    engine()->saveContact(&c, &error);
    QCOMPARE(error,  QContactManager::NoError);

    QStringList details;
    details << QContactName::DefinitionName << QContactAvatar::DefinitionName
            << QContactPhoneNumber::DefinitionName;

    QContactFetchRequest request;
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName, QContactPhoneNumber::FieldNumber);

    Slots slot;
    QObject::connect(&request, SIGNAL(resultsAvailable()),
            &slot, SLOT(resultsAvailable()));
    filter.setValue(QString("4872444"));
    filter.setMatchFlags(QContactFilter::MatchEndsWith);

    request.setFetchHint(fetchHint(details));
    request.setFilter(filter);

    engine()->startRequest(&request);
    engine()->waitForRequestFinishedImpl(&request, 0);

    // if it takes more, then something is wrong
    QVERIFY(request.isFinished());
    QVERIFY(!request.contacts().isEmpty());

    QVERIFY(!slot.contacts.isEmpty());

    bool containsThisId = false;
    foreach(const QContact &contact, slot.contacts)
    {
        if( contact.localId() == c.localId())
            containsThisId = true;
        bool containsPhone = false;
        foreach(const QContactDetail &detail, contact.details(QContactPhoneNumber::DefinitionName))
        {
            if(detail.value(QContactPhoneNumber::FieldNumber).contains("4872444"))
            {
                containsPhone = true;
                break;
            }
        }
        QVERIFY(containsPhone);
    }
    QVERIFY(containsThisId);

    // filter by birthday range
    QContactDetailRangeFilter rangeFilter;
    rangeFilter.setDetailDefinitionName(QContactBirthday::DefinitionName,
                                        QContactBirthday::FieldBirthday);
    // include lower & exclude upper by default
    rangeFilter.setRange(QDate(2010, 2, 14), QDate(2010, 2, 15));
    error = QContactManager::UnspecifiedError;
    QList<QContact> contacts(engine()->contacts(rangeFilter, NoSortOrders,
                                                fetchHint<QContactBirthday>(), &error));
    QCOMPARE(error,  QContactManager::NoError);
    QVERIFY(!contacts.isEmpty());
    bool containsOurContact(false);
    foreach(const QContact &cont, contacts) {
        QCOMPARE(cont.detail<QContactBirthday>().date(), QDate(2010, 2, 14));

        if(c.id() == cont.id()) {
            containsOurContact = true;
        }
    }
    QVERIFY(containsOurContact);
}

void
ut_qtcontacts_trackerplugin::testFilterContactsEndsWithAndPhoneNumber()
{
    QContact matchingContact;
    QContactName name;
    name.setFirstName("Zuba");
    name.setLastName("Zub");
    matchingContact.saveDetail(&name);
    QContactPhoneNumber phone;
    phone.setContexts(QContactPhoneNumber::ContextWork);
    phone.setNumber("3210987654321");
    matchingContact.saveDetail(&phone);
    QContactPhoneNumber phone1;
    phone1.setNumber("98653");
    matchingContact.saveDetail(&phone1);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&matchingContact, &error));
    QCOMPARE(error,  QContactManager::NoError);

    QStringList details;
    details << QContactName::DefinitionName << QContactAvatar::DefinitionName
            << QContactPhoneNumber::DefinitionName;

    QContactFetchRequest request;
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName, QContactPhoneNumber::FieldNumber);
    const QContactFetchHint detailFetchHint = fetchHint(details);
    // test matching of 7 last digits
    int matchCount = 7;
    qDebug() << "Test matching of" << matchCount << "last digits.";
    changeSetting(QctSettings::NumberMatchLengthKey, matchCount);

    // shorter phone numbers do exact match
    {
        filter.setValue("98653");
        filter.setMatchFlags(QContactFilter::MatchPhoneNumber);
        error = QContactManager::UnspecifiedError;
        QList<QContact> conts = engine()->contacts(filter, NoSortOrders, detailFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(!conts.isEmpty());
        bool containsMatchingC(false);
        foreach(const QContact &contact, conts) {
            if (contact.localId() == matchingContact.localId())
                containsMatchingC = true;
            bool containsPhone = false;
            foreach(const QContactDetail &detail, contact.details(QContactPhoneNumber::DefinitionName)) {
                // only exact match for shorter phone numbers
                if (detail.value(QContactPhoneNumber::FieldNumber)=="98653") {
                    containsPhone = true;
                    break;
                }
            }
            QVERIFY(containsPhone);
        }
        QVERIFY(containsMatchingC);
    }

    // settings phone number match
    {
        filter.setValue("37654321");
        filter.setMatchFlags(QContactFilter::MatchPhoneNumber);
        error = QContactManager::UnspecifiedError;
        QList<QContact> conts = engine()->contacts(filter, NoSortOrders, detailFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(!conts.isEmpty());
        bool containsMatchingC(false);
        foreach(const QContact &contact, conts) {
            if (contact.localId() == matchingContact.localId())
                containsMatchingC = true;
            bool containsPhone = false;
            foreach(const QContactDetail &detail, contact.details(QContactPhoneNumber::DefinitionName)) {
                // only exact match for shorter phone numbers
                if (qctNormalizePhoneNumber(detail.value(QContactPhoneNumber::FieldNumber)).right(matchCount)=="7654321") {
                    containsPhone = true;
                    break;
                }
            }
            QVERIFY(containsPhone);
        }
        QVERIFY(containsMatchingC);
    }

    Slots slot;
    QObject::connect(&request, SIGNAL(resultsAvailable()),
            &slot, SLOT(resultsAvailable()));

    {

        QString matchValue = "3000007654321";
        QContact nonMatchingContact;
        nonMatchingContact.saveDetail(&name);
        phone.setNumber("3210980654321");
        nonMatchingContact.saveDetail(&phone);
        error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&nonMatchingContact, &error));
        QCOMPARE(error,  QContactManager::NoError);

        filter.setValue(matchValue);
        filter.setMatchFlags(QContactFilter::MatchEndsWith);

        request.setFetchHint(detailFetchHint);
        request.setFilter(filter);

        engine()->startRequest(&request);
        engine()->waitForRequestFinishedImpl(&request, 0);
        QVERIFY(request.isFinished());
        QVERIFY(!slot.contacts.isEmpty());

        bool containsMatchingId = false;
        bool containsNonMatchingId = false;
        foreach(const QContact &contact, slot.contacts) {
            if (contact.localId() == nonMatchingContact.localId())
                containsNonMatchingId = true;
            if (contact.localId() == matchingContact.localId())
                containsMatchingId = true;
            bool containsPhone = false;
            foreach(const QContactDetail &detail, contact.details(QContactPhoneNumber::DefinitionName)) {
                if (detail.value(QContactPhoneNumber::FieldNumber).endsWith(matchValue.right(matchCount))) {
                    containsPhone = true;
                    break;
                }
            }
            QVERIFY(containsPhone);
        }
        QVERIFY(containsMatchingId);
        QVERIFY(!containsNonMatchingId);
    }

    {
        // test matching of 11 last digits
        matchCount = 11;
        qDebug() << "Test matching of" << matchCount << "last digits.";
        changeSetting(QctSettings::NumberMatchLengthKey, matchCount);
        QString matchValue = "3010987654321";
        QContact nonMatchingContact;
        nonMatchingContact.saveDetail(&name);
        phone.setNumber("3200987654321");
        nonMatchingContact.saveDetail(&phone);
        error = QContactManager::UnspecifiedError;
        engine()->saveContact(&nonMatchingContact, &error);
        QCOMPARE(error,  QContactManager::NoError);

        QContact matchingContactWithShorterNumber;
        QContactName name1;
        name1.setFirstName("ShortNumber");
        name1.setLastName("Zub1");
        matchingContactWithShorterNumber.saveDetail(&name1);
        QContactPhoneNumber phone1;
        phone1.setNumber("54321");
        matchingContactWithShorterNumber.saveDetail(&phone1);
        error = QContactManager::UnspecifiedError;
        engine()->saveContact(&matchingContactWithShorterNumber, &error);
        QCOMPARE(error,  QContactManager::NoError);


        filter.setValue(matchValue);
        filter.setMatchFlags(QContactFilter::MatchEndsWith);

        request.setFetchHint(detailFetchHint);
        request.setFilter(filter);

        engine()->startRequest(&request);
        engine()->waitForRequestFinishedImpl(&request, 0);

        QVERIFY(request.isFinished());
        QVERIFY(!slot.contacts.isEmpty());

        bool containsMatchingId = false;
        bool containsNonMatchingId = false;
        foreach(const QContact &contact, slot.contacts) {
            if (contact.localId() == nonMatchingContact.localId())
                containsNonMatchingId = true;
            if (contact.localId() == matchingContact.localId())
                containsMatchingId = true;
            bool containsPhone = false;
            foreach(const QContactPhoneNumber &detail, contact.details<QContactPhoneNumber>()) {
                if (detail.number().endsWith(matchValue.right(matchCount))) {
                    containsPhone = true;
                    break;
                }
            }
            QVERIFY(containsPhone);
        }
        QVERIFY(containsMatchingId);
        QVERIFY(!containsNonMatchingId);

        // now verify with short number
        filter.setValue("54321");
        filter.setMatchFlags(QContactFilter::MatchEndsWith);

        request.setFetchHint(detailFetchHint);
        request.setFilter(filter);

        engine()->startRequest(&request);
        engine()->waitForRequestFinishedImpl(&request, 0);

        QVERIFY(request.isFinished());
        QVERIFY(!slot.contacts.isEmpty());
        bool containsShort = false;
        foreach(const QContact &contact, slot.contacts) {
            if (contact.localId() == matchingContactWithShorterNumber.localId())
                containsShort = true;
        }
        QVERIFY(containsShort);
    }
}

void
ut_qtcontacts_trackerplugin::testNormalizePhoneNumber_data()
{
    QTest::addColumn<QString>("clipboardNumber");
    QTest::addColumn<QString>("formattedNumber");
    QTest::addColumn<QString>("cleanedNumber");
    QTest::addColumn<QString>("convertedNumber");
    QTest::addColumn<QString>("fullyNormalizedNumber");

    QTest::newRow("western")
            << QString::fromUtf8(LRE "+1 (122) 3344-5566" PDF)
            << QString::fromUtf8("+1 (122) 3344-5566")
            << QString::fromUtf8("+112233445566")
            << QString::fromUtf8("+1 (122) 3344-5566")
            << QString::fromUtf8("+112233445566");

    QTest::newRow("arabic")
            << QString::fromUtf8(LRE "+" A9 A7 A1 "-" A1 A1 "." A3 A3 "." A4 A4 "." A5 A5 PDF)
            << QString::fromUtf8("+" A9 A7 A1 "-" A1 A1 "." A3 A3 "." A4 A4 "." A5 A5)
            << QString::fromUtf8("+" A9 A7 A1 A1 A1 A3 A3 A4 A4 A5 A5)
            << QString::fromUtf8("+971-11.33.44.55")
            << QString::fromUtf8("+97111334455");
}

void
ut_qtcontacts_trackerplugin::testNormalizePhoneNumber()
{
    QFETCH(QString, clipboardNumber);
    QFETCH(QString, formattedNumber);
    QFETCH(QString, cleanedNumber);
    QFETCH(QString, convertedNumber);
    QFETCH(QString, fullyNormalizedNumber);

    changeSetting(QctSettings::NumberMatchLengthKey, 7);

    QCOMPARE(qctNormalizePhoneNumber(clipboardNumber, Qct::NoNormalization), clipboardNumber);
    QCOMPARE(qctNormalizePhoneNumber(formattedNumber, Qct::NoNormalization), formattedNumber);

    QCOMPARE(qctNormalizePhoneNumber(clipboardNumber, Qct::RemoveUnicodeFormatters | Qct::RemoveNumberFormatters), cleanedNumber);
    QCOMPARE(qctNormalizePhoneNumber(formattedNumber, Qct::RemoveNumberFormatters), cleanedNumber);

    QCOMPARE(qctNormalizePhoneNumber(clipboardNumber, Qct::RemoveUnicodeFormatters | Qct::ConvertToLatin), convertedNumber);
    QCOMPARE(qctNormalizePhoneNumber(formattedNumber, Qct::ConvertToLatin), convertedNumber);

    QCOMPARE(qctNormalizePhoneNumber(clipboardNumber, Qct::RemoveUnicodeFormatters | Qct::RemoveNumberFormatters | Qct::ConvertToLatin), fullyNormalizedNumber);
    QCOMPARE(qctNormalizePhoneNumber(formattedNumber, Qct::RemoveNumberFormatters | Qct::ConvertToLatin), fullyNormalizedNumber);

    QCOMPARE(qctMakeLocalPhoneNumber(clipboardNumber), fullyNormalizedNumber.right(7));
    QCOMPARE(qctMakeLocalPhoneNumber(formattedNumber), fullyNormalizedNumber.right(7));
}

void
ut_qtcontacts_trackerplugin::testFilterDTMFNumber()
{
    QContactLocalId idExactMatch, idExactMatchShort, idStrippedMatch;

    // Save a contact with a long number including DTMF codes
    {
        QContact contact;
        QContactPhoneNumber number;
        number.setNumber(QLatin1String("123456789p678"));
        contact.saveDetail(&number);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&contact, &error));
        QCOMPARE(error, QContactManager::NoError);
        idExactMatch = contact.localId();
    }

    // Save a contact with a long number
    {
        QContact contact;
        QContactPhoneNumber number;
        number.setNumber(QLatin1String("123456789"));
        contact.saveDetail(&number);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&contact, &error));
        QCOMPARE(error, QContactManager::NoError);
        idStrippedMatch = contact.localId();
    }

    // Save a contact with a short number including DTMF codes
    {
        QContact contact;
        QContactPhoneNumber number;
        number.setNumber(QLatin1String("963p98"));
        contact.saveDetail(&number);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&contact, &error));
        QCOMPARE(error, QContactManager::NoError);
        idExactMatchShort = contact.localId();
    }

    // Be nasty: also insert a nco:Contact linked to a number, to cover the following
    // use case:
    // 1. Store a contact with number "123456789"
    // 2. Call "123456789p45" -> commhistoryd writes a nco:Contact linked to "123456789p45"
    executeQuery(QString::fromLatin1("INSERT {"
                                     "  _:pn a nco:PhoneNumber; maemo:localPhoneNumber \"%1\" ."
                                     "  _:contact a nco:Contact; nco:hasPhoneNumber _:pn "
                                     "}").arg(qctMakeLocalPhoneNumber(QLatin1String("123456789p999"))),
                 QSparqlQuery::InsertStatement);

    QContactFetchRequest request;

    QContactDetailFilter numberFilter;
    numberFilter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName, QContactPhoneNumber::FieldNumber);
    numberFilter.setMatchFlags(QContactDetailFilter::MatchPhoneNumber);

    QContactLocalIdFilter idFilter;
    idFilter.add(idExactMatch);
    idFilter.add(idExactMatchShort);
    idFilter.add(idStrippedMatch);

    // Check that exact match works
    numberFilter.setValue(QString::fromLatin1("123456789p678"));
    request.setFilter(numberFilter & idFilter);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));
    QCOMPARE(request.contacts().size(), 1);
    QCOMPARE(request.contacts().first().localId(), idExactMatch);

    // Check that "drop DTMF if no exact match" works
    numberFilter.setValue(QString::fromLatin1("123456789p999"));
    request.setFilter(numberFilter & idFilter);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));
    QCOMPARE(request.contacts().size(), 1);
    QCOMPARE(request.contacts().first().localId(), idStrippedMatch);

    // Check that exact match for short numbers works
    numberFilter.setValue(QString::fromLatin1("963p98"));
    request.setFilter(numberFilter & idFilter);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));
    QCOMPARE(request.contacts().size(), 1);
    QCOMPARE(request.contacts().first().localId(), idExactMatchShort);
}

void
ut_qtcontacts_trackerplugin::testFilterContactsMatchPhoneNumberWithShortNumber_data()
{
    QTest::addColumn<QString>("shortNumber");
    QTest::addColumn<QString>("longNumber");

    QTest::newRow("long number ends with short")
            << QString::fromLatin1("321")
            << QString::fromLatin1("7654321");
    QTest::newRow("long number start with short")
            << QString::fromLatin1("765")
            << QString::fromLatin1("7654321");
    QTest::newRow("long number contains short in middle")
            << QString::fromLatin1("543")
            << QString::fromLatin1("7654321");
}

void
ut_qtcontacts_trackerplugin::testFilterContactsMatchPhoneNumberWithShortNumber()
{
    // creates two contacts with phone numbers, where one is a part of the other
    // when contacts are queried with a filter set to the short number,
    // only the contact with the short number should be delivered
    QFETCH(QString, shortNumber);
    QFETCH(QString, longNumber);

    // create both contacts
    QContact contact;
    SET_TESTNICKNAME_TO_CONTACT(contact);
    QContactPhoneNumber phonenumber;
    phonenumber.setNumber(shortNumber);
    contact.saveDetail(&phonenumber);

    QContact otherContact;
    SET_TESTNICKNAME_TO_CONTACT(otherContact);
    QContactPhoneNumber otherPhonenumber;
    otherPhonenumber.setNumber(longNumber);
    otherContact.saveDetail(&otherPhonenumber);

    // store both contacts
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&contact, &error));
    QCOMPARE(error,  QContactManager::NoError);

    error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->saveContact(&otherContact, &error));
    QCOMPARE(error,  QContactManager::NoError);

    // try to fetch contact with the short number
    QContactDetailFilter phoneNumberFilter;
    phoneNumberFilter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName, QContactPhoneNumber::FieldNumber);
    phoneNumberFilter.setMatchFlags(QContactFilter::MatchPhoneNumber);
    phoneNumberFilter.setValue(shortNumber);
    // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
    QContactIntersectionFilter filter;
    filter << TESTNICKNAME_FILTER << phoneNumberFilter;
    error = QContactManager::UnspecifiedError;
    QContactList contacts = engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    // Test fetch result
    QCOMPARE(contacts.count(), 1);
    QCOMPARE(contacts.first().localId(), contact.localId());
    QCOMPARE(contacts.first().detail<QContactPhoneNumber>().number(), shortNumber);
}

void
ut_qtcontacts_trackerplugin::testFilterTwoNameFields()
{
    // init test
    QMap<QContactLocalId, QContactName> names;
    for (int i = 0; i < 3; i++) {
        QContact c;
        QContactName name;
        name.setFirstName(QUuid::createUuid().toString() + QString::number(i));
        name.setLastName(QUuid::createUuid().toString() + QString::number(i));
        c.saveDetail(&name);
        QContactAvatar avatar;
        avatar.setImageUrl(QUrl(QUuid::createUuid().toString()));
        c.saveDetail(&avatar);
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        names.insert(c.localId(), name);
        addedContacts.append(c.localId());
    }

    // Init filter
    QContactLocalId searchId = names.keys().at(1);
    QString searchFirst = names.value(searchId).firstName();
    QString searchLast = names.value(searchId).lastName();
    QContactDetailFilter filterFirst;
    filterFirst.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    filterFirst.setMatchFlags(QContactFilter::MatchExactly);
    filterFirst.setValue(searchFirst);
    QContactDetailFilter filterLast;
    filterLast.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldLastName);
    filterLast.setMatchFlags(QContactFilter::MatchExactly);
    filterLast.setValue(searchLast);

    QContactUnionFilter unionFilter;
    unionFilter << filterFirst;
    unionFilter << filterLast;

    // Init request
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList contacts = engine()->contacts(unionFilter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    // Test fetch result
    QCOMPARE(contacts.count(), 1);
    QCOMPARE(contacts.first().localId(), searchId);
    QCOMPARE(contacts.first().detail<QContactName>().firstName(), searchFirst);
    QCOMPARE(contacts.first().detail<QContactName>().lastName(), searchLast);
}


void
ut_qtcontacts_trackerplugin::testFilterContactsWithBirthday_data()
{
    QTest::addColumn<QContact>("contact");
    QTest::addColumn<bool>("hasBirthday");

    {
        QContact contact;

        QContactName name;
        name.setCustomLabel("No Birthday");
        QVERIFY(contact.saveDetail(&name));

        QTest::newRow("none") << contact << false;
    }

    {
        QContact contact;

        QContactName name;
        name.setCustomLabel("Birthday");
        QVERIFY(contact.saveDetail(&name));

        QContactBirthday birthday;
        birthday.setDate(QDate::currentDate());
        QVERIFY(contact.saveDetail(&birthday));

        QTest::newRow("birthday") << contact << true;
    }

    {
        QContact contact;

        QContactName name;
        name.setCustomLabel("Birthday and Link");
        QVERIFY(contact.saveDetail(&name));

        QContactBirthday birthday;
        birthday.setDate(QDate::currentDate());
        birthday.setCalendarId(QUuid::createUuid().toString());
        QVERIFY(contact.saveDetail(&birthday));

        QTest::newRow("birthday-and-link") << contact << true;
    }
}

void
ut_qtcontacts_trackerplugin::testFilterContactsWithBirthday()
{
    QFETCH(QContact, contact);
    QFETCH(bool, hasBirthday);

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);
    }

    {
        QContactDetailFilter filter;

        filter.setDetailDefinitionName(QContactBirthday::DefinitionName);
        filter.setMatchFlags(QContactFilter::MatchExactly);

        QContactFetchHint hint;

        hint.setDetailDefinitionsHint(QStringList()
                                      << QContactName::DefinitionName
                                      << QContactBirthday::DefinitionName);
        hint.setOptimizationHints(QContactFetchHint::NoActionPreferences |
                                  QContactFetchHint::NoRelationships |
                                  QContactFetchHint::NoBinaryBlobs);


        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactList results = engine()->contacts(filter, NoSortOrders, hint, &error);
        QCOMPARE(error, QContactManager::NoError);

        bool contactFound = false;

        foreach(const QContact &match, results) {
            if (match.localId() == contact.localId()) {
                contactFound = true;
                break;
            }
        }

        QCOMPARE(contactFound, hasBirthday);
    }
}

void
ut_qtcontacts_trackerplugin::testLocalIdFilterManyIds_data()
{
    QTest::addColumn<int>("idCount");

    QTest::newRow("1") << 5;
    QTest::newRow("50") << 50;
    QTest::newRow("500") << 500;
    QTest::newRow("5000") << 5000;
}

void
ut_qtcontacts_trackerplugin::testLocalIdFilterManyIds()
{
    QFETCH(int, idCount);

    QContactLocalIdList ids;

    for(int i = 0, lastId = 0; i < idCount; ++i, lastId += qrand() % 5 + 1) {
        ids.append(lastId);
    }

    QContactLocalIdFilter filter;
    filter.setIds(ids);

    // throw away result, we only want to check error code
    QContactManager::Error error = QContactManager::UnspecifiedError;
    engine()->contacts(filter, NoSortOrders, fetchHint<QContactName>(), &error);

    QCOMPARE(error, QContactManager::NoError);
}

static void testContactFilter_data_nb182154(QContactTrackerEngine *engine)
{
    QString prefix = QUuid::createUuid().toString().remove(QRegExp("[{}-]"));
    QStringList keywordList(prefix);
    QList<PairOfStrings> testFields;
    QList<QContact> contacts;

    testFields << PairOfStrings(QContactName::DefinitionName,
                                QContactName::FieldFirstName);
    testFields << PairOfStrings(QContactName::DefinitionName,
                                QContactName::FieldLastName);
    testFields << PairOfStrings(QContactPhoneNumber::DefinitionName,
                                QContactPhoneNumber::FieldNumber);
    testFields << PairOfStrings(QContactEmailAddress::DefinitionName,
                                QContactEmailAddress::FieldEmailAddress);

    foreach(const PairOfStrings &name, testFields) {
        contacts.append(QContact());
        QContactDetail detail(name.first);
        keywordList.append(prefix + "_" + name.second);
        detail.setValue(name.second, keywordList.last());
        contacts.last().saveDetail(&detail);
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool success = engine->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(success);

    foreach(const QString &keyword, keywordList) {
        for(int i = 0; i < 4; ++i) {
            QContactUnionFilter filter;
            QStringList filterNames("name");

            // Query by last name
            QContactDetailFilter lastNameFilter;

            lastNameFilter.setDetailDefinitionName(QContactName::DefinitionName,
                                                   QContactName::FieldLastName);
            lastNameFilter.setMatchFlags(QContactFilter::MatchStartsWith);
            lastNameFilter.setValue(keyword);
            filter << lastNameFilter;

            // Query by first name
            QContactDetailFilter firstNameFilter;

            firstNameFilter.setDetailDefinitionName(QContactName::DefinitionName,
                                                    QContactName::FieldFirstName);
            firstNameFilter.setMatchFlags(QContactFilter::MatchStartsWith);
            firstNameFilter.setValue(keyword);
            filter << firstNameFilter;

            if (i & 1) {
                // Query by phone
                QContactDetailFilter phoneFilter;
                filterNames << "phone";

                phoneFilter.setDetailDefinitionName(QContactPhoneNumber::DefinitionName,
                                                    QContactPhoneNumber::FieldNumber);
                phoneFilter.setMatchFlags(QContactFilter::MatchStartsWith);
                phoneFilter.setValue(keyword);
                filter << phoneFilter;
            }

            if (i & 2) {
                // Query by email
                QContactDetailFilter emailFilter;
                filterNames << "email";

                emailFilter.setDetailDefinitionName(QContactEmailAddress::DefinitionName,
                                                    QContactEmailAddress::FieldEmailAddress);
                emailFilter.setMatchFlags(QContactFilter::MatchStartsWith);
                emailFilter.setValue(keyword);
                filter << emailFilter;
            }

            QList<int> matches;

            for(int i = 0; i < contacts.count(); ++i) {
                if (QContactManagerEngine::testFilter(filter, contacts[i])) {
                    matches.append(i);
                }
            }

            QString tag = "NB#182154: " + filterNames.join(", ") + " with " + keyword;
            QTest::newRow(qPrintable(tag)) << contacts << QContactFilter(filter) << matches;
        }
    }
}

void
ut_qtcontacts_trackerplugin::testContactFilter_data()
{
    QTest::addColumn< QContactList >("contacts");
    QTest::addColumn< QContactFilter >("filter");
    QTest::addColumn< QList<int> >("matches");

    testContactFilter_data_nb182154(engine());
}

void
ut_qtcontacts_trackerplugin::testContactFilter()
{
    QFETCH(QContactList, contacts);
    QFETCH(QContactFilter, filter);
    QFETCH(QList<int>, matches);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactLocalIdList fetchedIds = engine()->contactIds(filter, NoSortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);

    // verify all expected contacts are found
    foreach(int i, matches) {
        QContactLocalId localId = contacts[i].localId();
        QString message = "i=%1, localId=%2";

        QVERIFY2(fetchedIds.contains(localId),
                 qPrintable(message.arg(i).arg(localId)));
    }

    // verify no more than the expected contacts are found
    QCOMPARE(fetchedIds.count(), matches.count());
}


void
ut_qtcontacts_trackerplugin::testFilterOnContainsSubTypesByClass()
{
    // tests creates and saves contacts, each with one subtype more than the one before
    // then for each subtype contacts are fetched with that subtype as detailfilter

    // note used subtypes, with one of them as base class, which has to be first
    const QStringList subTypes = QStringList()
        << QContactAddress::SubTypePostal
        << QContactAddress::SubTypeDomestic
        << QContactAddress::SubTypeInternational
        << QContactAddress::SubTypeParcel
        << QLatin1String("Unittest-SecretAddress") // custom
        << QLatin1String("Unittest-DevNullAddress"); // custom

    // create contacts, each with one subtype more
    for (int i = 0; i<subTypes.count(); ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        // set test detail with subtypes
        QContactAddress address;
        address.setCountry(QLatin1String("Finnland"));
        const int givenSubTypesCount = i + 1;
        const QStringList givenSubTypes = subTypes.mid(0, givenSubTypesCount);
        address.setSubTypes(givenSubTypes);
        contact.saveDetail(&address);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
    }

    // fetch contacts, once for each subtype
    for (int i = 0; i<subTypes.count(); ++i) {
        QContactDetailFilter subTypeFilter;
        subTypeFilter.setDetailDefinitionName(QContactAddress::DefinitionName, QContactAddress::FieldSubTypes);
        subTypeFilter.setValue(subTypes[i]);
        subTypeFilter.setMatchFlags(QContactFilter::MatchContains);
        // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
        QContactIntersectionFilter filter;
        filter << TESTNICKNAME_FILTER << subTypeFilter;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedContacts =
            engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        const int expectedContactsCount = subTypes.count() - i;
        QCOMPARE(fetchedContacts.count(), expectedContactsCount);
        // TODO: compare actual contacts
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnSubTypesByProperty()
{
    // tests creates and saves x contacts, each with one subtype more than the one before
    // then for each subtype contacts are fetched with that subtype as detailfilter

    // note used subtypes, ideally with one of them as base class, which has to be first
    const QStringList subTypes = QStringList()
        << QContactUrl::SubTypeFavourite
        << QContactUrl::SubTypeHomePage
        << QContactUrl::SubTypeBlog;

    // create contacts, each with a different subtype
    for (int i = 0; i<subTypes.count(); ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        QContactUrl url;
        url.setUrl(QLatin1String("http://openismus.com/"));
        url.setSubType(subTypes[i]);
        contact.saveDetail(&url);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
    }

    // fetch contacts, once for each subtype
    for (int i = 0; i<subTypes.count(); ++i) {
        QContactDetailFilter subTypeFilter;
        subTypeFilter.setDetailDefinitionName(QContactUrl::DefinitionName, QContactUrl::FieldSubType);
        subTypeFilter.setValue(subTypes[i]);
        // using also subTypeFirstNameFilter to limit contacts to those used in the test
        QContactIntersectionFilter filter;
        filter << TESTNICKNAME_FILTER << subTypeFilter;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedContacts =
            engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QCOMPARE(fetchedContacts.count(), 1);
        // TODO: compare actual contacts
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnContainsSubTypesByNaoProperty()
{
    // tests creates and saves x contacts, each with one subtype more than the one before
    // then for each subtype contacts are fetched with that subtype as detailfilter

    // note used subtypes, ideally with one of them as base class, which has to be first
    const QStringList subTypes = QStringList()
        << QContactOnlineAccount::SubTypeImpp
        << QContactOnlineAccount::SubTypeSip
        << QContactOnlineAccount::SubTypeSipVoip
        << QContactOnlineAccount::SubTypeVideoShare;

    // create contacts, each with one subtype more
    for (int i = 0; i<subTypes.count(); ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        QContactOnlineAccount onlineAccount;
        onlineAccount.setAccountUri(QString::fromLatin1("knut%1@Yeti").arg(i));
        onlineAccount.setValue(QContactOnlineAccount__FieldAccountPath,
                               QString::fromLatin1("/org/Yeti/knut%1").arg(i));
        const int givenSubTypesCount = i + 1;
        const QStringList givenSubTypes = subTypes.mid(0, givenSubTypesCount);
        onlineAccount.setSubTypes(givenSubTypes);
        contact.saveDetail(&onlineAccount);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
    }

    // fetch contacts, once for each subtype
    for (int i = 0; i<subTypes.count(); ++i) {
        QContactDetailFilter subTypeFilter;
        subTypeFilter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, QContactOnlineAccount::FieldSubTypes);
        subTypeFilter.setValue(subTypes[i]);
        subTypeFilter.setMatchFlags(QContactFilter::MatchContains);
        // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
        QContactIntersectionFilter filter;
        filter << TESTNICKNAME_FILTER << subTypeFilter;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedContacts =
            engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        const int expectedContactsCount = subTypes.count() - i;
        QCOMPARE(fetchedContacts.count(), expectedContactsCount);
        // TODO: compare actual contacts
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnCustomDetail_data()
{
    QTest::addColumn<QString>("fieldId");
    QTest::addColumn<QVariantList>("fieldValues");
    QTest::addColumn<QVariant>("searchValue");
    QTest::addColumn<int>("searchFlags");

    QTest::newRow("string and contains")
            // fieldId
            << QString::fromLatin1("Spaceship")
            // fieldValues
            << (QVariantList()
                << QLatin1String("Business End")
                << QLatin1String("Heart of Gold")
                << QLatin1String("Billion Year Bunker"))
            // searchValue
            << QVariant(QLatin1String(" of "))
            // searchFlags
            << int(QContactFilter::MatchContains);

    QTest::newRow("string and startsWith")
            // fieldId
            << QString::fromLatin1("Person")
            // fieldValues
            << (QVariantList()
                << QLatin1String("Zaphod Beeblebrox")
                << QLatin1String("Not Zaphod")
                << QLatin1String("Trillian"))
            << QVariant(QLatin1String("Zaphod"))
            << int(QContactFilter::MatchStartsWith);

    QTest::newRow("string and endsWith")
            // fieldId
            << QString::fromLatin1("Weapon")
            // fieldValues
            << (QVariantList()
                << QLatin1String("Gun of Point of View")
                << QLatin1String("Point of View Gun")
                << QLatin1String("Kill-o-Zap blaster pistol"))
            // searchValue
            << QVariant(QLatin1String("Gun"))
            // searchFlags
            << int(QContactFilter::MatchEndsWith);

    QTest::newRow("string and exactly")
            // fieldId
            << QString::fromLatin1("Answer")
            // fieldValues
            << (QVariantList()
                << QLatin1String("Perhaps -42")
                << QLatin1String("42")
                << QLatin1String("42.2 Maybe"))
            // searchValue
            << QVariant(QLatin1String("42"))
            // searchFlags
            << int(QContactFilter::MatchExactly);

    QTest::newRow("stringlist and contains")
            // fieldId
            << QString::fromLatin1("Persons")
            // fieldValues
            << (QVariantList()
                << (QStringList()
                    << QLatin1String("Zaphod Beeblebrox")
                    << QLatin1String("Ford Prefect")
                    << QLatin1String("Trillian"))
                << (QStringList()
                    << QLatin1String("Zaphod")
                    << QLatin1String("Ford")
                    << QLatin1String("Trillian"))
                << (QStringList()
                    << QLatin1String("Marvin")
                    << QLatin1String("Slartibartfast")
                    << QLatin1String("Trillian")))
            // searchValue
            << QVariant(QStringList()
                    << QLatin1String("Zaphod"))
            // searchFlags
            << int(QContactFilter::MatchContains);

    QTest::newRow("stringlist and exactly")
            // fieldId
            << QString::fromLatin1("Weapons")
            // fieldValues
            << (QVariantList()
                << (QStringList()
                    << QLatin1String("Kill-o-Zap blaster pistol")
                    << QLatin1String("Point of View Gun"))
                << (QStringList()
                    << QLatin1String("Supernova bomb")
                    << QLatin1String("Kill-o-Zap blaster pistol")
                    << QLatin1String("Point of View Gun"))
                << (QStringList()
                    << QLatin1String("Supernova bomb")))
            // searchValue
            << QVariant(QStringList()
                    << QLatin1String("Kill-o-Zap blaster pistol")
                    << QLatin1String("Point of View Gun"))
            // searchFlags
            << int(QContactFilter::MatchExactly);

    QTest::newRow("stringlist and startsWith")
            // fieldId
            << QString::fromLatin1("Items")
            // fieldValues
            << (QVariantList()
                << (QStringList()
                    << QLatin1String("Towel")
                    << QLatin1String("Thinking Cap")
                    << QLatin1String("Digital watches"))
                << (QStringList()
                    << QLatin1String("Digital watches")
                    << QLatin1String("Towel")
                    << QLatin1String("Thinking Cap"))
                << (QStringList()
                    << QLatin1String("Digital watches")))
            // searchValue
            << QVariant(QStringList()
                    << QLatin1String("Towel")
                    << QLatin1String("Thinking Cap"))
            // searchFlags
            << int(QContactFilter::MatchStartsWith);

    QTest::newRow("stringlist and endsWith")
            // fieldId
            << QString::fromLatin1("Drives")
            // fieldValues
            << (QVariantList()
                << (QStringList()
                    << QLatin1String("Infinite Improbability Drive")
                    << QLatin1String("Hyperspace")
                    << QLatin1String("Bistromathic drive"))
                << (QStringList()
                    << QLatin1String("Hyperspace")
                    << QLatin1String("Bistromathic drive")
                    << QLatin1String("Infinite Improbability Drive"))
                << (QStringList()
                    << QLatin1String("Infinite Improbability Drive")))
            // searchValue
            << QVariant(QStringList()
                    << QLatin1String("Hyperspace")
                    << QLatin1String("Bistromathic drive"))
            // searchFlags
            << int(QContactFilter::MatchEndsWith);
}

void
ut_qtcontacts_trackerplugin::testFilterOnCustomDetail()
{
    // tests creates and saves contacts, each with a different value for a field of a custom detail
    // then for each value contacts are fetched with that value as detailfilter
    QFETCH(QString, fieldId);
    QFETCH(QVariantList, fieldValues);
    QFETCH(QVariant, searchValue);
    QFETCH(int, searchFlags);

    if (QVariant::StringList == searchValue.type()) {
        QSKIP("stringlist as type not supported right now", SkipAll);
    }

    // note used values
    const QString customDetailId = QLatin1String("GalaxyDetail");

    // create contacts, each with a different value
    for (int i = 0; i<fieldValues.count(); ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        QContactDetail customDetail(customDetailId);
        customDetail.setValue(fieldId, fieldValues[i]);
        contact.saveDetail(&customDetail);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
    }

    // fetch contacts with given search
    QContactDetailFilter customDetailFilter;
    customDetailFilter.setDetailDefinitionName(customDetailId, fieldId);
    customDetailFilter.setValue(searchValue);
    customDetailFilter.setMatchFlags((QContactFilter::MatchFlags)searchFlags);
    // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
    QContactIntersectionFilter filter;
    filter << TESTNICKNAME_FILTER << customDetailFilter;

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QList<QContact> fetchedContacts =
        engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error,  QContactManager::NoError);
    QCOMPARE(fetchedContacts.count(), 1);
    // TODO: compare actual contacts
}

void
ut_qtcontacts_trackerplugin::testFilterOnContactType()
{
    // tests creates and saves x contacts, each with one subtype more than the one before
    // then for each subtype contacts are fetched with that subtype as detailfilter

    // note used subtypes, ideally with one of them as base class, which has to be first
    const QStringList contactTypes = engine()->supportedContactTypes();

    // create contacts, each with a different type
    for (int i = 0; i<contactTypes.count(); ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);
        // set type
        contact.setType(contactTypes[i]);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
    }

    // fetch contacts, once for each subtype
    for (int i = 0; i<contactTypes.count(); ++i) {
        QContactDetailFilter contactTypeFilter;
        contactTypeFilter.setDetailDefinitionName(QContactType::DefinitionName, QContactType::FieldType);
        contactTypeFilter.setValue(contactTypes[i]);
        // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
        QContactIntersectionFilter filter;
        filter << TESTNICKNAME_FILTER << contactTypeFilter;

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QList<QContact> fetchedContacts =
            engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QCOMPARE(fetchedContacts.count(), 1);
        // TODO: compare actual contacts
    }
}

void
ut_qtcontacts_trackerplugin::testMergingContacts()
{
    QContact firstContact;
    fuzzContact(firstContact);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&firstContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(firstContact.localId());

    QList<QContactLocalId> secondIds;
    QList<QContact> mergeContacts;
    const int testMergingForNumberOfSorceContacts = 1;
    for (int i = 0; i < testMergingForNumberOfSorceContacts; i++) {
        QContact contact;
        fuzzContact(contact);
        mergeContacts << contact;
        error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&(mergeContacts.last()), &error));
        QCOMPARE(error,  QContactManager::NoError);
        secondIds<<mergeContacts.last().id().localId();
    }

    // reread stuff from tracker, all the detail uris
    mergeContacts = contacts(secondIds);
    QVERIFY(!mergeContacts.isEmpty());

    QctContactMergeRequest mergeRequest;
    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;
    foreach (QContactLocalId id, secondIds) {
        mergeIds.insert(firstContact.localId(), id);
    }

    mergeRequest.setMergeIds(mergeIds);

    // Start the request and wait for it to finish.
    QVERIFY(engine()->startRequest(&mergeRequest));
    QVERIFY(engine()->waitForRequestFinished(&mergeRequest, 2000));

    // once they are merged - that's it - no contacts or relationship track exists
    foreach (QContactLocalId mergedId, secondIds) {
        QContact second = contact(mergedId, QStringList()<<QContactName::DefinitionName);
        QCOMPARE(second.localId(), 0U); // as not existing
    }

    const QStringList mergedDetails = QStringList()
            << QContactPhoneNumber::DefinitionName
            << QContactEmailAddress::DefinitionName;

    firstContact  = contact(firstContact.localId(), mergedDetails);
    QVERIFY(firstContact.localId());

    // verify it contains all the data from merged contacts
    foreach (const QContact &mergedContact, mergeContacts) {
        foreach (const QContactDetail &detail, mergedContact.details()) {
            if (mergedDetails.contains(detail.definitionName())) {
                bool detailFound = false;

                foreach (const QContactDetail& fetchedDetail, firstContact.details(detail.definitionName())) {
                    if (detailMatches(fetchedDetail, detail)) {
                        detailFound = true;
                        break;
                    }
                }

                QVERIFY2(detailFound, qPrintable(detail.definitionName()));
            }
        }
    }
}

void
ut_qtcontacts_trackerplugin::testMergingGarbage()
{
    // Create a few normal contacts with phone number and name.
    QContactName name;
    QContactPhoneNumber tel;
    name.setLastName(QLatin1String(__func__));

    QContact c1;
    tel.setNumber("11223344");
    name.setFirstName(QLatin1String("First"));
    QVERIFY(c1.saveDetail(&name));
    QVERIFY(c1.saveDetail(&tel));

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&c1, &error));
        QCOMPARE(error, QContactManager::NoError);
    }

    QContact c2;
    tel.setNumber("55667788");
    name.setFirstName(QLatin1String("Second"));
    QVERIFY(c2.saveDetail(&name));
    QVERIFY(c2.saveDetail(&tel));

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&c2, &error));
        QCOMPARE(error, QContactManager::NoError);
    }

    QContact c3;
    name.setFirstName(QLatin1String("Third"));
    QVERIFY(c3.saveDetail(&name));

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&c3, &error));
        QCOMPARE(error, QContactManager::NoError);
    }

    // Pollute the first two contacts with date garbage.
    QScopedPointer<QSparqlResult> result;

    result.reset(executeQuery(QString::fromLatin1
                              ("INSERT {\n"
                               "  ?c1 nie:informationElementDate \"2012-02-13T01:00:00Z\", \"2012-02-13T02:00:00Z\" .\n"
                               "  ?c2 nie:informationElementDate \"2012-02-13T03:00:00Z\", \"2012-02-13T04:00:00Z\" .\n"
                               "} WHERE {\n"
                               "  ?c1 a nco:PersonContact FILTER(tracker:id(?c1) = %1) .\n"
                               "  ?c2 a nco:PersonContact FILTER(tracker:id(?c2) = %2) .\n"
                               "}").
                              arg(c1.localId()).arg(c2.localId()),
                              QSparqlQuery::InsertStatement));

    QVERIFY(not result.isNull() && not result->hasError());

    // Verify our we properly screwed up the first two contacts, but keep out the third.
    result.reset(executeQuery(QString::fromLatin1
                              ("SELECT tracker:id(?c) COUNT(?d) {\n"
                               "  ?c a nco:PersonContact; dc:date ?d\n"
                               "  FILTER(tracker:id(?c) IN (%1,%2,%3) &&\n"
                               "         ?d >= \"2012-02-13T01:00:00Z\" &&\n"
                               "         ?d <= \"2012-02-13T04:00:00Z\")\n"
                               "} GROUP BY ?c").
                              arg(c1.localId()).arg(c2.localId()).arg(c3.localId()),
                              QSparqlQuery::SelectStatement));

    QVERIFY(not result.isNull() && not result->hasError());

    {
        QHash<uint, int> dateCounts;

        while(result->next()) {
            dateCounts.insert(result->value(0).toUInt(), result->value(1).toInt());
        }

        QCOMPARE(dateCounts.value(c1.localId()), 2);
        QCOMPARE(dateCounts.value(c2.localId()), 2);
        QCOMPARE(dateCounts.value(c3.localId()), 0);
    }

    // Merge the first two contacts into the third.
    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;
    mergeIds.insertMulti(c3.localId(), c1.localId());
    mergeIds.insertMulti(c3.localId(), c2.localId());

    QctContactMergeRequest mergeRequest;
    mergeRequest.setMergeIds(mergeIds);

    QVERIFY(engine()->startRequest(&mergeRequest));
    QVERIFY(engine()->waitForRequestFinishedImpl(&mergeRequest, 0));

    QCOMPARE(mergeRequest.state(), QctContactMergeRequest::FinishedState);

    // Verify that even the merged contact is free of garbage.
    result.reset(executeQuery(QString::fromLatin1
                              ("SELECT tracker:id(?c) COUNT(?d) {\n"
                               "  ?c a nco:PersonContact; dc:date ?d\n"
                               "  FILTER(tracker:id(?c) IN (%1) &&\n"
                               "         ?d >= \"2012-02-13T01:00:00Z\" &&\n"
                               "         ?d <= \"2012-02-13T04:00:00Z\")\n"
                               "}").arg(c3.localId()),
                              QSparqlQuery::SelectStatement));

    QVERIFY(not result.isNull() && not result->hasError());

    {
        QHash<uint, int> dateCounts;

        while(result->next()) {
            dateCounts.insert(result->value(0).toUInt(), result->value(1).toInt());
        }

        QCOMPARE(dateCounts.value(c3.localId()), 0);
    }

    // Check phone numbers to verify we really merged contacts.
    // Otherwise our positive garbage verdict from above would be bogus.
    QStringList phoneNumbers;

    foreach(const QContactPhoneNumber &detail, contact(c3.localId()).details<QContactPhoneNumber>()) {
        QVERIFY(not detail.number().isEmpty());
        phoneNumbers += detail.number();
    }

    QCOMPARE(phoneNumbers.count(), 2);
    QVERIFY(phoneNumbers.contains(c1.detail<QContactPhoneNumber>().number()));
    QVERIFY(phoneNumbers.contains(c2.detail<QContactPhoneNumber>().number()));
}

static QString
ucFirst(const QString &str)
{
    if (str.isEmpty()) {
        return str;
    }

    QString ret = str;
    ret[0] = ret.at(0).toUpper();

    return ret;
}

void
ut_qtcontacts_trackerplugin::testMergeSyncTarget_data()
{
    QTest::addColumn<QStringList>("inputTargets");
    QTest::addColumn<QString>("finalTarget");
    QTest::addColumn<uint>("expectedError");

    const QString mfeSyncTarget1 = QContactSyncTarget__SyncTargetMfe
                                 + QString::fromLatin1("#accountId");
    const QString mfeSyncTarget2 = ucFirst(QContactSyncTarget__SyncTargetMfe)
                                 + QString::fromLatin1("#anotherId");
    const QString otherSyncTarget = QLatin1String("other");

    // We do mix the case in sync targets on purpose, sync target matching is
    // supposed to be case insensitive

    QTest::newRow("mfe + addressbook")
            << (QStringList() << QContactSyncTarget__SyncTargetMfe
                              << QContactSyncTarget__SyncTargetAddressBook)
            << QContactSyncTarget__SyncTargetMfe.operator QString()
            << uint(QContactManager::NoError);

    QTest::newRow("telepathy + Addressbook")
            << (QStringList() << QContactSyncTarget__SyncTargetTelepathy
                              << ucFirst(QContactSyncTarget__SyncTargetAddressBook))
            << ucFirst(QContactSyncTarget__SyncTargetAddressBook)
            << uint(QContactManager::NoError);

    QTest::newRow("Telepathy + mfe")
            << (QStringList() << ucFirst(QContactSyncTarget__SyncTargetTelepathy)
                              << QContactSyncTarget__SyncTargetMfe)
            << QContactSyncTarget__SyncTargetMfe.operator QString()
            << uint(QContactManager::NoError);

    QTest::newRow("telepathy + addressbook + Mfe")
            << (QStringList() << QContactSyncTarget__SyncTargetTelepathy
                              << QContactSyncTarget__SyncTargetAddressBook
                              << ucFirst(QContactSyncTarget__SyncTargetMfe))
            << ucFirst(QContactSyncTarget__SyncTargetMfe)
            << uint(QContactManager::NoError);

    QTest::newRow("mfe#accountId + addressbook")
            << (QStringList() << mfeSyncTarget1
                              << QContactSyncTarget__SyncTargetAddressBook)
            << mfeSyncTarget1
            << uint(QContactManager::NoError);

    QTest::newRow("mfe#accountId + mfe#accountId + addressbook")
            << (QStringList() << mfeSyncTarget1
                              << mfeSyncTarget1
                              << QContactSyncTarget__SyncTargetAddressBook)
            << mfeSyncTarget1
            << uint(QContactManager::NoError);

    QTest::newRow("mfe#accountId + Mfe#anotherId")
            << (QStringList() << mfeSyncTarget1
                              << ucFirst(mfeSyncTarget2))
            << QString()
            << uint(QContactManager::BadArgumentError);

    QTest::newRow("other + telepathy")
            << (QStringList() << otherSyncTarget
                              << QContactSyncTarget__SyncTargetTelepathy)
            << otherSyncTarget
            << uint(QContactManager::NoError);

    QTest::newRow("(empty) + telepathy")
            << (QStringList() << QString()
                              << QContactSyncTarget__SyncTargetTelepathy)
            << QContactSyncTarget__SyncTargetAddressBook.operator QString()
            << uint(QContactManager::NoError);

    QTest::newRow("telepathy + telepathy")
            << (QStringList() << QContactSyncTarget__SyncTargetTelepathy
                              << QContactSyncTarget__SyncTargetTelepathy)
            << QContactSyncTarget__SyncTargetTelepathy.operator QString()
            << uint(QContactManager::NoError);

    QTest::newRow("(empty) + (empty)")
            << (QStringList() << QString()
                              << QString())
            << QContactSyncTarget__SyncTargetAddressBook.operator QString()
            << uint(QContactManager::NoError);
}

void
ut_qtcontacts_trackerplugin::testMergeSyncTarget()
{
    QFETCH(QStringList, inputTargets);
    QFETCH(QString, finalTarget);
    QFETCH(uint, expectedError);

    QList<QContactLocalId> localIds;

    for (int i = 0; i < inputTargets.size(); ++i) {
        QContact contact;
        QContactName name;
        name.setFirstName(QString::fromLatin1("%1_%2").arg(Q_FUNC_INFO, i));
        contact.saveDetail(&name);
        QContactSyncTarget syncTarget;
        syncTarget.setSyncTarget(inputTargets.at(i));
        contact.saveDetail(&syncTarget);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);

        // We need a special hack to save contacts with sync target == telepathy,
        // since telepathy will get replaced by "addressbook" in the save request
        if (inputTargets.at(i) == QContactSyncTarget__SyncTargetTelepathy) {
            static const QString queryTemplate = QString::fromLatin1(
                        "DELETE {\n"
                        "  ?contact nie:generator ?generator\n"
                        "} WHERE {\n"
                        "  ?contact a nco:PersonContact; nie:generator ?generator\n"
                        "  FILTER(tracker:id(?contact) = %1)\n"
                        "}\n"
                        "INSERT {\n"
                        "  GRAPH <%3> {\n"
                        "    ?contact nie:generator \"%2\"\n"
                        "  }\n"
                        "} WHERE {\n"
                        "  ?contact a nco:PersonContact\n"
                        "  FILTER(tracker:id(?contact) = %1)\n"
                        "}");
            const QString query(queryTemplate.arg(contact.localId())
                                             .arg(QContactSyncTarget__SyncTargetTelepathy)
                                             .arg(QtContactsTrackerDefaultGraphIri));

            QScopedPointer<QSparqlResult> result(executeQuery(query, QSparqlQuery::InsertStatement));

            QVERIFY(not result.isNull());
        }

        localIds.append(contact.localId());
    }

    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;
    QContactLocalId masterId = localIds.takeFirst();

    foreach (QContactLocalId id, localIds) {
        mergeIds.insert(masterId, id);
    }

    QctContactMergeRequest request;
    request.setMergeIds(mergeIds);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinished(&request, 0));

    QCOMPARE(request.error(), QContactManager::Error(expectedError));

    if (not finalTarget.isEmpty()) {
        QContact mergedContact = contact(masterId);
        QCOMPARE(mergedContact.detail<QContactSyncTarget>().syncTarget(), finalTarget);
    }


    // Cleanup before next run
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QVERIFY(engine()->removeContact(masterId, &error));
    QCOMPARE(error, QContactManager::NoError);
}

void
ut_qtcontacts_trackerplugin::testMergeContactsWithGroups()
{
    QList<QContactLocalId> ids;
    QContactPhoneNumber number;
    QContactManager::Error error = QContactManager::UnspecifiedError;

    QContact group;
    QContactType type;
    QContactNickname name;

    type.setType(QContactType::TypeGroup);
    group.saveDetail(&type);
    name.setNickname(QString::fromLatin1("TestGroup_%1").arg(Q_FUNC_INFO));
    group.saveDetail(&name);
    engine()->saveContact(&group, &error);
    QCOMPARE(error, QContactManager::NoError);


    for (int i = 0; i< 2; ++i) {
        QContact contact;
        QContactName name;
        name.setFirstName(QString::fromLatin1("%1_%2").arg(Q_FUNC_INFO, i));
        contact.saveDetail(&name);
        if (i == 1) {
            number.setNumber("1234");
            number.setSubTypes(QContactPhoneNumber::SubTypeMobile);
            contact.saveDetail(&number);
        }

        QContactManager::Error error = QContactManager::UnspecifiedError;
        engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);

        QContactRelationship r;
        r.setFirst(group.id());
        r.setSecond(contact.id());
        r.setRelationshipType(QContactRelationship::HasMember);
        ids.append(contact.localId());

        error = QContactManager::UnspecifiedError;
        engine()->saveRelationship(&r, &error);
        QCOMPARE(error, QContactManager::NoError);
    }

    QCOMPARE(ids.length(), 2);

    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;
    mergeIds.insert (ids[0], ids[1]);

    QctContactMergeRequest request;
    request.setMergeIds(mergeIds);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QContact mergedContact = contact(ids[0]);
    QCOMPARE(mergedContact.detail<QContactPhoneNumber>().number(), number.number());
}

void
ut_qtcontacts_trackerplugin::updateIMContactStatus(const QString &contactIri,
                                                   const QString &imStatus)
{
    const QString queryString = QString::fromLatin1
            ("DELETE { <%1> nco:imPresence ?s } WHERE { <%1> nco:imPresence ?s }\n"
             "DELETE { <%1> nco:presenceLastModified ?t } WHERE { <%1> nco:presenceLastModified ?t }\n"
             "INSERT { <%1> nco:imPresence %2; nco:presenceLastModified '%3'^^xsd:dateTime }").
            arg(contactIri, imStatus, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    delete executeQuery(queryString, QSparqlQuery::InsertStatement);
}

void
ut_qtcontacts_trackerplugin::mergeContacts(QContact &contactTarget,
                                           const QList<QContactLocalId> &sourceContactIds)
{
    QMultiMap<QContactLocalId, QContactLocalId> mergeIds;

    foreach (QContactLocalId id, sourceContactIds) {
        mergeIds.insert (contactTarget.localId(), id);
    }

    QctContactMergeRequest mergeRequest;
    mergeRequest.setMergeIds(mergeIds);

    // Start the request and wait for it to finish.
    QVERIFY(engine()->startRequest(&mergeRequest));
    QVERIFY(engine()->waitForRequestFinished(&mergeRequest, 5000));

    // once they are merged - that's it - no contacts or relationship track exists
    foreach (QContactLocalId mergedId, sourceContactIds) {
        QContact second = contact(mergedId);
        QCOMPARE(second.localId(), 0U); // as not existing
    }

    QContact masterContact = contact(contactTarget.localId());
    QCOMPARE(masterContact.localId(), contactTarget.localId());
    contactTarget = masterContact;
}

void
ut_qtcontacts_trackerplugin::unmergeContact(QContact &masterContact,
                                            const QList<QContactOnlineAccount> &unmergeCriteria,
                                            /*out*/ QList<QContactLocalId> &newUnmergedContactIds)
{
    QList<QContactOnlineAccount> onlineAccounts = unmergeCriteria;
    QctUnmergeIMContactsRequest unmergeRequest;
    unmergeRequest.setSourceContact(masterContact);
    QCOMPARE(unmergeRequest.sourceContact(), masterContact);
    unmergeRequest.setUnmergeOnlineAccounts(onlineAccounts);
    QCOMPARE(unmergeRequest.unmergeOnlineAccounts(), onlineAccounts);

    // Start the request and wait for it to finish.
    QVERIFY(engine()->startRequest(&unmergeRequest));
    QVERIFY(engine()->waitForRequestFinished(&unmergeRequest, 2000));

    newUnmergedContactIds = unmergeRequest.unmergedContactIds();
    QList<QContact> contactsAfter = this->contacts(unmergeRequest.unmergedContactIds());
    QCOMPARE(contactsAfter.size(), unmergeCriteria.size());
    foreach (const QContact &contact, contactsAfter) {
        QList<QContactOnlineAccount> accounts = contact.details<QContactOnlineAccount>();
        QCOMPARE(accounts.size(), 1);
    }

    QContact before(masterContact);
    masterContact = contact(masterContact.localId());
    QCOMPARE(masterContact.details<QContactOnlineAccount>().size(), before.details<QContactOnlineAccount>().size() - unmergeCriteria.size());
}


void
ut_qtcontacts_trackerplugin::testMergeOnlineContacts()
{
    QList<QContact> contacts;

    for (int i = 0; i < 3; ++i) {
        const QString contactIri =
                QString::fromLatin1("contact:%1:%2").
                arg(QLatin1String(__func__), QString::number(i + 1));
        const QString imId =
                QString::fromLatin1("%1.%2@ovi.com").
                arg(QLatin1String(__func__), QString::number(i + 1));
        const QString accountId =
                QString::fromLatin1("/org/freedesktop/testMergeOnlineContacts/account/%1").
                arg(i + 1);

        uint contactId = insertIMContact(contactIri, imId,
                                         QLatin1String("nco:presence-status-available"),
                                         QLatin1String("Lost in Transition"), accountId);
        QContact c = contact(contactId, QStringList(QContactOnlineAccount::DefinitionName));
        QCOMPARE(c.localId(), contactId);
        contacts << c;
    }

    QContact masterContact = contacts.at(0);
    mergeContacts(masterContact, QList<QContactLocalId>() << contacts[1].localId() << contacts[2].localId());
    QList<QContactOnlineAccount> onlineAccounts = masterContact.details<QContactOnlineAccount>();
    QCOMPARE(onlineAccounts.size(), 3);

    // insert fuzzed content to master contact, to verify it works after editing IM Contact
    fuzzContact(masterContact, QStringList() << QContactName::DefinitionName);
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&masterContact, &error));
    QCOMPARE(error, QContactManager::NoError);

    // now unmerge them and verify all is the same as before...
    QList<QContactLocalId> newUnmergedContactIds;
    unmergeContact(masterContact, onlineAccounts, newUnmergedContactIds);
    QCOMPARE(newUnmergedContactIds.size(), 3);

    // delete local contact and try again, this time merging to local contact (previous was merging to another im contact)
    QVERIFY2(engine()->removeContact(masterContact.localId(), &error), QString("Removing contact with localid %1 failed").arg(masterContact.localId()).toLatin1().constData());
    QCOMPARE(error,  QContactManager::NoError);


    masterContact = QContact();
    fuzzContact(masterContact);
    QVERIFY(engine()->saveContact(&masterContact, &error));
    QCOMPARE(error, QContactManager::NoError);
    masterContact = contact(masterContact.localId());
    QContact originalMasterContact(masterContact);
    mergeContacts(masterContact, newUnmergedContactIds.mid(0,2));
    QCOMPARE(originalMasterContact.details<QContactOnlineAccount>().size() + 2, masterContact.details<QContactOnlineAccount>().size());

    // unmerge
    unmergeContact(masterContact, onlineAccounts.mid(0,2), newUnmergedContactIds);
    QCOMPARE(newUnmergedContactIds.size(), 2);

    // here we will hijack the test to test that cleanup keeps items that are not in qct graph (address)
    // and items in qct graph parented in some other graph (like merged contact's nco:IMAddress)
    QScopedPointer<QSparqlResult> insert
            (executeQuery(QLatin1String("INSERT {<testMergeOnlineContacts> a nco:PostalAddress}"),
                          QSparqlQuery::InsertStatement));
    QVERIFY(not insert.isNull());

    QContact testSaveInvolvingCleanup;
    QVERIFY(engine()->saveContact(&testSaveInvolvingCleanup, &error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY2(engine()->removeContact(testSaveInvolvingCleanup.localId(), &error), QString("Removing contact with localid %1 failed").arg(masterContact.localId()).toLatin1().constData());
    QCOMPARE(error,  QContactManager::NoError);

    QList<QContact> contactsAfter = this->contacts(newUnmergedContactIds);
    QCOMPARE(contactsAfter.size(), newUnmergedContactIds.size());
    foreach (const QContact &contact, contactsAfter) {
        QList<QContactOnlineAccount> accounts = contact.details<QContactOnlineAccount>();
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(contact.detail<QContactSyncTarget>().syncTarget(), QLatin1String("telepathy"));
    }

    // see NB#215611 - though postal address is "junk" - not having parent resource linking to it,
    // as it is inserted in other then qtcontacts-tracker graph QTrackerContactSaveRequest::
    // cleanupQueryString() - performing garbage collecting on ever save request -
    // should not delete it.
    const QScopedPointer<QSparqlResult> result
            (executeQuery(QLatin1String("ASK {<testMergeOnlineContacts> a nco:PostalAddress}"),
                          QSparqlQuery::AskStatement));

    QVERIFY(not result.isNull());
    QVERIFY(result->next());
    QCOMPARE(result->value(0).toInt(), 1);
}

void
ut_qtcontacts_trackerplugin::testIMContactsAndMetacontactMasterPresence()
{
    QSKIP("not supported right now", SkipAll);

    QList<unsigned int> idsToMerge;
    QContactLocalId masterContactId = 0; // using one master contact later for additional testing
    const QString serviceProviderId = QLatin1String("ovi.com");
    for(int i = 0; i < 2; ++i) {
        const QString contactIri =
                QString::fromLatin1("contact:%1:%2").
                arg(QLatin1String(__func__), QString::number(i + 1));
        const QString imId =
                QString::fromLatin1("%1.%2@ovi.com").
                arg(QLatin1String(__func__), QString::number(i + 1));
        const QString accountId =
                QString::fromLatin1("/org/freedesktop/testIMContactsAndMetacontactMasterPresence/account/%1").
                arg(i + 1);

        unsigned int contactId = insertIMContact(contactIri, imId,
                                                 QLatin1String("nco:presence-status-available"),
                                                 QLatin1String("Round and about"), accountId,
                                                 QLatin1String("jabber"), serviceProviderId);

        idsToMerge << contactId;

        QContact c = contact(contactId, QStringList(QContactOnlineAccount::DefinitionName));
        QCOMPARE(c.localId(), contactId);

        // verify IM account data
        QContactOnlineAccount account = c.detail<QContactOnlineAccount>();
        QCOMPARE(account.serviceProvider(), serviceProviderId);
        QCOMPARE(account.protocol(), QContactOnlineAccount::ProtocolJabber);
        QVERIFY(account.capabilities().contains(QLatin1String("TextChat")));
        QVERIFY(account.capabilities().contains(QLatin1String("AudioCalls")));

        // verify contact name
        QContact firstContact;
        QContactName name;
        name.setFirstName("FirstMetaWithIM"+QString::number(contactId));
        QVERIFY(firstContact.saveDetail(&name));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&firstContact, &error));
        QCOMPARE(error,  QContactManager::NoError);

        // save metarelationship
        QList<QContactRelationship> rels;
        rels << QContactRelationship();
        rels.last().setRelationshipType(QContactRelationship::IsSameAs);
        rels.last().setFirst(firstContact.id());
        masterContactId = firstContact.localId();
        rels.last().setSecond(c.id());

        error = QContactManager::UnspecifiedError;
        bool saved = engine()->saveRelationships(&rels, 0, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(saved);
    }

    QVERIFY(0 != masterContactId);

    // expected behavior - is that master contact contains all details aggregated
    {
        QList<QContact> cons =
                contacts(QList<QContactLocalId>() << masterContactId << 999999,
                         QStringList() << QContactOnlineAccount::DefinitionName
                                       << QContactPresence::DefinitionName);

        QCOMPARE(cons.size(), 1);
        QCOMPARE(cons.first().localId(), masterContactId);

        bool containsDetail = false;
        foreach(QContactPresence det, cons.first().details<QContactPresence>()) {
            if (det.linkedDetailUris().join("").contains("999999@ovi.com")) {
                QCOMPARE(uint(det.presenceState()), uint(QContactPresence::PresenceAvailable));
                containsDetail = true;
            }
        }

        QVERIFY(containsDetail);
    }

    //now update presence to IM Address and check it in contact (TODO and if signal is emitted)
    updateIMContactStatus("telepathy:/org/freedesktop/fake/account/999999!999999@ovi.com",
                          "nco:presence-status-offline");

    {
        QList<QContact> cons =
                contacts(QList<QContactLocalId>() << masterContactId << 999999,
                         QStringList() << QContactOnlineAccount::DefinitionName
                                       << QContactPresence::DefinitionName);

        QCOMPARE(cons.size(), 1);
        QCOMPARE(cons.first().localId(), masterContactId);

        QContactOnlineAccount accountDetail;
        QContactPresence presenceDetail;

        foreach(const QContactOnlineAccount &detail, cons.first().details<QContactOnlineAccount>()) {
            if (detail.detailUri().contains("999999@ovi.com")) {
                accountDetail = detail;
            }
        }

        foreach(const QContactPresence &detail, cons.first().details<QContactPresence>()) {
            if (detail.linkedDetailUris().join("").contains("999999@ovi.com")) {
                QCOMPARE(uint(detail.presenceState()), uint(QContactPresence::PresenceOffline));
                presenceDetail = detail;
            }
        }

        // test sanity of detail URIs
        QVERIFY(accountDetail.detailUri().startsWith("telepathy:"));
        QVERIFY(accountDetail.detailUri().endsWith("999999@ovi.com"));

        QVERIFY(presenceDetail.detailUri().startsWith("presence:"));
        QVERIFY(presenceDetail.detailUri().endsWith("999999@ovi.com"));

        // test double linking of detail URIs
        QVERIFY(accountDetail.linkedDetailUris().contains(presenceDetail.detailUri()));
        QVERIFY(presenceDetail.linkedDetailUris().contains(accountDetail.detailUri()));
    }

    // load contact should load also all merged content from other contacts (that dont exis anymore)
    {
        QList<QContact> cons =
                contacts(QList<QContactLocalId>() << masterContactId << 999999,
                         QStringList() << QContactOnlineAccount::DefinitionName
                                       << QContactPresence::DefinitionName);

        QCOMPARE(cons.size(), 1);
        QCOMPARE(cons.first().localId(), masterContactId);

        bool containsDetail = false;
        foreach(QContactPresence det, cons.first().details<QContactPresence>()) {
            if (det.linkedDetailUris().join("").contains("999999@ovi.com")) {
                QCOMPARE(uint(det.presenceState()), uint(QContactPresence::PresenceOffline));
                containsDetail = true;
            }
        }

        QVERIFY(containsDetail);
    }

    // remove them
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY2(engine()->removeContact(masterContactId, &error), "Removing a contact failed");
    QCOMPARE(error,  QContactManager::NoError);

    const QContactFetchHint guidFetchHint = fetchHint<QContactGuid>();

    foreach(const QContactLocalId &id, idsToMerge) {
        error = QContactManager::UnspecifiedError;
        QVERIFY2(engine()->contactImpl(id, guidFetchHint, &error).isEmpty(),
                 "Merged contact doesn't exist and fetching it should fail");
        QCOMPARE(error,  QContactManager::DoesNotExistError);
    }
}

void
ut_qtcontacts_trackerplugin::testIMContactsFiltering()
{
    QList<uint> idsToRemove;
    QList<QContactLocalId> idsToRetrieveThroughFilter;

    for (int i = 0; i < 3; i++ ) {
        const QString protocol = QLatin1String((i<2) ? "jabber" : "irc");
        const QString serviceProvider = QString::fromLatin1("ovi%1.com").arg(i/2);
        const QString imId = QString::fromLatin1("%1@ovi.com").arg(999995 + i);
        const QString accountPath = QString::fromLatin1("/org/freedesktop/testIMContactsFiltering/account/%1").arg(i/2);
        const QString contactIri = makeTelepathyIri(accountPath, imId);

        unsigned int contactId = insertIMContact(contactIri, imId,
                                                 QLatin1String("nco:presence-status-available"),
                                                 QLatin1String("Wearing purple blue glasses"),
                                                 accountPath, protocol, serviceProvider);

        idsToRemove << contactId;

        if (0 == i/2) {
            idsToRetrieveThroughFilter << contactId;
        }
    }

    {
        // now filter by service provider ovi0.com needs to return 2 contacts, 999995 & 999996
        QList<QContactLocalId> ids(idsToRetrieveThroughFilter);

        QContactDetailFilter filter;
        filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                       QContactOnlineAccount::FieldServiceProvider);
        filter.setValue(QString("ovi0.com"));
        filter.setMatchFlags(QContactFilter::MatchExactly);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QContactList contacts = engine()->contacts(filter, NoSortOrders, fetchHint<QContactOnlineAccount>(), &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY2(contacts.count() >= 2, qPrintable(QString::number(contacts.count())));

        foreach(const QContact &contact, contacts) {
            QVERIFY(contact.detail<QContactOnlineAccount>().serviceProvider() == "ovi0.com");
            ids.removeOne(contact.localId());
        }

        QVERIFY(ids.isEmpty());
    }

    // now account path filter
    {
        // now filter by account path 999995 & 999996
        QList<QContactLocalId> ids(idsToRetrieveThroughFilter);

        QContactDetailFilter filter;

        filter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName, "AccountPath");

        // see insertTpContact
        filter.setValue(QString("/org/freedesktop/testIMContactsFiltering/account/0"));
        filter.setMatchFlags(QContactFilter::MatchExactly);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QContactList contacts = engine()->contacts(filter, NoSortOrders, fetchHint<QContactOnlineAccount>(), &error);

        QCOMPARE(error, QContactManager::NoError);
        QVERIFY2(contacts.count() >= 2, qPrintable(QString::number(contacts.count())));

        foreach(const QContact &contact, contacts) {
            QCOMPARE(contact.detail<QContactOnlineAccount>().serviceProvider(),
                     QString::fromLatin1("ovi0.com"));
            ids.removeOne(contact.localId());
        }

        QVERIFY(ids.isEmpty());
    }


    // remove them
    foreach(QContactLocalId id, idsToRemove) {
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY2(engine()->removeContact(id, &error), qPrintable(QString::number(id)));
        QCOMPARE(error,  QContactManager::NoError);
    }

}

void
ut_qtcontacts_trackerplugin::testContactsWithoutMeContact() {
    const QContactLocalId zeroLocalId = 0;

    QContact c;
    QContactName name;
    name.setFirstName("Totally");
    name.setLastName("Unique");
    QVERIFY(c.saveDetail(&name));
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QVERIFY(engine()->saveContact(&c, &error));
    QCOMPARE(error,  QContactManager::NoError);
    const QContactLocalId id = c.localId();
    addedContacts.append(id);

    {
    // Prepare the requst - give filter to it and specify which fields to fetch. We fetch only the name.
    QStringList details;
    details << QContactName::DefinitionName;

    QContactLocalIdFetchRequest nameFetchRequest;
    // only fetch the one contact saved above
    nameFetchRequest.setFilter(localIdFilter(id));

    // Start the request and wait for it to finish.
    engine()->startRequest(&nameFetchRequest);
    engine()->waitForRequestFinished(&nameFetchRequest, 1000);

    // Requst finished. Test that only one contact is removed.
    QList<QContactLocalId> contacts = nameFetchRequest.ids();
    QVERIFY2(contacts.count() < 2, "We expected to get only one contact. Got more.");
    QVERIFY2(contacts.count() != 0, "We expected to get one contact. Got none.");
    QVERIFY2(contacts.first() == id, "Did not get the requested contact back.");
    }


    qDebug() << Q_FUNC_INFO << "try early delete";
    // test here deleting request too early
    for (int i = 0; i < 10; i++){
    // Prepare the requst - give filter to it and specify which fields to fetch. We fetch only the name.
    QStringList details;
    details << QContactName::DefinitionName;

    QContactLocalIdFetchRequest nameFetchRequest;
    // only fetch the one contact saved above
    nameFetchRequest.setFilter(localIdFilter(zeroLocalId));

    // Start the request and wait for it to finish.
    engine()->startRequest(&nameFetchRequest);
    qDebug() << Q_FUNC_INFO << 1;
    engine()->waitForRequestFinished(&nameFetchRequest, 0);
    qDebug() << Q_FUNC_INFO << 2;
    }
}

void
ut_qtcontacts_trackerplugin::testDefinitionNames()
{
    QMap<QString, QContactDetailDefinition> defs;
    QContactManager::Error error(QContactManager::UnspecifiedError);
    defs = engine()->detailDefinitions(QContactType::TypeContact, &error);
    QCOMPARE(error,  QContactManager::NoError);

    foreach(QString key, defs.keys()) {
        QCOMPARE(defs[key].name(), key);
    }

    defs = engine()->detailDefinitions("entirely random string", &error);
    QCOMPARE(error,  QContactManager::InvalidContactTypeError);
}

void
ut_qtcontacts_trackerplugin::testMeContact()
{
    const QString randomName(QUuid::createUuid().toString());
    QContactManager::Error error(QContactManager::NoError);

    const QContactLocalId meContactId(engine()->selfContactId(&error));
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(meContactId != 0);

    QContactId contactId;
    contactId.setLocalId(meContactId);

    QContactName name;
    name.setFirstName(randomName);

    QContact meContact;
    meContact.setId(contactId);
    QVERIFY(meContact.saveDetail(&name));

    QVERIFY(engine()->saveContact(&meContact, &error));
    QCOMPARE(error, QContactManager::NoError);

    meContact = QContact();
    name = meContact.detail<QContactName>();
    QVERIFY(name.isEmpty());

    QTest::qWait(100);

    qDebug() << Q_FUNC_INFO << 1;
    meContact = contact(meContactId, QStringList() << QContactName::DefinitionName);
    qDebug() << Q_FUNC_INFO << 2;
    QCOMPARE(meContact.localId(), meContactId);

    name = meContact.detail<QContactName>();
    QCOMPARE(name.firstName(), randomName);
}

const QList<ContactDetailSample>
ut_qtcontacts_trackerplugin::displayLabelDetailSamples(bool preferNickname, bool preferLastname)
{
    changeSetting(QctSettings::NameOrderKey, preferLastname ? QContactDisplayLabel__FieldOrderLastName
                                                            : QContactDisplayLabel__FieldOrderFirstName);
    changeSetting(QctSettings::PreferNicknameKey, preferNickname);

    QList<ContactDetailSample> detailList;

    // match values in "test-account-1.ttl" -
    // we resort to this file because presence properties are readonly
    const QString accountUri1 = QLatin1String("no-presence@talk.com");
    const QString accountUri2 = QLatin1String("first.last@talk.com");
    const QString accountPath = QLatin1String("/org/freedesktop/fake/account");

    QContactOnlineAccount onlineAccount1;
    onlineAccount1.setValue(QContactOnlineAccount::FieldAccountUri, accountUri1);
    onlineAccount1.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
    onlineAccount1.setDetailUri(makeTelepathyIri(accountPath, accountUri1));

    QContactOnlineAccount onlineAccount2;
    onlineAccount2.setValue(QContactOnlineAccount::FieldAccountUri, accountUri2);
    onlineAccount2.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
    onlineAccount2.setDetailUri(makeTelepathyIri(accountPath, accountUri2));

    QContactName customLabel;
    customLabel.setCustomLabel(QLatin1String("Custom Label"));

    QContactName name;
    name.setFirstName(QLatin1String("FirstName"));
    name.setLastName(QLatin1String("LastName"));

    detailList << makeDetailSample(QContactUrl::DefinitionName,
                                   QContactUrl::FieldUrl,
                                   QLatin1String("www.url.com"));
    detailList << makeDetailSample(QContactPhoneNumber::DefinitionName,
                                   QContactPhoneNumber::FieldNumber,
                                   QLatin1String("555444333"));
    detailList << makeDetailSample(QContactEmailAddress::DefinitionName,
                                   QContactEmailAddress::FieldEmailAddress,
                                   QLatin1String("first.last@mail.com"));
    detailList << makeDetailSample(onlineAccount1, onlineAccount1.accountUri());
    detailList << makeDetailSample(onlineAccount2, QLatin1String("The Dude"));
    detailList << makeDetailSample(QContactOrganization::DefinitionName,
                                   QContactOrganization::FieldName,
                                   QLatin1String("Company"));
    detailList << makeDetailSample(QContactName::DefinitionName,
                                   QContactName::FieldMiddleName,
                                   QLatin1String("Great"));
    detailList << makeDetailSample(customLabel, customLabel.customLabel());
    detailList << makeDetailSample(name, preferLastname ? QLatin1String("LastName FirstName")
                                                        : QLatin1String("FirstName LastName"));

    detailList.insert(detailList.count() - (preferNickname ? 0 : 2),
                      makeDetailSample(QContactNickname::DefinitionName,
                                       QContactNickname::FieldNickname,
                                       QLatin1String("The Great Dude")));

    return detailList;
}

static QContactDetail getDetailFromSample(const ContactDetailSample &sample,
                                          const QContact &contact)
{
    QContactDetail detail = contact.detail(sample.first.definitionName());

    if (detail.isEmpty()) {
        detail = sample.first;
    } else {
        const QVariantMap values = sample.first.variantValues();
        for(QVariantMap::ConstIterator i = values.constBegin(); i != values.constEnd(); ++i) {
            detail.setValue(i.key(), i.value());
        }
    }

    return detail;
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSynthesized_data()
{
    QTest::addColumn<bool>("preferNickname");
    QTest::addColumn<bool>("preferLastname");

    QTest::newRow("FirstName:Nickname") << false << false;
    QTest::newRow("LastName:Nickname") << false << true;
    QTest::newRow("Nickname:FirstName") << true << false;
    QTest::newRow("Nickname:LastName") << true << true;
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSynthesized()
{
    QFETCH(bool, preferNickname);
    QFETCH(bool, preferLastname);

    QContact contact;

    foreach(ContactDetailSample sample, displayLabelDetailSamples(preferNickname, preferLastname)) {
        QContactDetail detail = getDetailFromSample(sample, contact);
        QVERIFY(contact.saveDetail(&detail));

        if (QContactOnlineAccount::DefinitionName == detail.definitionName() &&
            QLatin1String("first.last@talk.com") == detail.value(QContactOnlineAccount::FieldAccountUri)) {
            QContactGlobalPresence presence;
            presence.setNickname(QLatin1String("The Dude"));
            QVERIFY(contact.saveDetail(&presence));
        }

        QContactManager::Error error;
        const QString displayLabel(engine()->synthesizedDisplayLabel(contact, &error));

        QCOMPARE(error, QContactManager::NoError);
        QCOMPARE(displayLabel, sample.second);
    }
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSaved_data()
{
    QTest::addColumn<bool>("preferNickname");
    QTest::addColumn<bool>("preferLastname");

    QTest::newRow("FirstName:Nickname") << false << false;
    QTest::newRow("LastName:Nickname") << false << true;
    QTest::newRow("Nickname:FirstName") << true << false;
    QTest::newRow("Nickname:LastName") << true << true;
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSaved()
{
    QFETCH(bool, preferNickname);
    QFETCH(bool, preferLastname);

    QContact contact;

    foreach(const ContactDetailSample &sample, displayLabelDetailSamples(preferNickname, preferLastname)) {
        // add another detail to the contact
        QContactDetail detail = getDetailFromSample(sample, contact);
        QVERIFY(contact.saveDetail(&detail));

        if (QContactOnlineAccount::DefinitionName == detail.definitionName() &&
            QLatin1String("first.last@talk.com") == detail.value(QContactOnlineAccount::FieldAccountUri)) {
            QContactGlobalPresence presence;
            presence.setNickname(QLatin1String("The Dude"));
            QVERIFY(contact.saveDetail(&presence));
        }

        // save contact and check the updated display label
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool isContactSaved = engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(isContactSaved);
        QCOMPARE(contact.detail<QContactDisplayLabel>().label(), sample.second);
    }
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelFetch_data()
{
    QTest::addColumn<QStringList>("definitionHints");
    QTest::addColumn<bool>("hasDisplayLabel");
    QTest::addColumn<bool>("preferNickname");
    QTest::addColumn<bool>("preferLastname");

    for(int i = 0; i < 4; ++i) {
        const bool preferNickname = bool(i / 2);
        const bool preferLastname = bool(i % 2);

        const QString tag = (QLatin1String(preferNickname ? "Nickname:" : "") +
                             QLatin1String(preferLastname ? "Lastname" : "Firstname") +
                             QLatin1String(preferNickname ? "" : ":Nickname"));

        QTest::newRow(tag + QLatin1String(":NoHints"))
                << QStringList()
                << true << preferNickname << preferLastname;
        QTest::newRow(tag + QLatin1String(":DisplayLabel"))
                << (QStringList() << QContactDisplayLabel::DefinitionName)
                << true << preferNickname << preferLastname;
        QTest::newRow(tag + QLatin1String(":Note"))
                << (QStringList() << QContactNote::DefinitionName)
                << false << preferNickname << preferLastname;
        QTest::newRow(tag + QLatin1String(":NoteAndDisplayLabel"))
                << (QStringList() << QContactNote::DefinitionName
                                  << QContactDisplayLabel::DefinitionName)
                << true << preferNickname << preferLastname;
    }
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelFetch()
{
    QFETCH(QStringList, definitionHints);
    QFETCH(bool, hasDisplayLabel);
    QFETCH(bool, preferNickname);
    QFETCH(bool, preferLastname);

    QContact contact;
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(definitionHints);

    foreach(ContactDetailSample sample, displayLabelDetailSamples(preferNickname, preferLastname)) {
        QContactDetail detail = getDetailFromSample(sample, contact);
        QVERIFY(contact.saveDetail(&detail));

        QContactManager::Error error;
        bool contactSaved(engine()->saveContact(&contact, &error));

        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);

        QContact fetchedContact;
        fetchedContact = engine()->contactImpl(contact.localId(), fetchHint, &error);

        QCOMPARE(error, QContactManager::NoError);
        QCOMPARE(fetchedContact.localId(), contact.localId());

        const QString expectedLabel = hasDisplayLabel ? sample.second : QString();
        QCOMPARE(fetchedContact.detail<QContactDisplayLabel>().label(), expectedLabel);
    }
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSynthesizedWithNameOrders_data()
{
    QTest::addColumn<QString>("nameOrder");
    QTest::addColumn<QString>("displayLabel");

    QTest::newRow("FirstName LastName")
            << QString::fromLatin1(QContactDisplayLabel__FieldOrderFirstName.latin1())
            << QString::fromLatin1("FirstName LastName");
    QTest::newRow("LastName FirstName")
            << QString::fromLatin1(QContactDisplayLabel__FieldOrderLastName.latin1())
            << QString::fromLatin1("LastName FirstName");
    QTest::newRow("Default") // is like "first-last"
            << QString()
            << QString::fromLatin1("FirstName LastName");
}

void
ut_qtcontacts_trackerplugin::testDisplayLabelSynthesizedWithNameOrders()
{
    QFETCH(QString, nameOrder);
    QFETCH(QString, displayLabel);


    QContactName nameDetail;
    nameDetail.setFirstName(QLatin1String("FirstName"));
    nameDetail.setLastName(QLatin1String("LastName"));

    QContact contact;
    QVERIFY(contact.saveDetail(&nameDetail));

    const QString createdDisplayLabel = engine()->createDisplayLabel(contact, nameOrder);

    QCOMPARE(createdDisplayLabel, displayLabel);
}

void
ut_qtcontacts_trackerplugin::testSyncTarget_data()
{
    QTest::addColumn<QString>("savedSyncTarget");
    QTest::addColumn<QString>("fetchedSyncTarget");

    // when no Sync target is specified, use default (engine's sync target)
    QTest::newRow("default") << "" << engine()->syncTarget();
    QTest::newRow("custom") << "rambazamba" << "rambazamba";

    if (engine()->isWeakSyncTarget(QContactSyncTarget__SyncTargetTelepathy)) {
        QTest::newRow("telepathy to addressbook") << QContactSyncTarget__SyncTargetTelepathy.latin1()
                                                  << engine()->syncTarget();
    }
}

void
ut_qtcontacts_trackerplugin::testSyncTarget()
{
    QFETCH(QString, savedSyncTarget);
    QFETCH(QString, fetchedSyncTarget);

    // tracker timestamps have a resolution of one second,
    // therefore we must wait a bit between runs
    sleep(1);

    QDateTime dt = QDateTime::currentDateTime().toUTC();

    QContact c;

    QContactName name;
    name.setCustomLabel(__func__);
    QVERIFY(c.saveDetail(&name));

    if (not savedSyncTarget.isEmpty()) {
        QContactSyncTarget syncTarget;
        syncTarget.setSyncTarget(savedSyncTarget);
        QVERIFY(c.saveDetail(&syncTarget));
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContact(&c, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != c.localId());
    QVERIFY(contactSaved);

    QContact noise;
    QVERIFY(noise.saveDetail(&name));
    QContactSyncTarget noiseSyncTarget;
    noiseSyncTarget.setSyncTarget("MFE");
    QVERIFY(noise.saveDetail(&noiseSyncTarget));
    contactSaved = engine()->saveContact(&noise, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != noise.localId());
    QVERIFY(contactSaved);

    QContact contactWithDefaultSyncTarget;
    QVERIFY(contactWithDefaultSyncTarget.saveDetail(&name));
    contactSaved = engine()->saveContact(&contactWithDefaultSyncTarget, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != contactWithDefaultSyncTarget.localId());
    QVERIFY(contactSaved);

    QContactDetailFilter detailFilter;
    detailFilter.setDetailDefinitionName(QContactSyncTarget::DefinitionName,
                                         QContactSyncTarget::FieldSyncTarget);
    detailFilter.setValue(fetchedSyncTarget);

    error = QContactManager::UnspecifiedError;
    QList<QContactLocalId> contactIds = engine()->contactIds(detailFilter, NoSortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactIds.contains(c.localId()));
    QVERIFY(not contactIds.contains(noise.localId()));

    QContactChangeLogFilter eventFilter;
    eventFilter.setEventType(QContactChangeLogFilter::EventAdded);
    eventFilter.setSince(dt);

    QContactDetailFilter detailFilterDefaultSyncTarget;
    detailFilterDefaultSyncTarget.setDetailDefinitionName(QContactSyncTarget::DefinitionName,
                                         QContactSyncTarget::FieldSyncTarget);
    detailFilterDefaultSyncTarget.setValue(engine()->syncTarget());


    error = QContactManager::UnspecifiedError;
    contactIds = engine()->contactIds((detailFilterDefaultSyncTarget | detailFilter) & eventFilter,
                                      NoSortOrders, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactIds.contains(c.localId()));
    QVERIFY(contactIds.contains(contactWithDefaultSyncTarget.localId()));
    QCOMPARE(contactIds.size(), 2);
}

/************* SECTION FOR TESTING NB#161788 and its dependant bugs **********************/

/* HOW THIS WORKS:
 * QList<editStruct> is filled with editStructs.
 * These structs are used for both editing contact and
 * verifying save results. NO SAVE SHOULD RESULT IN DUPLICATE ENTRIES.
 *
 * The editStruct in a QList<editStruct> is one edit. A list is a collection of edits to a
 * contact in chronological order. editStruct is an edit for the contact state as defined
 * by the previous editStruct in the list.
 *
 */

// simple helper to set both name and __LINE__ of a editStruct
#define EDITSTRUCT(ES, NAME) { ES.line = __LINE__; ES.name = NAME; }

// helper struct for edit testing
typedef struct editStruct {
    int line;
    QString name;
    QContactName NameTest;
    QContactPhoneNumber PhoneNumberTest;
    QContactEmailAddress EmailAddressTest;
    QContactUrl UrlTest;
    QContactAddress AddressTest;
} editStruct;

void
ut_qtcontacts_trackerplugin::setName(editStruct &es, QString first, QString last)
{
    if(!first.isEmpty()) {
        es.NameTest.setFirstName(first);
    }
    if(!last.isEmpty()) {
        es.NameTest.setLastName(last);
    }
}

void
ut_qtcontacts_trackerplugin::setEmail(editStruct &es, QString email, QString context)
{
    es.EmailAddressTest.setEmailAddress(email);

    if(!context.isEmpty()) {
        es.EmailAddressTest.setContexts(context);
    }
}

void
ut_qtcontacts_trackerplugin::setUrl(editStruct &es, QString url, QString context, QString subType)
{
    es.UrlTest.setUrl(url);

    if(!context.isEmpty()) {
        es.UrlTest.setContexts(context);
    }

    if(!subType.isEmpty()) {
        es.UrlTest.setSubType(subType);
    }
}

void
ut_qtcontacts_trackerplugin::setPhone(editStruct &es, QString phone, QString context, QString subType)
{
    es.PhoneNumberTest.setNumber(phone);

    if(!context.isEmpty())
    {
        es.PhoneNumberTest.setContexts(context);
    }

    if(!subType.isEmpty()) {
        es.PhoneNumberTest.setSubTypes(subType);
    }
}

void
ut_qtcontacts_trackerplugin::setAddress(editStruct &es, QString street, QString postcode,
                                        QString pobox, QString locality, QString region,
                                        QString country, QString context, QString subType)
{
    if(!street.isEmpty()) {
        es.AddressTest.setStreet(street);
    }
    if(!postcode.isEmpty()) {
        es.AddressTest.setPostcode(postcode);
    }
    if(!pobox.isEmpty()) {
        es.AddressTest.setPostOfficeBox(pobox);
    }
    if(!locality.isEmpty()) {
        es.AddressTest.setLocality(locality);
    }
    if(!region.isEmpty()) {
        es.AddressTest.setRegion(region);
    }
    if(!country.isEmpty()) {
        es.AddressTest.setCountry(country);
    }

    if(!context.isEmpty()) {
        es.AddressTest.setContexts(context);
    }

    if(!subType.isEmpty()) {
        es.AddressTest.setSubTypes(subType);
    }
}

void
ut_qtcontacts_trackerplugin::saveEditsToContact(editStruct &es, QContact &c)
{
    if(!es.NameTest.isEmpty()) {
        QVERIFY(c.saveDetail(&(es.NameTest)));
    }
    if(!es.EmailAddressTest.isEmpty()) {
        QVERIFY(c.saveDetail(&(es.EmailAddressTest)));
    }
    if(!es.UrlTest.isEmpty()) {
        QVERIFY(c.saveDetail(&(es.UrlTest)));
    }
    if(!es.PhoneNumberTest.isEmpty()) {
        QVERIFY(c.saveDetail(&(es.PhoneNumberTest)));
    }
    if(!es.AddressTest.isEmpty()) {
        QVERIFY(c.saveDetail(&(es.AddressTest)));
    }
}

/// @note for this part of the tests we verify that
// * there are no entry duplications when editing a specific detail
// * there are no undeleted details when deleting them
// So if a member variable from the editStruct is empty the edit
// essentially is deleting that, if the previous editStruct in the
// list had information there.
void
ut_qtcontacts_trackerplugin::verifyEdits(QContact &verify, editStruct &es)
{
    // tell output reader what we're verifying
    qDebug() << "Verifying: " << es.name << '(' << es.line << ") contact:" << verify.localId();
    if(!es.NameTest.isEmpty()) {
        QCOMPARE(verify.details(QContactName::DefinitionName).count(), 1);
    } else {
        QCOMPARE(verify.details(QContactName::DefinitionName).count(), 0);
    }
    if(!es.EmailAddressTest.isEmpty()) {
        QCOMPARE(verify.details(QContactEmailAddress::DefinitionName).count(), 1);
    } else {
        QCOMPARE(verify.details(QContactEmailAddress::DefinitionName).count(), 0);
    }
    if(!es.UrlTest.isEmpty()) {
        QCOMPARE(verify.details(QContactUrl::DefinitionName).count(), 1);
    } else {
        QCOMPARE(verify.details(QContactUrl::DefinitionName).count(), 0);
    }
    if(!es.PhoneNumberTest.isEmpty()) {
        QCOMPARE(verify.details(QContactPhoneNumber::DefinitionName).count(), 1);
    } else {
        QCOMPARE(verify.details(QContactPhoneNumber::DefinitionName).count(), 0);
    }
    if(!es.AddressTest.isEmpty()) {
        QCOMPARE(verify.details(QContactAddress::DefinitionName).count(), 1);
    } else {
        QCOMPARE(verify.details(QContactAddress::DefinitionName).count(), 0);
    }
}

void
ut_qtcontacts_trackerplugin::runEditList(QList<editStruct> &editList)
{
    static const QStringList editDetails(QStringList() <<
                                         QContactName::DefinitionName <<
                                         QContactPhoneNumber::DefinitionName <<
                                         QContactEmailAddress::DefinitionName <<
                                         QContactAddress::DefinitionName <<
                                         QContactUrl::DefinitionName);

    // Each run through editList creates one new contact.
    QContact c, verify;
    foreach(editStruct es, editList) {
        // apply the edits as specified by es to the contact
        saveEditsToContact(es, c);
        // save the contact to tracker
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        // fetch the saved contact from tracker
        verify = contact(c.localId(), editDetails);
        QVERIFY(c.localId() == verify.localId());
        // verify edits against es
        verifyEdits(verify, es);
        // empty from details, as we don't need them for next run. Note that ID and Type stay the same
        c.clearDetails();
        qDebug() << "End of test" << es.name << '(' << es.line << ')';
    }
}

void
ut_qtcontacts_trackerplugin::testEditCombinations_email()
{
    QList<editStruct> editList;

    editStruct e;
    EDITSTRUCT(e, "name, mail");
    setName(e, "a", "b");
    setEmail(e, "a@b.c");
    editList << e;

    editStruct e2;
    EDITSTRUCT(e2, "name, edit mail");
    setName(e2,"c", "d");
    setEmail(e2,"a@b.c1");
    editList << e2;

    editStruct e3;
    EDITSTRUCT(e3, "name, edit mail, url");
    setName(e3,"c", "d");
    setEmail(e3,"a@b.c3");
    setUrl(e3,"http://a.b.c");
    editList << e3;

    editStruct e4;
    EDITSTRUCT(e4, "name, edit mail (work), url");
    setName(e4, "c", "d");
    setEmail(e4, "a@b.c4", QContactDetail::ContextWork);
    setUrl(e4,"http://a.b.c");
    editList << e4;

    editStruct e5;
    EDITSTRUCT(e5, "name, edit mail (home), url");
    setName(e5, "c", "d");
    setEmail(e5, "a@b.c5", QContactDetail::ContextHome);
    setUrl(e5,"http://a.b.c");
    editList << e5;

    editStruct e6;
    EDITSTRUCT(e6, "name, edit mail (home), url");
    setName(e6, "c", "d");
    setEmail(e6, "a@b.c5", QContactDetail::ContextWork);
    setUrl(e6,"http://a.b.c");
    editList << e6;

    editStruct e7;
    EDITSTRUCT(e7, "edit name, del mail, del url");
    setName(e7, "a", "b");
    editList << e7;

    editStruct e8;
    EDITSTRUCT(e8, "name, add mail");
    setName(e8, "a", "b");
    setEmail(e8, "a@b.c8", QContactDetail::ContextWork);
    editList << e8;

    runEditList(editList);
}

void
ut_qtcontacts_trackerplugin::testEditCombinations_url()
{
    {
        QList<editStruct> editList;

        editStruct e;
        EDITSTRUCT(e, "name, url");
        setName(e, "a", "b");
        setUrl(e,"http://a.b.c");
        editList << e;

        editStruct e2;
        EDITSTRUCT(e2, "name, edit url");
        setName(e2,"a", "b");
        setUrl(e2,"http://a.b.c2");
        editList << e2;

        editStruct e3;
        EDITSTRUCT(e3, "name, edit url, email");
        setName(e3,"a", "b");
        setUrl(e3,"http://a.b.c3");
        setEmail(e3, "a@b.c");
        editList << e3;

        runEditList(editList);
    }

    {
        QList<editStruct> editList2;

        editStruct e;
        EDITSTRUCT(e, "name, url");
        setName(e, "a", "b");
        setUrl(e,"http://a.b.c");
        editList2 << e;

        editStruct e2;
        EDITSTRUCT(e2, "name, edit url");
        setName(e2,"a", "b");
        setUrl(e2,"http://a.b.c2");
        editList2 << e2;

        editStruct e3;
        EDITSTRUCT(e3, "name, edit url, phone");
        setName(e3,"a", "b");
        setUrl(e3,"http://a.b.c3");
        setPhone(e3, "12345");
        editList2 << e3;

        editStruct e4;
        EDITSTRUCT(e4, "name, edit url, edit phone, address");
        setName(e4, "a", "b");
        setUrl(e4, "http://a.b.c4");
        setPhone(e4, "54321");
        setAddress(e4, "Street 1", "20500", "PL 101", "Heretown", "Bigarea", "Kantri");
        editList2 << e4;

        editStruct e5;
        EDITSTRUCT(e5, "name, edit url, edit phone, edit address");
        setName(e5, "a", "b");
        setUrl(e5, "http://a.b.c5");
        setPhone(e5, "51423");
        setAddress(e5, "Alley 2", "00502", "PL 102", "Herecity", "Biggerarea", "Manner");
        editList2 << e5;

        runEditList(editList2);
    }
}

void
ut_qtcontacts_trackerplugin::testEditCombinations_address()
{
    QList<editStruct> editList;

    editStruct e;
    EDITSTRUCT(e, "name, street");
    setName(e, "a", "b");
    setAddress(e,"Street 1");
    editList << e;

    editStruct e2;
    EDITSTRUCT(e2, "name, edit street");
    setName(e2,"a", "b");
    setAddress(e2, "Street 2");
    editList << e2;

    editStruct e3;
    EDITSTRUCT(e3, "name, edit street");
    setName(e3,"a", "b");
    setAddress(e3, "Street 2", "20500");
    editList << e3;

    editStruct e4;
    EDITSTRUCT(e4, "name, edit street, url");
    setName(e4, "a", "b");
    setAddress(e4, "Street 3");
    setUrl(e4, "http://www.url.fi");
    editList << e4;

    runEditList(editList);
}

void
ut_qtcontacts_trackerplugin::testEditCombinations_phone()
{
    QList<editStruct> editList;

    editStruct e;
    EDITSTRUCT(e, "name, phone");
    setName(e, "a", "b");
    setPhone(e,"12345");
    editList << e;

    editStruct e2;
    EDITSTRUCT(e2, "name, edit phone");
    setName(e2,"a", "b");
    setPhone(e2,"54321");
    editList << e2;

    editStruct e3;
    EDITSTRUCT(e3, "name, delete phone");
    setName(e3, "a", "b");
    editList << e3;

    editStruct e4;
    EDITSTRUCT(e4, "name, phone, url");
    setName(e4, "a", "b");
    setPhone(e4, "444555");
    setUrl(e4, "http://a.b.c");
    editList << e4;

    editStruct e5;
    EDITSTRUCT(e5, "name, edit phone, edit url, address");
    setName(e5, "a", "b");
    setPhone(e5, "444556");
    setUrl(e5, "http://a.b.c2");
    setAddress(e5, "Here street 0");
    editList << e5;

    runEditList(editList);
}

/*************************** END SECTION NB#161788 *********************************/

/******************* SECTION FOR TESTING NB#168499 *********************************/

static void listVCardFiles(const QDir &dir, const QDir &root)
{
    const QDir::Filters options = QDir::Files|QDir::AllDirs|QDir::NoDotAndDotDot;
    const QDir::SortFlags sortFlags = QDir::DirsLast;
    const QStringList pattern("*.vcf");

    foreach(const QFileInfo &child, dir.entryInfoList(pattern, options, sortFlags)) {
        if (child.isDir()) {
            listVCardFiles(child.filePath(), root);
        } else {
            int i = child.fileName().lastIndexOf(".vcf");
            QString xml = child.fileName().left(i) + ".xml";

            if (not dir.exists(xml)) {
                xml.clear();
            }

            const QString tag = root.relativeFilePath(child.filePath());
            QTest::newRow(qPrintable(tag)) << child.filePath() << xml;
        }
    }
}

static void listVCardFiles(const QDir &dir)
{
    listVCardFiles(dir, dir);
}

void
ut_qtcontacts_trackerplugin::testVCardsAndSync_data()
{
    QTest::addColumn<QString>("vcfFileName");
    QTest::addColumn<QString>("xmlFileName");

    listVCardFiles(QFileInfo(referenceFileName("contacts.vcf")).dir());
}

static QList<QContactDetail> significantDetails(const QContact &contact)
{
    QList<QContactDetail> result;

    foreach(const QContactDetail &detail, contact.details()) {
        if (QContactTag::DefinitionName == detail.definitionName() &&
            "addressbook" == detail.value(QContactTag::FieldTag)) {
            continue;
        }

        if (QContactDisplayLabel::DefinitionName == detail.definitionName() ||
            QContactGuid::DefinitionName == detail.definitionName() ||
            QContactTimestamp::DefinitionName == detail.definitionName()) {
            continue;
        }

        result.append(detail);
    }

    return result;
}

void
ut_qtcontacts_trackerplugin::testVCardsAndSync()
{
    QFETCH(QString, vcfFileName);
    QFETCH(QString, xmlFileName);

    QList<QContact> vcardContacts = parseVCards(vcfFileName);
    QVERIFY(not vcardContacts.isEmpty());

    // save the converted contacts

    QMap<int, QContactManager::Error> errorMap;
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactsSaved = engine()->saveContacts(&vcardContacts, &errorMap, &error);

    for(int i = 0; i < vcardContacts.size(); ++i) {
        QVERIFY2(vcardContacts[i].localId() != 0, qPrintable(QString::number(i)));
    }

    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(errorMap.isEmpty());
    QVERIFY(contactsSaved);

    // Create hash table of saved contacts for easy lookup.
    // Also figure out which details must be fetched.

    QHash<QContactLocalId, QContact> savedContacts;
    QSet<QString> detailsToLoad;

    foreach(const QContact &contact, vcardContacts) {
        foreach(const QContactDetail &detail, contact.details()) {
            detailsToLoad += detail.definitionName();
        }

        savedContacts.insert(contact.localId(), contact);
    }

    if (detailsToLoad.contains(QContactThumbnail::DefinitionName)) {
        detailsToLoad += QContactAvatar::DefinitionName;
    }

    QCOMPARE(savedContacts.count(), vcardContacts.count());

    // Read them back and compare.

    QList<QContact> fetchedContacts = contacts(savedContacts.keys(), detailsToLoad.toList());
    QCOMPARE(fetchedContacts.count(), savedContacts.count());

    foreach(const QContact &fetchedContact, fetchedContacts) {
        const QContact savedContact(savedContacts.value(fetchedContact.localId()));

        QVERIFY(not savedContact.isEmpty());

        const QList<QContactDetail> fetchedDetailList = significantDetails(fetchedContact);
        const QList<QContactDetail> savedDetailList = significantDetails(savedContact);

        QList<QContactDetail> missingDetails = savedDetailList;

        foreach(const QContactDetail &savedDetail, savedDetailList) {
            foreach(const QContactDetail &fetchedDetail, fetchedDetailList) {
                if (detailMatches(savedDetail, fetchedDetail)) {
                    missingDetails.removeOne(savedDetail);
                    break;
                }
            }
        }

        if (not missingDetails.isEmpty()) {
            qDebug() << "\n   Available details:" << fetchedDetailList;
            qDebug() << "\n   Missing details:" << missingDetails;
            qDebug() << "\n   Contact id:" << fetchedContact.localId();
            QFAIL("Not all expected details were found in fetched contact");
        }

        QCOMPARE(fetchedDetailList.count(), savedDetailList.count());
    }

    // compare with reference XML file
    if (not xmlFileName.isEmpty()) {
        QSKIP("comparing fetched contact with reference data doesn't really work yet", SkipSingle);

        // apply a local Id we'll find in the XML file
        QCOMPARE(fetchedContacts.count(), 1);
        QContact &contact = fetchedContacts.first();
        QContactId contactId = contact.id();
        contactId.setLocalId(1);
        contact.setId(contactId);

        // now compare the contact with the reference data
        verifyContacts(fetchedContacts, xmlFileName);
        CHECK_CURRENT_TEST_FAILED;
    }
}

/*************************** END SECTION NB#168499 *********************************/

/******************* SECTION FOR TESTING NB#173388 *********************************/

void
ut_qtcontacts_trackerplugin::testSaveThumbnail_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("avatarUrl");
    QTest::addColumn<QString>("accountUri");
    QTest::addColumn<QString>("accountPath");
    QTest::addColumn<QString>("imAvatarUrl");

    // thumbnail creates new avatar detail
    QTest::newRow("no-avatar-yet")
            << "NB173388-with-thumb.vcf"
            << "" << "" << "" << "";

    // thumbnail replaces old avatar detail
    QTest::newRow("dangling-avatar")
            << "NB173388-with-thumb.vcf"
            << "file:///tmp/image.png"
            << "" << "" << "";

    // thumbnail creates new avatar detail, online avatar is kept
    QTest::newRow("online-avatar")
            << "NB173388-with-thumb.vcf"
            << "file:///tmp/image.png"
            << "first.last@talk.com"
            << "/org/freedesktop/fake/account"
            << "file:///home/user/.cache/avatars/a888d5a6-2434-480a-8798-23875437bcf3";
}

void
ut_qtcontacts_trackerplugin::testSaveThumbnail()
{
    QFETCH(QString, fileName);
    QFETCH(QString, avatarUrl);
    QFETCH(QString, accountUri);
    QFETCH(QString, accountPath);
    QFETCH(QString, imAvatarUrl);

    // read test contact from vcard file
    QList<QContact> contacts = parseVCards(referenceFileName(fileName));
    QCOMPARE(contacts.count(), 1);

    QContactOnlineAccount account;
    QContactAvatar avatar;

    // add account detail when needed
    if (not accountUri.isEmpty()) {
        account.setAccountUri(accountUri);
        account.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
        account.setDetailUri(makeTelepathyIri(accountPath, accountUri));
        contacts.first().saveDetail(&account);
    }

    // add avatar detail when needed
    if (not avatarUrl.isEmpty()) {
        if (not account.isEmpty()) {
            // link detail with online account
            avatar.setLinkedDetailUris(QStringList() << account.detailUri());
        }

        avatar.setImageUrl(QUrl::fromUserInput(avatarUrl));
        contacts.first().saveDetail(&avatar);
    }

    // save text contact
    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactSaved);

    QVERIFY(0 != contacts.first().localId());

    // read back the contact
    error = QContactManager::UnspecifiedError;
    QContact fetched = engine()->contactImpl(contacts.first().localId(), fetchHint<QContactAvatar>(), &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetched.localId(), contacts.first().localId());

    QList<QContactAvatar> fetchedAvatars = fetched.details<QContactAvatar>();
    const int expectedAvatarCount = (accountUri.isEmpty() ? 1 : 2);

    QCOMPARE(fetchedAvatars.count(), expectedAvatarCount);

    // verify the avatar details
    avatar = fetchedAvatars.takeFirst();
    QVERIFY(avatarUrl != avatar.imageUrl().toString());
    QVERIFY(avatar.imageUrl().isValid());

    // verify existance of the avatar image
    const QString avatarFileName = avatar.imageUrl().toLocalFile();
    QVERIFY(not avatarFileName.isNull());

    const QFileInfo avatarFileInfo = avatarFileName;
    QVERIFY(avatarFileInfo.exists());
    QVERIFY(avatarFileInfo.size() > 0);

    // check URL of IM avatar
    if (not fetchedAvatars.isEmpty()) {
        avatar = fetchedAvatars.takeFirst();
        QCOMPARE(avatar.imageUrl().toString(), imAvatarUrl);
        QVERIFY(avatar.linkedDetailUris().contains(account.detailUri()));
    }
}

/*************************** END SECTION NB#173388 *********************************/

void
ut_qtcontacts_trackerplugin::testCreateUuid()
{
    const QString helperFilePath =
            QCoreApplication::instance()->applicationFilePath() +
            QLatin1String("_helper");

    QProcess child;

    child.start(helperFilePath);
    child.waitForFinished();

    QString uuidsList1 = child.readAllStandardOutput();

    child.start(helperFilePath);
    child.waitForFinished();

    QString uuidsList2 = child.readAllStandardOutput();

    QCOMPARE(uuidsList2.length(), uuidsList1.length());
    QVERIFY2(uuidsList2 != uuidsList1, "generated UUIDs must be random");
}

void
ut_qtcontacts_trackerplugin::testPreserveUID()
{
    QList<QContact> contacts = parseVCards(referenceFileName("preserve-uid.vcf"));

    QCOMPARE(contacts.count(), 1);
    QCOMPARE(contacts.first().details<QContactGuid>().count(), 1);

    const QString expectedGuid = contacts.first().detail<QContactGuid>().guid();
    QCOMPARE(expectedGuid, QString::fromLatin1("fancy-uid-value"));

    QContactManager::Error error = QContactManager::UnspecifiedError;
    bool contactSaved = engine()->saveContacts(&contacts, 0, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(contactSaved);

    QVERIFY(0 != contacts.first().localId());
    QCOMPARE(contacts.first().detail<QContactGuid>().guid(), expectedGuid);

    error = QContactManager::UnspecifiedError;
    QContact fetched = engine()->contactImpl(contacts.first().localId(), fetchHint<QContactGuid>(), &error);
    QCOMPARE(error, QContactManager::NoError);

    QCOMPARE(fetched.localId(), contacts.first().localId());
    QCOMPARE(fetched.details<QContactGuid>().count(), 1);
    QCOMPARE(fetched.detail<QContactGuid>().guid(), expectedGuid);
}

void
ut_qtcontacts_trackerplugin::testFuzzing_data()
{
    QTest::addColumn<QString>("contactType");
    QTest::addColumn<QString>("definitionName");

    foreach(const QString &type, engine()->supportedContactTypes()) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(type, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(not definitions.isEmpty());

        foreach (const QString &name, getFuzzableDetailDefinitionNamesForType(definitions)) {
            QTest::newRow(qPrintable(type + "::" + name)) << type << name;
        }
    }
}

static QDateTime
randomDateTime()
{
    const double t = QDateTime::currentMSecsSinceEpoch();
    return QDateTime::fromMSecsSinceEpoch(t * qrand() / RAND_MAX);
}

static QVariantList
makeValues(const QContactDetailFieldDefinition &field)
{
    QVariantList result = field.allowableValues();

    if (result.isEmpty()) {
        switch(field.dataType()) {
        case QVariant::Invalid:
            break;

        case QVariant::Bool:
            result.append(true);
            result.append(false);
            break;

        case QVariant::Date:
            result.append(randomDateTime().date());
            break;

        case QVariant::DateTime:
            result.append(randomDateTime());
            break;

        case QVariant::Double:
            result.append(qrand() / double(RAND_MAX));
            break;

        case QVariant::Int:
            result.append(0);
            result.append(qrand());
            result.append(INT_MAX);
            result.append(INT_MIN);
            break;

        case QVariant::String:
            result.append(QUuid::createUuid().toString());
            break;

        case QVariant::StringList:
            result.append(QStringList() <<
                          QUuid::createUuid().toString() <<
                          QUuid::createUuid().toString());
            break;

        case QVariant::Url:
            result.append(QUrl("urn:uuid:" + QUuid::createUuid().toString().mid(1, 36)));
            break;

        default:
            qWarning() << "Unsupported data type:" << QVariant::typeToName(field.dataType());
            break;
        }
    } else if (QVariant::StringList == field.dataType()) {
        // convert string values into lists
        for(int i = 0; i < result.length(); ++i) {
            result[i] = QStringList(result[i].toString());
        }

        // append list with all possible values
        QStringList allValues;

        for(int i = 0; i < result.length(); ++i) {
            allValues.append(result[i].toStringList().first());
        }

        result.append(allValues);

        // make sure there also is an entry with exactly two values
        if (result.count() > 3) {
            result.insert(result.length() - 1, QStringList() <<
                          result[0].toStringList().first() <<
                          result[1].toStringList().first());
        }
    }

    return result;
}

static QVariantList
atLeastOne(const QVariantList &list)
{
    if (list.isEmpty()) {
        return QVariantList() << QVariant();
    }

    return list;
}

static bool
compareFuzzedDetail(const QContactDetail &reference, QContactDetail detail)
{
        // remove detail links - we don't generate them for the reference contacts
        detail.removeValue(QContactDetail::FieldDetailUri);
        detail.removeValue(QContactDetail::FieldLinkedDetailUris);

        return detailMatches(reference, detail);
}

static void
compareFuzzedContacts(const QString &definitionName,
                      const QContactList &referenceContacts,
                      const QMap<QContactLocalId, QContact> &fetchedContacts)
{
    int notFoundReferenceContactsCount = 0;
    QList<QContactLocalId> referencelessFetchedContactsLocalId = fetchedContacts.keys();

    foreach(const QContact &rc, referenceContacts) {
        QMap<QContactLocalId, QContact>::ConstIterator fc = fetchedContacts.find(rc.localId());

        if (fc == fetchedContacts.constEnd()) {
            qWarning() << "reference contact not found:" << rc;
            notFoundReferenceContactsCount += 1;
            continue;
        }
        referencelessFetchedContactsLocalId.removeOne(rc.localId());

        const QContactDetail referenceDetail = rc.detail(definitionName);
        QList<QContactDetail> fetchedDetails = fc->details(definitionName);

        if (fetchedDetails.isEmpty()) {
            qDebug() << "reference contact:" << rc;
            qDebug() << "fetched contact:" << *fc;
        }

        QCOMPARE(fetchedDetails.count(), 1);

        // finally compare the details field by field
        if (not compareFuzzedDetail(referenceDetail, fetchedDetails.first())) {
            qDebug() << "\n   Available detail:" << fetchedDetails.first();
            qDebug() << "\n   Expected detail:" << referenceDetail;
            qDebug() << "\n   Local contact id:" << rc.localId();
            QFAIL("fetched detail doesn't match saved detail");
        }
    }

    QCOMPARE(notFoundReferenceContactsCount, 0);

    const int referencelessFetchedContactsCount = referencelessFetchedContactsLocalId.count();
    foreach (QContactLocalId localId, referencelessFetchedContactsLocalId) {
            qWarning() << "fetched contact not in references:" << fetchedContacts[localId];
    }
    QCOMPARE(referencelessFetchedContactsCount, 0);
}

/*!\ Construct list of details fotr given \a definitionName, assigned to random values */
void
ut_qtcontacts_trackerplugin::fuzzDetailType(QList<QContactDetail> &details,
                                            const QString &definitionName) const
{
    // TODO: refactor testFuzzing() to use this method

    const QStringList skipDetailDefinitions(QStringList()
            << QContactThumbnail::DefinitionName
            << QContactOnlineAccount::DefinitionName
            << QContactRelevance::DefinitionName);

    if (skipDetailDefinitions.contains(definitionName)) {
        return;
    }

    QContactManager::Error error;
    const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(QContactType::TypeContact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(not definitions.isEmpty());
    const QContactDetailDefinition &def = definitions[definitionName];
    QPair<QString, QContactDetailFieldDefinition> contextField;
    QPair<QString, QContactDetailFieldDefinition> subTypeField;
    QContactDetailFieldDefinitionMap fields = def.fields();
    QContactDetailFieldDefinitionMap::Iterator fieldIter;

    // don't fuzz on Context or SubType(s) fields
    if (fields.end() != (fieldIter = fields.find(QContactDetail::FieldContext))) {
        contextField = qMakePair(fieldIter.key(), fieldIter.value());
        fields.erase(fieldIter);
    }

    if (fields.end() != (fieldIter = fields.find("SubType")) ||
        fields.end() != (fieldIter = fields.find("SubTypes"))) {
        subTypeField = qMakePair(fieldIter.key(), fieldIter.value());
        fields.erase(fieldIter);
    }

    const QContactDetailFieldDefinitionMap::ConstIterator first = fields.constBegin();
    const QContactDetailFieldDefinitionMap::ConstIterator last = fields.constEnd();

    // create details with exactly all possible fields
    foreach(const QVariant &context, atLeastOne(makeValues(contextField.second))) {
        if (context.toStringList().count() > 1) {
            qWarning("not supported yet: \"Context\" field with multiple values");
            continue;
        }

        foreach(const QVariant &subType, atLeastOne(makeValues(subTypeField.second))) {
            QContactDetail detail = QContactDetail(definitionName);

            if (not contextField.first.isEmpty()) {
                QVERIFY2(detail.setValue(contextField.first, context),
                         qPrintable(contextField.first));
            }

            if (not subTypeField.first.isEmpty()) {
                QVERIFY2(detail.setValue(subTypeField.first, subType),
                         qPrintable(subTypeField.first));
            }

            for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
                const QContactDetailFieldDefinition &field = i.value();
                const QVariantList values = makeValues(field);

                QVERIFY2(not values.isEmpty(), qPrintable(i.key()));
                QVERIFY2(detail.setValue(i.key(), values.first()), qPrintable(i.key()));
            }
            details << detail;
        }
    }
}

/*!\ Construct contact with all details assigned to random values */
void
ut_qtcontacts_trackerplugin::fuzzContact(QContact &contact, const QStringList &skipDetailDefinitions) const
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(QContactType::TypeContact, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(not definitions.isEmpty());

    for (QContactDetailDefinitionMap::ConstIterator detailDefinitionIterator = definitions.constBegin(); detailDefinitionIterator != definitions.constEnd(); detailDefinitionIterator++) {
        QList<QContactDetail> details;
        if (skipDetailDefinitions.contains(detailDefinitionIterator.key())) {
            continue;
        }
        fuzzDetailType(details, detailDefinitionIterator.key());
        for (QList<QContactDetail>::Iterator detail = details.begin(); detail != details.end(); detail++) {
            contact.saveDetail(&(*detail));
        }
    }
}

static void
modifyFields(const QString &definitionName, QContact &contact,
             const QContactDetailFieldDefinitionMap &fields)
{
    foreach(QContactDetail detail, contact.details(definitionName)) {
        for(QContactDetailFieldDefinitionMap::ConstIterator
            fieldIter = fields.constBegin(); fieldIter != fields.constEnd(); ++fieldIter) {
            QVariant value = detail.variantValue(fieldIter.key());

            if (value.isNull()) {
                continue;
            }

            QVariantList allowableValues = fieldIter->allowableValues();

            if (not allowableValues.isEmpty()) {
                allowableValues.removeAll(value);

                if (allowableValues.isEmpty()) {
                    continue; // no more allowable values left
                }

                const QVariant &newValue = allowableValues[qrand() % allowableValues.count()];
                if (newValue.type() == QVariant::String) {
                    switch(value.type()) {
                    case QVariant::String:
                        value = newValue;
                        break;

                    case QVariant::StringList:
                        value = QStringList(newValue.toString());
                        break;
                    default:
                        QFAIL(qPrintable(fieldIter.key() + ": unsupported data type with allowed values: " +
                                            value.typeName()));
                        break;
                    }
                } else {
                    QFAIL(qPrintable(fieldIter.key() + ": unsupported data type of allowed value: " +
                                        newValue.typeName()));
                }
            } else {
                switch(value.type()) {
                case QVariant::Bool:
                    value = not value.toBool();
                    break;

                case QVariant::Date:
                    value = value.toDate().addDays(7);
                    break;

                case QVariant::DateTime:
                {
                    const QDateTime original = value.toDateTime();
                    QDateTime modified = original.addDays(13);
                    modified.setUtcOffset(original.utcOffset());
                    value = modified;
                }
                    break;

                case QVariant::Double:
                    value = value.toDouble() + 23.42;
                    break;

                case QVariant::Int:
                    value = value.toInt() + 13;
                    break;

                case QVariant::String:
                    value = value.toString() + ";new";
                    break;

                case QVariant::StringList:
                    value = value.toStringList() << QUuid::createUuid().toString();
                    break;

                case QVariant::Url:
                    value = QUrl(value.toString() + ";new");
                    break;

                default:
                    QFAIL(qPrintable(fieldIter.key() + ": unsupported data type: " +
                                     value.typeName()));
                    break;
                }
            }

            detail.setValue(fieldIter.key(), value);
        }

        if (QContactTimestamp::DefinitionName == definitionName) {
            detail.removeValue(QContactTimestamp::FieldModificationTimestamp);
        }

        detail.removeValue(QContactDetail::FieldDetailUri);

        QVERIFY(contact.saveDetail(&detail));
    }
}

template <class Container>
void modifyFields(const QString &definitionName, Container &contacts,
                  const QContactDetailFieldDefinitionMap &fields)
{
    for(typename Container::Iterator contactIter = contacts.begin(); contactIter != contacts.end(); ++contactIter) {
        modifyFields(definitionName, *contactIter, fields);
    }
}

// fuzzing is a technqiue to test robustness by feeding the API with randomly generated data
void
ut_qtcontacts_trackerplugin::testFuzzing()
{
    QFETCH(QString, contactType);
    QFETCH(QString, definitionName);

    if (QContactOnlineAccount::DefinitionName == definitionName) {
        QSKIP("fuzzing of online account detail not supported yet", SkipSingle);
    }

    if (QContactRelevance::DefinitionName == definitionName) {
        QSKIP("fuzzing of relevance detail not supported yet", SkipSingle);
    }

    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(contactType, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(not definitions.isEmpty());

    const QContactDetailDefinition &def = definitions.value(definitionName);
    QPair<QString, QContactDetailFieldDefinition> contextField;
    QPair<QString, QContactDetailFieldDefinition> subTypeField;
    QContactDetailFieldDefinitionMap fields = def.fields();
    QContactDetailFieldDefinitionMap::Iterator fieldIter;

    // don't fuzz on Contact or SubType(s) fields
    if (fields.end() != (fieldIter = fields.find("Context"))) {
        contextField = qMakePair(fieldIter.key(), fieldIter.value());
        fields.erase(fieldIter);
    }

    if (fields.end() != (fieldIter = fields.find("SubType")) ||
        fields.end() != (fieldIter = fields.find("SubTypes"))) {
        subTypeField = qMakePair(fieldIter.key(), fieldIter.value());
        fields.erase(fieldIter);
    }

    const QContactDetailFieldDefinitionMap::ConstIterator first = fields.constBegin();
    const QContactDetailFieldDefinitionMap::ConstIterator last = fields.constEnd();

    QContactList referenceContacts;

    // create details with exactly one possible fields
    for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
        const QContactDetailFieldDefinition &field = i.value();
        const QVariantList values = makeValues(field);

        QVERIFY2(not values.isEmpty(), qPrintable(i.key()));

        foreach(const QVariant &v, values) {
            QVERIFY2(v.type() == field.dataType(),
                     qPrintable(QString("%3 - actual type=%2, expected type=%1").
                                arg(QVariant::typeToName(field.dataType())).
                                arg(v.typeName()).arg(i.key())));

            QContactDetail detail = QContactDetail(definitionName);
            QVERIFY2(detail.setValue(i.key(), v), qPrintable(i.key()));

            referenceContacts.append(QContact());

            QVERIFY2(referenceContacts.last().saveDetail(&detail), qPrintable(i.key()));
        }
    }

    // create details with exactly all possible fields
    foreach(const QVariant &context, atLeastOne(makeValues(contextField.second))) {
        if (context.toStringList().count() > 1) {
            qWarning("not supported yet: \"Context\" field with multiple values");
            continue;
        }

        foreach(const QVariant &subType, atLeastOne(makeValues(subTypeField.second))) {
            QContactDetail detail = QContactDetail(definitionName);

            if (not contextField.first.isEmpty()) {
                QVERIFY2(detail.setValue(contextField.first, context),
                         qPrintable(contextField.first));
            }

            if (not subTypeField.first.isEmpty()) {
                QVERIFY2(detail.setValue(subTypeField.first, subType),
                         qPrintable(subTypeField.first));
            }

            for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
                const QContactDetailFieldDefinition &field = i.value();
                const QVariantList values = makeValues(field);

                QVERIFY2(not values.isEmpty(), qPrintable(i.key()));
                QVERIFY2(detail.setValue(i.key(), values.first()), qPrintable(i.key()));
            }

            referenceContacts.append(QContact());
            QVERIFY(referenceContacts.last().saveDetail(&detail));
        }
    }

    for(int i = 1; i <= 5; ++i) {
        // save the reference contacts
        error = QContactManager::UnspecifiedError;
        bool success = engine()->saveContacts(&referenceContacts, 0, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(success);

        // figure out local ids of the saved contacts
        QContactLocalIdList localIds;

        foreach(const QContact &contact, referenceContacts) {
            QVERIFY(0 != contact.localId());
            localIds.append(contact.localId());
        }

        // fetch contacts for comparision
        error = QContactManager::UnspecifiedError;
        QMap<QContactLocalId, QContact> fetchedContacts;

        foreach(const QContact &contact, engine()->contacts(localIdFilter(localIds), NoSortOrders, fetchHint(definitionName), &error)) {
            fetchedContacts.insert(contact.localId(), contact);
        }

        QCOMPARE(error, QContactManager::NoError);

        // compare fetched contacts
        compareFuzzedContacts(definitionName, referenceContacts, fetchedContacts);
        CHECK_CURRENT_TEST_FAILED;

        // modify contacts to test updates
        modifyFields(definitionName, fetchedContacts, fields);
        referenceContacts = fetchedContacts.values();
    }
}

/// returns a random int from the range [@p min .. @p max), behaviour undefined if min >= max
static int
randomInt(int min, int max)
{
    return double(qrand())/double(RAND_MAX+1.0)*(max-min) + min;
}

/// returns a random string with a length of the range [@p minLength .. @p maxLength)
/// using letters a-zA-Z, digits 0-9 and '_'. First char will be a letter.
/// TODO: use for fuzzing tests instead of QGuuid, best with version that adds letters other than a-zA-Z as well
static QString
makeAsciiName(int minLength, int maxLength)
{
    static const char lettersData[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static const char digitsAndOtherChars[] = "0123456789_";
    static const QString chars = QLatin1String(lettersData) + QLatin1String(digitsAndOtherChars);
    static const int behindLastCharIndex = chars.count();
    static const int behindLastLetterIndex = sizeof(lettersData) -1; // -1 cares for \0 at end of string

    const int length = randomInt(minLength, maxLength);

    if (length == 0) {
        return QString();
    }

    QString result;
    result.resize(length);
    // first char a letter
    int index = randomInt(0, behindLastLetterIndex);
    result[0] = chars[index];
    // other chars can be anything
    for( int i = 1; i<length; ++i ) {
        index = randomInt(0, behindLastCharIndex);
        result[i] = chars[index];
    }
    return result;
}

static void
makeAccountValues(QString &accountUri, QString &accountPath)
{
    // use account name between 1..15 chars (TODO: max possible length unkown)
    const QString accountName = makeAsciiName(1, 16);
    // use account name between 1..15 chars (TODO: max possible length unknown)
    const QString serviceName = makeAsciiName(1, 16);

    accountUri = accountName % QLatin1Char('@') % serviceName;
    accountPath = QLatin1String("/org/freedesktop/unittest/Account/")
                  % serviceName % QLatin1Char('/') % accountName;
}

/// test fetching of contacts using FieldAccountPath or FieldAccountUri of QContactOnlineAccount
void
ut_qtcontacts_trackerplugin::testFilterOnQContactOnlineAccount()
{
    // number of contacts used in this test
    static const int testContactCount = 5;
    // collects created account contacts;
    QList<QContact> contacts;

    // create contacts, each with a accountname
    for (int i=0; i<testContactCount; ++i) {
        QContact contact;
        SET_TESTNICKNAME_TO_CONTACT(contact);

        // set online account
        QContactOnlineAccount onlineAccount;
        QString accountUri, accountPath;
        makeAccountValues(accountUri, accountPath);
        onlineAccount.setAccountUri(accountUri);
        onlineAccount.setValue(QContactOnlineAccount__FieldAccountPath,
                               accountPath);
        QVERIFY(contact.saveDetail(&onlineAccount));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);

        contacts << contact;
    }

    // fetch each contact, once for each account
    foreach (const QContact &contact, contacts) {
        const QContactOnlineAccount onlineAccountDetail = contact.detail<QContactOnlineAccount>();
        const QString accountPath = onlineAccountDetail.value(QContactOnlineAccount__FieldAccountPath);
        const QString accountUri = onlineAccountDetail.accountUri();
        // use accountPath to fetch
        {
            QContactDetailFilter accountPathFilter;
            accountPathFilter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                                    QContactOnlineAccount__FieldAccountPath);
            accountPathFilter.setValue(accountPath);
            accountPathFilter.setMatchFlags(QContactFilter::MatchExactly);
            // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
            QContactIntersectionFilter filter;
            filter << TESTNICKNAME_FILTER << accountPathFilter;
            QContactManager::Error error = QContactManager::UnspecifiedError;
            const QList<QContact> fetchedContacts =
                engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);

            QCOMPARE(error,  QContactManager::NoError);
            QCOMPARE(fetchedContacts.count(), 1);
            const QContact fetchedContact = fetchedContacts.first();
            QCOMPARE(fetchedContact.localId(), contact.localId());
            const QContactOnlineAccount fetchedOnlineAccountDetail = fetchedContact.detail<QContactOnlineAccount>();
            QCOMPARE(fetchedOnlineAccountDetail.value(QContactOnlineAccount__FieldAccountPath),
                    accountPath);
            QCOMPARE(fetchedOnlineAccountDetail.accountUri(), accountUri);
        }
        // use accountUri to fetch
        {
            QContactDetailFilter accountUriFilter;
            accountUriFilter.setDetailDefinitionName(QContactOnlineAccount::DefinitionName,
                                                    QContactOnlineAccount::FieldAccountUri);
            accountUriFilter.setValue(accountUri);
            accountUriFilter.setMatchFlags(QContactFilter::MatchExactly);
            // using also TESTNICKNAME_FILTER to limit contacts to those used in the test
            QContactIntersectionFilter filter;
            filter << TESTNICKNAME_FILTER << accountUriFilter;
            QContactManager::Error error = QContactManager::UnspecifiedError;
            const QList<QContact> fetchedContacts =
                engine()->contacts(filter, NoSortOrders, NoFetchHint, &error);

            QCOMPARE(error,  QContactManager::NoError);
            QCOMPARE(fetchedContacts.count(), 1);
            const QContact fetchedContact = fetchedContacts.first();
            QCOMPARE(fetchedContact.localId(), contact.localId());
            const QContactOnlineAccount fetchedOnlineAccountDetail = fetchedContact.detail<QContactOnlineAccount>();
            QCOMPARE(fetchedOnlineAccountDetail.value(QContactOnlineAccount__FieldAccountPath),
                    accountPath);
            QCOMPARE(fetchedOnlineAccountDetail.accountUri(), accountUri);
        }
    }
}

void
ut_qtcontacts_trackerplugin::makeFuzzedSingleDetailFieldContacts(QContactList &contacts,
                                                                 const QString &detailName,
                                                                 const QString &fieldName,
                                                                 const QContactDetailFieldDefinition &field,
                                                                 const QString &contactType)
{
    const QVariantList values = makeValues(field);

    QVERIFY2(not values.isEmpty(), qPrintable(fieldName));

    // create one contact per value
    foreach(const QVariant &value, values) {
        QVERIFY2(value.type() == field.dataType(),
                qPrintable(QString("%3 - actual type=%2, expected type=%1").
                            arg(QVariant::typeToName(field.dataType())).
                            arg(value.typeName()).arg(fieldName)));

        QContact contact;
        contact.setType(contactType);

        // set test detail
        QContactDetail detail = QContactDetail(detailName);
        QVERIFY2(detail.setValue(fieldName, value), qPrintable(fieldName));
        QVERIFY2(contact.saveDetail(&detail), qPrintable(fieldName));

        // store contact
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool success = engine()->saveContact(&contact, &error);
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(success);

        contacts.append(contact);
    }
}

void
ut_qtcontacts_trackerplugin::fieldsWithoutContextAndSubType(QContactDetailFieldDefinitionMap &fields,
                                                            const QString &contactType,
                                                            const QString &definitionName)
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(contactType, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(not definitions.isEmpty());

    const QContactDetailDefinition &detailDefinition = definitions.value(definitionName);

    fields = detailDefinition.fields();

    QContactDetailFieldDefinitionMap::Iterator fieldIter;

    if (fields.end() != (fieldIter = fields.find(QContactDetail::FieldContext))) {
        fields.erase(fieldIter);
    }

    if (fields.end() != (fieldIter = fields.find("SubType")) ||
        fields.end() != (fieldIter = fields.find("SubTypes"))) {
        fields.erase(fieldIter);
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnDetailFieldWildcard_data()
{
    QTest::addColumn<QString>("contactType");
    QTest::addColumn<QString>("definitionName");

    static const QStringList untestedDetails = QStringList()
        // mandatory details for every QContact, always existing, so useless to filter for
        << QContactType::DefinitionName
        << QContactDisplayLabel::DefinitionName
        // automatically created for every QContact on saving by qtcontacts-tracker, so useless to filter for
        << QContactGuid::DefinitionName
        << QContactSyncTarget::DefinitionName
        << QContactTimestamp::DefinitionName
        << QContactGender::DefinitionName
        // readonly, can't create in test
        << QContactGlobalPresence::DefinitionName
        << QContactPresence::DefinitionName
        << QContactThumbnail::DefinitionName;

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(contactType, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(not definitions.isEmpty());

        foreach(const QContactDetailDefinition &definition, definitions) {
            const QString definitionName = definition.name();
            if (not untestedDetails.contains(definitionName)) {
                QTest::newRow(qPrintable(contactType + "::" + definitionName)) << contactType << definitionName;
            }
        }
    }
}

/// tests the fetching with a wildcard filter on a field of a detail
/// For each detail loops over all its possible fields and creates a contact
/// for each of some reference values for that field,
/// with only that detail and only that field set.
/// Then for each field tests to fetch the contacts with that field set,
/// using the wildcard filter on that field.
/// To test that contacts which don't have that field present are not fetched,
/// first all contacts for all fields are created, additionally an contact with an empty detail,
/// and stored to the database. Only then the fetching is done for each field,
/// using an additional QContactLocalIdFilter to restrict to contacts from this test.
/// Code similar to testFuzzing(), keep in sync
void
ut_qtcontacts_trackerplugin::testFilterOnDetailFieldWildcard()
{
    QFETCH(QString, contactType);
    QFETCH(QString, definitionName);

    if (QContactOnlineAccount::DefinitionName == definitionName) {
        QSKIP("filter of online account detail exists not supported yet", SkipSingle);
    }

    if (QContactRelevance::DefinitionName == definitionName) {
        QSKIP("filter of relevance detail exists not supported yet", SkipSingle);
    }

    if (QContactAvatar::DefinitionName == definitionName) {
        QSKIP("filter of avatar detail exists not supported yet", SkipSingle);
    }

    // add an contact with an empty detail which should never be fetched by wildcard filter
    QContact emptyDetailContact;
    {
        emptyDetailContact.setType(contactType);

        // set empty test detail
        QContactDetail detail = QContactDetail(definitionName);
        QVERIFY2(emptyDetailContact.saveDetail(&detail),
                 qPrintable(QString::fromLatin1("could not save empty detail %1").arg(definitionName)));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool success = engine()->saveContact(&emptyDetailContact, &error);
        registerForCleanup(emptyDetailContact);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(success);
    }

    // get the details fields, without Context or SubType(s) fields
    QContactDetailFieldDefinitionMap fields;
    fieldsWithoutContextAndSubType(fields, contactType, definitionName);
    CHECK_CURRENT_TEST_FAILED;

    QHash<QString, QContactList> referenceContactsPerField;

    const QContactDetailFieldDefinitionMap::ConstIterator first = fields.constBegin();
    const QContactDetailFieldDefinitionMap::ConstIterator last = fields.constEnd();

    // create sets of contacts per field with only that field set
    for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
        const QString &fieldName = i.key();
        const QContactDetailFieldDefinition &field = i.value();

        // create one contact per value
        QContactList referenceContacts;
        makeFuzzedSingleDetailFieldContacts(referenceContacts, definitionName, fieldName, field, contactType);
        CHECK_CURRENT_TEST_FAILED;
        referenceContactsPerField.insert(fieldName, referenceContacts);
    }

    // collect localIds of all created contacts, incl. empty detail contact
    QContactLocalIdList usedLocalIds;
    usedLocalIds.append(emptyDetailContact.localId());
    foreach (const QContactList &referenceContacts, referenceContactsPerField) {
        foreach (const QContact &contact, referenceContacts) {
            QVERIFY(0 != contact.localId());
            usedLocalIds.append(contact.localId());
        }
    }

    // now test fetching contacts with wildcard filter
    for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
        const QString &fieldName = i.key();
        const QContactList &referenceContacts = referenceContactsPerField[fieldName];

        QContactDetailFilter wildcardFilter;
        wildcardFilter.setDetailDefinitionName(definitionName, fieldName);
        // TODO: as of Qt Mobility 1.1.1 it is not defined how a wildcard is set
        // For now in qtcontacts-tracker a null QVariant value and any flag are interpreted as such
        // See nb#218267 for a request for a QContactFilter::MatchWildcard
        wildcardFilter.setValue(QVariant());
        wildcardFilter.setMatchFlags(QContactFilter::MatchContains);

        // using intersection with localId filter to restrict to contacts from this test
        const QContactFilter filter = (localIdFilter(usedLocalIds) & wildcardFilter);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QMap<QContactLocalId, QContact> fetchedContacts;
        foreach(const QContact &contact, engine()->contacts(filter, NoSortOrders, fetchHint(definitionName), &error)) {
            fetchedContacts.insert(contact.localId(), contact);
        }
        QCOMPARE(error, QContactManager::NoError);

        // compare fetched contacts
        compareFuzzedContacts(definitionName, referenceContacts, fetchedContacts);
        CHECK_CURRENT_TEST_FAILED;
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnDetailExists_data()
{
    QTest::addColumn<QString>("contactType");
    QTest::addColumn<QString>("definitionName");

    static const QStringList untestedDetails = QStringList()
        // mandatory details for every QContact, always existing, so useless to filter for
        << QContactType::DefinitionName
        << QContactDisplayLabel::DefinitionName
        // automatically created for every QContact on saving by qtcontacts-tracker, so useless to filter for
        << QContactGuid::DefinitionName
        << QContactSyncTarget::DefinitionName
        << QContactTimestamp::DefinitionName
        << QContactGender::DefinitionName
        // readonly?
        << QContactGlobalPresence::DefinitionName
        << QContactPresence::DefinitionName
        << QContactThumbnail::DefinitionName;

    foreach(const QString &contactType, engine()->supportedContactTypes()) {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(contactType, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(not definitions.isEmpty());

        foreach(const QContactDetailDefinition &definition, definitions) {
            const QString definitionName = definition.name();
            if (not untestedDetails.contains(definitionName)) {
                QTest::newRow(qPrintable(contactType + "::" + definitionName)) << contactType << definitionName;
            }
        }
    }
}

/// tests the fetching with a filter on "detail x exists".
/// For each detail loops over all its possible fields and creates a contact
/// for each of some reference values for that field,
/// with only that detail and only that field set.
/// Then fetches all these contacts using the exists filter on that detail,
/// Uses an empty contact with no details at all to check that not too many contact are fetched.
/// Code similar to testFuzzing(), keep in sync
void
ut_qtcontacts_trackerplugin::testFilterOnDetailExists()
{
    QFETCH(QString, contactType);
    QFETCH(QString, definitionName);

    if (QContactOnlineAccount::DefinitionName == definitionName) {
        QSKIP("filter of online account detail exists not supported yet", SkipSingle);
    }

    if (QContactRelevance::DefinitionName == definitionName) {
        QSKIP("filter of relevance detail exists not supported yet", SkipSingle);
    }

    if (QContactAvatar::DefinitionName == definitionName) {
        QSKIP("filter of avatar detail exists not supported yet", SkipSingle);
    }

    // add an empty contact which should not be fetched by exists filter
    QContact emptyContact;
    {
        emptyContact.setType(contactType);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool success = engine()->saveContact(&emptyContact, &error);
        registerForCleanup(emptyContact);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(success);
    }

    // get the details fields, without Context or SubType(s) fields
    QContactDetailFieldDefinitionMap fields;
    fieldsWithoutContextAndSubType(fields, contactType, definitionName);
    CHECK_CURRENT_TEST_FAILED;

    const QContactDetailFieldDefinitionMap::ConstIterator first = fields.constBegin();
    const QContactDetailFieldDefinitionMap::ConstIterator last = fields.constEnd();

    // test exists filter for each field
    for(QContactDetailFieldDefinitionMap::ConstIterator i = first; i != last; ++i) {
        const QString &fieldName = i.key();
        const QContactDetailFieldDefinition &field = i.value();

        // create one contact per value
        QContactList referenceContacts;
        makeFuzzedSingleDetailFieldContacts(referenceContacts, definitionName, fieldName, field, contactType);
        CHECK_CURRENT_TEST_FAILED;

        // now test fetching all contacts with exists filter
        QContactDetailFilter existsFilter;
        existsFilter.setDetailDefinitionName(definitionName, QString());

        // using intersection with localId filter to restrict to contacts from this test, incl. empty contact
        QContactLocalIdList usedLocalIds;
        usedLocalIds.append(emptyContact.localId());
        foreach(const QContact &contact, referenceContacts) {
            QVERIFY(0 != contact.localId());
            usedLocalIds.append(contact.localId());
        }
        const QContactFilter filter = (localIdFilter(usedLocalIds) & existsFilter);

        QContactManager::Error error = QContactManager::UnspecifiedError;
        QMap<QContactLocalId, QContact> fetchedContacts;
        foreach(const QContact &contact, engine()->contacts(filter, NoSortOrders, fetchHint(definitionName), &error)) {
            fetchedContacts.insert(contact.localId(), contact);
        }
        QCOMPARE(error, QContactManager::NoError);

        // compare fetched contacts
        compareFuzzedContacts(definitionName, referenceContacts, fetchedContacts);
        CHECK_CURRENT_TEST_FAILED;
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnContext_data()
{
    QTest::addColumn<QString>("contactType");
    QTest::addColumn<QString>("definitionName");

    foreach(const QString &type, engine()->supportedContactTypes()) {
        QContactManager::Error error = QContactManager::UnspecifiedError;

        const QContactDetailDefinitionMap definitions = engine()->detailDefinitions(type, &error);

        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(not definitions.isEmpty());

        foreach (const QString &name, getFuzzableDetailDefinitionNamesForType(definitions)) {
            if (definitions.value(name).fields().contains(QContactDetail::FieldContext)) {
                QTest::newRow(qPrintable(type + "::" + name)) << type << name;
            }
        }
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnContext()
{
    QFETCH(QString, contactType);
    QFETCH(QString, definitionName);

    QContact c;

    QContactType typeDetail;
    typeDetail.setType(contactType);

    const QString nick = QLatin1String(Q_FUNC_INFO) + contactType + definitionName;

    QContactNickname nickDetail;
    nickDetail.setNickname(nick);

    c.saveDetail(&nickDetail);

    QList<QContactDetail> details;
    fuzzDetailType(details, definitionName);

    if (details.isEmpty()) {
        // Detail was in the skip list of fuzzDetailType
        return;
    }

    const QString context = details.first().contexts().isEmpty() ? QString()
                                                                 : details.first().contexts().first();

    c.saveDetail(&details.first());

    saveContact(c);

    QContactFetchRequest request;
    QContactIntersectionFilter ifilter;

    QContactDetailFilter typeFilter;
    typeFilter.setDetailDefinitionName(QContactType::DefinitionName, QContactType::FieldType);
    typeFilter.setValue(contactType);

    QContactDetailFilter nickFilter;
    nickFilter.setDetailDefinitionName(QContactNickname::DefinitionName, QContactNickname::FieldNickname);
    nickFilter.setValue(nick);

    QContactDetailFilter contextFilter;
    contextFilter.setDetailDefinitionName(definitionName, QContactDetail::FieldContext);
    contextFilter.setValue(context);

    ifilter << typeFilter << nickFilter << contextFilter;

    request.setFilter(ifilter);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.error() == QContactManager::NoError);

    bool contactFound = false;

    foreach(const QContact &rc, request.contacts()) {
        if (rc.localId() == c.localId()) {
            contactFound = true;
            break;
        }
    }

    QVERIFY(contactFound);
}

// Related bug: NB#192949
void
ut_qtcontacts_trackerplugin::testUnionFilterUniqueness()
{
    QList<QContactLocalId> addedContactIds;

    // Save some contacts
    {
        // match: first/last
        QContact c;
        QContactName name;
	name.setFirstName(QLatin1String("Foxy"));
        name.setLastName(QLatin1String("Fran"));
        c.saveDetail(&name);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        addedContactIds.append(c.localId());
    }
    {
        // match: last
        QContact c;
        QContactName name;
	name.setFirstName(QLatin1String("Robert"));
        name.setLastName(QLatin1String("Frost"));
        c.saveDetail(&name);

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        addedContactIds.append(c.localId());
    }

    QContactUnionFilter filter;

    // Query by last name
    QContactDetailFilter lastNameFilter;
    lastNameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldLastName);
    lastNameFilter.setMatchFlags(QContactFilter::MatchStartsWith);
    lastNameFilter.setValue(QLatin1String("F"));
    filter << lastNameFilter;

    // Query by first name
    QContactDetailFilter firstNameFilter;
    firstNameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    firstNameFilter.setMatchFlags(QContactFilter::MatchStartsWith);
    firstNameFilter.setValue(QLatin1String("F"));
    filter << firstNameFilter;

     // Search
    QContactManager::Error error(QContactManager::UnspecifiedError);
    QList<QContactLocalId> contactIds = engine()->contactIds(filter, NoSortOrders, &error);

    for(int i = 0; i < addedContactIds.count(); ++i) {
        // make sure that the search returned each added contact ONCE
        QCOMPARE(contactIds.count(addedContactIds[i]) + i, 1 + i);
    }
}

void
ut_qtcontacts_trackerplugin::testFilterOnDetailFieldValueWithSingleSpaceOrEmptyString_data()
{
    QTest::addColumn<QString>("matchString");
    QTest::addColumn<int>("matchFlag");
    QTest::addColumn<QStringList>("matchedStrings");
    QTest::addColumn<QStringList>("unmatchedStrings");

    const QString emptyNotNullString = QLatin1String("");
    const QString oneSpaceString = QLatin1String(" ");
    const QString oneSpaceAtBeginString = QLatin1String(" begin");
    const QString oneSpaceAtEndString = QLatin1String("end ");
    const QString oneSpaceAtBeginAndEndString = QLatin1String(" beginEnd ");
    const QStringList spaceStrings = QStringList()
        << oneSpaceString << oneSpaceAtBeginString << oneSpaceAtEndString << oneSpaceAtBeginAndEndString;

    QTest::newRow("Empty and MatchExactly")
        << emptyNotNullString
        << (int)QContactFilter::MatchExactly
        << (QStringList() << emptyNotNullString)
        << (QStringList() << QUuid::createUuid().toString() << spaceStrings);
    QTest::newRow("Empty and MatchContains")
        << emptyNotNullString
        << (int)QContactFilter::MatchContains
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString() << spaceStrings)
        << QStringList();
    QTest::newRow("Empty and MatchStartsWith")
        << emptyNotNullString
        << (int)QContactFilter::MatchStartsWith
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString() << spaceStrings)
        << QStringList();
    QTest::newRow("Empty and MatchEndsWith")
        << emptyNotNullString
        << (int)QContactFilter::MatchEndsWith
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString() << spaceStrings)
        << QStringList();

    QTest::newRow("Space and MatchExactly")
        << oneSpaceString
        << (int)QContactFilter::MatchExactly
        << (QStringList() << oneSpaceString)
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString()
                          << oneSpaceAtBeginString << oneSpaceAtEndString << oneSpaceAtBeginAndEndString);
    QTest::newRow("Space and MatchContains")
        << oneSpaceString
        << (int)QContactFilter::MatchContains
        << (QStringList() << oneSpaceString
                          << oneSpaceAtBeginString << oneSpaceAtEndString << oneSpaceAtBeginAndEndString)
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString());
    QTest::newRow("Space and MatchStartsWith")
        << oneSpaceString
        << (int)QContactFilter::MatchStartsWith
        << (QStringList() << oneSpaceString
                          << oneSpaceAtBeginString << oneSpaceAtBeginAndEndString)
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString()
                          << oneSpaceAtEndString);
    QTest::newRow("Space and MatchEndsWith")
        << oneSpaceString
        << (int)QContactFilter::MatchEndsWith
        << (QStringList() << oneSpaceString
                          << oneSpaceAtEndString << oneSpaceAtBeginAndEndString)
        << (QStringList() << emptyNotNullString << QUuid::createUuid().toString()
                          << oneSpaceAtBeginString);
}

void
ut_qtcontacts_trackerplugin::testFilterOnDetailFieldValueWithSingleSpaceOrEmptyString()
{
    QFETCH(QString, matchString);
    QFETCH(int, matchFlag);
    QFETCH(QStringList, matchedStrings);
    QFETCH(QStringList, unmatchedStrings);

    // create test contacts
    QList<QContactLocalId> createdLocalIds;
    foreach(const QString &string, matchedStrings+unmatchedStrings) {
        QContact contact;
        QContactName name;
        {
            name.setFirstName(string);
            contact.saveDetail(&name);
        }

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&contact, &error));
        registerForCleanup(contact);
        QCOMPARE(error,  QContactManager::NoError);

        createdLocalIds.append(contact.localId());
    }

    // create filter
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    filter.setMatchFlags((QContactFilter::MatchFlags)matchFlag);
    filter.setValue(matchString);

    // fetch contacts using the filter
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList contacts = engine()->contacts(filter & localIdFilter(createdLocalIds), NoSortOrders, NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);

    // check results
    if (contacts.count() != matchedStrings.count()) {
        qDebug() << "fetched contacts:" << contacts;
        qDebug() << "matching strings:" << matchedStrings;
    }
    QCOMPARE(contacts.count(), matchedStrings.count());
}

void
ut_qtcontacts_trackerplugin::testFilterInvalid()
{
    // create test contacts
    QContact contact;
    QContactName name;
    name.setFirstName(QString::fromLatin1(Q_FUNC_INFO));
    contact.saveDetail(&name);

    QContactManager::Error error = QContactManager::UnspecifiedError;
    engine()->saveContact(&contact, &error);
    QCOMPARE(error, QContactManager::NoError);

    // test filters
    QContactFetchRequest request;
    QContactInvalidFilter invalidFilter;

    request.setFilter(invalidFilter);
    engine()->startRequest(&request);
    engine()->waitForRequestFinishedImpl(&request, 0);

    QCOMPARE(request.contacts().size(), 0);

    QContactUnionFilter unionFilter;
    QContactDetailFilter nameFilter;
    nameFilter.setDetailDefinitionName(QContactName::DefinitionName, QContactName::FieldFirstName);
    nameFilter.setValue(QString::fromLatin1(Q_FUNC_INFO));
    unionFilter << nameFilter << invalidFilter;

    request.setFilter(unionFilter);
    engine()->startRequest(&request);
    engine()->waitForRequestFinishedImpl(&request, 0);

    QCOMPARE(request.contacts().size(), 1);
    QVERIFY(request.contacts().first().localId() == contact.localId());

    QContactIntersectionFilter intersectionFilter;
    intersectionFilter << invalidFilter << nameFilter;

    request.setFilter(intersectionFilter);
    engine()->startRequest(&request);
    engine()->waitForRequestFinishedImpl(&request, 0);

    QCOMPARE(request.contacts().size(), 0);
}


//!\ verify that behavior corresponding to both
void
ut_qtcontacts_trackerplugin::testPartialSave_data()
{
    QTest::addColumn<QString>("interfaceVersion");
    QTest::newRow("emulated") << QString::fromLatin1("1");
    QTest::newRow("native") << QString::fromLatin1("2");
}

// copied from http://qt.gitorious.org/qt-mobility/contacts/trees/master/tests/auto/qcontactmanager
void
ut_qtcontacts_trackerplugin::testPartialSave()
{
    QFETCH(QString, interfaceVersion);

    QMap<QString, QString> params = makeEngineParams();
    params.insert(QLatin1String(QTCONTACTS_IMPLEMENTATION_VERSION_NAME), interfaceVersion);
    QScopedPointer<QContactManager> cm(new QContactManager(QLatin1String("tracker"), params));

    QVERIFY(not cm.isNull());
    QCOMPARE(cm->error(), QContactManager::NoError);
    QCOMPARE(cm->managerName(), QLatin1String("tracker"));

    QList<QContact> contacts = parseVCards(QByteArray(
            "BEGIN:VCARD\r\nFN:Alice\r\nN:Alice\r\nTEL:12345\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nFN:Bob\r\nN:Bob\r\nTEL:5678\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nFN:Carol\r\nN:Carol\r\nEMAIL:carol@example.com\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nFN:David\r\nN:David\r\nORG:DavidCorp\r\nEND:VCARD\r\n"));

    QCOMPARE(contacts.count(), 4);

    QVERIFY(contacts[0].displayLabel() == QLatin1String("Alice"));
    QVERIFY(contacts[1].displayLabel() == QLatin1String("Bob"));
    QVERIFY(contacts[2].displayLabel() == QLatin1String("Carol"));
    QVERIFY(contacts[3].displayLabel() == QLatin1String("David"));


    // First save these contacts
    QVERIFY(cm->saveContacts(&contacts));
    QList<QContact> originalContacts = contacts;

    // Now try some partial save operations
    // 0) empty mask == full save
    // 1) Ignore an added phonenumber
    // 2) Only save a modified phonenumber, not a modified email
    // 3) Remove an email address & phone, mask out phone
    // 4) new contact, no details in the mask
    // 5) new contact, some details in the mask
    // 6) Have a bad manager uri in the middle
    // 7) Have a non existing contact in the middle

    QContactPhoneNumber pn;
    pn.setNumber("111111");
    contacts[0].saveDetail(&pn);
    QCOMPARE(contacts[0].details<QContactPhoneNumber>().count(),2);

    QContactDetail customDetail("CustomDetail");
    customDetail.setValue("CustomField", "CustomValue");
    contacts[0].saveDetail(&customDetail);


    // 0) empty mask
    QVERIFY(cm->saveContacts(&contacts, QStringList()));

    // That should have updated everything
    QContact a = cm->contact(originalContacts[0].localId(), NoFetchHint);
    QCOMPARE(a.details<QContactPhoneNumber>().count(),2);
    QCOMPARE(a.details("CustomDetail").count(),1);

    // 1) Add a phone number to b, mask it out
    contacts[1].saveDetail(&pn);
    QCOMPARE(contacts[1].details<QContactPhoneNumber>().count(),2);
    QVERIFY(cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
    QVERIFY(cm->errorMap().isEmpty());
    QContact b = cm->contact(originalContacts[1].localId(), NoFetchHint);
    QCOMPARE(b.details<QContactPhoneNumber>().count(),1);
    QContactFetchHint fetchHint;
    fetchHint.setDetailDefinitionsHint(QStringList()
                                        << "CustomDetail"
                                        << QContactPhoneNumber::DefinitionName);
    a = cm->contact(originalContacts[0].localId(), fetchHint);
    QCOMPARE(a.details<QContactPhoneNumber>().count(),2);
    QCOMPARE(contacts[0].details("CustomDetail").count(),1);
    QCOMPARE(a.details("CustomDetail").count(),1);


    // 2) save a modified detail in the mask
    QContactEmailAddress e;
    e.setEmailAddress("example@example.com");
    contacts[1].saveDetail(&e); // contacts[1] should have both phone and email
    QCOMPARE(contacts[1].details<QContactEmailAddress>().count(),1);
    QVERIFY(cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
    QVERIFY(cm->errorMap().isEmpty());
    b = cm->contact(originalContacts[1].localId(), NoFetchHint);
    QCOMPARE(b.details<QContactPhoneNumber>().count(),1);
    QCOMPARE(b.details<QContactEmailAddress>().count(),1);

    // 3) Remove an email address and a phone number
    QVERIFY(contacts[1].removeDetail(&e));
    QVERIFY(contacts[1].removeDetail(&pn));
    QCOMPARE(contacts[1].details<QContactEmailAddress>().count(),0);
    QCOMPARE(contacts[1].details<QContactPhoneNumber>().count(),1);
    QVERIFY(cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
    QVERIFY(cm->errorMap().isEmpty());
    b = cm->contact(originalContacts[1].localId(), NoFetchHint);
    QCOMPARE(b.details<QContactPhoneNumber>().count(),1);
    QCOMPARE(b.details<QContactEmailAddress>().count(),0);

    // 4 - New contact, no details in the mask
    QContact newContact = originalContacts[3];
    newContact.setId(QContactId());
    contacts.append(newContact);

    // emulated partial save request doesn't catch this error
    if (restrictive) {
        QVERIFY(not cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
        QCOMPARE(cm->errorMap().count(), 1);
        QCOMPARE(cm->errorMap().value(4), QContactManager::BadArgumentError);
    } else {
        QVERIFY(cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
        QVERIFY(cm->errorMap().isEmpty());
    }

    // 4b - New contact, empty mask
    QVERIFY(cm->saveContacts(&contacts, QStringList()));
    QVERIFY(cm->errorMap().isEmpty());
    QVERIFY(contacts[4].localId() != 0); // Saved
    b = cm->contact(contacts[4].localId(), NoFetchHint);
    QCOMPARE(b.details<QContactOrganization>().count(),1); // Saved
    QCOMPARE(b.details<QContactName>().count(),1); // Saved

    // 5 - New contact, some details in the mask
    newContact = originalContacts[2];
    newContact.setId(QContactId());
    contacts.append(newContact);

    // emulated partial save request doesn't catch this error
    if (restrictive) {
        QVERIFY(not cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
        QCOMPARE(cm->errorMap().count(), 1);
        QCOMPARE(cm->errorMap().value(5), QContactManager::BadArgumentError);
    } else {
        QVERIFY(cm->saveContacts(&contacts, QStringList(QContactEmailAddress::DefinitionName)));
        QVERIFY(cm->errorMap().isEmpty());
    }

    // 5b - New contact, empty mask
    QVERIFY(cm->saveContacts(&contacts, QStringList()));
    QVERIFY(cm->errorMap().isEmpty());
    QVERIFY(contacts[5].localId() != 0); // Saved
    b = cm->contact(contacts[5].localId(), NoFetchHint);
    QCOMPARE(b.details<QContactEmailAddress>().count(),1);
    QCOMPARE(b.details<QContactName>().count(),1); // Saved
    QCOMPARE(b.details("CustomDetail").count(),0); // not saved

    // 6) Have a bad manager uri in the middle followed by a custom detail
    QContactId id4(contacts[4].id());
    QContactId badId(id4);
    badId.setManagerUri(QString("invalid:"));
    contacts[4].setId(badId);
    contacts[5].saveDetail(&customDetail);
    QVERIFY(!cm->saveContacts(&contacts, QStringList("CustomDetail")));
    QCOMPARE(cm->errorMap().count(), 1);
    // should be BadArgumentError in discussion, see als QTMOBILITY-1816
    QCOMPARE(cm->errorMap().value(4), QContactManager::DoesNotExistError);

    a = cm->contact(contacts[5].localId(), fetchHint);
    QCOMPARE(a.details("CustomDetail").count(),1);

    // 7) Have a non existing contact in the middle followed by a save error

    badId = id4;
    badId.setLocalId(987234); // something nonexistent
    contacts[4].setId(badId);
    QVERIFY(!cm->saveContacts(&contacts, QStringList("CustomDetail")));
    QCOMPARE(cm->errorMap().count(), 1);
    QCOMPARE(cm->errorMap().value(4), QContactManager::DoesNotExistError);
}

static QContactDetailFilter
nicknameFilter(const QString &nickName)
{
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactNickname::DefinitionName, QContactNickname::FieldNickname);
    filter.setMatchFlags(QContactDetailFilter::MatchExactly);
    filter.setValue(nickName);
    return filter;
}

void
ut_qtcontacts_trackerplugin::testPartialSaveAndWeakSyncTargets_data()
{
    QTest::addColumn<QString>("syncTarget");
    QTest::addColumn<bool>("isWeakSyncTarget");

    // create a contact with qct sync target
    QTest::newRow("addressbook") << QContactSyncTarget__SyncTargetAddressBook.operator QString() << false;
    // create a contact with a weak sync target, using the default weak sync target "telepathy"
    QTest::newRow("telepathy")   << QContactSyncTarget__SyncTargetTelepathy.operator QString()   << true;
    // create a contact with another sync target
    QTest::newRow("mfe")         << QContactSyncTarget__SyncTargetMfe.operator QString()         << false;
}

void
ut_qtcontacts_trackerplugin::testPartialSaveAndWeakSyncTargets()
{
    QFETCH(QString, syncTarget);
    QFETCH(bool, isWeakSyncTarget);

    // 1. create contact in tracker, using nickname to identify the created contact on first load
    const QString nickname = TESTNICKNAME_NICKNAME;

    const QString queryString = QString::fromLatin1(
        "INSERT {\n"
        "  GRAPH <%1> {\n"
        "    <%2> a nco:PersonContact ;\n"
        "       nco:nickname \"%3\" ;\n"
        "       nie:generator \"%4\" ;\n"
        "       nco:hasAffiliation _:_work1 .\n"
        "    _:_work1 a nco:Affiliation ;\n"
        "       rdfs:label \"Work\" ;\n"
        "       nco:hasEmailAddress <mailto:meego@meego.com> .\n"
        "    <mailto:meego@meego.com> a nco:EmailAddress ;\n"
        "       nco:emailAddress \"meego@meego.com\" .\n"
        "  }\n"
        "} WHERE {\n"
        "}").arg(QtContactsTrackerDefaultGraphIri,
                 makeAnonymousIri(QUuid::createUuid()),
                 nickname,
                 syncTarget);

    {
        QScopedPointer<QSparqlResult> result(executeQuery(queryString, QSparqlQuery::InsertStatement));
        QVERIFY(not result.isNull());
    }

    // 2. load contact, just email address, no sync target
    QContact contact;
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QContactFetchHint fetchHint;
        fetchHint.setDetailDefinitionsHint( QStringList(QContactEmailAddress::DefinitionName) );
        QList<QContact> contacts = engine()->contacts(nicknameFilter(nickname), NoSortOrders, fetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);
        QCOMPARE(contacts.count(), 1);

        contact = contacts.at(0);
        registerForCleanup(contact);
    }

    // 3. change email address detail
    QContactEmailAddress emailAddress = contact.detail<QContactEmailAddress>();
    emailAddress.setEmailAddress(QLatin1String("other@meego.com"));
    QVERIFY(contact.saveDetail(&emailAddress));

    // 4. save partially email address detail
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QList<QContact> contacts = QList<QContact>() << contact;
        const QStringList detailMask = QStringList(QContactEmailAddress::DefinitionName);

        const bool contactSaved = engine()->saveContacts(&contacts, detailMask, 0, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(contactSaved);
    }

    // 5. reload contact completely
    QContact fetchedContact;
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        fetchedContact = engine()->contact(contact.localId(), NoFetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);
    }

    // 6. check email address and sync target
    QCOMPARE(fetchedContact.detail<QContactEmailAddress>().emailAddress(),
             contact.detail<QContactEmailAddress>().emailAddress());

    if (isWeakSyncTarget) {
        QCOMPARE(fetchedContact.detail<QContactSyncTarget>().syncTarget(),
                 QContactSyncTarget__SyncTargetAddressBook.operator QString());
    } else {
        QCOMPARE(fetchedContact.detail<QContactSyncTarget>().syncTarget(), syncTarget);
    }
}


void
ut_qtcontacts_trackerplugin::testPartialSaveAndThumbnails_data()
{
    QTest::addColumn<bool>("doWriteNewThumbnail");
    QTest::addColumn<bool>("withLinkedDetailUri");
    QTest::addColumn<bool>("hasExistingAvatar");
    QTest::addColumn<bool>("isExistingAvatarPersonal");
    QTest::addColumn<bool>("shouldFetchedPersonalAvatarBeEmpty");
    QTest::addColumn<bool>("shouldFetchedPersonalAvatarBeOld");
    QTest::addColumn<bool>("shouldFetchedOnlineAvatarBeEmpty");

    const bool ignored = false;
    const bool doWrite = true; const bool doNotWrite = false;
    const bool withLink = true; const bool withoutLink = false;
    const bool hasAvatar = true; const bool hasNoAvatar = false;
    const bool isPersonal = true; const bool isOnline = false;
    const bool emptyFetchedPersonal = true; const bool notEmptyFetchedPersonal = false;
    const bool oldFetchedPersonal = true; const bool notOldFetchedPersonal = false;
    const bool emptyFetchedOnline = true; const bool notEmptyFetchedOnline = false;

    QTest::newRow("no new with no old")                   << doNotWrite << ignored << hasNoAvatar << ignored
                                                          << emptyFetchedPersonal << ignored << emptyFetchedOnline;
    QTest::newRow("no new with old online account")       << doNotWrite << ignored << hasAvatar  << isOnline
                                                          << emptyFetchedPersonal << ignored << notEmptyFetchedOnline;
    QTest::newRow("no new with old personal")             << doNotWrite << ignored << hasAvatar  << isPersonal
                                                          << notEmptyFetchedPersonal << oldFetchedPersonal << emptyFetchedOnline;
    QTest::newRow("new unlinked with no old")             << doWrite  << withoutLink << hasNoAvatar << ignored
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << emptyFetchedOnline;
    QTest::newRow("new linked with no old")               << doWrite  << withLink << hasNoAvatar << ignored
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << emptyFetchedOnline;
    QTest::newRow("new with unlinked old online account") << doWrite  << withoutLink << hasAvatar  << isOnline
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << notEmptyFetchedOnline;
    QTest::newRow("new with linked old online account")   << doWrite  << withLink << hasAvatar  << isOnline
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << notEmptyFetchedOnline;
    QTest::newRow("new unlinked with old personal")       << doWrite  << withoutLink << hasAvatar  << isPersonal
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << emptyFetchedOnline;
    QTest::newRow("new linked with old personal")         << doWrite  << withLink << hasAvatar  << isPersonal
                                                          << notEmptyFetchedPersonal << notOldFetchedPersonal << emptyFetchedOnline;
}


void
ut_qtcontacts_trackerplugin::testPartialSaveAndThumbnails()
{
    QFETCH(bool, doWriteNewThumbnail);
    QFETCH(bool, withLinkedDetailUri);
    QFETCH(bool, hasExistingAvatar);
    QFETCH(bool, isExistingAvatarPersonal);
    QFETCH(bool, shouldFetchedPersonalAvatarBeEmpty);
    QFETCH(bool, shouldFetchedPersonalAvatarBeOld);
    QFETCH(bool, shouldFetchedOnlineAvatarBeEmpty);

    // 1. create contact in tracker, using nickname to identify the created contact on first load
    const QString nickname = TESTNICKNAME_NICKNAME;

    const QString existingAvatarImageUrl =
        QString::fromLatin1("file:///tmp/meego-%1.png").arg(QUuid::createUuid().toString());
    const QString insertAvatarFileLinkQueryString = QString::fromLatin1(
        "INSERT {\n"
        "    _:_ a nfo:FileDataObject, nie:InformationElement ;\n"
        "       nie:url \"%1\" .\n"
        "} WHERE {\n"
        "}\n").arg(existingAvatarImageUrl);

    const QString accountPath = QString::fromLatin1("/com/meego/testAccountPath/%1").arg(qctUuidString(QUuid::createUuid()));
    const QString accountUri = QLatin1String("meego@meego.com");
    const QString imIri =
        QString::fromLatin1("telepathy:%1!%2").arg(accountPath, accountUri);
    const QString insertIMAddressQueryString = QString::fromLatin1(
        "INSERT {\n"
        "  GRAPH <%1> {\n"
        "    <%1> a nco:IMAddress, nie:InformationElement ;\n"
        "       nco:imAvatar ?_Avatar_Resource ;\n"
        "       nco:imID '%2' .\n"
        "    <telepathy:%3> a nco:IMAccount;\n"
        "      nco:imDisplayName '';\n"
        "      nco:hasIMContact <%1>.\n"
        "  }\n"
        "} WHERE {\n"
        "    ?_Avatar_Resource nie:url \"%4\"\n"
        "}\n").arg(imIri, accountUri, accountPath, existingAvatarImageUrl);

    const QString queryStringBegin = QString::fromLatin1(
        "INSERT {\n"
        "  GRAPH <%1> {\n"
        "    <%2> a nco:PersonContact ;\n"
        "       nco:nickname \"%3\" ;\n"
        "       nie:generator \"addressbook\"");
    const QString queryStringEndWithoutAvatar = QString::fromLatin1(
        " .\n"
        "  }\n"
        "} WHERE {\n"
        "}\n");
    const QString queryStringEndWithPersonalAvatar = QString::fromLatin1(
        " ;\n"
        "       nco:photo ?_Avatar_Resource .\n"
        "  }\n"
        "} WHERE {\n"
        "    ?_Avatar_Resource nie:url \"%1\"\n"
        "}\n").arg(existingAvatarImageUrl);
    const QString queryStringEndWithOnlineAvatar = QString::fromLatin1(
        " ;\n"
        "       nco:hasAffiliation _:_work1 .\n"
        "    _:_work1 a nco:Affiliation ;\n"
        "       rdfs:label \"Work\" ;\n"
        "       nco:hasIMAddress <%1> .\n"
        "  }\n"
        "} WHERE {\n"
        "}\n").arg(imIri);

    QString queryString;

    if (hasExistingAvatar) {
        queryString += insertAvatarFileLinkQueryString;
        if (not isExistingAvatarPersonal) {
            queryString += insertIMAddressQueryString;
        }
    }

    queryString += queryStringBegin;

    if (hasExistingAvatar) {
        if (isExistingAvatarPersonal) {
            queryString += queryStringEndWithPersonalAvatar;
        } else {
            queryString += queryStringEndWithOnlineAvatar;
        }
    } else {
        queryString += queryStringEndWithoutAvatar;
    }

    queryString = queryString.arg(QtContactsTrackerDefaultGraphIri,
                                  makeAnonymousIri(QUuid::createUuid()),
                                  nickname);

    {
        QScopedPointer<QSparqlResult> result(executeQuery(queryString, QSparqlQuery::InsertStatement));
        QVERIFY(not result.isNull());
    }

    // 2. load contact, just nickname, no avatar
    QContact contact;
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QContactFetchHint fetchHint;
        fetchHint.setDetailDefinitionsHint( QStringList(QContactNickname::DefinitionName) );

        QList<QContact> contacts = engine()->contacts(nicknameFilter(nickname), NoSortOrders, fetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);
        QCOMPARE(contacts.count(), 1);

        contact = contacts.at(0);
        registerForCleanup(contact);
    }

    // 3. set a/no thumbnail to the contact
    if (doWriteNewThumbnail) {
        QContactThumbnail thumbnail;
        // set very tiny image, should not be resized, is filled with random data
        thumbnail.setThumbnail(QImage(5, 5, QImage::Format_ARGB32));

        if (withLinkedDetailUri) {
            QContactDetail target = contact.detail<QContactNickname>();
            target.setDetailUri(makeAnonymousIri(QUuid::createUuid()));
            QVERIFY(contact.saveDetail(&target));

            thumbnail.setLinkedDetailUris(target.detailUri());
        }

        QVERIFY(contact.saveDetail(&thumbnail));
    }

    // 4. save partially the thumbnail
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QList<QContact> contacts = QList<QContact>() << contact;
        const QStringList detailMask(QContactThumbnail::DefinitionName);

        const bool contactSaved = engine()->saveContacts(&contacts, detailMask, 0, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(contactSaved);
    }

    // 5. reload contact completely
    QContact fetchedContact;
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        fetchedContact = engine()->contact(contact.localId(), NoFetchHint, &error);
        QCOMPARE(error, QContactManager::NoError);
    }

    // 6. check avatar url
    QContactAvatar fetchedPersonalAvatar;
    QContactAvatar fetchedOnlineAvatar;

    const QContactOnlineAccount onlineAccount = fetchedContact.detail<QContactOnlineAccount>();
    const bool hasOnlineAccount = (hasExistingAvatar && not isExistingAvatarPersonal);
    QCOMPARE(onlineAccount.isEmpty(), not hasOnlineAccount);

    const QList<QContactAvatar> fetchedAvatars = fetchedContact.details<QContactAvatar>();

    foreach(const QContactAvatar &avatar, fetchedAvatars) {
        if (hasOnlineAccount && avatar.linkedDetailUris().contains(onlineAccount.detailUri()) ) {
            QVERIFY(fetchedOnlineAvatar.isEmpty());
            fetchedOnlineAvatar = avatar;
        } else {
            QVERIFY(fetchedPersonalAvatar.isEmpty());
            fetchedPersonalAvatar = avatar;
        }
    }

    if (shouldFetchedPersonalAvatarBeEmpty) {
        QVERIFY(fetchedPersonalAvatar.isEmpty());
    } else {
        if (shouldFetchedPersonalAvatarBeOld) {
            QCOMPARE(fetchedPersonalAvatar.imageUrl().toString(), existingAvatarImageUrl);
        } else {
            QVERIFY(not fetchedPersonalAvatar.imageUrl().isEmpty());
            QVERIFY(fetchedPersonalAvatar.imageUrl().toString() != existingAvatarImageUrl);
        }
    }

    if (shouldFetchedOnlineAvatarBeEmpty) {
        QVERIFY(fetchedOnlineAvatar.isEmpty());
    } else {
        QCOMPARE(fetchedOnlineAvatar.imageUrl().toString(), existingAvatarImageUrl);
    }
}


static void
detailSetSubtract(QSet<QContactDetail> &s1, const QSet<QContactDetail> &s2)
{
    foreach (QContactDetail d2, s2) {
        d2.removeValue(QContactDetail::FieldDetailUri);
        d2.removeValue(QContactDetail::FieldLinkedDetailUris);

        foreach (const QContactDetail &d1, s1) {
            QContactDetail d1Clean = d1;
            d1Clean.removeValue(QContactDetail::FieldDetailUri);
            d1Clean.removeValue(QContactDetail::FieldLinkedDetailUris);

            if (detailMatches(d1Clean, d2)) {
                s1.remove(d1);
                break;
            }
        }
    }
}

void
ut_qtcontacts_trackerplugin::testPartialSaveFuzz_data()
{
    QTest::addColumn<QString>("contactType");

    QTest::newRow("contact") << QContactType::TypeContact.operator QString();
    // fuzzDetailType() does only support contact so far
    // QTest::newRow("group") << QContactType::TypeGroup;
}

void
ut_qtcontacts_trackerplugin::testPartialSaveFuzz()
{
    static const QSet<QString> skipDetails = QSet<QString>()
        // cannot be changed by client
        << QContactType::DefinitionName
        << QContactDisplayLabel::DefinitionName
        // cannot be changed after initial storing by client
        << QContactTimestamp::DefinitionName // at least the created field cannot be changed
        // changed by another process, readonly
        << QContactRelevance::DefinitionName
        ;

    QFETCH(QString, contactType);

    const QMap<QString, QContactDetailDefinition> definitions = engine()->detailDefinitions(contactType, 0);
    const QStringList definitionNames = definitions.keys();

    // First create a full contact with all details set
    QContact c;

    fuzzContact(c);
    saveContact(c);

    c = contact(c.localId());

    foreach (const QString &detailName, definitionNames) {
        if (skipDetails.contains(detailName)) {
            continue;
        }

        qDebug() << "Testing" << detailName;

        modifyFields(detailName, c, definitions.value(detailName).fields());

        const QContact original = c;

        QContactSaveRequest request;
        request.setContact(c);
        request.setDefinitionMask(QStringList() << detailName);
        QVERIFY(engine()->startRequest(&request));
        QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

        c = contact(c.localId());

        foreach (const QString &definitionName, definitionNames) {
            if (skipDetails.contains(definitionName)) {
                continue;
            }

            QSet<QContactDetail> originalDetails = original.details(definitionName).toSet();
            QSet<QContactDetail> fetchedDetails = c.details(definitionName).toSet();

            QVERIFY(originalDetails.size() == fetchedDetails.size());

            detailSetSubtract(originalDetails, fetchedDetails);

            foreach (const QContactDetail &detail, originalDetails) {
                qDebug() << "Lost detail" << definitionName << detail;
            }

            if (not originalDetails.isEmpty()) {
                foreach (const QContactDetail &detail, fetchedDetails) {
                    qDebug() << "Fetched detail" << detail;
                }
            }

            QVERIFY(originalDetails.isEmpty());
        }

        qDebug() << detailName << "OK";
    }
}


void
ut_qtcontacts_trackerplugin::testNotSavingReadOnlyDetails()
{
    QContactPhoneNumber tel1;
    tel1.setNumber("11223344");

    QContactPhoneNumber tel2;
    tel2.setNumber("55667788");

    for(int i = 0; i < 2; ++i) {
        // build the contact
        QSet<QString> expectedPhoneNumbers;
        expectedPhoneNumbers += tel1.number();

        switch(i) {
        case 0:
            // first save both phone numbers...
            expectedPhoneNumbers += tel2.number();
            break;

        case 1:
            // ...and only mark tel2 as read-only in the second run to make sure
            // we also remove tel2 if it existed in an old version of the contact
            // but is marked as read-only now.
            engine()->setDetailAccessConstraints(&tel2, tel2.accessConstraints() |
                                                 QContactDetail::ReadOnly);
            break;

        default:
            QFAIL("unexpected state");
        }

        QContact contact;
        QVERIFY(contact.saveDetail(&tel1));
        QVERIFY(contact.saveDetail(&tel2));
        QCOMPARE(contact.details<QContactPhoneNumber>().count(), 2);

        // save the contact
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&contact, &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);

        // fetch the contact
        error = QContactManager::UnspecifiedError;
        contact = engine()->contactImpl(contact.localId(), fetchHint<QContactPhoneNumber>(), &error);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());

        // verify the contact
        QList<QContactPhoneNumber> fetchedPhoneNumbers = contact.details<QContactPhoneNumber>();
        QCOMPARE(fetchedPhoneNumbers.count(), expectedPhoneNumbers.count());

        foreach(const QContactPhoneNumber &tel, fetchedPhoneNumbers) {
            QVERIFY2(expectedPhoneNumbers.remove(tel.number()), qPrintable(tel.number()));
        }

        QVERIFY2(expectedPhoneNumbers.isEmpty(), "all expected phone numbers found");
    }
}

void
ut_qtcontacts_trackerplugin::testFetchingNonQctResourcesAsReadOnlyDetails_data()
{
    QTest::addColumn<QString>("graphIri");
    QTest::addColumn<QString>("sharedGraphIri");
    QTest::addColumn<QString>("predicate");
    QTest::addColumn<QString>("sharedPredicate");
    QTest::addColumn<QString>("deletePredicate");
    QTest::addColumn<QString>("deleteSharedPredicate");
    QTest::addColumn<bool>("hasAffiliation");
    QTest::addColumn<QString>("detailId");
    QTest::addColumn<int>("accessConstraints");

    static const bool isReadOnly = true;
    static const bool isNotReadOnly = false;

    static const struct GraphData {
        QString graphName;
        QString graphIri;
        bool isReadOnly;
    } graphDataList[] = {
        {QLatin1String("Default/no graph"), QString(),                              isNotReadOnly },
        {QLatin1String("qct graph"),        QtContactsTrackerDefaultGraphIri,       isNotReadOnly },
        {QLatin1String("some other graph"), QLatin1String("test:graphForReadOnly"), isReadOnly }
    };
    static const int graphDataCount = sizeof(graphDataList)/sizeof(graphDataList[0]);

    static const bool hasAffiliation = true;
    static const bool hasNoAffiliation = false;

    static const struct DetailData {
        const char *title;
        const char *predicate;
        const char *sharedPredicate;
        const char *deletePredicate;
        const char *deleteSharedPredicate;
        bool hasAffiliation;
        const char *detailId;
    } detailDataList[] = {
        {
            "QContactAddress (Country, Subtype: Postal)",
            // do _not_ change to use [] notation, see nb#236387
            "%1 nco:hasPostalAddress _:PA . _:PA a nco:PostalAddress; nco:country 'MeeGoLand'.", 0,
            "%1 nco:hasPostalAddress _:PA . _:PA a rdfs:Resource .", 0,
            hasAffiliation,
            QContactAddress::DefinitionName.latin1()
        },
        {
            "QContactAddress (Street, Subtype: Parcel)",
            "%1 nco:hasPostalAddress _:PA . _:PA a nco:ParcelDeliveryAddress; nco:streetAddress 'Highway to ?'.", 0,
            "%1 nco:hasPostalAddress _:PA . _:PA a rdfs:Resource .", 0,
            hasAffiliation,
            QContactAddress::DefinitionName.latin1()
        },
        {
            "QContactBirthday",
            "%1 nco:birthDate '2010-05-18T12:00:00Z'^^xsd:dateTime.", 0,
            "%1 nco:birthDate '2010-05-18T12:00:00Z'^^xsd:dateTime.", 0,
            hasNoAffiliation,
            QContactBirthday::DefinitionName.latin1()
        },
        {
            "QContactEmailAddress",
            "%1 nco:hasEmailAddress <mailto:ogeem@meego.com> .",
            "<mailto:ogeem@meego.com> a nco:EmailAddress . <mailto:ogeem@meego.com> nco:emailAddress 'ogeem@meego.com'^^xsd:string .",
            "%1 nco:hasEmailAddress <mailto:ogeem@meego.com> .",
            "<mailto:ogeem@meego.com> a rdfs:Resource .",
            hasAffiliation,
            QContactEmailAddress::DefinitionName.latin1()
        },
        {
            "QContactGender",
            "%1 nco:gender nco:gender-male .", 0,
            "%1 nco:gender nco:gender-male .", 0,
            hasNoAffiliation,
            QContactGender::DefinitionName.latin1()
        },
        {
            "QContactGeoLocation",
            "%1 nco:hasLocation _:L . _:L a slo:GeoLocation; slo:latitude '0.1e6'^^xsd:double; slo:longitude '1.392e6'^^xsd:double.", 0,
            "%1 nco:hasLocation _:L . _:L a rdfs:Resource .", 0,
            hasNoAffiliation,
            QContactGeoLocation::DefinitionName.latin1()
        },
        {
            "QContactHobby",
            "%1 nco:hobby 'Bugfixing'.", 0,
            "%1 nco:hobby 'Bugfixing'.", 0,
            hasNoAffiliation,
            QContactHobby::DefinitionName.latin1()
        },
        {
            "QContactNickname",
            "%1 nco:nickname 'MeeFixer'.", 0,
            "%1 nco:nickname 'MeeFixer'.", 0,
            hasNoAffiliation,
            QContactNickname::DefinitionName.latin1()
        },
        {
            "QContactNote",
            "%1 nco:note 'Fixes bugs faster than his shadow.' .", 0,
            "%1 nco:note 'Fixes bugs faster than his shadow.' .", 0,
            hasNoAffiliation,
            QContactNote::DefinitionName.latin1()
        },
        {
            "QContactPhoneNumber (Subtypes: Voice, Fax)",
            "%1 nco:hasPhoneNumber <tel:+975312468> . ",
            "<tel:+975312468> a nco:VoicePhoneNumber,nco:FaxNumber .<tel:+975312468> nco:phoneNumber '975312468'.",
            "%1 nco:hasPhoneNumber <tel:+975312468> .",
            "<tel:+975312468> a rdfs:Resource .",
            hasAffiliation,
            QContactPhoneNumber::DefinitionName.latin1()
        },
        {
            "QContactPhoneNumber (Subtypes: Voice, Cell, Pager)",
            "%1 nco:hasPhoneNumber <tel:+975312468> .",
            "<tel:+975312468> a nco:VoicePhoneNumber,nco:CellPhoneNumber,nco:PagerNumber . <tel:+975312468> nco:phoneNumber '975312468'.",
            "%1 nco:hasPhoneNumber <tel:+975312468> .",
            "<tel:+975312468> a rdfs:Resource .",
            hasAffiliation,
            QContactPhoneNumber::DefinitionName.latin1()
        },
        {
            "QContactRingtone (Audio)",
            "%1 maemo:contactAudioRingtone <http://meego.com/ringtone.wav> .",
            "<http://meego.com/ringtone.wav> a nfo:FileDataObject; nie:url 'http://meego.com/ringtone.wav' .",
            "%1 maemo:contactAudioRingtone <http://meego.com/ringtone.wav> .",
            "<http://meego.com/ringtone.wav> a rdfs:Resource .",
            hasNoAffiliation,
            QContactRingtone::DefinitionName.latin1()
        },
        {
            "QContactTag",
            "%1 nao:hasTag _:T .",
            "_:T a nao:Tag; nao:prefLabel 'Test' .",
            "%1 nao:hasTag _:T .",
            " _:T a rdfs:Resource .",
            hasNoAffiliation,
            QContactTag::DefinitionName.latin1()
        },
        {
            "QContactUrl (Subtype: Favourite)",
            "%1 nco:url 'http://openismus.com/' .", 0,
            "%1 nco:url 'http://openismus.com/' .", 0,
            hasAffiliation,
            QContactUrl::DefinitionName.latin1()
        },
        {
            "QContactUrl (Subtype: HomePage)",
            "%1 nco:websiteUrl 'http://meego.com/' .", 0,
            "%1 nco:websiteUrl 'http://meego.com/' .", 0,
            hasAffiliation,
            QContactUrl::DefinitionName.latin1()
        }
    };
    static const int detailDataCount = sizeof(detailDataList)/sizeof(detailDataList[0]);

    for(int i = 0; i < graphDataCount; ++i) {
        const GraphData &graphData = graphDataList[i];
        for(int d = 0; d < detailDataCount; ++d) {
            const DetailData &detailData = detailDataList[d];
            for(int s = 0; s < graphDataCount; ++s) {
                const GraphData &sharedGraphData = graphDataList[s];

                const bool hasSharedResources = (detailData.sharedPredicate != 0);
                QString rowTitle = detailData.title + QLatin1String(" - ") + graphData.graphName;

                if (hasSharedResources) {
                    rowTitle += QLatin1String(" - ") + sharedGraphData.graphName;
                }

                QTest::newRow(rowTitle)
                    << graphData.graphIri
                    << sharedGraphData.graphIri
                    << QString::fromLatin1(detailData.predicate)
                    << QString::fromLatin1(detailData.sharedPredicate)
                    << QString::fromLatin1(detailData.deletePredicate)
                    << QString::fromLatin1(detailData.deleteSharedPredicate)
                    << detailData.hasAffiliation
                    << QString::fromLatin1(detailData.detailId)
                    << (graphData.isReadOnly ? (int)QContactDetail::ReadOnly : 0);

                // no need to loop through all graphs for shared resources?
                if (not hasSharedResources) {
                    break;
                }
            }
        }
    }
}


static QString
createReadOnlyInsertQueryString(const QString &predicate, const QString &sharedPredicate,
                                const QString &graphIri, const QString &sharedGraphIri,
                                bool hasAffiliation, QContactLocalId contactLocalId)
{
    static const QString affiliationTemplate = QLatin1String(
    "     _:Affiliation a nco:Affiliation .\n"
    "     ?contact nco:hasAffiliation _:Affiliation .\n");
    static const QString affiliationResource = QLatin1String("_:Affiliation");

    static const QString contactTemplate = QString();
    static const QString contactResource = QLatin1String("?contact");

    static const QString insertTemplate = QLatin1String(
    "INSERT {\n"
    "%1\n"
    "}\n");
    static const QString graphTemplate = QLatin1String(
    "   GRAPH <%1> {\n"
    "   %2\n"
    "   }\n");
    static const QString noGraphTemplate = QLatin1String(
    "   %1\n");

    const QString fullPredicate =
        (hasAffiliation ? affiliationTemplate : contactTemplate) +
        predicate.arg(hasAffiliation ? affiliationResource : contactResource);

    QString predicates;
    // first shared predicates, so they are defined when used in normal predicates
    if (not sharedPredicate.isEmpty()) {
        predicates +=
            sharedGraphIri.isEmpty() ? noGraphTemplate.arg(sharedPredicate) : graphTemplate.arg(sharedGraphIri, sharedPredicate);
    }
    predicates +=
        graphIri.isEmpty() ? noGraphTemplate.arg(fullPredicate) : graphTemplate.arg(graphIri, fullPredicate);

    const QString queryString =
        insertTemplate.arg(predicates) +
        QString::fromLatin1("WHERE { ?contact a nco:PersonContact FILTER(tracker:id(?contact) = %1) }\n").arg(contactLocalId);

    return queryString;
}

static QString
createReadOnlyDeleteQueryString(const QString &predicate, const QString &sharedPredicate,
                                const QString &graphIri, const QString &sharedGraphIri,
                                bool hasAffiliation, QContactLocalId contactLocalId)
{
    static const QString affiliationTemplate = QLatin1String(
    "\n"
    "?contact nco:hasAffiliation _:Affiliation .\n"
    "_:Affiliation a nco:Affiliation .");
    static const QString affiliationResource = QLatin1String("_:Affiliation");

    static const QString contactTemplate = QString();
    static const QString contactResource = QLatin1String("?contact");

    static const QString deleteTemplate = QLatin1String(
    "DELETE {\n"
    "%1\n"
    "}\n");
    static const QString graphTemplate = QLatin1String(
    "   GRAPH <%1> {\n"
    "   %2\n"
    "   }\n");
    static const QString noGraphTemplate = QLatin1String(
    "   %1\n");

    const QString fullPredicate =
        predicate.arg(hasAffiliation ? affiliationResource : contactResource) +
        (hasAffiliation ? affiliationTemplate : contactTemplate);

    QString predicates =
        graphIri.isEmpty() ? noGraphTemplate.arg(fullPredicate) : graphTemplate.arg(graphIri, fullPredicate);
    if (not sharedPredicate.isEmpty()) {
        predicates +=
            sharedGraphIri.isEmpty() ? noGraphTemplate.arg(sharedPredicate) : graphTemplate.arg(sharedGraphIri, sharedPredicate);
    }

    const QString queryString =
        deleteTemplate.arg(predicates) +
        QString::fromLatin1("WHERE { ?contact a nco:PersonContact FILTER(tracker:id(?contact) = %1) }\n").arg(contactLocalId);

    return queryString;
}

void
ut_qtcontacts_trackerplugin::testFetchingNonQctResourcesAsReadOnlyDetails()
{
    QFETCH(QString, graphIri);
    QFETCH(QString, sharedGraphIri);
    QFETCH(QString, predicate);
    QFETCH(QString, sharedPredicate);
    QFETCH(QString, deletePredicate);
    QFETCH(QString, deleteSharedPredicate);
    QFETCH(bool,    hasAffiliation);
    QFETCH(QString, detailId);
    QFETCH(int,     accessConstraints);

    QContact contact;

    // save the contact
    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&contact, &error);
        registerForCleanup(contact);
        QCOMPARE(error, QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);
    }

    // add the detail
    {
        const QString queryString =
            createReadOnlyInsertQueryString(predicate, sharedPredicate, graphIri, sharedGraphIri,
                                            hasAffiliation, contact.localId());
        QScopedPointer<QSparqlResult> result(executeQuery(queryString, QSparqlQuery::InsertStatement));
        QVERIFY(not result.isNull());
    }

    // fetch the contact
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContact fetchedContact = engine()->contactImpl(contact.localId(), NoFetchHint, &error);
    QCOMPARE(error, QContactManager::NoError);
    QVERIFY(0 != fetchedContact.localId());

    // verify the accessConstraints
    const QList<QContactDetail> fetchedDetails = fetchedContact.details(detailId);
    QCOMPARE(fetchedDetails.count(), 1);
    QCOMPARE((int)fetchedDetails.first().accessConstraints(), accessConstraints);

    // clean up, as tracker might mess with some same triples in different graphs
    {
        const QString queryString =
            createReadOnlyDeleteQueryString(deletePredicate, deleteSharedPredicate,
                                            graphIri, sharedGraphIri,
                                            hasAffiliation, contact.localId());
        QScopedPointer<QSparqlResult> result(executeQuery(queryString, QSparqlQuery::InsertStatement));
        QVERIFY(not result.isNull());
    }
}

void
ut_qtcontacts_trackerplugin::testDetailsFromServicesAreLinked_data()
{
    const QString accountId = QLatin1String("foo@bar.org");
    const QString accountPath = QLatin1String("/org/freedesktop/testDetailsFromServiceAreLinked/account/0");
    const QString telepathyIri = makeTelepathyIri(accountPath, accountId);

    QTest::addColumn<QString>("graphIri");
    QTest::addColumn<QStringList>("expectedLinkedDetailUris");

    QTest::newRow("fromService")
        << telepathyIri
        << QStringList(telepathyIri);

    QTest::newRow("notFromService")
        << QtContactsTrackerDefaultGraphIri.operator QString()
        << QStringList();
}

void
ut_qtcontacts_trackerplugin::testDetailsFromServicesAreLinked()
{
    QFETCH(QString, graphIri);
    QFETCH(QStringList, expectedLinkedDetailUris);

    const QString contactIri = QLatin1String("contact:testDetailsFromServiceAreLinked:") +
                               QString::fromLatin1(QTest::currentDataTag());
    const QString accountId = QLatin1String("foo@bar.org");
    const QString accountPath = QLatin1String("/org/freedesktop/testDetailsFromServiceAreLinked/account/0");
    const QString telepathyIri = makeTelepathyIri(accountPath, accountId);

    // 1. create im contact in tracker
    const QContactLocalId cid =
        insertIMContact(contactIri,
                        accountId,
                        QLatin1String("nco:presence-status-available"),
                        QLatin1String("Testing the linked details from services"),
                        accountPath);
    // 2. add some details to the contact (one by affiliation, one directly on contact)
    {
        const QString query = QString::fromLatin1(
            "INSERT {\n"
            "  GRAPH <%1> {\n"
            "    <urn:x-maemo-phone:mobile:+7044866478>\n"
            "      a nco:CellPhoneNumber, nco:PhoneNumber ;\n"
            "      nco:phoneNumber \"+7044866478\" ;\n"
            "      maemo:localPhoneNumber \"044866478\" .\n"
            "    <%2>\n"
            "      nco:birthDate \"1980-02-14T00:00:00Z\" ;\n"
            "      nco:hasAffiliation [\n"
            "        a nco:Affiliation ;\n"
            "        rdfs:label \"Work\" ;\n"
            "        nco:hasPhoneNumber <urn:x-maemo-phone:mobile:+7044866478> ] .\n"
            "  }\n"
            "}\n").arg(graphIri, contactIri);
        QScopedPointer<QSparqlResult> result
                (executeQuery(query,
                              QSparqlQuery::InsertStatement));

        QVERIFY(not result.isNull());
    }

    // 3. fetch contact and check the details if links to online account detail are present and correct
    const QContact fetchedContact = contact(cid);

    QCOMPARE(fetchedContact.details<QContactOnlineAccount>().count(), 1);
    const QString onlineAccountDetailUri = fetchedContact.detail<QContactOnlineAccount>().detailUri();
    QCOMPARE(onlineAccountDetailUri, telepathyIri);

    QCOMPARE(fetchedContact.details<QContactBirthday>().count(), 1);
    const QContactBirthday birthday = fetchedContact.details<QContactBirthday>().at(0);
    QCOMPARE(birthday.date(), QDate(1980,2,14));
    QCOMPARE(birthday.linkedDetailUris(), expectedLinkedDetailUris);

    QCOMPARE(fetchedContact.details<QContactPhoneNumber>().count(), 1);
    const QContactPhoneNumber phoneNumber = fetchedContact.details<QContactPhoneNumber>().at(0);
    QCOMPARE(phoneNumber.number(), QLatin1String("+7044866478"));
    QCOMPARE(sortedStringList(phoneNumber.subTypes()), impliedPhoneNumberTypes(QStringList(QContactPhoneNumber::SubTypeMobile)));
    QCOMPARE(phoneNumber.linkedDetailUris(), expectedLinkedDetailUris);
}

void
ut_qtcontacts_trackerplugin::testDetailsFromServicesAreLinkedWithMergedContacts_data()
{
    const QString accountIdTemplate = QLatin1String("foo%1@bar.org");
    const QString accountPathTemplate = QLatin1String("/org/freedesktop/testDetailsFromServiceAreLinked/account/%1");
    const QString telepathyIri1 = makeTelepathyIri(accountPathTemplate.arg(1), accountIdTemplate.arg(1));
    const QString telepathyIri2 = makeTelepathyIri(accountPathTemplate.arg(2), accountIdTemplate.arg(2));

    QTest::addColumn<QString>("graphIri1");
    QTest::addColumn<QString>("graphIri2");
    QTest::addColumn<QStringList>("expectedLinkedDetailUris1");
    QTest::addColumn<QStringList>("expectedLinkedDetailUris2");

    QTest::newRow("allFromService")
        << telepathyIri1
        << telepathyIri2
        << QStringList(telepathyIri1)
        << QStringList(telepathyIri2);

    QTest::newRow("secondFromService")
        << QtContactsTrackerDefaultGraphIri.operator QString()
        << telepathyIri2
        << QStringList()
        << QStringList(telepathyIri2);

    QTest::newRow("firstFromService")
        << telepathyIri1
        << QtContactsTrackerDefaultGraphIri.operator QString()
        << QStringList(telepathyIri1)
        << QStringList();

    QTest::newRow("noneFromService")
        << QtContactsTrackerDefaultGraphIri.operator QString()
        << QtContactsTrackerDefaultGraphIri.operator QString()
        << QStringList()
        << QStringList();
}

void
ut_qtcontacts_trackerplugin::testDetailsFromServicesAreLinkedWithMergedContacts()
{
    QFETCH(QString, graphIri1);
    QFETCH(QString, graphIri2);
    QFETCH(QStringList, expectedLinkedDetailUris1);
    QFETCH(QStringList, expectedLinkedDetailUris2);

    // 1. create two im contacts in tracker,
    const QString contactIriTemplate = QLatin1String("contact:testDetailsFromServicesAreLinkedWithMergedContacts:") +
                                       QString::fromLatin1(QTest::currentDataTag()) + QLatin1String(":%1");
    const QString accountIdTemplate = QLatin1String("foo%1@bar.org");
    const QString accountPathTemplate = QLatin1String("/org/freedesktop/testDetailsFromServiceAreLinked/account/%1");

    const QString contactIri1 = contactIriTemplate.arg(1);
    const QString accountId1 = accountIdTemplate.arg(1);
    const QString accountPath1 = accountPathTemplate.arg(1);
    const QString contactIri2 = contactIriTemplate.arg(2);
    const QString accountId2 = accountIdTemplate.arg(2);
    const QString accountPath2 = accountPathTemplate.arg(2);
    const QString telepathyIri1 = makeTelepathyIri(accountPath1, accountId1);
    const QString telepathyIri2 = makeTelepathyIri(accountPath2, accountId2);

    const QContactLocalId cid1 =
        insertIMContact(contactIri1,
                        accountId1,
                        QLatin1String("nco:presence-status-available"),
                        QLatin1String("Testing the linked details from services"),
                        accountPath1);
    const QContactLocalId cid2 =
        insertIMContact(contactIri2,
                        accountId2,
                        QLatin1String("nco:presence-status-available"),
                        QLatin1String("Testing the linked details from services"),
                        accountPath2);

    // 2. add different details to the contacts (one by same affiliation Work, one directly on contact)
    {
        const QString query = QString::fromLatin1(
            "INSERT {\n"
            "  GRAPH <%1> {\n"
            "    <urn:x-maemo-phone:mobile:+7044866478>\n"
            "      a nco:CellPhoneNumber, nco:PhoneNumber ;\n"
            "      nco:phoneNumber \"+7044866478\" ;\n"
            "      maemo:localPhoneNumber \"044866478\" .\n"
            "    <%2>\n"
            "      nco:birthDate \"1980-02-14T00:00:00Z\" ;\n"
            "      nco:hasAffiliation [\n"
            "        a nco:Affiliation ;\n"
            "        rdfs:label \"Work\" ;\n"
            "        nco:hasPhoneNumber <urn:x-maemo-phone:mobile:+7044866478> ] .\n"
            "  }\n"
            "  GRAPH <%3> {\n"
            "    <mailto:maemo@maemo.org>\n"
            "      a nco:EmailAddress ;\n"
            "      nco:emailAddress 'maemo@maemo.org' .\n"
            "    <%4>\n"
            "      nco:gender nco:gender-male ;\n"
            "      nco:hasAffiliation [\n"
            "        a nco:Affiliation ;\n"
            "        rdfs:label \"Work\" ;\n"
            "        nco:hasEmailAddress <mailto:maemo@maemo.org> ] .\n"
            "  }\n"
            "}\n").arg(graphIri1, contactIri1, graphIri2, contactIri2);
        QScopedPointer<QSparqlResult> result
                (executeQuery(query,
                              QSparqlQuery::InsertStatement));

        QVERIFY(not result.isNull());
    }

    // 3. merge contacts
    QContact mergedContact = contact(cid1);
    mergeContacts(mergedContact, QList<QContactLocalId>() << cid2);

    // 4. check the details if links to online account detail are present and correct
    const QList<QContactOnlineAccount> onlineAccounts = mergedContact.details<QContactOnlineAccount>();
    QCOMPARE(onlineAccounts.count(), 2);
    const QStringList onlineAccountDetailUris = QStringList()
        << onlineAccounts.at(0).detailUri() << onlineAccounts.at(1).detailUri();
    QVERIFY(onlineAccountDetailUris.contains(telepathyIri1));
    QVERIFY(onlineAccountDetailUris.contains(telepathyIri2));

    QCOMPARE(mergedContact.details<QContactBirthday>().count(), 1);
    const QContactBirthday birthday = mergedContact.details<QContactBirthday>().at(0);
    QCOMPARE(birthday.date(), QDate(1980,2,14));
    QCOMPARE(birthday.linkedDetailUris(), expectedLinkedDetailUris1);

    QCOMPARE(mergedContact.details<QContactPhoneNumber>().count(), 1);
    const QContactPhoneNumber phoneNumber = mergedContact.details<QContactPhoneNumber>().at(0);
    QCOMPARE(phoneNumber.number(), QLatin1String("+7044866478"));
    QCOMPARE(sortedStringList(phoneNumber.subTypes()), impliedPhoneNumberTypes(QStringList(QContactPhoneNumber::SubTypeMobile)));
    QCOMPARE(phoneNumber.linkedDetailUris(), expectedLinkedDetailUris1);

    QCOMPARE(mergedContact.details<QContactGender>().count(), 1);
    const QContactGender gender = mergedContact.details<QContactGender>().at(0);
    QCOMPARE(gender.gender(), QContactGender::GenderMale.operator QString());
    QCOMPARE(gender.linkedDetailUris(), expectedLinkedDetailUris2);

    QCOMPARE(mergedContact.details<QContactEmailAddress>().count(), 1);
    const QContactEmailAddress emailAddress = mergedContact.details<QContactEmailAddress>().at(0);
    QCOMPARE(emailAddress.emailAddress(), QLatin1String("maemo@maemo.org"));
    QCOMPARE(emailAddress.linkedDetailUris(), expectedLinkedDetailUris2);
}

void
ut_qtcontacts_trackerplugin::testFavorite()
{
    QContactLocalId contactLocalId;

    // Create a contact
    {
        QContact c;
        QContactName name;
        name.setFirstName(QLatin1String("Ima"));
        name.setLastName(QLatin1String("Favorite"));
        QVERIFY(c.saveDetail(&name));

        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
        contactLocalId = c.localId();
    }

    // find contact, verify no favorite tag
    {
        QContact c;
        c = engine()->contactImpl(contactLocalId, NoFetchHint, 0);
        QContactName cn = c.detail<QContactName>();
        QCOMPARE(cn.firstName(), QString("Ima"));
        QCOMPARE(cn.lastName(), QString("Favorite"));
        QVERIFY(c.details<QContactFavorite>().isEmpty());
    }

    // find contact, tag favorite
    {
        QContact c;
        c = engine()->contactImpl(contactLocalId, NoFetchHint, 0);
        QContactName cn = c.detail<QContactName>();
        QCOMPARE(cn.firstName(), QString("Ima"));
        QCOMPARE(cn.lastName(), QString("Favorite"));

        QContactFavorite fav;
        fav.setFavorite(true);
        QVERIFY(fav.isFavorite());

        QVERIFY(c.saveDetail(&fav));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
    }

    // find contact, check favorite is set
    {
        QContact c;
        c = engine()->contactImpl(contactLocalId, NoFetchHint, 0);
        QContactName cn = c.detail<QContactName>();
        QCOMPARE(cn.firstName(), QString("Ima"));
        QCOMPARE(cn.lastName(), QString("Favorite"));

        QContactFavorite fav = c.detail<QContactFavorite>();
        QVERIFY(fav.isFavorite());
    }

    // find contact, untag favorite
    {
        QContact c;
        c = engine()->contactImpl(contactLocalId, NoFetchHint, 0);
        QContactName cn = c.detail<QContactName>();
        QCOMPARE(cn.firstName(), QString("Ima"));
        QCOMPARE(cn.lastName(), QString("Favorite"));

        QContactFavorite fav = c.detail<QContactFavorite>();
        fav.setFavorite(false);
        QVERIFY(not fav.isFavorite());

        QVERIFY(c.saveDetail(&fav));
        QContactManager::Error error(QContactManager::UnspecifiedError);
        QVERIFY(engine()->saveContact(&c, &error));
        QCOMPARE(error,  QContactManager::NoError);
    }

    // find contact, check favorite is unset
    {
        QContact c;
        c = engine()->contactImpl(contactLocalId, NoFetchHint, 0);
        QContactName cn = c.detail<QContactName>();
        QCOMPARE(cn.firstName(), QLatin1String("Ima"));
        QCOMPARE(cn.lastName(), QLatin1String("Favorite"));

        QContactFavorite fav = c.detail<QContactFavorite>();
        QVERIFY(not fav.isFavorite());
    }
}

void
ut_qtcontacts_trackerplugin::testAnniversary()
{
    const QString calendarId = QUuid::createUuid().toString();
    QContactLocalId contactLocalId;
    QStringList anniversaryTypes;

    {
        QContactAnniversary anniversary;
        anniversary.setOriginalDate(randomDateTime().date());
        anniversary.setCalendarId(calendarId);
        anniversary.setSubType(QContactAnniversary::SubTypeEngagement);
        anniversaryTypes += anniversary.subType();

        QContact contact;
        QVERIFY(contact.saveDetail(&anniversary));

        QContactManager::Error error = QContactManager::UnspecifiedError;
        const bool contactSaved = engine()->saveContact(&contact, &error);
        QCOMPARE(error,  QContactManager::NoError);
        QVERIFY(0 != contact.localId());
        QVERIFY(contactSaved);

        contactLocalId = contact.localId();
    }

    {
        const QString query =
                QString::fromLatin1("INSERT { ?e ncal:categories \"%1\" } WHERE { ?e ncal:uid \"%2\" }").
                arg(QContactAnniversary::SubTypeWedding, calendarId);

        QScopedPointer<QSparqlResult> result(executeQuery(query, QSparqlQuery::InsertStatement));
        QVERIFY(not result.isNull());
    }

    {
        QContactManager::Error error = QContactManager::UnspecifiedError;
        QContact contact = engine()->contactImpl(contactLocalId, NoFetchHint, &error);
        QCOMPARE(contact.localId(), contactLocalId);
        QCOMPARE(error,  QContactManager::NoError);

        const QList<QContactAnniversary> anniversaryDetails = contact.details<QContactAnniversary>();
        QCOMPARE(anniversaryDetails.count(), 2);

        foreach(const QContactAnniversary &anniversary, anniversaryDetails) {
            QCOMPARE(anniversary.originalDate(), anniversaryDetails.first().originalDate());
            QCOMPARE(anniversary.calendarId(), anniversaryDetails.first().calendarId());
            anniversaryTypes.removeOne(anniversary.subType());
        }

        QVERIFY(anniversaryTypes.isEmpty());
    }
}

void
ut_qtcontacts_trackerplugin::testOnlineAccount()
{
    const QString accountPath = QLatin1String("/org/freedesktop/testAccountPath/account/0");
    const QString accountUri = QLatin1String("ut@test.org");

    QContact c;
    QContactOnlineAccount account;
    account.setAccountUri(accountUri);
    account.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
    c.saveDetail(&account);
    saveContact(c);

    c = contact(c.localId());
    QCOMPARE(c.details<QContactOnlineAccount>().size(), 1);
    QCOMPARE(c.detail<QContactOnlineAccount>().detailUri(), QString::fromLatin1("telepathy:%1!%2").arg(accountPath, accountUri));
    QCOMPARE(c.detail<QContactOnlineAccount>().value(QContactOnlineAccount__FieldAccountPath), accountPath);
    QCOMPARE(c.detail<QContactOnlineAccount>().accountUri(), accountUri);
}

EvilRequestKiller::EvilRequestKiller(QContactAbstractRequest *request,
                                     Qt::ConnectionType connectionType,
                                     QContactAbstractRequest::State deletionState,
                                     QObject *parent)
    : QObject(parent)
    , m_request(request)
    , m_deletionState(deletionState)
{
    connect(m_request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
            this, SLOT(onStateChanged(QContactAbstractRequest::State)),
            connectionType);
}

void
EvilRequestKiller::onStateChanged(QContactAbstractRequest::State state)
{
    if (m_deletionState == state) {
        delete m_request;
    }
}

void
ut_qtcontacts_trackerplugin::testDeleteFromStateChangedHandler_data()
{
    QTest::addColumn<Qt::ConnectionType>("connectionType");
    QTest::addColumn<QContactAbstractRequest::State>("deletionState");

    QTest::newRow("queued/active")   << Qt::QueuedConnection << QContactAbstractRequest::ActiveState;
    QTest::newRow("queued/canceled") << Qt::QueuedConnection << QContactAbstractRequest::CanceledState;
    QTest::newRow("queued/finished") << Qt::QueuedConnection << QContactAbstractRequest::FinishedState;

    QTest::newRow("direct/active")   << Qt::DirectConnection << QContactAbstractRequest::ActiveState;
    QTest::newRow("direct/canceled") << Qt::DirectConnection << QContactAbstractRequest::CanceledState;
    QTest::newRow("direct/finished") << Qt::DirectConnection << QContactAbstractRequest::FinishedState;
}

void
ut_qtcontacts_trackerplugin::testDeleteFromStateChangedHandler()
{
    QFETCH(Qt::ConnectionType, connectionType);
    QFETCH(QContactAbstractRequest::State, deletionState);

    QContactManager manager(QLatin1String("tracker"), makeEngineParams());
    QCOMPARE(manager.managerName(), QLatin1String("tracker"));

    QContactLocalIdFilter filter;
    filter.setIds(QContactLocalIdList() << 42);

    // run a few iterations to hopefully trigger race conditions
    for(int iteration = 0; iteration < 100; ++iteration) {
        QPointer<QContactFetchRequest> request = new QContactFetchRequest;
        new EvilRequestKiller(request, connectionType, deletionState, request);

        request->setManager(&manager);
        request->setFilter(filter);

        if (QContactAbstractRequest::CanceledState == deletionState) {
            QTimer::singleShot(0, request, SLOT(cancel()));
        }

        QVERIFY(request->start());
        // we surely have a dead-lock if such simple request doesn't finish within a second
        QVERIFY(request.isNull() || request->waitForFinished(0));
        QVERIFY(request.isNull());
    }
}

void
ut_qtcontacts_trackerplugin::testDetailUriEncoding()
{
    static const QString imSeparator = QLatin1String("!");
    static const QString telepathyPrefix = QLatin1String("telepathy:");

    const QString accountPath = QLatin1String("/org/freedesktop/testDetailUriEncoding/<account>/0");
    const QString imId = QLatin1String("<contactId>");

    // QContactOnlineAccount detailUri + AccountPath

    QContact c;
    QContactOnlineAccount account;

    account.setValue(QContactOnlineAccount__FieldAccountPath, accountPath);
    account.setAccountUri(imId);
    account.setServiceProvider(QLatin1String("DisplayName"));
    c.saveDetail(&account);

    saveContact(c);
    c = contact(c.localId());
    account = c.detail<QContactOnlineAccount>();

    QCOMPARE(account.detailUri(), (telepathyPrefix % accountPath % imSeparator % imId).operator QString());
    QCOMPARE(account.value(QContactOnlineAccount__FieldAccountPath), accountPath);

    // QContactPhoneNumber detailUri

    c = QContact();

    const QString email = QLatin1String("<hello>@world.com");

    QContactEmailAddress emailAddress;
    emailAddress.setEmailAddress(email);
    c.saveDetail(&emailAddress);

    saveContact(c);

    c = contact(c.localId());

    QCOMPARE(c.detail<QContactEmailAddress>().detailUri(), QLatin1String("mailto:") + email);

    // QContactPresence detaiUri

    const QContactLocalId cid = insertIMContact(QLatin1String("test:testDetailUriEncoding:1"),
                                                QLatin1String("%3cfoo%3e@bar.org"),
                                                QLatin1String("nco:presence-status-available"),
                                                QLatin1String("Testing the encoding"),
                                                QLatin1String("/org/freedesktop/testDetailUriEncoding/%3caccount%3e/0"));

    c = contact(cid);

    QCOMPARE(c.detail<QContactPresence>().detailUri(),
             QString::fromLatin1("presence:/org/freedesktop/testDetailUriEncoding/<account>/0!<foo>@bar.org"));
}

/***************************     Helper functions for unit tests   ***************'*/

QContact
ut_qtcontacts_trackerplugin::contact(QContactLocalId id, const QStringList &details)
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContact result = engine()->contactImpl(id, fetchHint(details), &error);

    if (QContactManager::NoError != error) {
        qctWarn(QTest::toString(error));
    }

    return result;
}

QContact
ut_qtcontacts_trackerplugin::contact(const QString &iri, const QStringList &details)
{
    return contact(resolveTrackerId(iri), details);
}

QList<QContact>
ut_qtcontacts_trackerplugin::contacts(const QList<QContactLocalId> &ids, const QStringList &details)
{
    QContactManager::Error error = QContactManager::UnspecifiedError;
    QContactList result = engine()->contacts(localIdFilter(ids), NoSortOrders, fetchHint(details), &error);

    if (QContactManager::NoError != error) {
        qctWarn(QTest::toString(error));
    }

    return result;
}

QCT_TEST_MAIN(ut_qtcontacts_trackerplugin)
