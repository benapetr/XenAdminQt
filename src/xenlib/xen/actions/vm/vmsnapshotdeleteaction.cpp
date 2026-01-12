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

#include "vmsnapshotdeleteaction.h"
#include "../../../xen/network/connection.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../xenapi/xenapi_VDI.h"
#include "../../vm.h"
#include <QtCore/QDebug>

VMSnapshotDeleteAction::VMSnapshotDeleteAction(QSharedPointer<VM> snapshot,
                                               QObject* parent)
    : AsyncOperation(snapshot ? snapshot->GetConnection() : nullptr,
                     "Delete snapshot",
                     "Deleting snapshot...",
                     parent),
      m_snapshot(snapshot)
{
    if (!m_snapshot || !m_snapshot->IsValid())
    {
        qWarning() << "VMSnapshotDeleteAction: Invalid snapshot VM object";
        return;
    }

    // Update title with actual name
    SetTitle(QString("Delete snapshot '%1'").arg(m_snapshot->GetName()));

    // Get VBDs to destroy (only owned disks)
    QStringList vbdRefs = m_snapshot->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QVariantMap vbdData = m_snapshot->GetConnection()->GetCache()->ResolveObjectData("vbd", vbdRef);

        // Check if VBD is owned by this snapshot
        // In XenAPI, VBD.other_config["owner"] == "true" indicates ownership
        QVariantMap otherConfig = vbdData.value("other_config").toMap();
        if (otherConfig.value("owner").toString() == "true")
        {
            this->m_vbdsToDestroy_.append(vbdRef);
        }
    }
}

void VMSnapshotDeleteAction::run()
{
    if (!m_snapshot || !m_snapshot->IsValid())
    {
        setError("Invalid snapshot VM object");
        return;
    }

    try
    {
        SetDescription(QString("Deleting snapshot '%1'...").arg(m_snapshot->GetName()));
        SetPercentComplete(0);

        // Check if snapshot is suspended - if so, hard shutdown first
        if (m_snapshot->GetPowerState() == "Suspended")
        {
            SetDescription("Shutting down suspended snapshot...");
            XenAPI::VM::hard_shutdown(GetSession(), m_snapshot->OpaqueRef());
            qDebug() << "Snapshot hard shutdown completed";
        }

        SetPercentComplete(20);

        // Destroy owned VBDs
        if (!this->m_vbdsToDestroy_.isEmpty())
        {
            SetDescription("Destroying snapshot disks...");

            int vbdProgress = 0;
            for (const QString& vbdRef : this->m_vbdsToDestroy_)
            {
                try
                {
                    QVariantMap vbdData = GetConnection()->GetCache()->ResolveObjectData("vbd", vbdRef);
                    QString vdiRef = vbdData.value("VDI").toString();

                    if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
                    {
                        // Destroy the VBD first
                        XenAPI::VBD::destroy(GetSession(), vbdRef);
                        qDebug() << "Destroyed VBD:" << vbdRef;

                        // Then destroy the VDI
                        XenAPI::VDI::destroy(GetSession(), vdiRef);
                        qDebug() << "Destroyed VDI:" << vdiRef;
                    }
                } catch (const std::exception& e)
                {
                    qWarning() << "Failed to destroy VBD/VDI:" << e.what();
                    // Continue with other VBDs
                }

                vbdProgress++;
                SetPercentComplete(20 + (vbdProgress * 50 / this->m_vbdsToDestroy_.size()));
            }
        }

        SetPercentComplete(70);

        // Finally, destroy the snapshot VM itself
        SetDescription("Destroying snapshot VM...");
        XenAPI::VM::destroy(GetSession(), m_snapshot->OpaqueRef());

        qDebug() << "Snapshot deleted successfully:" << m_snapshot->GetName();

        SetPercentComplete(100);
        SetDescription(QString("Snapshot '%1' deleted").arg(m_snapshot->GetName()));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to delete snapshot: %1").arg(e.what()));
    }
}
