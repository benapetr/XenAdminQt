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

#include "vmmigrateaction.h"
#include "../../../xen/network/connection.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <stdexcept>

VMMigrateAction::VMMigrateAction(XenConnection* connection,
                                 const QString& vmRef,
                                 const QString& destinationHostRef,
                                 QObject* parent)
    : AsyncOperation(connection,
                     QString("Migrating VM"),
                     QString("Migrating VM to another host"),
                     parent),
      m_vmRef(vmRef),
      m_destinationHostRef(destinationHostRef)
{
}

void VMMigrateAction::run()
{
    try
    {
        setPercentComplete(0);
        setDescription("Preparing migration...");

        // Get VM and host data from cache
        QVariantMap vmData = connection()->GetCache()->ResolveObjectData("vm", m_vmRef);
        if (vmData.isEmpty())
        {
            throw std::runtime_error("VM not found in cache");
        }

        QVariantMap hostData = connection()->GetCache()->ResolveObjectData("host", m_destinationHostRef);
        if (hostData.isEmpty())
        {
            throw std::runtime_error("Destination host not found in cache");
        }

        QString vmName = vmData.value("name_label").toString();
        QString hostName = hostData.value("name_label").toString();

        // Check if VM is resident on a host
        QString residentOnRef = vmData.value("resident_on").toString();
        QString sourceHostName;

        if (!residentOnRef.isEmpty() && residentOnRef != "OpaqueRef:NULL")
        {
            QVariantMap residentHostData = connection()->GetCache()->ResolveObjectData("host", residentOnRef);
            sourceHostName = residentHostData.value("name_label").toString();

            setTitle(QString("Migrating %1 from %2 to %3")
                         .arg(vmName)
                         .arg(sourceHostName)
                         .arg(hostName));
        } else
        {
            setTitle(QString("Migrating %1 to %2").arg(vmName).arg(hostName));
        }

        setPercentComplete(10);
        setDescription(QString("Migrating %1 to %2...").arg(vmName).arg(hostName));

        // Start the migration with live migration enabled
        QVariantMap options;
        options["live"] = "true";

        QString taskRef = XenAPI::VM::async_pool_migrate(session(), m_vmRef, m_destinationHostRef, options);

        // Poll the task to completion
        pollToCompletion(taskRef, 10, 100);

        setDescription(QString("VM migrated successfully to %1").arg(hostName));

    } catch (const std::exception& e)
    {
        QString errorMsg = QString::fromUtf8(e.what());

        // Check for specific error about VDI_MISSING (tools ISO issue)
        if (errorMsg.contains("VM_MIGRATE_FAILED") && errorMsg.contains("VDI_MISSING"))
        {
            setError("Migration failed: Please eject any mounted ISOs (especially XenServer Tools) and try again");
        } else
        {
            setError(QString("Failed to migrate VM: %1").arg(errorMsg));
        }
    }
}
