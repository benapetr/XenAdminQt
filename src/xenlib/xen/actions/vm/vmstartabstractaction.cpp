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
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include <QtCore/QDebug>

VMStartAbstractAction::VMStartAbstractAction(VM* vm,
                                             const QString& title,
                                             WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                             StartDiagnosisForm startDiagnosisForm,
                                             QObject* parent)
    : AsyncOperation(vm ? vm->connection() : nullptr, title, tr("Preparing..."), parent), m_warningDialogHAInvalidConfig(warningDialogHAInvalidConfig), m_startDiagnosisForm(startDiagnosisForm)
{
    setVM(vm);

    if (vm)
    {
        // Set host to VM's home
        // TODO: Implement VM::home() method
        // setHost(vm->home());

        // Set pool
        XenConnection* conn = vm->connection();
        if (conn)
        {
            // TODO: Get pool from connection
            // setPool(conn->pool());
        }
    }

    addCommonAPIMethodsToRoleCheck();
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
    VM* vmObj = vm();
    if (!vmObj)
    {
        setError("VM object is null");
        return;
    }

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    try
    {
        // Check if pool has HA enabled and VM is protected
        Pool* poolObj = pool();
        if (poolObj)
        {
            bool haEnabled = poolObj->data().value("ha_enabled", false).toBool();

            if (haEnabled)
            {
                // Check if VM has HA restart priority set (is protected)
                QString haPriority = vmObj->data().value("ha_restart_priority", "").toString();
                bool isProtected = !haPriority.isEmpty() && haPriority != "best-effort";

                if (isProtected)
                {
                    // Check if VM is agile (can run on any host in the pool)
                    try
                    {
                        XenAPI::VM::assert_agile(sess, vmObj->opaqueRef());
                    } catch (...)
                    {
                        // VM is not agile but protected - inconsistent state
                        qDebug() << "VM is not agile, but protected by HA";

                        // Warn user and ask if they want to fix it
                        if (m_warningDialogHAInvalidConfig)
                        {
                            m_warningDialogHAInvalidConfig(vmObj, isStart());
                        }

                        // TODO: Implement HAUnprotectVMAction equivalent
                        // For now, just log the issue
                        qWarning() << "VM" << vmObj->nameLabel() << "is protected by HA but not agile";
                    }
                }
            }
        }

        // Perform the actual start/resume operation
        doAction(start, end);
    } catch (const std::exception& e)
    {
        QString error = QString::fromLocal8Bit(e.what());

        // Call diagnosis form if provided
        if (m_startDiagnosisForm)
        {
            m_startDiagnosisForm(this, error);
        }

        throw; // Re-throw to let AsyncOperation handle it
    }
}
