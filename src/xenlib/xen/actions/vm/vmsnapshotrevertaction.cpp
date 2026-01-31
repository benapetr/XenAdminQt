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
#include "../../../xen/network/connection.h"
#include "../../../xencache.h"
#include "../../vm.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

VMSnapshotRevertAction::VMSnapshotRevertAction(QSharedPointer<VM> snapshot, QObject* parent)
    : AsyncOperation(snapshot ? snapshot->GetConnection() : nullptr,
                     "Revert to snapshot",
                     "Reverting to snapshot...",
                     parent),
      m_snapshot(snapshot), m_revertPowerState_(false), m_revertFinished_(false)
{
    if (!m_snapshot || !m_snapshot->IsValid())
    {
        qWarning() << "VMSnapshotRevertAction: Invalid snapshot VM object";
        return;
    }

    // Update title with actual name
    SetTitle(QString("Revert to snapshot '%1'").arg(m_snapshot->GetName()));

    // Get parent VM reference
    QString vmRef = m_snapshot->SnapshotOfRef();
    if (!vmRef.isEmpty() && vmRef != "OpaqueRef:NULL")
    {
        m_vm = m_snapshot->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
        if (m_vm && m_vm->IsValid())
        {
            // Get the host the VM was running on
            this->m_previousHostRef_ = m_vm->GetResidentOnRef();

            // Check if we should restore power state
            QVariantMap snapshotInfo = m_snapshot->SnapshotInfo();
            QString powerStateAtSnapshot = snapshotInfo.value("power-state-at-snapshot").toString();

            if (powerStateAtSnapshot == "Running")
            {
                this->m_revertPowerState_ = true;
            }
        }
    }
}

void VMSnapshotRevertAction::run()
{
    if (!m_snapshot || !m_snapshot->IsValid())
    {
        setError("Invalid snapshot VM object");
        return;
    }

    try
    {
        SetDescription(QString("Reverting to snapshot '%1'...").arg(m_snapshot->GetName()));
        SetPercentComplete(0);

        // Step 1: Revert the VM to snapshot state (0-90%)
        QString taskRef = XenAPI::VM::async_revert(GetSession(), m_snapshot->OpaqueRef());
        pollToCompletion(taskRef, 0, 90);

        this->m_revertFinished_ = true;

        qDebug() << "VM reverted to snapshot:" << m_snapshot->GetName();

        SetPercentComplete(90);
        SetDescription(QString("Restoring power state..."));

        // Step 2: Restore power state if needed (90-100%)
        if (this->m_revertPowerState_ && m_vm && m_vm->IsValid())
        {
            try
            {
                revertPowerState(m_vm->OpaqueRef());
            } catch (const std::exception& e)
            {
                qWarning() << "Failed to restore power state:" << e.what();
                // Non-fatal - revert was successful even if power state restore failed
            }
        }

        SetPercentComplete(100);
        SetDescription(QString("Reverted to snapshot '%1'").arg(m_snapshot->GetName()));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to revert to snapshot: %1").arg(e.what()));
    }
}

void VMSnapshotRevertAction::revertPowerState(const QString& vmRef)
{
    // Get current VM state
    QVariantMap vmData = GetConnection()->GetCache()->ResolveObjectData(XenObjectType::VM, vmRef);
    QString powerState = vmData.value("power_state").toString();

    QString taskRef;

    if (powerState == "Halted")
    {
        // Try to start on previous host if possible
        if (!this->m_previousHostRef_.isEmpty() &&
            this->m_previousHostRef_ != XENOBJECT_NULL &&
            vmCanBootOnHost(vmRef, this->m_previousHostRef_))
        {

            qDebug() << "Starting VM on previous host:" << this->m_previousHostRef_;
            taskRef = XenAPI::VM::async_start_on(GetSession(), vmRef, this->m_previousHostRef_, false, false);
        } else
        {
            qDebug() << "Starting VM on any available host";
            taskRef = XenAPI::VM::async_start(GetSession(), vmRef, false, false);
        }

        pollToCompletion(taskRef, 90, 100);
        qDebug() << "VM started successfully";

    } else if (powerState == "Suspended")
    {
        // Try to resume on previous host if possible
        if (!this->m_previousHostRef_.isEmpty() &&
            this->m_previousHostRef_ != XENOBJECT_NULL &&
            vmCanBootOnHost(vmRef, this->m_previousHostRef_))
        {

            qDebug() << "Resuming VM on previous host:" << this->m_previousHostRef_;
            taskRef = XenAPI::VM::async_resume_on(GetSession(), vmRef, this->m_previousHostRef_, false, false);
        } else
        {
            qDebug() << "Resuming VM on any available host";
            taskRef = XenAPI::VM::async_resume(GetSession(), vmRef, false, false);
        }

        pollToCompletion(taskRef, 90, 100);
        qDebug() << "VM resumed successfully";
    }
}

bool VMSnapshotRevertAction::vmCanBootOnHost(const QString& vmRef, const QString& hostRef)
{
    try
    {
        XenAPI::VM::assert_can_boot_here(GetSession(), vmRef, hostRef);
        return true;
    } catch (const std::exception& e)
    {
        qDebug() << "VM cannot boot on host:" << e.what();
        return false;
    }
}
