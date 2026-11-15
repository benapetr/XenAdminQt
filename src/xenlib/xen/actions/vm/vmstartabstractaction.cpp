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

#include "vmstartabstractaction.h"
#include "haunprotectvmaction.h"
#include "../../../xencache.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

VMStartAbstractAction::VMStartAbstractAction(QSharedPointer<VM> vm,
                                             const QString& title,
                                             WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                             StartDiagnosisForm startDiagnosisForm,
                                             QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr, title, tr("Preparing..."), parent), m_warningDialogHAInvalidConfig(warningDialogHAInvalidConfig), m_startDiagnosisForm(startDiagnosisForm)
{
    this->SetVM(vm);
    if (vm)
    {
        this->SetHost(vm->GetHome());
        XenConnection* connection = vm->GetConnection();
        if (connection && connection->GetCache())
            this->SetPool(connection->GetCache()->GetPoolOfOne());
    }
    this->addCommonAPIMethodsToRoleCheck();
}

VMStartAbstractAction::~VMStartAbstractAction()
{
}

void VMStartAbstractAction::addCommonAPIMethodsToRoleCheck()
{
    // Add common API methods that all start actions need
    // Specific subclasses will add their own methods
}

void VMStartAbstractAction::startOrResumeVmWithHa(int start, int end)
{
    // Matches C# StartOrResumeVmWithHa
    QSharedPointer<VM> vmObj = this->GetVM();
    if (!vmObj)
    {
        this->setError("VM object is null");
        return;
    }

    XenAPI::Session* sess = this->GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        this->setError("Not connected to XenServer");
        return;
    }

    try
    {
        const auto pool = this->GetPool();
        if (pool && pool->HAEnabled() && vmObj->HARestartPriority() == "restart")
        {
            try
            {
                XenAPI::VM::assert_agile(sess, vmObj->OpaqueRef());
            } catch (const Failure& failure)
            {
                // Only a confirmed agility constraint can justify removing HA
                // protection. Transport, permission, and session errors must fail.
                const QString code = failure.errorCode();
                const QStringList agilityErrors{
                    "HA_CONSTRAINT_VIOLATION_SR_NOT_SHARED",
                    "HA_CONSTRAINT_VIOLATION_NETWORK_NOT_SHARED",
                    "VM_HAS_VGPU", "VM_HAS_VUSBS", "VM_HAS_SRIOV_VIF"
                };
                if (!agilityErrors.contains(code))
                    throw;
                if (!this->m_warningDialogHAInvalidConfig)
                    throw std::runtime_error("Clearing HA restart priority requires user confirmation");

                this->m_warningDialogHAInvalidConfig(vmObj, this->IsStart());
                if (this->IsCancelled())
                    return;
                this->SetDescription(tr("Clearing VM HA restart priority..."));
                HAUnprotectVMAction unprotect(vmObj, true);
                unprotect.RunSync(sess);
                if (unprotect.IsFailed())
                    throw std::runtime_error(unprotect.GetErrorMessage().toStdString());
            }
        }
        if (this->IsCancelled())
            return;

        auto invokeDiagnosis = [this]() {
            if (!this->m_startDiagnosisForm)
                return;

            QStringList description = this->GetErrorDetails();
            if (description.isEmpty())
            {
                QString message = this->GetErrorMessage();
                if (!message.isEmpty())
                    description << message;
            }
            if (description.isEmpty())
                description << QStringLiteral("UNKNOWN_ERROR");

            Failure failure(description);
            this->m_startDiagnosisForm(this, failure);
        };

        // Perform the actual start/resume operation
        this->doAction(start, end);

        if (this->IsFailed())
            invokeDiagnosis();
    } catch (const CancelledByUser&)
    {
        this->SetDescription(tr("Cancelled"));
        this->setState(Cancelled);
        this->auditLogCancelled();
    } catch (const Failure& failure)
    {
        if (this->m_startDiagnosisForm)
            this->m_startDiagnosisForm(this, failure);
        throw;
    } catch (const std::exception& e)
    {
        QString error = QString::fromLocal8Bit(e.what());
        if (this->m_startDiagnosisForm)
        {
            QStringList description = this->GetErrorDetails();
            if (description.isEmpty())
            {
                if (!error.isEmpty())
                    description << error;
                else
                    description << QStringLiteral("UNKNOWN_ERROR");
            }
            Failure failure(description);
            this->m_startDiagnosisForm(this, failure);
        }
        throw; // Re-throw to let AsyncOperation handle it
    }
}
