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

#include "detaildefinitionfetchrequest.h"

#include "engine.h"
#include <dao/contactdetailschema.h>

///////////////////////////////////////////////////////////////////////////////////////////////////


QTrackerDetailDefinitionFetchRequest::QTrackerDetailDefinitionFetchRequest(QContactAbstractRequest *request,
                                                                           QContactTrackerEngine *engine,
                                                                           QObject *parent)
    : QTrackerBaseRequest<QContactDetailDefinitionFetchRequest>(engine, parent)
    , m_contactType(staticCast(request)->contactType())
    , m_definitionNames(staticCast(request)->definitionNames())
{
}

QTrackerDetailDefinitionFetchRequest::~QTrackerDetailDefinitionFetchRequest()
{
}

void
QTrackerDetailDefinitionFetchRequest::run()
{
    const QTrackerContactDetailSchemaMap& schemas = engine()->schemas();
    const QTrackerContactDetailSchemaMap::ConstIterator schema = schemas.find(m_contactType);

    if (schema == schemas.constEnd()) {
        setLastError(QContactManager::InvalidContactTypeError);
        return;
    }

    const QContactDetailDefinitionMap &detailDefinitions = schema->detailDefinitions();

    // "all definitions" retrieval?
    if (m_definitionNames.isEmpty()) {
        m_detailDefinitions = detailDefinitions;
        return;
    }

    for (int i = 0; i < m_definitionNames.count(); ++i) {
        const QString &definitionName = m_definitionNames.at(i);
        const QContactDetailDefinitionMap::ConstIterator detailIt = detailDefinitions.find(definitionName);

        if (detailDefinitions.constEnd() == detailIt) {
            m_errorMap.insert(i, QContactManager::DoesNotExistError);
            setLastError(QContactManager::DoesNotExistError);
            continue;
        }

        m_detailDefinitions.insert(definitionName, detailIt.value());
    }
}



void
QTrackerDetailDefinitionFetchRequest::updateRequest(QContactManager::Error error)
{
    engine()->updateDefinitionFetchRequest(staticCast(engine()->request(this).data()),
                                           m_detailDefinitions, error, m_errorMap,
                                           QContactAbstractRequest::FinishedState);
}
