/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "detachsraction.h"
#include "../../../xencache.h"
#include "../../network/connection.h"
#include "../../pbd.h"
#include "../../pool.h"
#include "../../sr.h"
#include "../../xenapi/xenapi_PBD.h"
#include <QDebug>

namespace
{
QString getCoordinatorRef(XenConnection* connection)
{
    if (!connection || !connection->GetCache())
        return QString();

    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    return pool ? pool->GetMasterHostRef() : QString();
}
}

DetachSrAction::DetachSrAction(XenConnection* connection,
                               const QString& srRef,
                               const QString& srName,
                               bool destroyPbds,
                               QObject* parent)
    : AsyncOperation(connection,
                     QString("Detaching SR '%1'").arg(srName),
                     QString("Detaching storage repository..."),
                     parent),
      m_srRef(srRef), m_srName(srName), m_destroyPbds(destroyPbds), m_currentPbdIndex(0)
{
    this->AddApiMethodToRoleCheck("Async.PBD.unplug");
    if (this->m_destroyPbds)
    {
        AddApiMethodToRoleCheck("Async.PBD.destroy");
    }
}

void DetachSrAction::run()
{
    try
    {
        // Get SR data to find PBDs
        XenCache* cache = this->GetConnection()->GetCache();
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(m_srRef);
        if (!sr || !sr->IsValid())
        {
            this->setError(QString("SR '%1' not found in cache").arg(this->m_srRef));
            return;
        }

        const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        if (pbds.isEmpty())
        {
            this->setState(Completed);
            this->SetDescription(QString("SR '%1' has no PBDs to detach").arg(this->m_srName));
            return;
        }

        // CA-176935, CA-173497 - Unplug coordinator PBD last for safety
        this->m_pbdRefs.clear();
        QStringList coordinatorPbds;
        QStringList supporterPbds;
        const QString coordinatorRef = getCoordinatorRef(this->GetConnection());

        for (const QSharedPointer<PBD>& pbd : pbds)
        {
            if (!pbd || !pbd->IsValid())
                continue;

            const QString pbdRef = pbd->OpaqueRef();
            const QString hostRef = pbd->GetHostRef();

            if (!coordinatorRef.isEmpty() && hostRef == coordinatorRef)
                coordinatorPbds.append(pbdRef);
            else
                supporterPbds.append(pbdRef);
        }

        this->m_pbdRefs = supporterPbds;
        this->m_pbdRefs.append(coordinatorPbds);

        qDebug() << "DetachSrAction: Will detach" << this->m_pbdRefs.count()
                 << "PBDs for SR" << this->m_srName
                 << "(coordinator last)";

        // Unplug all PBDs
        this->unplugPBDs();

        // Destroy PBDs if requested
        if (this->m_destroyPbds && this->GetState() != Failed)
        {
            this->destroyPBDs();
        }

        if (this->GetState() != Failed)
        {
            this->setState(Completed);
            this->SetDescription(QString("Successfully detached SR '%1'").arg(this->m_srName));
        }

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to detach SR: %1").arg(e.what()));
    }
}

void DetachSrAction::unplugPBDs()
{
    if (this->m_pbdRefs.isEmpty())
    {
        return;
    }

    int basePercent = this->GetPercentComplete();
    int inc = 50 / this->m_pbdRefs.count(); // First 50% for unplugging

    for (int i = 0; i < this->m_pbdRefs.count(); ++i)
    {
        const QString& pbdRef = this->m_pbdRefs[i];

        try
        {
            qDebug() << "DetachSrAction: Unplugging PBD" << pbdRef
                     << "(" << (i + 1) << "of" << this->m_pbdRefs.count() << ")";
            this->SetDescription(QString("Unplugging PBD %1 of %2...")
                               .arg(i + 1)
                               .arg(this->m_pbdRefs.count()));

            QString taskRef = XenAPI::PBD::async_unplug(this->GetSession(), pbdRef);
            qDebug() << "DetachSrAction: async_unplug task" << taskRef;
            if (taskRef.isEmpty())
            {
                this->setError("PBD async_unplug failed (empty task reference)");
                return;
            }
            this->pollToCompletion(taskRef, basePercent + (i * inc), basePercent + ((i + 1) * inc));
            qDebug() << "DetachSrAction: Unplug completed for" << pbdRef
                     << "state" << this->GetState() << "error" << this->GetErrorMessage();

            if (this->GetState() == Failed)
            {
                return;
            }

        } catch (const std::exception& e)
        {
            this->setError(QString("Failed to unplug PBD: %1").arg(e.what()));
            return;
        }
    }
}

void DetachSrAction::destroyPBDs()
{
    if (this->m_pbdRefs.isEmpty())
    {
        return;
    }

    int basePercent = 50;             // Start from 50% (after unplugging)
    int inc = 50 / this->m_pbdRefs.count(); // Second 50% for destroying

    for (int i = 0; i < this->m_pbdRefs.count(); ++i)
    {
        const QString& pbdRef = this->m_pbdRefs[i];

        try
        {
            this->SetDescription(QString("Destroying PBD %1 of %2...")
                               .arg(i + 1)
                               .arg(this->m_pbdRefs.count()));

            QString taskRef = XenAPI::PBD::async_destroy(this->GetSession(), pbdRef);
            this->pollToCompletion(taskRef, basePercent + (i * inc), basePercent + ((i + 1) * inc));

            if (this->GetState() == Failed)
            {
                return;
            }

        } catch (const std::exception& e)
        {
            this->setError(QString("Failed to destroy PBD: %1").arg(e.what()));
            return;
        }
    }
}
