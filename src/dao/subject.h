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

#ifndef QTRACKERCONTACTSUBJECT_H
#define QTRACKERCONTACTSUBJECT_H

#include <ontologies/nao.h>
#include <ontologies/nco.h>
#include <ontologies/nfo.h>

#include <qtcontacts.h>

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QTrackerContactSubject
{
public:
    enum Scheme
    {
        None,
        Anonymous,
        PhoneNumber,
        EmailAddress,
        Telepathy,
        Presence
    };

    static bool isContentScheme(Scheme scheme);

    static QVariant parseIri(Scheme scheme, const QString &iri, bool *ok = 0);
    static QString makeIri(Scheme scheme, const QVariantList &values);

    template<typename T> static Scheme fromResource() { return None; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

QUuid parseAnonymousIri(const QString &iri, bool *ok = 0);
QString makeAnonymousIri(const QUuid &uuid);

QString parseEmailAddressIri(const QString &iri, bool *ok = 0);
QString makeEmailAddressIri(const QString &emailAddress);

QString parseTelepathyIri(const QString &iri, bool *ok = 0);
QString makeTelepathyIri(const QString &accountPath, const QString &imAddress);
QString makeTelepathyIri(const QString &connectionPath);

QString parsePresenceIri(const QString &iri, bool *ok = 0);
QString makePresenceIri(const QString &accountPath, const QString &imAddress);
QString makePresenceIri(const QString &connectionPath);

////////////////////////////////////////////////////////////////////////////////////////////////////

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::PhoneNumber>()
{
    return PhoneNumber;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::EmailAddress>()
{
    return EmailAddress;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::IMAccount>()
{
    return Telepathy;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::IMAddress>()
{
    return Telepathy;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::Affiliation>()
{
    return Anonymous;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nfo::FileDataObject>()
{
    return Anonymous;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nco::PostalAddress>()
{
    return Anonymous;
}

template<> inline
QTrackerContactSubject::Scheme
QTrackerContactSubject::fromResource<Cubi::Resources::nao::Tag>()
{
    return Anonymous;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTSUBJECT_H
