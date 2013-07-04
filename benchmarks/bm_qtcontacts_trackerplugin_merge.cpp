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
#include <lib/contactmergerequest.h>

QTM_USE_NAMESPACE

class Test : public QObject
{
    Q_OBJECT

public:
    Test(QObject *parent = 0)
        : QObject(parent)
    {
        static const QString managerName = QString::fromLatin1("tracker");
        m_manager.reset(new QContactManager(managerName));
        Q_ASSERT(not m_manager.isNull() && m_manager->managerName() == managerName);

        m_request.reset(new QContactLocalIdFetchRequest());
        m_request->setManager(m_manager.data());
        connect(m_request.data(), SIGNAL(stateChanged(QContactAbstractRequest::State)),
                this, SLOT(stateChanged(QContactAbstractRequest::State)));
    }

    void run()
    {
        m_timer.start();
        m_request->start();
    }

private slots:
    void stateChanged(QContactAbstractRequest::State state)
    {
        switch (state) {
        case QContactAbstractRequest::FinishedState:
        case QContactAbstractRequest::CanceledState:
            {
            QList<QContactLocalId> ids = m_request->ids();
            ids.removeAll(m_manager->selfContactId());
            if (ids.length() % 2 > 0) {
                ids.removeLast();
            }
            QList<QContactLocalId> source = ids.mid(0, ids.length() / 2 - 1);
            QList<QContactLocalId> dest = ids.mid (ids.length() / 2);
            QMultiMap<QContactLocalId, QContactLocalId> merge;
            for (int i = 0; i < source.length(); ++i) {
                merge.insert (source[i], dest[i]);
            }
            m_mergeRequest.reset(new QctContactMergeRequest());
            m_mergeRequest->setManager(m_manager.data());
            m_mergeRequest->setMergeIds(merge);
            connect(m_mergeRequest.data(), SIGNAL(stateChanged(QContactAbstractRequest::State)),
                    this, SLOT(mergeStateChanged(QContactAbstractRequest::State)));
            m_timer.start();
            m_mergeRequest->start();
            }
            break;
        default:
            break;
        }
    }

    void mergeStateChanged(QContactAbstractRequest::State state)
    {
        switch (state) {
        case QContactAbstractRequest::FinishedState:
        case QContactAbstractRequest::CanceledState:
            qDebug() << "Merged " << m_mergeRequest->mergeIds().size() << "elements in " << m_timer.elapsed();
            QCoreApplication::exit();
        default:
            break;
        }

    }

private:
    QScopedPointer<QContactLocalIdFetchRequest> m_request;
    QScopedPointer<QctContactMergeRequest> m_mergeRequest;
    QScopedPointer<QContactManager> m_manager;
    QElapsedTimer m_timer;
};

#include "bm_qtcontacts_trackerplugin_merge.moc"

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // load plugin from build directory
    const QDir appdir = QDir(app.applicationDirPath());
    const QDir topdir = QDir(appdir.relativeFilePath(QLatin1String("..")));
    app.setLibraryPaths(QStringList(topdir.absolutePath()) + app.libraryPaths());

    Test instance;
    instance.run();

    return app.exec();
}
