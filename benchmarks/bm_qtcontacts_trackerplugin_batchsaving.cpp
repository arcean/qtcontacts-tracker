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

#include <QtCore>

#include <qtcontacts.h>

QTM_USE_NAMESPACE

static const unsigned int DEFAULT_CONTACTS = 1000;
static const unsigned int DEFAULT_GROUPCONTACTS = 0;
static const unsigned int DEFAULT_CONTACTSPERGROUP = 50;
static const unsigned int DEFAULT_BATCH_SIZE = 100;

static const char isGroupPropertyId[] = "isGroup";

class Benchmark : public QObject
{
    Q_OBJECT

public:
    Benchmark(unsigned int contactsCount, unsigned int groupsCount,
              unsigned int contactsPerGroupCount, unsigned int bsizeCount, bool fullDetails,
              bool overwriteContacts, const QStringList &detailMask, QObject *parent = 0)
        : QObject(parent)
        , m_contactCount(contactsCount)
        , m_groupsCount(groupsCount)
        , m_contactsPerGroupCount(contactsPerGroupCount)
        , m_batchSize(bsizeCount)
        , m_full(fullDetails)
        , m_overwrite(overwriteContacts)
        , m_detailMask(detailMask)
    {
        QMap<QString, QString> params;
        params[QLatin1String("debug")] = QLatin1String("no-nagging");

        const QString managerName = QLatin1String("tracker");
        m_manager = new QContactManager(managerName, params);
        Q_ASSERT(0 != m_manager);

        qDebug() << "Contact manager created:" << m_manager->managerUri();

        if (m_manager->managerName() != managerName) {
            qFatal("Unexpected contact manager. Aborting.");
        }

        if (not m_detailMask.isEmpty()) {
            const QStringList supportedDetails = m_manager->detailDefinitions().keys();
            foreach (const QString &detail, m_detailMask) {
                if (not supportedDetails.contains(detail)) {
                    qDebug()<< "Unsupported detail" << detail << "in overwrite mask. Aborting.";
                    exit(2);
                }
            }
        }
    }

    virtual ~Benchmark()
    {
        delete m_manager;
    }

    void performBatch()
    {
        uint todo = m_contactCount;

        do {
            const uint bsize = (todo > m_batchSize) ? m_batchSize : todo;
            const bool createNewContacts = (m_contacts.isEmpty() || not m_overwrite);

            qDebug() << "Perform batch add:" << bsize << "contacts";
            QList<QContact> contacts = createNewContacts ? generateContacts(bsize) : m_contacts;

            QContactSaveRequest request;
            request.setContacts(contacts);
            request.setManager(m_manager);
            if (not createNewContacts) {
                request.setDefinitionMask(m_detailMask);
            }

            qDebug() << &request << bsize << todo;

            connect(&request, SIGNAL(resultsAvailable()),
                    this, SLOT(onContactSaveRequestResultAvailable()));
            connect(&request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
                    this, SLOT(onStateChanged(QContactAbstractRequest::State)));

            qDebug() << "Before start";
            request.start();

            request.waitForFinished();
            qDebug() << "Batch finished.";

            todo -= bsize;
            if (createNewContacts) {
                m_contacts += request.contacts();
            }
        } while (0 < todo);
    }

    void performGroupBatch()
    {
        uint todo = m_groupsCount;

        do {
            const uint bsize = (todo > m_batchSize) ? m_batchSize : todo;
            const bool createNewGroups = (m_groups.isEmpty() || not m_overwrite);

            qDebug() << "Perform batch add:" << bsize << "groups";
            QList<QContact> groups = createNewGroups ? generateGroups(bsize) : m_groups;

            QContactSaveRequest request;
            request.setProperty(isGroupPropertyId, QVariant(true));
            request.setContacts(groups);
            request.setManager(m_manager);

            qDebug() << &request << bsize << todo;

            connect(&request, SIGNAL(resultsAvailable()),
                    this, SLOT(onContactSaveRequestResultAvailable()));
            connect(&request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
                    this, SLOT(onStateChanged(QContactAbstractRequest::State)));

            qDebug() << "Before Group start";
            request.start();

            request.waitForFinished();
            qDebug() << "Group Batch finished.";

            todo -= bsize;

            if (createNewGroups) {
                m_groups += request.contacts();
            }
        } while (0 < todo);
    }

    void performHasMemberBatch()
    {
        // don't also do overwriting, just store the relationships for the existing groups and contacts
        unsigned int groupsCount = (m_overwrite ? qMin(m_groupsCount,m_batchSize) : m_groupsCount);
        unsigned int todo = groupsCount * m_contactsPerGroupCount;
        unsigned int bsize;

         while (0 < todo) {
            bsize = (todo > m_batchSize) ? m_batchSize : todo;

            qDebug() << "Perform batch add:" << bsize << "relationships";
            QContactRelationshipSaveRequest request;
            request.setRelationships(generateRelationships(bsize));
            request.setManager(m_manager);
            qDebug() << &request << bsize << todo;

            connect(&request, SIGNAL(resultsAvailable()),
                    this, SLOT(onRelationshipSaveRequestResultAvailable()));
            connect(&request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
                    this, SLOT(onStateChanged(QContactAbstractRequest::State)));

            qDebug() << "Before Relationship start";
            request.start();

            request.waitForFinished();
            qDebug() << "Relationship Batch finished.";

            todo -= bsize;

            m_relationships += request.relationships();
        };
    }

    void performCleanup()
    {
        QList<QContactLocalId> localIds;

        foreach(const QContact &c, m_groups) {
            if (0 != c.localId()) {
                localIds += c.localId();
            }
        }

        if (not localIds.isEmpty()) {
            qDebug() << "Removing groups:" << localIds.size();
            m_manager->removeContacts(localIds);
        }

        localIds.clear();

        foreach(const QContact &c, m_contacts) {
            if (0 != c.localId()) {
                localIds += c.localId();
            }
        }

        if (not localIds.isEmpty()) {
            qDebug() << "Removing contacts:" << localIds.size();
            m_manager->removeContacts(localIds);
        }
    }

private slots:
    void onContactSaveRequestResultAvailable()
    {
        qDebug("Result available!!!");
        QContactSaveRequest* request = qobject_cast<QContactSaveRequest*>(sender());
        Q_ASSERT(request);

        if (request->isFinished()) {
            const bool isGroup = request->property(isGroupPropertyId).isValid();
            qDebug(isGroup ? "Saved %d groups" : "Saved %d contacts", request->contacts().count());
        }
    }

    void onRelationshipSaveRequestResultAvailable()
    {
        qDebug("Result available!!!");
        QContactRelationshipSaveRequest* request = qobject_cast<QContactRelationshipSaveRequest*>(sender());
        Q_ASSERT(request);

        if (request->isFinished()) {
            qDebug("Saved %d relationships", request->relationships().count());
        }
    }

    void onStateChanged(QContactAbstractRequest::State state)
    {
        qDebug("%s state changed!", Q_FUNC_INFO);

        QContactAbstractRequest *request = qobject_cast<QContactAbstractRequest*>(sender());

        // Make sure the request is valid
        if (m_manager != request->manager()) {
            qDebug("ERROR: INVALID REQUEST!!!!!!!!!!!!!!");
            return; // ignore these
        }

        // Print state
        switch (state) {
        case QContactAbstractRequest::InactiveState :
            qDebug("%s changed state: Inactive", Q_FUNC_INFO);
            break;
        case QContactAbstractRequest::ActiveState :
            qDebug("%s changed state: Active", Q_FUNC_INFO);
            break;
        case QContactAbstractRequest::CanceledState :
            qDebug("%s changed state: Canceled", Q_FUNC_INFO);
            break;
        case QContactAbstractRequest::FinishedState :
            qDebug("%s changed state: Finished", Q_FUNC_INFO);
            break;
        default:
            qDebug("%s UNKNOWN STATE: %d", Q_FUNC_INFO, state);
        }
    }

private:
    QList<QContact> generateContacts(unsigned int contactsCount) const
    {
        unsigned int offset = m_contacts.count();
        QList<QContact> result;

        if (contactsCount > 0) {
            qDebug("Generating %d contacts", contactsCount);
        }

        for (unsigned int i = offset; i < contactsCount + offset; ++i) {
            QContact contact;

            const bool isMale = (i % 2 == 0);
            QContactName name;
            name.setFirstName(QLatin1String("First ") + QString::number(i));
            name.setLastName(QLatin1String("Last ") + QString::number(i));
            name.setPrefix(QLatin1String(isMale ? "Mr." : "Mrs."));

            contact.saveDetail(&name);

            QContactEmailAddress email1;
            email1.setEmailAddress(QLatin1String("home") + QString::number(i) +
                                   QLatin1String("@example.com"));

            if (!i % 3) {
                email1.setContexts(i % 3 == 1 ? QContactDetail::ContextHome
                                              : QContactDetail::ContextWork);
            }

            contact.saveDetail(&email1);

            QContactPhoneNumber phone;
            phone.setNumber(QString::number(qrand()));
            contact.saveDetail(&phone);

            if (m_full) {
                QContactGender gender;
                gender.setGender(isMale ? QLatin1String(QContactGender::GenderMale)
                                        : QLatin1String(QContactGender::GenderFemale));
                contact.saveDetail(&gender);

                QContactBirthday birthday;
                birthday.setDate(QDate(1980 + i % 20, 1 + i % 10, 1 + i % 25));
                contact.saveDetail(&birthday);

                QContactAnniversary anniversary;
                anniversary.setOriginalDate(QDate(1990 + i % 15, 1 + i % 11, 2 + i % 23));
                contact.saveDetail(&anniversary);

                QContactGuid guid;
                guid.setGuid(QString::number(1 + i));
                contact.saveDetail(&guid);

                QContactNickname nick;
                nick.setNickname(QLatin1String("Covax") + QString::number(i));
                contact.saveDetail(&nick);

                QContactOrganization organization;
                organization.setAssistantName(QLatin1String("The Mighty Assistant") + QString::number(i));
                organization.setLocation(QLatin1String("Some location") + QString::number(i));
                organization.setName(QLatin1String("Organization X") + QString::number(i));
                organization.setTitle(QLatin1String("Programmer") + QString::number(i));
                organization.setDepartment(QStringList() << QLatin1String("Department Nr. ") + QString::number(i));
                contact.saveDetail(&organization);

                QContactFamily family;
                QStringList children;
                children << QLatin1String("Michael Jr.");
                children << QLatin1String("Vicky");
                family.setChildren(children);
                family.setSpouse(QLatin1String("The Mighty Spouse"));
                contact.saveDetail(&family);

                QContactUrl url;
                url.setUrl(QLatin1String("http://www.example.com/") + QString::number(i));
                contact.saveDetail(&url);

                // Home Address
                QContactAddress homeAddress;
                homeAddress.setStreet(QLatin1String("Street ") + QString::number(i));
                homeAddress.setRegion(QLatin1String("State ") + QString::number(i));
                homeAddress.setLocality(QLatin1String("City ") + QString::number(i));
                homeAddress.setCountry(QLatin1String("Country ") + QString::number(i));
                homeAddress.setPostcode(QLatin1String("Post ") + QString::number(i));
                homeAddress.setContexts(QContactDetail::ContextHome);
                contact.saveDetail(&homeAddress);

                QContactNote note;
                note.setNote(QLatin1String("Note: this is the note number ") + QString::number(i));
                contact.saveDetail(&note);
            }

            result << contact;
        }

        return result;
    }
    QList<QContact> generateGroups(unsigned int groupsCount) const
    {
        unsigned int offset = m_groups.count();
        QList<QContact> result;

        if (groupsCount > 0) {
            qDebug("Generating %d groups", groupsCount);
        }

        for (unsigned i = offset; i < groupsCount + offset; ++i) {
            QContact group;
            group.setType(QContactType::TypeGroup);

            QContactNickname nickName;
            nickName.setNickname(QLatin1String("Group ") + QString::number(i));

            group.saveDetail(&nickName);

            QContactEmailAddress email1;
            email1.setEmailAddress(QLatin1String("home") + QString::number(i) +
                                   QLatin1String("@example.com"));

            if (!i % 3) {
                email1.setContexts(i % 3 == 1 ? QContactDetail::ContextHome
                                              : QContactDetail::ContextWork);
            }

            group.saveDetail(&email1);

            QContactPhoneNumber phone;
            phone.setNumber(QString::number(qrand()));
            group.saveDetail(&phone);

            if (m_full) {
                QContactAnniversary anniversary;
                anniversary.setOriginalDate(QDate(1990 + i % 15, 1 + i % 11, 2 + i % 23));
                group.saveDetail(&anniversary);

                QContactOrganization organization;
                organization.setAssistantName(QLatin1String("The Mighty Assistant") + QString::number(i));
                organization.setLocation(QLatin1String("Some location") + QString::number(i));
                organization.setName(QLatin1String("Organization X") + QString::number(i));
                organization.setTitle(QLatin1String("Programmer") + QString::number(i));
                organization.setDepartment(QStringList() << QLatin1String("Department Nr. ") + QString::number(i));
                group.saveDetail(&organization);

                QContactUrl url;
                url.setUrl(QLatin1String("http://www.example.com/") + QString::number(i));
                group.saveDetail(&url);

                // Home Address
                QContactAddress homeAddress;
                homeAddress.setStreet(QLatin1String("Street ") + QString::number(i));
                homeAddress.setRegion(QLatin1String("State ") + QString::number(i));
                homeAddress.setLocality(QLatin1String("City ") + QString::number(i));
                homeAddress.setCountry(QLatin1String("Country ") + QString::number(i));
                homeAddress.setPostcode(QLatin1String("Post ") + QString::number(i));
                homeAddress.setContexts(QContactDetail::ContextHome);
                group.saveDetail(&homeAddress);

                QContactNote note;
                note.setNote(QLatin1String("Note: this is the note number ") + QString::number(i));
                group.saveDetail(&note);
            }

            result << group;
        }

        return result;
    }
    QList<QContactRelationship> generateRelationships(unsigned int relationshipCount) const
    {
        // Spread the relationships by iterating over the groups and contacts:
        // first group gets HasMember relationship with the first m_contactsPerGroupCount contacts,
        // second group with the next m_contactsPerGroupCount contacts, and so on.
        // If the last contact is reached, continue with first contact.
        const unsigned int contactsCount = m_contacts.count();
        QList<QContactRelationship> result;

        if (relationshipCount > 0) {
            qDebug("Generating %d relationships", relationshipCount);
        }
        unsigned int offset = m_relationships.count();
        unsigned int g = offset / m_contactsPerGroupCount;
        unsigned int cInGroup = offset % m_contactsPerGroupCount;
        unsigned int c = offset % contactsCount;

        for (unsigned i = offset; i < relationshipCount + offset; ++i) {
            const QContact &group = m_groups[g];
            const QContact &contact = m_contacts[c];
            QContactRelationship relationship;
            relationship.setRelationshipType(QContactRelationship::HasMember);
            relationship.setFirst(group.id());
            relationship.setSecond(contact.id());

            result << relationship;

            ++c;
            if (c >= contactsCount ) {
                c = 0;
            }
            ++cInGroup;
            if (cInGroup >= m_contactsPerGroupCount) {
                cInGroup = 0;
                ++g;
            }
        }
        return result;
    }

private:
    QContactManager* m_manager;
    QList<QContact> m_contacts;
    QList<QContact> m_groups;
    QList<QContactRelationship> m_relationships;
    unsigned int m_contactCount;
    unsigned int m_groupsCount;
    unsigned int m_contactsPerGroupCount;
    unsigned int m_batchSize;
    bool m_full;
    bool m_overwrite;
    QStringList m_detailMask;
};

#include "bm_qtcontacts_trackerplugin_batchsaving.moc"

static const QString contactCountOption = QString::fromLatin1("--contacts=");
static const QString groupCountOption = QString::fromLatin1("--group-count=");
static const QString contactsPerGroupCountOption = QString::fromLatin1("--contacts-per-group=");
static const QString batchSizeOption = QString::fromLatin1("--batch-size=");
static const QString overwriteDetailsOption = QString::fromLatin1("--overwrite=");
static const QLatin1String fullContactsOption = QLatin1String("--full");
static const QLatin1String overwriteContactsOption = QLatin1String("--overwrite");
static const QLatin1String cleanupAfterOption = QLatin1String("--cleanup");

int main(int argc, char* argv[])
{
    qsrand(time(0));

    QCoreApplication app(argc, argv);

    // load plugin from build directory
    const QDir appdir = QDir(app.applicationDirPath());
    const QDir topdir = QDir(appdir.relativeFilePath(QLatin1String("..")));
    app.setLibraryPaths(QStringList(topdir.absolutePath()) + app.libraryPaths());

    // parse command line arguemtns
    unsigned int contactCount = DEFAULT_CONTACTS;
    unsigned int groupCount = DEFAULT_GROUPCONTACTS;
    unsigned int contactsPerGroupCount = DEFAULT_CONTACTSPERGROUP;
    unsigned int batchSize = DEFAULT_BATCH_SIZE;
    QStringList detailMask;
    bool full = false;
    bool overwrite = false;
    bool cleanupAfter = false;

    foreach (const QString& argument, QCoreApplication::arguments()) {
        if (argument == QCoreApplication::arguments()[0]) {
            continue;
        }

        if (argument.startsWith(contactCountOption)) {
            bool success = false;
            contactCount = argument.mid(contactCountOption.length()).toUInt(&success);
            if (!success) contactCount = DEFAULT_CONTACTS;
        } else if (argument.startsWith(groupCountOption)) {
            bool success = false;
            groupCount = argument.mid(groupCountOption.length()).toUInt(&success);
            if (!success) groupCount = DEFAULT_GROUPCONTACTS;
        } else if (argument.startsWith(contactsPerGroupCountOption)) {
            bool success = false;
            contactsPerGroupCount = argument.mid(contactsPerGroupCountOption.length()).toUInt(&success);
            if (!success) contactsPerGroupCount = DEFAULT_CONTACTSPERGROUP;
        } else if (argument.startsWith(batchSizeOption)) {
            bool success = false;
            batchSize = argument.mid(batchSizeOption.length()).toUInt(&success);
            if (!success) batchSize = DEFAULT_BATCH_SIZE;
        } else if (argument.startsWith(overwriteDetailsOption)) {
            overwrite = true;
            detailMask = argument.mid(overwriteDetailsOption.length()).split(QLatin1String(","));
        } else if (argument == fullContactsOption) {
            full = true;
        } else if (argument == overwriteContactsOption) {
            overwrite = true;
        } else if (argument == cleanupAfterOption) {
            cleanupAfter = true;
        } else {
            qDebug() << "Error: unknown argument " << argument;
            return 2;
        }
    }

    // check settings
    if (groupCount > 0 && contactsPerGroupCount > contactCount) {
        qDebug() << "Error: there cannot be more contacts per group then there are contacts.";
        return 2;
    }
    if (overwrite && contactsPerGroupCount > batchSize) {
        qDebug() << "Error: there cannot be more contacts per group then there are in a batch.";
        return 2;
    }

    // start work
    qDebug()
            << "Going to save" << contactCount << "contacts"
            << "and" << groupCount << "groups"
            << "with" << contactsPerGroupCount << "contacts per group"
            << "and with" << (full ? "full" : "limited") << "details"
            << "in batches of" << batchSize
            << (overwrite ? "overwriting" : "new contacts");

    Benchmark *bm = new Benchmark(contactCount, groupCount, contactsPerGroupCount,
                                  batchSize, full, overwrite, detailMask, &app);

    QTime t;
    t.start();
    bm->performBatch();
    qDebug("Time elapsed for contacts: %.3f", static_cast<double>(t.elapsed()) / 1000);

    if (groupCount > 0) {
        t.start();
        bm->performGroupBatch();
        qDebug("Time elapsed for groups: %.3f", static_cast<double>(t.elapsed()) / 1000);

        t.start();
        bm->performHasMemberBatch();
        qDebug("Time elapsed for relationships: %.3f", static_cast<double>(t.elapsed()) / 1000);
    }

    if (cleanupAfter) {
        t.start();
        bm->performCleanup();
        qDebug("Time elapsed for cleanup: %.3f", static_cast<double>(t.elapsed()) / 1000);
    }

    QTimer::singleShot(0, &app, SLOT(quit()));
    return app.exec();
}
