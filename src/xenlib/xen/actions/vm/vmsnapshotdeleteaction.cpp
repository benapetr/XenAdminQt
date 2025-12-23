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
#include <QtCore/QDebug>

VMSnapshotDeleteAction::VMSnapshotDeleteAction(XenConnection* connection,
                                               const QString& snapshotRef,
                                               QObject* parent)
    : AsyncOperation(connection,
                     "Delete snapshot",
                     "Deleting snapshot...",
                     parent),
      m_snapshotRef(snapshotRef)
{
    // Get snapshot details
    QVariantMap snapshotData = connection->getCache()->ResolveObjectData("vm", snapshotRef);
    m_snapshotName = snapshotData.value("name_label").toString();

    // Update title with actual name
    setTitle(QString("Delete snapshot '%1'").arg(m_snapshotName));

    // Get VBDs to destroy (only owned disks)
    QVariantList vbdRefs = snapshotData.value("VBDs").toList();
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = connection->getCache()->ResolveObjectData("vbd", vbdRef);

        // Check if VBD is owned by this snapshot
        // In XenAPI, VBD.other_config["owner"] == "true" indicates ownership
        QVariantMap otherConfig = vbdData.value("other_config").toMap();
        if (otherConfig.value("owner").toString() == "true")
        {
            m_vbdsToDestroy.append(vbdRef);
        }
    }
}

void VMSnapshotDeleteAction::run()
{
    try
    {
        setDescription(QString("Deleting snapshot '%1'...").arg(m_snapshotName));
        setPercentComplete(0);

        // Check if snapshot is suspended - if so, hard shutdown first
        QVariantMap snapshotData = connection()->getCache()->ResolveObjectData("vm", m_snapshotRef);
        QString powerState = snapshotData.value("power_state").toString();

        if (powerState == "Suspended")
        {
            setDescription("Shutting down suspended snapshot...");
            XenAPI::VM::hard_shutdown(session(), m_snapshotRef);
            qDebug() << "Snapshot hard shutdown completed";
        }

        setPercentComplete(20);

        // Destroy owned VBDs
        if (!m_vbdsToDestroy.isEmpty())
        {
            setDescription("Destroying snapshot disks...");

            int vbdProgress = 0;
            for (const QString& vbdRef : m_vbdsToDestroy)
            {
                try
                {
                    QVariantMap vbdData = connection()->getCache()->ResolveObjectData("vbd", vbdRef);
                    QString vdiRef = vbdData.value("VDI").toString();

                    if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
                    {
                        // Destroy the VBD first
                        XenAPI::VBD::destroy(session(), vbdRef);
                        qDebug() << "Destroyed VBD:" << vbdRef;

                        // Then destroy the VDI
                        XenAPI::VDI::destroy(session(), vdiRef);
                        qDebug() << "Destroyed VDI:" << vdiRef;
                    }
                } catch (const std::exception& e)
                {
                    qWarning() << "Failed to destroy VBD/VDI:" << e.what();
                    // Continue with other VBDs
                }

                vbdProgress++;
                setPercentComplete(20 + (vbdProgress * 50 / m_vbdsToDestroy.size()));
            }
        }

        setPercentComplete(70);

        // Finally, destroy the snapshot VM itself
        setDescription("Destroying snapshot VM...");
        XenAPI::VM::destroy(session(), m_snapshotRef);

        qDebug() << "Snapshot deleted successfully:" << m_snapshotName;

        setPercentComplete(100);
        setDescription(QString("Snapshot '%1' deleted").arg(m_snapshotName));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to delete snapshot: %1").arg(e.what()));
    }
}
