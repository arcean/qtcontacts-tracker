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

#include "ut_qtcontacts_trackerplugin_common.h"
#include "resourcecleanser.h"

#include <dao/contactdetailschema.h>
#include <dao/subject.h>
#include <dao/support.h>
#include <lib/logger.h>
#include <lib/settings.h>
#include <lib/sparqlconnectionmanager.h>
#include <lib/sparqlresolver.h>
#include <lib/threadlocaldata.h>
#include <lib/constants.h>

#include <QContactFetchRequest>
#include <QContactLocalIdFilter>
#include <QContactRemoveRequest>
#include <QContactSaveRequest>

#include <QVersitReader>
#include <QVersitContactImporter>

#include <QtXml/QDomDocument>

const QList<QContactSortOrder> ut_qtcontacts_trackerplugin_common::NoSortOrders;
const QContactFetchHint ut_qtcontacts_trackerplugin_common::NoFetchHint;

ut_qtcontacts_trackerplugin_common::ut_qtcontacts_trackerplugin_common(const QDir &dataDir,
                                                                       const QDir &srcDir,
                                                                       QObject *parent)
    : QObject(parent)
    , m_engine(0)
    , m_dataDir(dataDir)
    , m_srcDir(srcDir)
    , m_uuid(QUuid::createUuid())
{
    QDir localPluginDir = QCoreApplication::applicationDirPath();
    localPluginDir.cd (QLatin1String("../.."));

    QStringList libraryPaths = qApp->libraryPaths();
    libraryPaths.prepend(localPluginDir.absolutePath());

    qApp->setLibraryPaths(libraryPaths);

    // parse DEBUG envvar
    const QStringList debugFlags =
            QProcessEnvironment::systemEnvironment().
            value(QLatin1String("DEBUG")).trimmed().toLower().
            split(QRegExp(QLatin1String("\\s*(,|\\s)\\s*")));

    m_verbose = debugFlags.contains(QLatin1String("verbose"));
}

ut_qtcontacts_trackerplugin_common::~ut_qtcontacts_trackerplugin_common()
{
    Q_ASSERT(0 == m_engine);

    // Really delete any deferred-deleted objects, which QTest doesn't seem
    // to delete otherwise. This helps valgrind's leak check.
    while (QCoreApplication::hasPendingEvents()) {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}

QString
ut_qtcontacts_trackerplugin_common::uniqueTestId(const QString& id) const
{
    QString result = QLatin1String(metaObject()->className()) % QLatin1Char(':') % m_uuid;
    if (not id.isEmpty()) {
        result += QLatin1Char(':') % id;
    }
    return result;
}

QString
ut_qtcontacts_trackerplugin_common::makeUniqueName(const QString &id) const
{
    static const int counterDigits = 5; // 99999 is latest, sorting will fail with higher numbers
    static int counter = 0;
    return (uniqueTestId(id) %
            QLatin1Char('_') % QString::fromLatin1("%1").arg(counter++, counterDigits, 10, QLatin1Char('0')));
}

void
ut_qtcontacts_trackerplugin_common::setTestNicknameToContact(QContact &contact,
                                                             const QString &id) const
{
    QContactNickname nicknameDetail;
    const QString nickname = makeUniqueName(id);
    nicknameDetail.setNickname(nickname);
    QVERIFY(contact.saveDetail(&nicknameDetail));
}

QContactDetailFilter
ut_qtcontacts_trackerplugin_common::testNicknameFilter(const QString &id) const
{
    QContactDetailFilter filter;
    filter.setDetailDefinitionName(QContactNickname::DefinitionName, QContactNickname::FieldNickname);
    filter.setMatchFlags(QContactDetailFilter::MatchStartsWith);
    filter.setValue(uniqueTestId(id));
    return filter;
}

uint
ut_qtcontacts_trackerplugin_common::resolveTrackerId(const QString &iri)
{
    const QList<uint> ids = resolveTrackerIds(QStringList(iri));
    return (not ids.isEmpty() ? ids.first() : 0);
}

QList<uint>
ut_qtcontacts_trackerplugin_common::resolveTrackerIds(const QStringList &iris)
{
    QctTrackerIdResolver resolver(iris);

    if (not resolver.lookupAndWait()) {
        return QList<uint>();
    }

    return resolver.trackerIds();
}

QString
ut_qtcontacts_trackerplugin_common::onlineAvatarPath(const QString &accountPath)
{
    const QString avatarHash = QLatin1String(QCryptographicHash::hash(accountPath.toLocal8Bit(),
                                                                      QCryptographicHash::Md5).toHex());
    return QDir(QLatin1String("/home/user/.cache/telepathy/avatars")).filePath(avatarHash);
}

QSet<QString>
ut_qtcontacts_trackerplugin_common::definitionNames(const QList<QContact> &contacts)
{
    QSet<QString> result;

    foreach(const QContact &c, contacts) {
        result += definitionNames(c.details());
    }

    return result;
}

QSet<QString>
ut_qtcontacts_trackerplugin_common::definitionNames(const QList<QContactDetail> &details)
{
    QSet<QString> result;

    foreach(const QContactDetail &d, details) {
        result += d.definitionName();
    }

    return result;
}

QContactLocalIdList
ut_qtcontacts_trackerplugin_common::localContactIds(const QList<QContact> &contacts)
{
    QContactLocalIdList result;

    foreach(const QContact &c, contacts) {
        result += c.localId();
    }

    return result;
}

QMap<QString, QString>
ut_qtcontacts_trackerplugin_common::makeEngineParams() const
{
    QMap<QString, QString> params;

    params.insert(QLatin1String("debug"), QLatin1String("no-nagging"));
    params.insert(QLatin1String("avatar-types"), QLatin1String("all"));

    return params;
}

QContactTrackerEngine *
ut_qtcontacts_trackerplugin_common::engine() const
{
    // need lazy initialization because *_data() is called before init() -- QTBUG-11186
    if (0 == m_engine) {
        m_engine = new QContactTrackerEngine(makeEngineParams());
    }

    return m_engine;
}

void
ut_qtcontacts_trackerplugin_common::resetEngine()
{
    delete m_engine;
    m_engine = 0;
}

static QString
findCPUModel()
{
    QFile cpuinfo(QLatin1String("/proc/cpuinfo"));
    QString modelName;

    if (cpuinfo.open(QFile::ReadOnly)) {
        const QString text = QString::fromAscii(cpuinfo.readAll());
        int i = text.indexOf(QLatin1String("model name"));

        if (i != -1 && (i = text.indexOf(QLatin1Char(':'), i)) != -1) {
            modelName = text.mid(i + 1, text.indexOf(QLatin1Char('\n'), i + 1) - i - 1).trimmed();
            modelName = modelName.replace(QRegExp(QLatin1String("\\s{2,}")), QLatin1String(" "));
        }
    }

    return modelName;
}

void
ut_qtcontacts_trackerplugin_common::testHostInfo()
{
    const QString username = QString::fromLocal8Bit(qgetenv("USER"));

    QTextStream(stdout) << "package=" << PACKAGE << '\n'
                        << "version=" << VERSION << '\n'
                        << "hostname=" << QHostInfo::localHostName() << '\n'
                        << "username=" << username << '\n'
                        << "cpumodel=" << findCPUModel() << '\n';
}

void
ut_qtcontacts_trackerplugin_common::cleanup()
{
    if (m_engine) {
        m_localIds.removeAll(0);
        QctResourceIriResolver resolver(m_localIds);

        if (resolver.lookupAndWait()) {
            ResourceCleanser(QSet<QString>::fromList(resolver.resourceIris())).run();
        }

        m_localIds.clear();
    }

    resetEngine();
    resetSettings();
}

// FIXME: remove again once QtMobility provides more verbose contact validation utilities
static bool validateContact(QContactTrackerEngine *engine, const QContact &contact,
                            QContactManager::Error &error, QString &what)
{
    QList<QString> uniqueDefinitionIds;

    // check that each detail conforms to its definition as supported by this manager.
    for (int i=0; i < contact.details().count(); i++) {
        const QContactDetail& d = contact.details().at(i);
        QVariantMap values = d.variantValues();
        QContactManager::Error detailError = QContactManager::NoError;
        QContactDetailDefinition def = engine->detailDefinition(d.definitionName(),
                                                                contact.type(), &detailError);

        if (detailError != QContactManager::NoError) {
            error = detailError;
            what = (QString::fromLatin1("%1: Detail lookup error %2.").
                    arg(d.definitionName(), QString::number(detailError)));
            return false;
        }

        // check that the definition is supported
        if (def.isEmpty()) {
            error = QContactManager::InvalidDetailError;
            what = (QString::fromLatin1("%1: Unsupported detail definition.").
                    arg(d.definitionName()));
            return false;
        }

        // check uniqueness
        if (def.isUnique()) {
            if (uniqueDefinitionIds.contains(def.name())) {
                error = QContactManager::AlreadyExistsError;
                what = (QString::fromLatin1("%1: Unique detail, only one instance permitted.").
                        arg(d.definitionName()));
                return false;
            }
            uniqueDefinitionIds.append(def.name());
        }

        QList<QString> keys = values.keys();
        for (int i=0; i < keys.count(); i++) {
            const QString& key = keys.at(i);

            if (key == QContactDetail::FieldDetailUri) {
                continue;
            }

            // check that no values exist for nonexistent fields.
            if (!def.fields().contains(key)) {
                error = QContactManager::InvalidDetailError;
                what = (QString::fromLatin1("%1: Value for unknown %2 field.").
                        arg(d.definitionName(), key));
                return false;
            }

            QContactDetailFieldDefinition field = def.fields().value(key);
            // check that the type of each value corresponds to the allowable field type
            if (static_cast<int>(field.dataType()) != values.value(key).userType()) {
                error = QContactManager::InvalidDetailError;
                what = (QString::fromLatin1("%1: Field %2 expects data of %3 type.").
                        arg(d.definitionName(), key,
                            QString::fromLatin1(QVariant::typeToName(field.dataType()))));
                return false;
            }

            // check that the value is allowable
            // if the allowable values is an empty list, any are allowed.
            if (!field.allowableValues().isEmpty()) {
                // if the field datatype is a list, check that it contains only allowable values
                if (field.dataType() == QVariant::List || field.dataType() == QVariant::StringList) {
                    QList<QVariant> innerValues = values.value(key).toList();
                    for (int i = 0; i < innerValues.size(); i++) {
                        if (!field.allowableValues().contains(innerValues.at(i))) {
                            error = QContactManager::InvalidDetailError;
                            what = (QString::fromLatin1("%1: Invalid value for %2 field: %3.").
                                    arg(d.definitionName(), key, innerValues.at(i).toString()));
                            return false;
                        }
                    }
                } else if (!field.allowableValues().contains(values.value(key))) {
                    // the datatype is not a list; the value wasn't allowed.
                    error = QContactManager::InvalidDetailError;
                    what = (QString::fromLatin1("%1: Invalid value for %2 field: %3.").
                            arg(d.definitionName(), key, values.value(key).toString()));
                    return false;
                }
            }
        }
    }

    return true;
}

// FIXME: remove again once QtMobility provides more verbose relationship validation utilities
static bool
validateRelationship(QContactTrackerEngine */*manager*/,
                     const QContactRelationship &/*relationship*/,
                     QContactManager::Error &/*error*/, QString &/*what*/)
{
    //FIXME: do some useful validation here
    return true;
}

void
ut_qtcontacts_trackerplugin_common::saveContact(QContact &contact)
{
    QList<QContact> contactList;
    contactList.append(contact);

    saveContacts(contactList);

    QCOMPARE(contactList.count(), 1);
    contact = contactList[0];
}

void
ut_qtcontacts_trackerplugin_common::saveContacts(QList<QContact> &contacts)
{
    QVERIFY(not contacts.isEmpty());

    foreach(const QContact &contact, contacts) {
        QContactManager::Error error;
        QString what;

        if (not validateContact(engine(), contact, error, what)) {
            foreach(const QContactDetail &d, contact.details()) {
                qDebug() << d.definitionName() << d.variantValues();
            }

            QFAIL(qPrintable(QString::fromLatin1("error %1: %2").arg(error).arg(what)));
        }
    }

    // add the contact to database
    QContactSaveRequest request;
    request.setContacts(contacts);
    QVERIFY(engine()->startRequest(&request));

    if (m_verbose) {
        qDebug() << "saving" << request.contacts().count() << "contact(s)";
    }

    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    // verify the contact got saved
    QVERIFY(request.isFinished());
    QCOMPARE(request.error(), QContactManager::NoError);

    // copy back the saved contacts
    contacts = request.contacts();

    // remember the local id so that we can remove the contact from database later
    foreach(const QContact &contact, contacts) {
        QVERIFY(contact.localId());
        m_localIds.append(contact.localId());
    }
}

void
ut_qtcontacts_trackerplugin_common::fetchContact(const QContactLocalId &id,
                                                 QContact &result)
{
    QList<QContact> contactList;
    fetchContacts(QList<QContactLocalId>() << id, contactList);
    QCOMPARE(contactList.count(), 1);
    result = contactList[0];
}

void
ut_qtcontacts_trackerplugin_common::fetchContact(const QContactFilter &filter,
                                                 QContact &result)
{
    QList<QContact> contactList;
    fetchContacts(filter, contactList);
    QCOMPARE(contactList.count(), 1);
    result = contactList[0];
}

void
ut_qtcontacts_trackerplugin_common::fetchContacts(const QList<QContactLocalId> &ids,
                                                  QList<QContact> &result)
{
    QContactFetchByIdRequest request;

    request.setLocalIds(ids);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.isFinished());
    result = request.contacts();
}

void
ut_qtcontacts_trackerplugin_common::fetchContacts(const QContactFilter &filter,
                                                  QList<QContact> &result)
{
    fetchContacts(filter, NoSortOrders, result);
}

void
ut_qtcontacts_trackerplugin_common::fetchContacts(const QContactFilter &filter,
                                                  const QList<QContactSortOrder> &sorting,
                                                  QList<QContact> &result)
{
    QContactFetchRequest request;

    if (QContactFilter::InvalidFilter != filter.type())
        request.setFilter(filter);
    request.setSorting(sorting);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.isFinished());
    result = request.contacts();
}

void
ut_qtcontacts_trackerplugin_common::fetchContactLocalId(const QContactFilter &filter,
                                                        QContactLocalId &result)
{
    QList<QContactLocalId> contactLocalIdList;
    fetchContactLocalIds(filter, contactLocalIdList);
    QCOMPARE(contactLocalIdList.count(), 1);
    result = contactLocalIdList[0];
}

void
ut_qtcontacts_trackerplugin_common::fetchContactLocalIds(const QContactFilter &filter,
                                                         QList<QContactLocalId> &result)
{
    fetchContactLocalIds(filter, NoSortOrders, result);
}

void
ut_qtcontacts_trackerplugin_common::fetchContactLocalIds(const QContactFilter &filter,
                                                         const QList<QContactSortOrder> &sorting,
                                                         QList<QContactLocalId> &result)
{
    QContactLocalIdFetchRequest request;

    if (QContactFilter::InvalidFilter != filter.type())
        request.setFilter(filter);
    request.setSorting(sorting);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.isFinished());
    result = request.ids();
}

void
ut_qtcontacts_trackerplugin_common::saveRelationship(const QContactRelationship &relationship)
{
    QList<QContactRelationship> relationshipList;
    relationshipList.append(relationship);

    saveRelationships(relationshipList);
}

void
ut_qtcontacts_trackerplugin_common::saveRelationships(const QList<QContactRelationship> &relationships)
{
    QVERIFY(not relationships.isEmpty());

    foreach(const QContactRelationship &relationship, relationships) {
        QContactManager::Error error;
        QString what;

        if (not validateRelationship(m_engine, relationship, error, what)) {
            qDebug() << relationship;

            QFAIL(qPrintable(QString::fromLatin1("error %1: %2").arg(error).arg(what)));
        }
    }

    // add the relationship to database
    QContactRelationshipSaveRequest request;
    request.setRelationships(relationships);
    QVERIFY(engine()->startRequest(&request));

    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    // verify the relationship got saved
    QVERIFY(request.isFinished());

    QCOMPARE((int) request.error(),
             (int) QContactManager::NoError);

    // no need to note the relationships stored, should be removed automatically with the contacts in tracker
}

void
ut_qtcontacts_trackerplugin_common::fetchRelationship(const QContactId &firstId,
                                                      const QString &relationshipType,
                                                      const QContactId &secondId,
                                                      QContactRelationship &result)
{
    // fetch relationship from database
    QContactRelationshipFetchRequest request;
    request.setRelationshipType(relationshipType);
    request.setFirst(firstId);
    request.setSecond(secondId);

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.isFinished());

    QCOMPARE((int) request.error(),
             (int) QContactManager::NoError);

    const QList<QContactRelationship> relationships = request.relationships();
    QCOMPARE(relationships.count(), 1);
    result = relationships[0];
}


void
ut_qtcontacts_trackerplugin_common::fetchRelationships(const QString &relationshipType,
                                                       const QContactId &participantId,
                                                       QContactRelationship::Role role,
                                                       QList<QContactRelationship> &result)
{
    if (role == QContactRelationship::Either && participantId != QContactId()) {
        QList<QContactRelationship> firstResult;
        fetchRelationships(relationshipType, participantId,
                           QContactRelationship::First,
                           firstResult);
        CHECK_CURRENT_TEST_FAILED;

        QList<QContactRelationship> secondResult;
        fetchRelationships(relationshipType, participantId,
                           QContactRelationship::Second,
                           secondResult);
        CHECK_CURRENT_TEST_FAILED;

        result = firstResult + secondResult;
        return;
    }

    QContactRelationshipFetchRequest request;

    request.setRelationshipType(relationshipType);

    switch(role) {
    case QContactRelationship::First:
        request.setFirst(participantId);
        break;
    case QContactRelationship::Second:
        request.setSecond(participantId);
        break;
    case QContactRelationship::Either:
        break;
    }

    QVERIFY(engine()->startRequest(&request));
    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    QVERIFY(request.isFinished());

    QCOMPARE((int) request.error(),
             (int) QContactManager::NoError);

    result = request.relationships();
}

void
ut_qtcontacts_trackerplugin_common::removeRelationship(const QContactRelationship &relationship)
{
    QList<QContactRelationship> relationshipList;
    relationshipList.append(relationship);

    removeRelationships(relationshipList);
}

void
ut_qtcontacts_trackerplugin_common::removeRelationships(const QList<QContactRelationship> &relationships)
{
    QVERIFY(not relationships.isEmpty());

    foreach(const QContactRelationship &relationship, relationships) {
        QContactManager::Error error;
        QString what;

        if (not validateRelationship(m_engine, relationship, error, what)) {
            qDebug() << relationship;

            QFAIL(qPrintable(QString::fromLatin1("error %1: %2").arg(error).arg(what)));
        }
    }

    // remove the relationship from database
    QContactRelationshipRemoveRequest request;
    request.setRelationships(relationships);
    QVERIFY(engine()->startRequest(&request));

    QVERIFY(engine()->waitForRequestFinishedImpl(&request, 0));

    // verify the relationship got removed
    QVERIFY(request.isFinished());

    QCOMPARE((int) request.error(),
             (int) QContactManager::NoError);
}

uint
ut_qtcontacts_trackerplugin_common::insertIMContact(const QString &contactIri, const QString &imId,
                                                    const QString &imPresence, const QString &statusMessage,
                                                    const QString &accountPath, const QString &protocol,
                                                    const QString &serviceProvider,
                                                    const QString &nameGiven, const QString &nameFamily)
{
    const QString queryString = qStringArg(QString::fromLatin1
            ("INSERT {\n"
             "  _:_ a nfo:FileDataObject; nie:url 'file://%10'\n"
             "}\n"
             "WHERE {\n"
             "  FILTER(NOT EXISTS {\n"
             "    ?resource a nfo:FileDataObject;\n"
             "    nie:url 'file://%10' .\n"
             "  })\n"
             "}\n"

             "DELETE {\n"
             "  <telepathy:%3!%2> a nco:IMAddress, nie:InformationElement\n"
             "}\n"

             "INSERT {\n"
             "  GRAPH <telepathy:%3!%2> {\n"
             "    <telepathy:%3!%2> a nco:IMAddress, nie:InformationElement;\n"
             "      nco:imID '%2';\n"
             "      nco:imNickname '%8%9';\n"
             "      nco:imPresence %4;\n"
             "      nco:imStatusMessage '%5';\n"
             "      nco:imCapability nco:im-capability-text-chat, nco:im-capability-audio-calls;\n"
             "      nco:imProtocol '%6';\n"
             "      nco:imAvatar ?file;\n"
             "      nco:presenceLastModified '%11'^^xsd:dateTime.\n"
             "    <telepathy:%3> a nco:IMAccount;\n"
             "      nco:imDisplayName '%7';\n"
             "      nco:hasIMContact <telepathy:%3!%2>.\n"
             "  }\n"
             "}\n"
             "WHERE {\n"
             "  ?file a nfo:FileDataObject; \n"
             "  nie:url 'file://%10'\n ."
             "}\n"

             // insert into AB graph
             "INSERT INTO <%12> {\n"
             "  <%1> a nco:PersonContact;\n"
             "    nie:contentCreated '%11';\n"
             "    nie:generator 'telepathy'.\n"
             "}\n"

             // insert into private TP graph
             "INSERT INTO <%13> {\n"
             "  <affiliation:%3!%2> a nco:Affiliation;\n"
             "    rdfs:label 'Other';\n"
             "    nco:hasIMAddress <telepathy:%3!%2>.\n"
             "  <%1>\n"
             "    nco:hasAffiliation <affiliation:%3!%2>.\n"
             "}\n"

             // insert into graph of IM address
             "INSERT INTO <telepathy:%3!%2> {\n"
             "  <%1>\n"
             "    nco:nameGiven '%8';\n"
             "    nco:nameFamily '%9'.\n"
             "}\n"), QStringList()
            << contactIri << imId << accountPath << imPresence << statusMessage << protocol
            << serviceProvider << nameGiven << nameFamily << onlineAvatarPath(accountPath)
            << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
            << QtContactsTrackerDefaultGraphIri
            << QtContactsTrackerTelepathyGraphIri);

    QScopedPointer<QSparqlResult> result(executeQuery(queryString, QSparqlQuery::InsertStatement));

    if (result.isNull()) {
        return 0;
    }

    return resolveTrackerId(contactIri);
}

QSet<QString>
ut_qtcontacts_trackerplugin_common::findTestSlotNames()
{
    QSet<QString> testSlots;

    for(int i = 0; i < metaObject()->methodCount(); ++i) {
        const QMetaMethod &method = metaObject()->method(i);

        if (QMetaMethod::Private != method.access() ||
            QMetaMethod::Slot != method.methodType()) {
            continue;
        }

        const char *signature = method.signature();
        const char *parenthesis = strchr(signature, '(');

        if (0 != qstrcmp(parenthesis, "()")) {
            continue;
        }

        testSlots.insert(QString::fromLatin1(signature, parenthesis - signature));
    }

    return testSlots;
}

QList<QContact>
ut_qtcontacts_trackerplugin_common::parseVCards(const QString &fileName, int limit)
{
    QFile file(fileName);

    if (not file.open(QFile::ReadOnly)) {
        qWarning("Cannot open %s: %s",
                 qPrintable(file.fileName()),
                 qPrintable(file.errorString()));

        return QList<QContact>();
    }

    return parseVCards(file.readAll(), limit);
}

QList<QContact>
ut_qtcontacts_trackerplugin_common::parseVCards(const QByteArray &vcardData, int limit)
{
    if (limit != INT_MAX) {
        int offset = 0;

        for(int i = 0; i < limit; ++i) {
            static const char endToken[] = "END:VCARD";
            int end = vcardData.indexOf(endToken, offset);

            if (-1 == end) {
                break;
            }

            offset = end + sizeof(endToken) - 1;
        }

        return parseVCards(vcardData.left(offset));
    }

    QVersitReader reader(vcardData);

    if (not reader.startReading()) {
        qWarning() << "Starting to read vCards failed:" << reader.error();
        return QList<QContact>();
    }

    if (not reader.waitForFinished() || QVersitReader::NoError != reader.error()) {
        qWarning() << "Reading vCards failed:" << reader.error();
        return QList<QContact>();
    }

    QList<QVersitDocument> documents = reader.results();

    while(documents.count() > limit) {
        documents.removeLast();
    }

    QVersitContactImporter importer;

    if (not importer.importDocuments(documents)) {
        qWarning() << "Importing vCards failed:" << importer.errors();
        return QList<QContact>();
    }

    return importer.contacts();
}


QString
ut_qtcontacts_trackerplugin_common::referenceFileName(const QString &fileName)
{
    QString path(QDir(QLatin1String("data")).absoluteFilePath(fileName));

    if (not QFile::exists(path)) {
        path = QDir(m_srcDir.filePath(QLatin1String("data"))).filePath(fileName);
    }

    if (not QFile::exists(path)) {
        path = m_dataDir.filePath(fileName);
    }

    return path;
}

QString
ut_qtcontacts_trackerplugin_common::loadReferenceFile(const QString &fileName)
{
    QFile referenceFile(referenceFileName(fileName));

    if (not referenceFile.open(QFile::ReadOnly)) {
        qWarning() << referenceFile.fileName() << ":" << referenceFile.errorString();
        return QString();
    }

    return QString::fromLocal8Bit(referenceFile.readAll());
}

static QVariant::Type
referenceFieldDataType(const QString &definitionName, const QContactDetailDefinition &definition)
{
    if (definition.fields().contains(definitionName)) {
        return definition.fields().value(definitionName).dataType();
    }

    if (QContactType::FieldLinkedDetailUris == definitionName) {
        return QVariant::StringList;
    }

    return QVariant::String;
}

static bool
loadReferenceField(const QDomElement &element, QContactDetail *detail,
                   const QContactDetailDefinition &definition = QContactDetailDefinition())
{
    const QVariant::Type dataType = referenceFieldDataType(element.tagName(), definition);

    QVariant value = element.text();

    if (QVariant::StringList == dataType) {
        value = value.toString().split(QLatin1Char(';'), QString::SkipEmptyParts);
    } else if (not value.convert(dataType)) {
        const QString typeName = QLatin1String(QVariant::typeToName(dataType));
        if (QContactDetail::FieldLinkedDetailUris == element.tagName()) { qDebug() << "TYPE!" << typeName; }
        qctWarn(QString::fromLatin1("Cannot convert %1 detail to %2").
                arg(detail->definitionName(), typeName));
        return false;
    }

    return detail->setValue(element.tagName(), value), true;
}

static bool
loadReferenceDetail(const QDomElement &detailElement, QContactDetail *detail,
                    const QContactDetailDefinition &definition = QContactDetailDefinition())
{
    for(QDomElement fieldElement = detailElement.firstChildElement();
        not fieldElement.isNull(); fieldElement = fieldElement.nextSiblingElement()) {

        if (not loadReferenceField(fieldElement, detail, definition)) {
            return false;
        }
    }

    if (detail->isEmpty()) {
        return loadReferenceField(detailElement, detail, definition);
    }

    return false;
}

static QContactType
contactType(QDomElement &contact)
{
    for(QDomElement element = contact.firstChildElement();
        not element.isNull(); element = element.nextSiblingElement()) {
        if (element.tagName() == QContactType::DefinitionName) {
            QContactType detail;
            loadReferenceDetail(element, &detail);
            contact.removeChild(element);
            return detail;
        }
    }

    QContactType detail;
    detail.setType(QContactType::TypeContact);
    return detail;
}

QList<QContact>
ut_qtcontacts_trackerplugin_common::loadReferenceContacts(ReferenceContactMode mode,
                                                          const QString &fileName)
{
    QFile file(referenceFileName(fileName));

    if (not file.open(QFile::ReadOnly)) {
        qctWarn(file.errorString());
        return QList<QContact>();
    }

    int errorLine, errorColumn;
    QString documentError;
    QDomDocument document;

    if (not document.setContent(&file, &documentError, &errorLine, &errorColumn)) {
        qctWarn(QString::fromLatin1("line %1, column %2: %3").
                arg(QString::number(errorLine), QString::number(errorColumn),
                    documentError));
        return QList<QContact>();
    }

    QContactId contactId;
    contactId.setManagerUri(engine()->managerUri());

    QMap<QString, int> subjects;
    QList<QContact> contacts;

    for(QDomElement contactElement = document.documentElement().firstChildElement();
        not contactElement.isNull(); contactElement = contactElement.nextSiblingElement()) {

        if (contactElement.tagName() != QLatin1String("Contact")) {
            continue;
        }

        // Add an iri attribute at the Contact element to resolve the contact's local tracker id.
        const QString contactIri = contactElement.attribute(QLatin1String("iri"));

        if (not contactIri.isEmpty()) {
            subjects.insert(contactIri, contacts.count());
        }

        contacts += QContact();
        contacts.last().setType(contactType(contactElement));

        const QTrackerContactDetailSchema &schema = engine()->schema(contacts.last().type());
        const QContactDetailDefinitionMap &definitions = schema.detailDefinitions();

        for(QDomElement detailElement = contactElement.firstChildElement();
            not detailElement.isNull(); detailElement = detailElement.nextSiblingElement()) {
            QContactDetail detail = QContactDetail(detailElement.tagName());
            loadReferenceDetail(detailElement, &detail, definitions.value(detail.definitionName()));

            if (QContactDisplayLabel::DefinitionName == detail.definitionName()) {
                const QString label = detail.value(QContactDisplayLabel::FieldLabel);
                QContactManagerEngine::setContactDisplayLabel(&contacts.last(), label);
            } else {
                contacts.last().saveDetail(&detail), qPrintable(detail.definitionName());
            }
        }

        // Add an iri attribute at the Contact element to resolve the contact's local tracker id.
        if (mode == GenerateReferenceContactIds) {
            contactId.setLocalId(contactId.localId() + 1);
            contacts.last().setId(contactId);
        }
    }

    // apply real contact ids if applyable
    if (mode == ResolveReferenceContactIris && not subjects.isEmpty()) {
        QctTrackerIdResolver resolver(subjects.keys());

        if (not resolver.lookupAndWait()) {
            qctWarn("Cannot resolve local ids of the reference contacts");
            return QList<QContact>();
        }

        for(int i = 0; i < subjects.count(); ++i) {
            const QString &contactIri = resolver.resourceIris().at(i);
            contactId.setLocalId(resolver.trackerIds().at(i));

            if (0 == contactId.localId()) {
                qctWarn(QString::fromLatin1("Cannot resolve local contact id for %1").
                        arg(contactIri));
                continue;
            }

            contacts[subjects.value(contactIri)].setId(contactId);
        }
    }

    return contacts;
}

QSparqlResult *
ut_qtcontacts_trackerplugin_common::executeQuery(const QString &queryString,
                                                 QSparqlQuery::StatementType type) const
{
    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn("No QtSparql connection available.");
        return 0;
    }

    const QSparqlQuery query(queryString, type);
    QScopedPointer<QSparqlResult> result(connection.syncExec(query));

    if (result->hasError()) {
        qctWarn(result->lastError().message());
        return 0;
    }

    return result.take();
}

QStringList
ut_qtcontacts_trackerplugin_common::extractResourceIris(const QString &text)
{
    // extract resource IRIs
    QRegExp iriPattern(QLatin1String("<([a-z]+:[^>]+[^#])>"));
    QStringList resourceIris;

    for(int i = 0;; i += iriPattern.matchedLength()) {
        if (-1 == (i = text.indexOf(iriPattern, i))) {
            break;
        }

        const QString iri = iriPattern.capturedTexts().at(1);

        // simulate set, but preserve order
        if (not resourceIris.contains(iri)) {
            resourceIris.append(iri);
        }
    }

    return resourceIris;
}

QStringList
ut_qtcontacts_trackerplugin_common::loadRawTuples(const QString &fileName)
{
    QSparqlConnection &connection = QctSparqlConnectionManager::defaultConnection();

    if (not connection.isValid()) {
        qctWarn("No QtSparql connection available.");
        return QStringList();
    }

    QString rawTuples = loadReferenceFile(fileName);
    const QStringList resourceIris = extractResourceIris(rawTuples);

    // process the turtle file's prefix directives
    QRegExp prefixPattern(QLatin1String("@prefix\\s+(\\w+):\\s+<([^>]+)>\\s*\\."));

    for(int i; -1 != (i = rawTuples.indexOf(prefixPattern)); ) {
        connection.addPrefix(prefixPattern.cap(1), prefixPattern.cap(2));
        rawTuples.remove(i, prefixPattern.matchedLength());
    }

    // cleaning resources found in the tuple file
    ResourceCleanser(resourceIris.toSet()).run();

    // run INSERT query to load the tuples
    const QString queryString = QString::fromLatin1("INSERT { %1 }").arg(rawTuples);
    const QSparqlQuery query(queryString, QSparqlQuery::InsertStatement);
    const QScopedPointer<QSparqlResult> result(connection.syncExec(query));

    if (result->hasError()) {
        qctWarn(result->lastError().message());
    }

    return resourceIris;
}

QStringList
ut_qtcontacts_trackerplugin_common::loadRawTuples(const QString &fileName,
                                                  const QRegExp &subjectFilter)
{
    return loadRawTuples(fileName).filter(subjectFilter);
}

QStringList
ut_qtcontacts_trackerplugin_common::loadRawContacts(const QString &fileName)
{
    const QRegExp hasContactIri(QLatin1String("^contact(group)?:"));
    return loadRawTuples(fileName, hasContactIri);
}

static bool
isExpectedDetail(const QContactDetail &candidate, const QContactDetail &reference)
{
    if (candidate.definitionName() != reference.definitionName()) {
        return false;
    }

    const QVariantMap candidateValues = candidate.variantValues();
    QVariantMap referenceValues = reference.variantValues();

    for(QVariantMap::ConstIterator ci =
        candidateValues.constBegin(); ci != candidateValues.constEnd(); ++ci) {
        QVariantMap::Iterator ri = referenceValues.find(ci.key());

        if (ri == referenceValues.constEnd() || ci->type() != ri->type()) {
            return false;
        }

        if (QVariant::StringList == ci->type()) {
            if (ci->toStringList().toSet() != ri->toStringList().toSet()) {
                return false;
            }
        } else if (*ci != *ri) {
            return false;
        }

        ri = referenceValues.erase(ri);
    }

    return referenceValues.isEmpty();
}

template<class T> static QString
debugString(const T &item)
{
    QString result;
    QDebug(&result) << item;
    return result;
}

void
ut_qtcontacts_trackerplugin_common::verifyContacts(QList<QContact> candidateContacts,
                                                   QList<QContact> referenceContacts)
{
    QVERIFY(not referenceContacts.isEmpty());

    for(int i = 0; i < candidateContacts.count(); ++i) {
        const QContact &candidate = candidateContacts.at(i);
        QList<QContact>::Iterator reference;

        for(reference = referenceContacts.begin(); reference != referenceContacts.end(); ++reference) {
            if (reference->id() == candidate.id()) {
                break;
            }
        }

        QVERIFY2(reference != referenceContacts.end(),
                 qPrintable(QString::fromLatin1("Contact #%1, localId=%2: Unexpected contact").
                            arg(QString::number(i), QString::number(candidate.localId()))));

        if (m_verbose) {
            qDebug() << "candidate" << candidate;
            qDebug() << "reference" << *reference;
        }

        QList<QContactDetail> referenceDetailList = reference->details();

        foreach(const QContactDetail& candidateDetail, candidate.details()) {
            QList<QContactDetail>::Iterator referenceDetail;

            for(referenceDetail = referenceDetailList.begin();
                referenceDetail != referenceDetailList.end(); ++referenceDetail) {
                if (isExpectedDetail(candidateDetail, *referenceDetail)) {
                    break;
                }
            }

            QVERIFY2(referenceDetail != referenceDetailList.end(),
                     qPrintable(QString::fromLatin1("Contact #%1, localId=%2: "
                                                    "Unexpected detail: %3, reference details: %4").
                                arg(QString::number(i), QString::number(candidate.localId()),
                                    debugString(candidateDetail),
                                    debugString(reference->details(candidateDetail.definitionName())))));

            referenceDetailList.erase(referenceDetail);
        }

        QVERIFY2(referenceDetailList.isEmpty(),
                 qPrintable(QString::fromLatin1("Contact #%1, localId=%2: Details missing: %3").
                            arg(QString::number(i), QString::number(candidate.localId()),
                                QStringList(definitionNames(referenceDetailList).toList()).
                                join(QLatin1String(", ")))));

        referenceContacts.erase(reference);
    }

    if (not referenceContacts.isEmpty()) {
        qDebug() << "unmatched contacts:" << referenceContacts;
    }

    QVERIFY(referenceContacts.isEmpty());
}

void
ut_qtcontacts_trackerplugin_common::verifyContacts(const QList<QContact> &candidateContacts,
                                                   const QString &referenceFileName)
{
    const QList<QContact> reference = loadReferenceContacts(ResolveReferenceContactIris,
                                                            referenceFileName);
    QVERIFY2(not reference.isEmpty(), qPrintable(referenceFileName));
    verifyContacts(candidateContacts, reference);
}

QString
ut_qtcontacts_trackerplugin_common::qStringArg(const QString &format, const QStringList &args)
{
    static const QRegExp tokenExp = QRegExp(QLatin1String("%\\d+"));
    static const QString join = QString::fromLatin1("");

    const int nArgs = args.size();
    int pos = 0;
    int lastPos = 0;
    QStringList tokens;

    while ((pos = tokenExp.indexIn(format, pos)) != -1) {
        const int matchedLength = tokenExp.matchedLength();

        QString arg = QString::fromRawData(format.constData() + pos + 1, matchedLength - 1);

        bool ok = false;
        const int intVal = arg.toInt(&ok);

        if (not ok) {
            continue;
        }

        if (intVal < 1 || intVal > nArgs) {
            qctWarn(QString::fromLatin1("Invalid argument %%1, skipping").arg(arg));
            continue;
        }

        tokens.append(QString::fromRawData(format.constData() + lastPos, pos - lastPos));

        pos += matchedLength;
        lastPos = pos;

        tokens.append(args.at(intVal - 1));
    }

    tokens.append(QString::fromRawData(format.constData() + lastPos, format.size() - lastPos));

    return tokens.join(join);
}

QVariant
ut_qtcontacts_trackerplugin_common::changeSetting(const QString &key, const QVariant &value)
{
    QctSettings *const settings = QctThreadLocalData::instance()->settings();
    const QVariant oldValue = settings->value(key);
    settings->setValue(key, value);

    if (not m_oldSettings.contains(key)) {
        m_oldSettings.insert(key, oldValue);
    }

    return oldValue;
}

void
ut_qtcontacts_trackerplugin_common::resetSettings()
{
    QctSettings *const settings = QctThreadLocalData::instance()->settings();

    for(QVariantHash::ConstIterator it = m_oldSettings.constBegin(); it != m_oldSettings.constEnd(); ++it) {
        settings->setValue(it.key(), it.value());
    }

    m_oldSettings.clear();
}
