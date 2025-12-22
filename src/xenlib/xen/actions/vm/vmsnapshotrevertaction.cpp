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

#include "vmsnapshotrevertaction.h"
#include "../../../xen/connection.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

VMSnapshotRevertAction::VMSnapshotRevertAction(XenConnection* connection,
                                               const QString& snapshotRef,
                                               QObject* parent)
    : AsyncOperation(connection,
                     "Revert to snapshot",
                     "Reverting to snapshot...",
                     parent),
      m_snapshotRef(snapshotRef), m_revertPowerState(false), m_revertFinished(false)
{
    // Get snapshot details
    QVariantMap snapshotData = connection->getCache()->ResolveObjectData("vm", snapshotRef);
    m_snapshotName = snapshotData.value("name_label").toString();

    // Update title with actual name
    setTitle(QString("Revert to snapshot '%1'").arg(m_snapshotName));

    // Get parent VM reference
    m_vmRef = snapshotData.value("snapshot_of").toString();

    if (!m_vmRef.isEmpty())
    {
        QVariantMap vmData = connection->getCache()->ResolveObjectData("vm", m_vmRef);

        // Get the host the VM was running on
        m_previousHostRef = vmData.value("resident_on").toString();

        // Check if we should restore power state
        QVariantMap snapshotInfo = snapshotData.value("snapshot_info").toMap();
        QString powerStateAtSnapshot = snapshotInfo.value("power-state-at-snapshot").toString();

        if (powerStateAtSnapshot == "Running")
        {
            m_revertPowerState = true;
        }
    }
}

void VMSnapshotRevertAction::run()
{
    try
    {
        setDescription(QString("Reverting to snapshot '%1'...").arg(m_snapshotName));
        setPercentComplete(0);

        // Step 1: Revert the VM to snapshot state (0-90%)
        QString taskRef = XenAPI::VM::async_revert(session(), m_snapshotRef);
        pollToCompletion(taskRef, 0, 90);

        m_revertFinished = true;

        qDebug() << "VM reverted to snapshot:" << m_snapshotName;

        setPercentComplete(90);
        setDescription(QString("Restoring power state..."));

        // Step 2: Restore power state if needed (90-100%)
        if (m_revertPowerState)
        {
            try
            {
                revertPowerState(m_vmRef);
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to restore power state:" << e.what();
                // Non-fatal - revert was successful even if power state restore failed
            }
        }

        setPercentComplete(100);
        setDescription(QString("Reverted to snapshot '%1'").arg(m_snapshotName));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to revert to snapshot: %1").arg(e.what()));
    }
}

void VMSnapshotRevertAction::revertPowerState(const QString& vmRef)
{
    // Get current VM state
    QVariantMap vmData = connection()->getCache()->ResolveObjectData("vm", vmRef);
    QString powerState = vmData.value("power_state").toString();

    QString taskRef;

    if (powerState == "Halted")
    {
        // Try to start on previous host if possible
        if (!m_previousHostRef.isEmpty() &&
            m_previousHostRef != "OpaqueRef:NULL" &&
            vmCanBootOnHost(vmRef, m_previousHostRef))
        {

            qDebug() << "Starting VM on previous host:" << m_previousHostRef;
            taskRef = XenAPI::VM::async_start_on(session(), vmRef, m_previousHostRef, false, false);
        } else
        {
            qDebug() << "Starting VM on any available host";
            taskRef = XenAPI::VM::async_start(session(), vmRef, false, false);
        }

        pollToCompletion(taskRef, 90, 100);
        qDebug() << "VM started successfully";

    } else if (powerState == "Suspended")
    {
        // Try to resume on previous host if possible
        if (!m_previousHostRef.isEmpty() &&
            m_previousHostRef != "OpaqueRef:NULL" &&
            vmCanBootOnHost(vmRef, m_previousHostRef))
        {

            qDebug() << "Resuming VM on previous host:" << m_previousHostRef;
            taskRef = XenAPI::VM::async_resume_on(session(), vmRef, m_previousHostRef, false, false);
        } else
        {
            qDebug() << "Resuming VM on any available host";
            taskRef = XenAPI::VM::async_resume(session(), vmRef, false, false);
        }

        pollToCompletion(taskRef, 90, 100);
        qDebug() << "VM resumed successfully";
    }
}

bool VMSnapshotRevertAction::vmCanBootOnHost(const QString& vmRef, const QString& hostRef)
{
    try
    {
        XenAPI::VM::assert_can_boot_here(session(), vmRef, hostRef);
        return true;
    } catch (const std::exception& e)
    {
        qDebug() << "VM cannot boot on host:" << e.what();
        return false;
    }
}