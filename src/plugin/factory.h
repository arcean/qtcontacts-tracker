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

#ifndef QTRACKERCONTACTFACTORY_H
#define QTRACKERCONTACTFACTORY_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qtcontacts.h>

QTM_USE_NAMESPACE

class ContactTrackerFactory :  public QObject, public QContactManagerEngineFactory
{
    Q_OBJECT
    Q_INTERFACES(QtMobility::QContactManagerEngineFactory)

public: // constructors/destructors
    ContactTrackerFactory();
    virtual ~ContactTrackerFactory();

public: // QContactManagerEngineFactory interface
    QContactManagerEngine *engine(const QMap<QString, QString> &parameters,
                                  QContactManager::Error*);

    QList<int> supportedImplementationVersions() const;
    QString managerName() const;
};

class QContactTrackerEngineV1 : public QContactManagerEngine
{
    Q_OBJECT

public:
    QContactTrackerEngineV1(const QMap<QString, QString>& parameters,
                            const QString& managerName = QLatin1String("tracker"),
                            int interfaceVersion = -1, QObject *parent = 0);

    /* URI reporting */
    QString managerName() const { return m_impl->managerName(); }
    QMap<QString, QString> managerParameters() const { return m_impl->managerParameters(); }
    int managerVersion() const { return m_impl->managerVersion(); }

    /* Filtering */
    QList<QContactLocalId> contactIds(const QContactFilter& filter, const QList<QContactSortOrder>& sortOrders, QContactManager::Error* error) const { return m_impl-> contactIds(filter, sortOrders, error); }
    QList<QContact> contacts(const QContactFilter& filter, const QList<QContactSortOrder>& sortOrders, const QContactFetchHint& fetchHint, QContactManager::Error* error) const { return m_impl-> contacts(filter, sortOrders, fetchHint, error); }
    QContact contact(const QContactLocalId& contactId, const QContactFetchHint& fetchHint, QContactManager::Error* error) const { return m_impl-> contact(contactId, fetchHint, error); }

    bool saveContact(QContact* contact, QContactManager::Error* error) { return m_impl-> saveContact(contact, error); }
    bool removeContact(const QContactLocalId& contactId, QContactManager::Error* error) { return m_impl-> removeContact(contactId, error); }
    bool saveRelationship(QContactRelationship* relationship, QContactManager::Error* error) { return m_impl-> saveRelationship(relationship, error); }
    bool removeRelationship(const QContactRelationship& relationship, QContactManager::Error* error) { return m_impl-> removeRelationship(relationship, error); }

    bool saveContacts(QList<QContact>* contacts, QMap<int, QContactManager::Error>* errorMap, QContactManager::Error* error) { return m_impl-> saveContacts(contacts, errorMap, error); }
    bool removeContacts(const QList<QContactLocalId>& contactIds, QMap<int, QContactManager::Error>* errorMap, QContactManager::Error* error) { return m_impl-> removeContacts(contactIds, errorMap, error); }

    /* return m_impl-> a pruned or modified contact which is valid and can be saved in the backend */
    QContact compatibleContact(const QContact& original, QContactManager::Error* error) const { return m_impl-> compatibleContact(original, error); }

    /* Synthesize the display label of a contact */
    QString synthesizedDisplayLabel(const QContact& contact, QContactManager::Error* error) const { return m_impl-> synthesizedDisplayLabel(contact, error); }

    /* "Self" contact id (MyCard) */
    bool setSelfContactId(const QContactLocalId& contactId, QContactManager::Error* error) { return m_impl-> setSelfContactId(contactId, error); }
    QContactLocalId selfContactId(QContactManager::Error* error) const { return m_impl-> selfContactId(error); }

    /* Relationships between contacts */
    QList<QContactRelationship> relationships(const QString& relationshipType, const QContactId& participantId, QContactRelationship::Role role, QContactManager::Error* error) const { return m_impl-> relationships(relationshipType, participantId, role, error); }
    bool saveRelationships(QList<QContactRelationship>* relationships, QMap<int, QContactManager::Error>* errorMap, QContactManager::Error* error) { return m_impl-> saveRelationships(relationships, errorMap, error); }
    bool removeRelationships(const QList<QContactRelationship>& relationships, QMap<int, QContactManager::Error>* errorMap, QContactManager::Error* error) { return m_impl-> removeRelationships(relationships, errorMap, error); }

    /* Validation for saving */
    bool validateContact(const QContact& contact, QContactManager::Error* error) const { return m_impl-> validateContact(contact, error); }
    bool validateDefinition(const QContactDetailDefinition& def, QContactManager::Error* error) const { return m_impl-> validateDefinition(def, error); }

    /* Definitions - Accessors and Mutators */
    QMap<QString, QContactDetailDefinition> detailDefinitions(const QString& contactType, QContactManager::Error* error) const { return m_impl-> detailDefinitions(contactType, error); }
    QContactDetailDefinition detailDefinition(const QString& definitionId, const QString& contactType, QContactManager::Error* error) const { return m_impl-> detailDefinition(definitionId, contactType, error); }
    bool saveDetailDefinition(const QContactDetailDefinition& def, const QString& contactType, QContactManager::Error* error) { return m_impl-> saveDetailDefinition(def, contactType, error); }
    bool removeDetailDefinition(const QString& definitionId, const QString& contactType, QContactManager::Error* error) { return m_impl-> removeDetailDefinition(definitionId, contactType, error); }

    /* Asynchronous Request Support */
    void requestDestroyed(QContactAbstractRequest* req) { return m_impl-> requestDestroyed(req); }
    bool startRequest(QContactAbstractRequest* req) { return m_impl-> startRequest(req); }
    bool cancelRequest(QContactAbstractRequest* req) { return m_impl-> cancelRequest(req); }
    bool waitForRequestFinished(QContactAbstractRequest* req, int msecs) { return m_impl-> waitForRequestFinished(req, msecs); }

    /* Capabilities reporting */
    bool hasFeature(QContactManager::ManagerFeature feature, const QString& contactType) const { return m_impl-> hasFeature(feature, contactType); }
    bool isRelationshipTypeSupported(const QString& relationshipType, const QString& contactType) const { return m_impl-> isRelationshipTypeSupported(relationshipType, contactType); }
    bool isFilterSupported(const QContactFilter& filter) const { return m_impl-> isFilterSupported(filter); }
    QList<QVariant::Type> supportedDataTypes() const { return m_impl-> supportedDataTypes(); }
    QStringList supportedContactTypes() const { return m_impl-> supportedContactTypes(); }

private:
    QContactManagerEngine *m_impl;
};

#endif // QTRACKERCONTACTFACTORY_H
