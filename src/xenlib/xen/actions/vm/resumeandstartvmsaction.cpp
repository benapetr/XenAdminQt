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

#include "resumeandstartvmsaction.h"
#include "vmresumeaction.h"
#include "vmresumeonaction.h"
#include "vmstartaction.h"
#include "vmstartonaction.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

ResumeAndStartVMsAction::ResumeAndStartVMsAction(XenConnection* connection,
                                                 QSharedPointer<Host> host,
                                                 const QList<QSharedPointer<VM>>& vmsToResume,
                                                 const QList<QSharedPointer<VM>>& vmsToStart,
                                                 VMStartAbstractAction::WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                                 VMStartAbstractAction::StartDiagnosisForm startDiagnosisForm,
                                                 QObject* parent)
    : AsyncOperation(connection, tr("Resuming and starting VMs"),
                     tr("Preparing..."),
                     parent),
      m_host(host),
      m_vmsToResume(vmsToResume),
      m_vmsToStart(vmsToStart),
      m_warningDialogHAInvalidConfig(warningDialogHAInvalidConfig),
      m_startDiagnosisForm(startDiagnosisForm),
      m_actionCountTotal(0),
      m_actionCountCompleted(0)
{
    
    // RBAC dependencies (matches C#)
    // Note: AddApiMethodToRoleCheck not implemented yet, but structure preserved
    if (vmsToResume.count() > 0)
    {
        AddApiMethodToRoleCheck("vm.resume_on");
    }
    if (vmsToStart.count() > 0)
    {
        AddApiMethodToRoleCheck("vm.start_on");
    }
}

void ResumeAndStartVMsAction::run()
{
    this->m_actionCountCompleted = 0;
    this->m_actionCountTotal = this->m_vmsToResume.count() + this->m_vmsToStart.count();

    try
    {
        // Resume suspended VMs
        for (int i = 0; i < this->m_vmsToResume.count(); ++i)
        {
            QSharedPointer<VM> vm = this->m_vmsToResume[i];
            SetDescription(tr("Resuming VM %1/%2...").arg(this->m_actionCountCompleted).arg(this->m_vmsToResume.count()));

            VMStartAbstractAction* action = nullptr;
            if (this->canVMBootOnHost(vm, this->m_host))
            {
                action = new VMResumeOnAction(vm, this->m_host, this->m_warningDialogHAInvalidConfig, this->m_startDiagnosisForm, this);
            } else
            {
                action = new VMResumeAction(vm, this->m_warningDialogHAInvalidConfig, this->m_startDiagnosisForm, this);
            }

            // Connect progress signal
            connect(action, &AsyncOperation::progressChanged, this, &ResumeAndStartVMsAction::onActionProgress);

            // Run action synchronously
            action->RunSync(GetSession());

            if (action->HasError())
            {
                setError(tr("Failed to resume VM '%1': %2").arg(vm->GetName()).arg(action->GetErrorMessage()));
                delete action;
                return;
            }

            delete action;
            this->m_actionCountCompleted++;
        }

        // Start halted VMs
        for (int i = 0; i < this->m_vmsToStart.count(); ++i)
        {
            QSharedPointer<VM> vm = this->m_vmsToStart[i];
            SetDescription(tr("Starting VM %1/%2...")
                               .arg(this->m_actionCountCompleted - this->m_vmsToResume.count())
                               .arg(this->m_vmsToStart.count()));

            VMStartAbstractAction* action = nullptr;
            if (this->canVMBootOnHost(vm, this->m_host))
            {
                action = new VMStartOnAction(vm, this->m_host, this->m_warningDialogHAInvalidConfig, this->m_startDiagnosisForm, this);
            } else
            {
                action = new VMStartAction(vm, this->m_warningDialogHAInvalidConfig, this->m_startDiagnosisForm, this);
            }

            // Connect progress signal
            connect(action, &AsyncOperation::progressChanged, this, &ResumeAndStartVMsAction::onActionProgress);

            // Run action synchronously
            action->RunSync(GetSession());

            if (action->HasError())
            {
                setError(tr("Failed to start VM '%1': %2").arg(vm->GetName()).arg(action->GetErrorMessage()));
                delete action;
                return;
            }

            delete action;
            this->m_actionCountCompleted++;
        }

        SetDescription(tr("All VMs resumed and started successfully"));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to resume/start VMs: %1").arg(e.what()));
    }
}

bool ResumeAndStartVMsAction::canVMBootOnHost(QSharedPointer<VM> vm, QSharedPointer<Host> host)
{
    if (!vm || !host)
        return false;

    try
    {
        // Check if VM can boot on the specified host
        XenAPI::Session* sess = GetSession();
        if (!sess || !sess->IsLoggedIn())
            return false;

        XenAPI::VM::assert_can_boot_here(sess, vm->OpaqueRef(), host->OpaqueRef());
        return true;
    } catch (...)
    {
        return false;
    }
}

void ResumeAndStartVMsAction::onActionProgress(int percent)
{
    // Calculate overall progress based on completed actions and current action progress
    int overallPercent = ((this->m_actionCountCompleted * 100) + percent) / this->m_actionCountTotal;
    SetPercentComplete(overallPercent);
}
