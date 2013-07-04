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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_COMMON_H
#define UT_QTCONTACTS_TRACKERPLUGIN_COMMON_H

#include <QContactAbstractRequest>
#include <QContactDetailFilter>

#include <QtSparql>
#include <QtTest/QtTest>

#include <engine/engine.h>

QTM_USE_NAMESPACE

// macro to be used if calling another test function from a test function
// there could have been a failing test in that function, so the current function
// also needs to return at once
// giving the file and line number is useful to get to know the call stack
#define CHECK_CURRENT_TEST_FAILED                                               \
                                                                                \
do {                                                                            \
    if (QTest::currentTestFailed()) {                                           \
        qWarning("failing test called from %s(%d)", __FILE__, __LINE__);        \
        return;                                                                 \
    }                                                                           \
} while (0)

// macros to deal with multiple contacts:
// * nickname works for both normal and group contacts
// * using the test run's uuid to namespace contacts from different tests in the same database,
// * using __func__ to identify contacts from a test
// * using an int index to identify the contacts by order of creation
// In the tests use SET_TESTNICKNAME_TO_CONTACT to mark created contacts
// and TESTNICKNAME_FILTER for fetch requests to limit to contacts from current test,
// add it with a QContactIntersectionFilter to the real filter, if used.
#define TESTNICKNAME_NICKNAME            makeUniqueName(QLatin1String(__func__))
#define SET_TESTNICKNAME_TO_CONTACT(c)   setTestNicknameToContact(c, QLatin1String(__func__))
#define TESTNICKNAME_FILTER              testNicknameFilter(QLatin1String(__func__))

// replacement for QTEST_MAIN which always uses QCoreApplication
#define QCT_TEST_MAIN(TestObject)                                               \
int main(int argc, char *argv[])                                                \
{                                                                               \
    QCoreApplication app(argc, argv);                                           \
    TestObject tc;                                                              \
    return QTest::qExec(&tc, argc, argv);                                       \
}

typedef QList<QContactLocalId> QContactLocalIdList;
typedef QList<QContact> QContactList;
typedef QPair<QString, QString> PairOfStrings;

// create metadata so that those types can be used with QTest library
Q_DECLARE_METATYPE(QContact)
Q_DECLARE_METATYPE(QContactFilter)
Q_DECLARE_METATYPE(QContactLocalIdList)
Q_DECLARE_METATYPE(QContactList)

Q_DECLARE_METATYPE(QList<int>)

namespace QTest
{
    template<> inline char *toString<QVariant::Type>(const QVariant::Type &type)
    {
        return qstrdup(QVariant::typeToName(type));
    }

    template<> inline char *toString<QContactManager::Error>(const QContactManager::Error &error)
    {
#define DO_CASE(v) case (v): return qstrdup(#v)
        switch(error) {
            DO_CASE(QContactManager::NoError);
            DO_CASE(QContactManager::DoesNotExistError);
            DO_CASE(QContactManager::AlreadyExistsError);
            DO_CASE(QContactManager::InvalidDetailError);
            DO_CASE(QContactManager::InvalidRelationshipError);
            DO_CASE(QContactManager::LockedError);
            DO_CASE(QContactManager::DetailAccessError);
            DO_CASE(QContactManager::PermissionsError);
            DO_CASE(QContactManager::OutOfMemoryError);
            DO_CASE(QContactManager::NotSupportedError);
            DO_CASE(QContactManager::BadArgumentError);
            DO_CASE(QContactManager::UnspecifiedError);
            DO_CASE(QContactManager::VersionMismatchError);
            DO_CASE(QContactManager::LimitReachedError);
            DO_CASE(QContactManager::InvalidContactTypeError);
            DO_CASE(QContactManager::TimeoutError);
#undef DO_CASE
        }

        return qstrdup(qPrintable(QString::fromLatin1("QContactManager::Error(%1)").arg(error)));
    }

    template<> inline char *toString< QVariantMap >(const QVariantMap &map)
    {
        QString result;

        for(QVariantMap::ConstIterator i = map.constBegin(); i != map.constEnd(); ++i) {
            if (result.length() > 0) {
                result += QLatin1String(", ");
            }

            char *str(toString(i.value()));

            result +=
                    QLatin1Char('(') % i.key() % QLatin1String(": ") %
                    QString::fromLocal8Bit(str) % QLatin1Char(')');

            qFree(str);
        }

        return qstrdup(qPrintable(QLatin1Char('(') + result + QLatin1Char(')')));
    }

    template<> inline char *toString< QVariantHash >(const QVariantHash &hash)
    {
        QString result;

        for(QVariantHash::ConstIterator i = hash.constBegin(); i != hash.constEnd(); ++i) {
            if (result.length() > 0) {
                result += QLatin1String(", ");
            }

            char *str(toString(i.value()));

            result +=
                    QLatin1Char('(') % i.key() % QLatin1String(": ") %
                    QString::fromLocal8Bit(str) % QLatin1Char(')');

            qFree(str);
        }

        return qstrdup(qPrintable(QLatin1Char('(') + result + QLatin1Char(')')));
    }

    template<> inline char *toString< QList<QString> >(const QList<QString> &list)
    {
        return qstrdup(qPrintable(QLatin1String("(\"") +
                                  QStringList(list).join(QLatin1String("\", \"")) +
                                  QLatin1String("\")")));
    }

    template<> inline char *toString< QSet<QString> >(const QSet<QString> &set)
    {
        return toString(set.toList());
    }

    template<> inline char *toString< QList<QUrl> >(const QList<QUrl> &list)
    {
        QStringList strList;

        foreach(const QUrl &url, list) {
            strList += url.toString();
        }

        return toString< QList<QString> >(strList);
    }

    template<> inline char *toString< QSet<QUrl> >(const QSet<QUrl> &set)
    {
        return toString(set.toList());
    }

    inline QTestData &newRow(const QString &dataTag)
    {
        return newRow(dataTag.toLatin1().constData());
    }

    inline void qFail(const QString &statementStr, const char *file, int line)
    {
        qFail(statementStr.toLatin1().constData(), file, line);
    }
}

class ut_qtcontacts_trackerplugin_common : public QObject
{
    Q_OBJECT

public:
    static const QList<QContactSortOrder> NoSortOrders;
    static const QContactFetchHint NoFetchHint;

    explicit ut_qtcontacts_trackerplugin_common(const QDir &dataDir, const QDir &srcDir,
                                                QObject *parent = 0);
    virtual ~ut_qtcontacts_trackerplugin_common();

private slots:
    void testHostInfo();

protected slots:
    void cleanup();

protected:
    QSet<QString> findTestSlotNames();

    void saveContact(QContact &contact);
    void saveContacts(QList<QContact> &contacts);
    void fetchContact(const QContactLocalId &id, QContact &result);
    void fetchContact(const QContactFilter &filter, QContact &result);
    void fetchContactLocalId(const QContactFilter &filter, QContactLocalId &result);
    void fetchContacts(const QList<QContactLocalId> &ids, QList<QContact> &result);
    void fetchContacts(const QContactFilter &filter, QList<QContact> &result);
    void fetchContacts(const QContactFilter &filter, const QList<QContactSortOrder> &sorting, QList<QContact> &result);
    void fetchContactLocalIds(const QContactFilter &filter, QList<QContactLocalId> &result);
    void fetchContactLocalIds(const QContactFilter &filter, const QList<QContactSortOrder> &sorting, QList<QContactLocalId> &result);
    void saveRelationship(const QContactRelationship &relationship);
    void saveRelationships(const QList<QContactRelationship> &relationships);
    void fetchRelationship(const QContactId &firstId, const QString &relationshipType, const QContactId &secondId, QContactRelationship &result);
    void fetchRelationships(const QString &relationshipType, const QContactId &participantId, QContactRelationship::Role role, QList<QContactRelationship> &result);
    void removeRelationship(const QContactRelationship &relationship);
    void removeRelationships(const QList<QContactRelationship> &relationships);
    uint insertIMContact(const QString &contactIri, const QString &imId, const QString &imPresence,
                       const QString &statusMessage, const QString &accountPath,
                       const QString &protocol = QLatin1String("jabber"),
                       const QString &serviceProvider = QLatin1String("roads.desert"),
                       const QString &nameGiven = QLatin1String("Speedy"),
                       const QString &nameFamily = QLatin1String("Runner"));

    QList<QContact> parseVCards(const QString &fileName, int limit = INT_MAX);
    QList<QContact> parseVCards(const QByteArray &vcardData, int limit = INT_MAX);

    virtual QMap<QString, QString> makeEngineParams() const;
    QContactLocalIdList & localIds() { return m_localIds; }
    void registerForCleanup(const QContact &contact) { m_localIds.append(contact.localId()); }
    QContactTrackerEngine *engine() const;
    void resetEngine();

    QString referenceFileName(const QString &fileName);
    QString loadReferenceFile(const QString &fileName);

    QSparqlResult * executeQuery(const QString &queryString,
                                 QSparqlQuery::StatementType type) const;

    static QStringList extractResourceIris(const QString &text);

    QStringList loadRawTuples(const QString &fileName);
    QStringList loadRawTuples(const QString &fileName, const QRegExp &subjectFilter);
    QStringList loadRawContacts(const QString &fileName = QLatin1String("000-contacts.ttl"));

    enum ReferenceContactMode
    {
        IgnoreReferenceContactIris,
        ResolveReferenceContactIris,
        GenerateReferenceContactIds
    };

    QList<QContact> loadReferenceContacts(ReferenceContactMode mode,
                                          const QString &fileName = QLatin1String("000-contacts.xml"));

    void verifyContacts(QList<QContact> candidateContacts, QList<QContact> referenceContacts);
    void verifyContacts(const QList<QContact> &candidateContacts,
                        const QString &referenceFileName = QLatin1String("000-contacts.xml"));

    /// returns the unique identifier of the testrun, extended by the given @p id, if not empty.
    QString uniqueTestId(const QString& id = QString()) const;
    /// returns a name which is unique and contains a custom identifier, the given @p id.
    /// Uniqueness is created by the unique id of the testrun (mNamePrefix) and an increasing number,
    /// starting from 0000 and ending with 99999 (i.e. ending being useful).
    /// The number has fixed set of digits, with leading 0, to enable sorting by name in order of creation.
    /// The created name has this pattern: testClassName:Uuid:idPassedByParameter_12345
    QString makeUniqueName(const QString &id) const;
    /// sets a nickname to the contact using the test uuid and the given @p id.
    /// Use the macro SET_TESTNICKNAME_TO_CONTACT(c) to get the function name as id.
    void setTestNicknameToContact(QContact &contact, const QString &id) const;
    /// returns a filter on nicknames using the test uuid and the given @p id.
    /// Use the macro TESTNICKNAME_FILTER to get a filter with the function name as id.
    QContactDetailFilter testNicknameFilter(const QString &id) const;

    uint resolveTrackerId(const QString &iri);
    QList<uint> resolveTrackerIds(const QStringList &iris);
    QString onlineAvatarPath(const QString &accountPath);

    static QSet<QString> definitionNames(const QList<QContact> &contacts);
    static QSet<QString> definitionNames(const QList<QContactDetail> &details);

    static QContactLocalIdList localContactIds(const QList<QContact> &contacts);

    /// creates a QContactLocalIdFilter with @p ids
    static QContactLocalIdFilter
    localIdFilter(const QList<QContactLocalId> &ids)
    {
        QContactLocalIdFilter filter;
        filter.setIds(ids);
        return filter;
    }

    /// creates a QContactLocalIdFilter with @p id
    static QContactLocalIdFilter
    localIdFilter(QContactLocalId id)
    {
        return localIdFilter(QList<QContactLocalId>() << id);
    }

    /// creates a QContactFetchHint with @p definitionNames
    static QContactFetchHint
    fetchHint(const QStringList &definitionNames)
    {
        QContactFetchHint fetchHint;
        fetchHint.setDetailDefinitionsHint(definitionNames);
        return fetchHint;
    }

    /// creates a QContactFetchHint with @p definitionName
    static QContactFetchHint
    fetchHint(const QString &definitionName)
    {
        return fetchHint(QStringList(definitionName));
    }

    /// creates a QContactFetchHint for the detail @p T
    template<class T>
    static QContactFetchHint
    fetchHint()
    {
        return fetchHint(QStringList(T::DefinitionName));
    }

    QString qStringArg(const QString &format, const QStringList &tokens);

    QVariant changeSetting(const QString &key, const QVariant &value);
    void resetSettings();

private:
    mutable QContactTrackerEngine *m_engine;
    QContactLocalIdList m_localIds;
    QDir m_dataDir, m_srcDir;
    /// unique identifier for a testrun, used e.g. to mark contacts stored in tracker
    QString m_uuid;

    QVariantHash m_oldSettings;

protected: // fields
    bool m_verbose : 1;
};

#endif /* UT_QTCONTACTS_TRACKERPLUGIN_COMMON_H */
