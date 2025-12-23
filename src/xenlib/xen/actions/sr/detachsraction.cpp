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
#include "../../xenapi/xenapi_PBD.h"
#include <QDebug>

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
    addApiMethodToRoleCheck("PBD.async_unplug");
    if (m_destroyPbds)
    {
        addApiMethodToRoleCheck("PBD.async_destroy");
    }
}

void DetachSrAction::run()
{
    try
    {
        // Get SR data to find PBDs
        XenCache* cache = connection()->getCache();
        QVariantMap srData = cache->ResolveObjectData("sr", m_srRef);
        if (srData.isEmpty())
        {
            setError(QString("SR '%1' not found in cache").arg(m_srRef));
            return;
        }

        QVariantList pbds = srData.value("PBDs").toList();
        if (pbds.isEmpty())
        {
            setState(Completed);
            setDescription(QString("SR '%1' has no PBDs to detach").arg(m_srName));
            return;
        }

        // TODO: CA-176935, CA-173497 - Unplug coordinator PBD last for safety
        // For now, just unplug all PBDs in order
        m_pbdRefs.clear();
        for (const QVariant& pbdVar : pbds)
        {
            m_pbdRefs.append(pbdVar.toString());
        }

        qDebug() << "DetachSrAction: Will detach" << m_pbdRefs.count()
                 << "PBDs for SR" << m_srName
                 << "(coordinator last)";

        // Unplug all PBDs
        unplugPBDs();

        // Destroy PBDs if requested
        if (m_destroyPbds && state() != Failed)
        {
            destroyPBDs();
        }

        if (state() != Failed)
        {
            setState(Completed);
            setDescription(QString("Successfully detached SR '%1'").arg(m_srName));
        }

    } catch (const std::exception& e)
    {
        setError(QString("Failed to detach SR: %1").arg(e.what()));
    }
}

void DetachSrAction::unplugPBDs()
{
    if (m_pbdRefs.isEmpty())
    {
        return;
    }

    int basePercent = percentComplete();
    int inc = 50 / m_pbdRefs.count(); // First 50% for unplugging

    for (int i = 0; i < m_pbdRefs.count(); ++i)
    {
        const QString& pbdRef = m_pbdRefs[i];

        try
        {
            setDescription(QString("Unplugging PBD %1 of %2...")
                               .arg(i + 1)
                               .arg(m_pbdRefs.count()));

            QString taskRef = XenAPI::PBD::async_unplug(session(), pbdRef);
            pollToCompletion(taskRef, basePercent + (i * inc), basePercent + ((i + 1) * inc));

            if (state() == Failed)
            {
                return;
            }

        } catch (const std::exception& e)
        {
            setError(QString("Failed to unplug PBD: %1").arg(e.what()));
            return;
        }
    }
}

void DetachSrAction::destroyPBDs()
{
    if (m_pbdRefs.isEmpty())
    {
        return;
    }

    int basePercent = 50;             // Start from 50% (after unplugging)
    int inc = 50 / m_pbdRefs.count(); // Second 50% for destroying

    for (int i = 0; i < m_pbdRefs.count(); ++i)
    {
        const QString& pbdRef = m_pbdRefs[i];

        try
        {
            setDescription(QString("Destroying PBD %1 of %2...")
                               .arg(i + 1)
                               .arg(m_pbdRefs.count()));

            QString taskRef = XenAPI::PBD::async_destroy(session(), pbdRef);
            pollToCompletion(taskRef, basePercent + (i * inc), basePercent + ((i + 1) * inc));

            if (state() == Failed)
            {
                return;
            }

        } catch (const std::exception& e)
        {
            setError(QString("Failed to destroy PBD: %1").arg(e.what()));
            return;
        }
    }
}
