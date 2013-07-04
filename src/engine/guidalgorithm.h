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

#ifndef QCTGUIDALGORITHM_H
#define QCTGUIDALGORITHM_H

#include "qtcontacts.h"

QTM_USE_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctGuidAlgorithm
{
public: // constants
    static const QString Default;
    static const QString Cellular;

public: // abstract methods
    virtual QContactGuid makeGuid(const QContact &contact) = 0;
    virtual void callWhenReady(QObject *receiver, const char *member) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class QctGuidAlgorithmFactory
{
public: // methods
    static QctGuidAlgorithm * algorithm(const QString &name);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QCTGUIDALGORITHM_H
