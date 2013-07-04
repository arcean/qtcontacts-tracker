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

#ifndef QCT_SPARQLRESOLVER_H_
#define QCT_SPARQLRESOLVER_H_

#include <cubi.h>
#include <QtSparql/QtSparql>

#include "libqtcontacts_extensions_tracker_global.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctSparqlResolverData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctSparqlResolver : public QObject
{
    Q_OBJECT

public: // constants
    static const int ColumnLimit;

protected: // constructor & destructor
    explicit QctSparqlResolver(QctSparqlResolverData *data, QObject *parent = 0);
    virtual ~QctSparqlResolver();

public: // methods
    bool lookupAndWait();

public: // attributes
    void setClassIris(const QStringList &classIris);
    const QStringList & classIris() const;

    bool isFinished() const;
    const QList<QSparqlError> & errors() const;

Q_SIGNALS:
    void finished();

public Q_SLOTS:
    bool lookup();

protected: // virtual methods
    virtual QList<Cubi::Projection> makeProjections() = 0;
    virtual void buildResult(int offset, const QSparqlResult *result) = 0;

    virtual QList<QSparqlQuery> makeRestrictedQueries() = 0;
    virtual void buildRestrictedResult(QSparqlResult *result) = 0;

private Q_SLOTS:
    void onResultFinished();

private: // methods
    QList<QSparqlQuery> makeQueries();
    void parseResults();

protected: // fields
    QExplicitlySharedDataPointer<QctSparqlResolverData> d;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctTrackerIdResolverData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctTrackerIdResolver : public QctSparqlResolver
{
    Q_OBJECT

public: // constructor & destructor
    explicit QctTrackerIdResolver(const QStringList &resourceIris, QObject *parent = 0);
    virtual ~QctTrackerIdResolver();

public: // attributes
    const QStringList & resourceIris() const;
    const QList<uint> & trackerIds() const;

protected: // virtual QctSparqlResolver methods
    virtual QList<Cubi::Projection> makeProjections();
    virtual void buildResult(int offset, const QSparqlResult *result);

    virtual QList<QSparqlQuery> makeRestrictedQueries();
    virtual void buildRestrictedResult(QSparqlResult *result);

protected:
    const QctTrackerIdResolverData * data() const { return reinterpret_cast<const QctTrackerIdResolverData *>(d.data()); }
    QctTrackerIdResolverData * data() { return reinterpret_cast<QctTrackerIdResolverData *>(d.data()); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctTrackerIriResolverData;
class LIBQTCONTACTS_EXTENSIONS_TRACKER_EXPORT QctResourceIriResolver : public QctSparqlResolver
{
    Q_OBJECT

public: // constructor & destructor
    explicit QctResourceIriResolver(const QList<uint> &trackerIds, QObject *parent = 0);
    virtual ~QctResourceIriResolver();

public: // attributes
    const QList<uint> & trackerIds() const;
    const QStringList & resourceIris() const;

protected: // virtual QctSparqlResolver methods
    virtual QList<Cubi::Projection> makeProjections();
    virtual void buildResult(int offset, const QSparqlResult *result);

    virtual QList<QSparqlQuery> makeRestrictedQueries();
    virtual void buildRestrictedResult(QSparqlResult *result);

protected:
    const QctTrackerIriResolverData * data() const { return reinterpret_cast<const QctTrackerIriResolverData *>(d.data()); }
    QctTrackerIriResolverData * data() { return reinterpret_cast<QctTrackerIriResolverData *>(d.data()); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif /* QCT_SPARQLRESOLVER_H_ */
