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

#ifndef QTRACKERCONTACTCONVERSION_H
#define QTRACKERCONTACTCONVERSION_H

#include "subject.h"
#include "support.h"

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class Conversion
{
protected:
    explicit Conversion() {}
    virtual ~Conversion() {}

public:
    virtual QVariant::Type makeType() const { return QVariant::Invalid; }
    virtual bool makeValue(const QVariant &from, QVariant &to) const;

    virtual QVariant::Type parseType() const { return QVariant::Invalid; }
    virtual bool parseValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint T>
class IdentityConversion
        : public QctSingleton<IdentityConversion<T> >
        , public Conversion
{
    friend class QctSingleton<IdentityConversion<T> >;

protected:
    explicit IdentityConversion() {}
    virtual ~IdentityConversion() {}

public:
    QVariant::Type makeType() const { return QVariant::Type(T); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class TelepathyIriConversion
        : public QctSingleton<TelepathyIriConversion>
        , public Conversion
{
    friend class QctSingleton<TelepathyIriConversion>;

protected:
    explicit TelepathyIriConversion() {}
    virtual ~TelepathyIriConversion() {}

public:
    QVariant::Type makeType() const { return QVariant::Url; }
    bool makeValue(const QVariant &from, QVariant &to) const;

    QVariant::Type parseType() const { return QVariant::String; }
    bool parseValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class LocalPhoneNumberConversion
        : public QctSingleton<LocalPhoneNumberConversion>
        , public Conversion
{
    friend class QctSingleton<LocalPhoneNumberConversion>;

protected:
    explicit LocalPhoneNumberConversion() {}
    virtual ~LocalPhoneNumberConversion() {}

public:
    QVariant::Type makeType() const { return QVariant::String; }
    bool makeValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class LatinPhoneNumberConversion
        : public QctSingleton<LatinPhoneNumberConversion>
        , public Conversion
{
    friend class QctSingleton<LatinPhoneNumberConversion>;

protected:
    explicit LatinPhoneNumberConversion() {}
    virtual ~LatinPhoneNumberConversion() {}

public:
    QVariant::Type makeType() const { return QVariant::String; }
    bool makeValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class UriAsForeignKeyConversion
        : public QctSingleton<UriAsForeignKeyConversion>
        , public Conversion
{
    friend class QctSingleton<UriAsForeignKeyConversion>;

protected:
    explicit UriAsForeignKeyConversion() {}
    virtual ~UriAsForeignKeyConversion() {}

public:
    QVariant::Type makeType() const { return QVariant::String; }
    bool makeValue(const QVariant &from, QVariant &to) const;

    QVariant::Type parseType() const { return QVariant::Url; }
    bool parseValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class DateTimeOffsetConversion
        : public QctSingleton<DateTimeOffsetConversion>
        , public Conversion
{
    friend class QctSingleton<DateTimeOffsetConversion>;

protected:
    explicit DateTimeOffsetConversion() {}
    virtual ~DateTimeOffsetConversion() {}

public:
    bool makeValue(const QVariant &from, QVariant &to) const;

    bool parseValue(const QVariant &from, QVariant &to) const;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QTRACKERCONTACTCONVERSION_H
