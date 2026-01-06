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

#include "createvmfastaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

CreateVMFastAction::CreateVMFastAction(XenConnection* connection,
                                       QSharedPointer<VM> templateVM,
                                       QObject* parent)
    : AsyncOperation(connection,
                     QString("Creating VM from template '%1'").arg(templateVM ? templateVM->GetName() : ""),
                     "",
                     parent),
      m_template(templateVM)
{
    if (!m_template)
        throw std::invalid_argument("Template VM cannot be null");
}

void CreateVMFastAction::run()
{
    try
    {
        QString originalName = m_template->GetName();

        // Clone template with hidden name (matches C# MakeHiddenName)
        SetDescription(QString("Cloning template '%1'").arg(originalName));
        QString hiddenName = QString("__gui__%1").arg(originalName);

        QString taskRef = XenAPI::VM::async_clone(GetSession(), m_template->OpaqueRef(), hiddenName);
        pollToCompletion(taskRef, 0, 80);

        QString newVmRef = GetResult();
        qDebug() << "CreateVMFastAction: Cloned VM ref:" << newVmRef;

        // Provision the VM
        SetDescription("Provisioning VM");
        taskRef = XenAPI::VM::async_provision(GetSession(), newVmRef);
        pollToCompletion(taskRef, 80, 90);

        // Generate unique name and set it
        SetDescription("Saving VM properties");
        QString newName = generateUniqueName(originalName);
        XenAPI::VM::set_name_label(GetSession(), newVmRef, newName);

        // Store the created VM ref as result
        SetResult(newVmRef);

        SetDescription(QString("VM '%1' created successfully").arg(newName));

    } catch (const std::exception& e)
    {
        setError(QString("Failed to create VM: %1").arg(e.what()));
    }
}

QString CreateVMFastAction::generateUniqueName(const QString& baseName)
{
    // Get all existing VMs
    QList<QVariantMap> allVMs = GetConnection()->GetCache()->GetAllData("vm");

    // Build set of existing names
    QSet<QString> existingNames;
    for (const QVariantMap& vmData : allVMs)
    {
        existingNames.insert(vmData.value("name_label").toString());
    }

    // Try base name first
    if (!existingNames.contains(baseName))
    {
        return baseName;
    }

    // Try numbered variants
    for (int i = 1; i < 1000; i++)
    {
        QString candidate = QString("%1 (%2)").arg(baseName).arg(i);
        if (!existingNames.contains(candidate))
        {
            return candidate;
        }
    }

    // Fallback: append timestamp
    return QString("%1 (%2)").arg(baseName).arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss"));
}
