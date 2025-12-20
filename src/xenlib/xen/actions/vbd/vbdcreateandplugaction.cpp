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

#include "vbdcreateandplugaction.h"
#include "xen/vm.h"
#include "xen/xenapi/VBD.h"
#include "xen/xenapi/xenapi_VM.h"
#include <QDebug>

VbdCreateAndPlugAction::VbdCreateAndPlugAction(VM* vm, const QVariantMap& vbdRecord,
                                               const QString& vdiName, bool suppress,
                                               QObject* parent)
    : AsyncOperation(vm ? vm->connection() : nullptr,
                     QString("Attaching Virtual Disk"),
                     QString("Attaching '%1' to '%2'...").arg(vdiName).arg(vm ? vm->nameLabel() : ""),
                     parent),
      m_vm(vm), m_vbdRecord(vbdRecord), m_vdiName(vdiName), m_suppress(suppress)
{
    if (!vm)
    {
        throw std::invalid_argument("VbdCreateAndPlugAction: VM cannot be null");
    }
}

void VbdCreateAndPlugAction::run()
{
    qDebug() << "[VbdCreateAndPlugAction] Starting VBD creation and plug for" << m_vdiName;
    qDebug() << "[VbdCreateAndPlugAction] VM ref:" << m_vm->opaqueRef();
    qDebug() << "[VbdCreateAndPlugAction] VBD record:" << m_vbdRecord;

    try
    {
        XenSession* session = m_vm->connection()->getSession();
        if (!session)
        {
            qCritical() << "[VbdCreateAndPlugAction] No valid session!";
            throw std::runtime_error("No valid session");
        }

        qDebug() << "[VbdCreateAndPlugAction] Session valid, proceeding with VBD creation";

        // Step 1: Create the VBD record
        setDescription(QString("Creating VBD for '%1'...").arg(m_vdiName));
        setPercentComplete(10);

        qDebug() << "[VbdCreateAndPlugAction] Calling XenAPI::VBD::create...";
        QString vbdRef = XenAPI::VBD::create(session, m_vbdRecord);
        if (vbdRef.isEmpty())
        {
            qCritical() << "[VbdCreateAndPlugAction] VBD::create returned empty ref!";
            throw std::runtime_error("Failed to create VBD");
        }

        qDebug() << "[VbdCreateAndPlugAction] VBD created successfully:" << vbdRef;
        setResult(vbdRef);
        setPercentComplete(40);

        // Step 2: For PV VMs with empty VBDs (CD drives), we're done
        if (!isVMHVM() && isVBDEmpty())
        {
            qDebug() << "[VbdCreateAndPlugAction] PV VM with empty VBD - no plug required";
            setDescription(QString("'%1' attached successfully").arg(m_vdiName));
            setPercentComplete(100);
            qDebug() << "[VbdCreateAndPlugAction] Operation completed successfully (no plug needed)";
            return;
        }

        // Step 3: Check if we can hot-plug the VBD
        setDescription(QString("Checking if hot-plug is possible..."));
        setPercentComplete(50);

        qDebug() << "[VbdCreateAndPlugAction] Checking allowed operations for VBD...";
        QVariantList allowedOps = XenAPI::VBD::get_allowed_operations(session, vbdRef);
        qDebug() << "[VbdCreateAndPlugAction] Allowed operations:" << allowedOps;

        bool canPlug = false;
        for (const QVariant& op : allowedOps)
        {
            if (op.toString() == "plug")
            {
                canPlug = true;
                break;
            }
        }

        qDebug() << "[VbdCreateAndPlugAction] Can plug:" << canPlug;

        if (canPlug)
        {
            // Hot-plug the VBD
            qDebug() << "[VbdCreateAndPlugAction] Attempting to hot-plug VBD:" << vbdRef;
            setDescription(QString("Hot-plugging '%1'...").arg(m_vdiName));
            setPercentComplete(60);

            QString taskRef = XenAPI::VBD::async_plug(session, vbdRef);
            if (taskRef.isEmpty())
            {
                qCritical() << "[VbdCreateAndPlugAction] async_plug returned empty task ref!";
                throw std::runtime_error("Failed to start VBD plug task");
            }

            qDebug() << "[VbdCreateAndPlugAction] Hot-plug task started:" << taskRef;
            qDebug() << "[VbdCreateAndPlugAction] Polling task to completion...";

            // Poll the task to completion
            pollToCompletion(taskRef, 60, 100);

            qDebug() << "[VbdCreateAndPlugAction] Task polling completed";
            setDescription(QString("'%1' attached and plugged successfully").arg(m_vdiName));
            setPercentComplete(100);
            qDebug() << "[VbdCreateAndPlugAction] Operation completed successfully (hot-plugged)";
        } else
        {
            // Can't hot-plug - check if VM is running and inform user
            qDebug() << "[VbdCreateAndPlugAction] Hot-plug not available, checking VM power state...";
            QString vmRef = m_vm->opaqueRef();
            QVariantMap vmRecord = XenAPI::VM::get_record(session, vmRef);
            QString powerState = vmRecord.value("power_state").toString();

            qDebug() << "[VbdCreateAndPlugAction] VM power state:" << powerState;

            if (powerState != "Halted")
            {
                // Determine message based on VBD type
                QString vbdType = m_vbdRecord.value("type").toString();
                QString instruction;

                if (vbdType == "CD")
                {
                    instruction = "The new DVD drive has been created. Please reboot the VM to access it.";
                } else
                {
                    instruction = "The new disk has been created. Please shut down and restart the VM to access it.";
                }

                qDebug() << "[VbdCreateAndPlugAction] Emitting user instruction:" << instruction;
                emit showUserInstruction(instruction);
            }

            setDescription(QString("'%1' attached (reboot required)").arg(m_vdiName));
            setPercentComplete(100);
            qDebug() << "[VbdCreateAndPlugAction] Operation completed successfully (reboot required)";
        }

        qDebug() << "[VbdCreateAndPlugAction] run() method completed successfully";

    } catch (const std::exception& e)
    {
        qCritical() << "[VbdCreateAndPlugAction] Exception caught:" << e.what();
        throw;
    }
}

bool VbdCreateAndPlugAction::isVMHVM() const
{
    try
    {
        XenSession* session = m_vm->connection()->getSession();
        QString vmRef = m_vm->opaqueRef();
        QVariantMap vmRecord = XenAPI::VM::get_record(session, vmRef);

        // HVM mode is indicated by HVM_boot_policy field
        return vmRecord.contains("HVM_boot_policy") &&
               !vmRecord.value("HVM_boot_policy").toString().isEmpty();
    } catch (...)
    {
        // If we can't determine, assume HVM (safer)
        return true;
    }
}

bool VbdCreateAndPlugAction::isVBDEmpty() const
{
    return m_vbdRecord.value("empty", false).toBool();
}
