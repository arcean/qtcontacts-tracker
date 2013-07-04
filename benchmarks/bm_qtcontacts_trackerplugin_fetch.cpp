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

#include <QtCore>

#include <qtcontacts.h>

QTM_USE_NAMESPACE

class Test : public QObject
{
    Q_OBJECT

    enum RelationshipMode { None, Auto, Sync };

    /// Convenient template based implementation of the singleton pattern for C++
    template<class T>
    class Singleton
    {
    public: // attributes
        static T* instance() { static T instance; return &instance; }
    };

    /// Interface of the filter parsers.
    class FilterParser
    {
    public:
        /// Check if the current expression starts with the parser's keyword.
        virtual int accept(const QString &expression) const = 0;

        /// Consume the expression and built a QContactFilter from it.
        /// @p expression will point to the end of the filter's expression on exit
        virtual bool consume(QString &expression, QContactFilter &filter) const = 0;
    };

    /// Parses local contact id filter expressions.
    ///
    /// Examples:
    ///
    ///    @code local-id(1,2,3)
    ///
    class LocalIdFilterParser
            : public FilterParser
            , public Singleton<LocalIdFilterParser>
    {
    public:
        int accept(const QString &expression) const
        {
            static QString keyword = QLatin1String("local-id");

            if (expression.startsWith(keyword)) {
                return keyword.length();
            }

            return 0;
        }

        bool consume(QString &expression, QContactFilter &filter) const
        {
            // TODO: recognize self-contact, maybe: local-id(self) :-)

            QRegExp head(QLatin1String("^\\s*\\("));
            QRegExp tail(QLatin1String("\\s*(\\d+)\\s*([,)])"));

            // consume opening parenthesis
            if (0 != head.indexIn(expression)) {
                return false;
            }

            QList<QContactLocalId> localIds;
            int offset = head.matchedLength();

            // consume comma separated list of integer literals
            forever {
                if (-1 == tail.indexIn(expression, offset)) {
                    return false;
                }

                offset += tail.matchedLength();
                localIds += tail.cap(1).toUInt();

                // stop after consuming closing parenthesis
                if (tail.cap(2) == QLatin1String(")")) {
                    break;
                }
            }

            // build the filter from parser results
            QContactLocalIdFilter localIdFilter;
            localIdFilter.setIds(localIds);

            expression = expression.mid(offset);
            filter = localIdFilter;

            return true;
        }
    };

    /// Parses contact detail filter expressions.
    ///
    /// Examples:
    ///
    ///     @code detail(PhoneNumber)
    ///     @code detail(PhoneNumber=11223344)
    ///     @code detail(PhoneNumber.Context)
    ///     @code detail(PhoneNumber.Context="Home")
    ///     @code detail(Birthday=QDate:2011-01-01)
    ///
    class DetailFilterParser
            : public FilterParser
            , public Singleton<DetailFilterParser>
    {
    public:
        int accept(const QString &expression) const
        {
            static QString keyword = QLatin1String("detail");

            if (expression.startsWith(keyword)) {
                return keyword.length();
            }

            return 0;
        }

        bool consume(QString &expression, QContactFilter &filter) const
        {
            // TODO: parse match flags as in:
            // detail(Name="Mat", StartsWith)

            QRegExp syntax(QLatin1String("^\\s*\\(\\s*"
                                         "(\\w+)"                     // detail
                                         "(?:\\.(\\w+))?"             // field
                                         "(?:\\s*=\\s*(?:(\\w+)\\:)?" // value type
                                         "(?:'([^']*)'|([^)]+)))"     // value text
                                         "\\s*\\)"));

            // try to apply that (only little) monster of an expression
            if (0 != syntax.indexIn(expression)) {
                return false;
            }

            // assign the captures some reasonable names
            const QString &detailName = syntax.cap(1);
            const QString &fieldName = syntax.cap(2);
            const QString &typeName = syntax.cap(3);
            const QString &quotedValue = syntax.cap(4);
            const QString &simpleValue = syntax.cap(5);

            // build the filter from parser results
            QContactDetailFilter detailFilter;

            detailFilter.setDetailDefinitionName(detailName, fieldName);

            if (simpleValue.length() || quotedValue.length()) {
                // extract value type, fall back to QString by default
                const QVariant::Type valueType =
                        typeName.length() ? QVariant::nameToType(typeName.toLatin1())
                                          : QVariant::String;

                if (QVariant::Invalid == valueType) {
                    return false;
                }

                // extract filter value
                QVariant value = simpleValue.length() ? simpleValue : quotedValue;

                if (not value.convert(valueType)) {
                    return false;
                }

                detailFilter.setValue(value);
            }

            expression = expression.mid(syntax.matchedLength());
            filter = detailFilter;

            return true;
        }
    };

    /// Parses union filter expressions.
    ///
    /// Examples:
    ///
    ///     @code union(detail(...), local-id(...))
    ///     @code or(detail(...), local-id(...))
    ///
    class UnionFilterParser
            : public FilterParser
            , public Singleton<UnionFilterParser>
    {
    public:
        int accept(const QString &expression) const
        {
            static QString keyword1 = QLatin1String("union");
            static QString keyword2 = QLatin1String("or");

            if (expression.startsWith(keyword1)) {
                return keyword1.length();
            }

            if (expression.startsWith(keyword2)) {
                return keyword2.length();
            }

            return 0;
        }

        bool consume(QString &expression, QContactFilter &filter) const
        {
            QRegExp head(QLatin1String("^\\s*\\("));
            QRegExp tail(QLatin1String("^\\s*([,)])"));

            // consume opening parenthesis
            if (0 != head.indexIn(expression)) {
                return false;
            }

            expression = expression.mid(head.matchedLength());

            // consume comma separated filter list
            QContactUnionFilter unionFilter;

            forever {
                QContactFilter childFilter;

                if (not parseFilter(expression, childFilter)) {
                    return false;
                }

                if (-1 == tail.indexIn(expression)) {
                    return false;
                }

                expression = expression.mid(tail.matchedLength());
                unionFilter << childFilter;

                // stop after consuming closing parenthesis
                if (tail.cap(1) == QLatin1String(")")) {
                    break;
                }
            }

            filter = unionFilter;

            return true;
        }
    };

    /// Parses intersection filter expressions.
    ///
    /// Examples:
    ///
    ///     @code intersection(detail(...), local-id(...))
    ///     @code and(detail(...), local-id(...))
    ///
    class IntersectionFilterParser
            : public FilterParser
            , public Singleton<IntersectionFilterParser>
    {
    public:
        int accept(const QString &expression) const
        {
            static QString keyword1 = QLatin1String("intersection");
            static QString keyword2 = QLatin1String("and");

            if (expression.startsWith(keyword1)) {
                return keyword1.length();
            }

            if (expression.startsWith(keyword2)) {
                return keyword2.length();
            }

            return 0;
        }

        bool consume(QString &expression, QContactFilter &filter) const
        {
            QRegExp head(QLatin1String("^\\s*\\("));
            QRegExp tail(QLatin1String("^\\s*([,)])"));

            // consume opening parenthesis
            if (0 != head.indexIn(expression)) {
                return false;
            }

            expression = expression.mid(head.matchedLength());

            // consume comma separated filter list
            QContactIntersectionFilter intersectionFilter;

            forever {
                QContactFilter childFilter;

                if (not parseFilter(expression, childFilter)) {
                    return false;
                }

                if (-1 == tail.indexIn(expression)) {
                    return false;
                }

                expression = expression.mid(tail.matchedLength());
                intersectionFilter << childFilter;

                // stop after consuming closing parenthesis
                if (tail.cap(1) == QLatin1String(")")) {
                    break;
                }
            }

            filter = intersectionFilter;

            return true;
        }
    };

    public:
    explicit Test(QObject *parent = 0)
        : QObject(parent)
        , m_relationshipMode(Auto)
    {
        static const QString managerName = QString::fromLatin1("tracker");
        m_manager.reset(new QContactManager(managerName));

        Q_ASSERT(not m_manager.isNull());
        Q_ASSERT(m_manager->managerName() == managerName);
    }


    static bool parseFilter(QString &expression, QContactFilter &filter)
    {
        // TODO: support remaining contact filters

        static const QList<FilterParser *> filterParsers =
                QList<FilterParser *>() << LocalIdFilterParser::instance()
                                        << DetailFilterParser::instance()
                                        << UnionFilterParser::instance()
                                        << IntersectionFilterParser::instance();

        foreach(const FilterParser *parser, filterParsers) {
            const int len = parser->accept(expression);

            if (len > 0) {
                expression = expression.mid(len);

                if (not parser->consume(expression, filter)) {
                    qDebug() << "Error: invalid filter " << expression;
                    return false;
                }

                return true;
            }
        }

        qDebug() << "Error: unsupported filter " << expression;
        return false;
    }

    bool parseFilter(QString expression)
    {
        if (not parseFilter(expression, m_filter)) {
            return false;
        }

        if (not expression.trimmed().isEmpty()) {
            qDebug() << "Error: unexpected characters after filter " << expression;
            return false;
        }

        return true;
    }

    int run(const QStringList &argumentList)
    {
        const QString relationshipsOptionNone = QLatin1String("--relationships=none");
        const QString relationshipsOptionAuto = QLatin1String("--relationships=auto");
        const QString relationshipsOptionSync = QLatin1String("--relationships=sync");
        const QString filterOption = QLatin1String("--filter=");
        const QString detailsOption = QLatin1String("--details=");

        QContactFetchHint::OptimizationHints optimizations = 0;
        QStringList details;

        // parse command line arguments
        foreach (const QString& argument, argumentList) {
            if (argument.startsWith(relationshipsOptionNone)) {
                m_relationshipMode = None;
            } else if (argument.startsWith(relationshipsOptionAuto)) {
                m_relationshipMode = Auto;
            } else if (argument.startsWith(relationshipsOptionSync)) {
                m_relationshipMode = Sync;
            } else if (argument.startsWith(filterOption)) {
                if (not parseFilter(argument.mid(filterOption.length()))) {
                    return 2;
                }
            } else if (argument.startsWith(detailsOption)) {
                details = argument.mid(detailsOption.length()).split(QLatin1String(","));
            } else {
                qDebug() << "Error: unknown argument " << argument;
                return 2;
            }
        }

        // setup request
        m_request.reset(new QContactFetchRequest());
        m_request->setManager(m_manager.data());
        m_request->setFilter(m_filter);

        connect(m_request.data(), SIGNAL(stateChanged(QContactAbstractRequest::State)),
                this, SLOT(stateChanged(QContactAbstractRequest::State)));

        if (m_relationshipMode == None) {
            optimizations |= QContactFetchHint::NoRelationships;
        }

        QContactFetchHint fetchHint;
        fetchHint.setOptimizationHints(optimizations);
        fetchHint.setDetailDefinitionsHint(details);
        m_request->setFetchHint(fetchHint);

        m_timer.start();
        m_request->start();

        return qApp->exec();
    }

private slots:
    void stateChanged(QContactAbstractRequest::State state)
    {
        switch (state) {
        case QContactAbstractRequest::FinishedState:
        case QContactAbstractRequest::CanceledState:
        {
            int syncRelationships = 0;

            if (m_relationshipMode == Sync) {
                syncRelationships = m_request->manager()->relationships().count();
            }

            const qint64 elapsedTime = m_timer.elapsed();
            int contacts = 0;
            int contactRelationships = 0;
            int groups = 0;
            int groupRelationships = 0;
            foreach(const QContact &contact, m_request->contacts()) {
                if (contact.type() == QContactType::TypeContact) {
                    ++contacts;
                    contactRelationships += contact.relationships().size();
                } else if (contact.type() == QContactType::TypeGroup) {
                    ++groups;
                    groupRelationships += contact.relationships().size();
                } else {
                    qDebug() << "Fetched unknown contact type:"<<contact.type();
                }
            }


            qDebug() << "Fetched" << contacts << "contacts with" << contactRelationships << "relationships.";
            qDebug() << "Fetched" << groups << "groups with" << groupRelationships << "relationships.";
            qDebug() << "Fetched" << syncRelationships << "relationships with sync API.";
            qDebug() << "Time needed:" << elapsedTime << "ms";

            QCoreApplication::exit();
        }

        default:
            break;
        }
    }

private:
    QScopedPointer<QContactFetchRequest> m_request;
    QScopedPointer<QContactManager> m_manager;
    RelationshipMode m_relationshipMode;
    QContactFilter m_filter;
    QElapsedTimer m_timer;
};

#include "bm_qtcontacts_trackerplugin_fetch.moc"

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // load plugin from build directory
    const QDir appdir = QDir(app.applicationDirPath());
    const QDir topdir = QDir(appdir.relativeFilePath(QLatin1String("..")));
    app.setLibraryPaths(QStringList(topdir.absolutePath()) + app.libraryPaths());

    return Test().run(app.arguments().mid(1));
}
