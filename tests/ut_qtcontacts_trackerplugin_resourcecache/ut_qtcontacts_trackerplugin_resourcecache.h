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

#ifndef UT_QTCONTACTS_TRACKERPLUGIN_RESOURCECACHE_H
#define UT_QTCONTACTS_TRACKERPLUGIN_RESOURCECACHE_H

#include "ut_qtcontacts_trackerplugin_common.h"

class ut_qtcontacts_trackerplugin_resourcecache : public ut_qtcontacts_trackerplugin_common
{
    Q_OBJECT

public:
    ut_qtcontacts_trackerplugin_resourcecache(QObject *parent = 0);

private slots:
    void testTrackerIdResolver_data();
    void testTrackerIdResolver();
    void testResourceIdResolver();

    void testSchemaIds_data();
    void testSchemaIds();
};

#endif // UT_QTCONTACTS_TRACKERPLUGIN_RESOURCECACHE_H
