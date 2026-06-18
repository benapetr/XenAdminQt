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

#include "importvmaction.h"
#include "../../network/httpclient.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../../xencache.h"
#include "../../sr.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../vm.h"
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QDebug>

const QString ImportVmAction::IMPORT_TASK = "import_task";

ImportVmAction::ImportVmAction(XenConnection* connection, const QString& hostRef, const QString& filename, const QString& srRef, QObject* parent)
    : AsyncOperation(connection, tr("Importing VM"), tr("Preparing import..."), parent)
    , hostRef_(hostRef)
    , filename_(filename)
    , srRef_(srRef)
{
    this->SetSafeToExit(false);
    this->SetCanCancel(true);
    
    QFileInfo fileInfo(filename);
    QString poolName = "XenServer"; // Default
    
    if (this->GetConnection())
    {
        QSharedPointer<Pool> pool = this->GetConnection()->GetCache()->GetPoolOfOne();
        if (pool && pool->IsValid())
            poolName = pool->GetName();
    }
    
    this->SetTitle(tr("Importing '%1' to %2").arg(fileInfo.fileName(), poolName));

    // RBAC dependencies (matches C# ImportVmAction)
    this->AddApiMethodToRoleCheck("vm.set_name_label");
    this->AddApiMethodToRoleCheck("http/put_import");
    this->AddApiMethodToRoleCheck("sr.scan");
    if (!this->hostRef_.isEmpty())
        this->AddApiMethodToRoleCheck("vm.set_affinity");
}

int ImportVmAction::vmsWithName(const QString& name)
{
    if (!this->GetConnection() || !this->GetConnection()->GetCache())
        return 0;

    int count = 0;
    QList<QSharedPointer<XenObject>> allVMs = this->GetConnection()->GetCache()->GetAll(XenObjectType::VM);
    
    for (const QSharedPointer<XenObject>& obj : allVMs)
    {
        if (auto vm = qSharedPointerDynamicCast<VM>(obj))
        {
            if (vm->IsValid() && vm->GetName() == name)
                count++;
        }
    }
    
    return count;
}

QString ImportVmAction::defaultVMName(const QString& vmName)
{
    QString name = vmName;
    int count = this->vmsWithName(vmName);
    
    if (count > 1)
    {
        int i = 0;
        while (this->vmsWithName(name) > 0)
        {
            i++;
            name = QString("%1 (%2)").arg(vmName).arg(i);
        }
    }
    
    return name;
}

QString ImportVmAction::uploadFile()
{
    if (!this->GetConnection() || !this->GetSession())
    {
        this->setError("No connection or session available");
        return QString();
    }

    // Determine target host
    QString targetHost;
    if (!this->hostRef_.isEmpty())
    {
        QSharedPointer<Host> host = this->GetConnection()->GetCache()->ResolveObject<Host>(XenObjectType::Host, this->hostRef_);
        if (host && host->IsValid())
            targetHost = host->GetAddress();
    }
    
    if (targetHost.isEmpty())
    {
        // Use SR's storage host or pool master
        QSharedPointer<SR> sr = this->GetConnection()->GetCache()->ResolveObject<SR>(this->srRef_);
        if (sr && sr->IsValid())
        {
            // Try to get SR's preferred host
            QSharedPointer<Host> host = sr->GetHost();
            if (host && host->IsValid())
                targetHost = host->GetAddress();
        }
    }
    
    if (targetHost.isEmpty())
    {
        // Fall back to connection hostname
        targetHost = this->GetConnection()->GetHostname();
    }

    qDebug() << "ImportVmAction: Uploading to" << targetHost;

    // Create task
    try
    {
        QString taskRef = XenAPI::Task::Create(this->GetSession(),
                                               "put_import_task",
                                               targetHost);
        this->SetRelatedTaskRef(taskRef);
        this->importTaskRef_ = taskRef;
        
        qDebug() << "ImportVmAction: Created task" << taskRef;
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to create import task: %1").arg(e.what()));
        return QString();
    }

    // Prepare query parameters
    QMap<QString, QString> params;
    params["task_id"] = this->importTaskRef_;
    params["session_id"] = this->GetSession()->GetSessionID();
    params["sr_id"] = this->srRef_;
    params["restore"] = "false";
    params["force"] = "false";

    // The upload runs on the AsyncOperation worker thread, so keep the HTTP client
    // local to that thread rather than parenting it to this UI-affine QObject.
    HttpClient httpClient;

    // Upload file with progress tracking
    QFileInfo fileInfo(this->filename_);
    bool success = httpClient.putFile(
        this->filename_,
        targetHost,
        "/import",
        params,
        [this, fileInfo](int percent) {
            QString poolRef = this->GetConnection()->GetCache()->GetPoolRef();
            this->SetDescription(tr("Uploading %1 to %2 (%3%)")
                                 .arg(fileInfo.fileName())
                                 .arg(poolRef)
                                 .arg(percent));
            this->SetPercentComplete(percent);
        },
        [this]() -> bool {
            return this->IsCancelled();
        }
    );

    if (!success)
    {
        const QString httpError = httpClient.lastError();

        // XenServer may close the upload connection without returning a final HTTP
        // response even though the import task continues and completes successfully.
        // Treat the XenAPI task as authoritative before reporting the transport error.
        try
        {
            this->pollToCompletion(this->importTaskRef_);
        }
        catch (const std::exception& e)
        {
            this->setError(QString("%1: %2").arg(httpError, e.what()));
            return QString();
        }

        if (this->IsFailed() || this->IsCancelled())
            return QString();

        const QString result = this->GetResult();
        qWarning() << "ImportVmAction: HTTP upload response was unavailable, but the import task succeeded:"
                   << httpError;
        return result;
    }

    // Poll task to completion
    try
    {
        this->pollToCompletion(this->importTaskRef_);
        QString result = this->GetResult();
        qDebug() << "ImportVmAction: Upload completed, VM ref:" << result;
        return result;
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Import failed: %1").arg(e.what()));
        return QString();
    }
}

void ImportVmAction::run()
{
    this->SetSafeToExit(false);
    this->SetDescription(tr("Preparing import..."));

    // Upload file; the returned string is the task result (VM OpaqueRef or empty)
    QString vmRef = this->uploadFile();
    if (this->IsCancelled())
    {
        this->setState(OperationState::Cancelled);
        return;
    }
    if (vmRef.isEmpty() && this->IsFailed())
        return; // uploadFile already called setState(Failed)

    // ── Discover VM from cache ────────────────────────────────────────────
    // Primary: task result is the OpaqueRef — wait for it to land in cache.
    // Fallback: scan all VMs for other_config["import_task"] == task_ref
    //           (matches C# ImportVmAction vm-scan pattern).
    this->SetDescription(tr("Waiting for VM to be created..."));

    const QString taskRef = this->importTaskRef_; // preserved before pollToCompletion destroys the task

    auto findVmByTaskRef = [this, &taskRef]() -> QString
    {
        if (taskRef.isEmpty() || !this->GetConnection() || !this->GetConnection()->GetCache())
            return QString();
        const auto allObjs = this->GetConnection()->GetCache()->GetAll(XenObjectType::VM);
        for (const auto& obj : allObjs)
        {
            if (auto vm = qSharedPointerDynamicCast<VM>(obj))
            {
                if (!vm->IsValid())
                    continue;
                const QVariantMap oc = vm->GetOtherConfig();
                if (oc.value(ImportVmAction::IMPORT_TASK).toString() == taskRef)
                    return vm->OpaqueRef();
            }
        }
        return QString();
    };

    // Poll up to 10 seconds for VM to materialise in cache
    for (int attempts = 0; attempts < 100 && !this->IsCancelled(); ++attempts)
    {
        // Try the direct ref first
        if (!vmRef.isEmpty() && this->GetConnection() && this->GetConnection()->GetCache())
        {
            QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
            if (vm && vm->IsValid())
            {
                this->vmRef_ = vmRef;
                break;
            }
        }

        // Fallback: scan by import_task other_config key
        const QString foundRef = findVmByTaskRef();
        if (!foundRef.isEmpty())
        {
            qDebug() << "ImportVmAction: found VM by import_task scan:" << foundRef;
            this->vmRef_ = foundRef;
            break;
        }

        QThread::msleep(100);
    }

    if (this->IsCancelled())
    {
        this->setState(OperationState::Cancelled);
        return;
    }

    if (this->vmRef_.isEmpty())
    {
        this->setError("VM was not created properly");
        this->setState(OperationState::Failed);
        return;
    }

    // Get VM record
    try
    {
        QVariantMap vmRecord = XenAPI::VM::get_record(this->GetSession(), this->vmRef_);
        bool isTemplate = vmRecord.value("is_a_template").toBool();

        // Falcon+: if a default template with this name already exists (import_task mismatch),
        // the import landed on an existing template — fail early (matches C# ImportVmAction.Run)
        if (isTemplate && vmRecord.value("is_default_template").toBool())
        {
            // Compare platform_version against Falcon threshold (2.2.50)
            bool isFalconOrGreater = true; // assume true if we can't determine
            const auto allHosts = this->GetConnection()->GetCache()->GetAll(XenObjectType::Host);
            for (const auto& obj : allHosts)
            {
                if (auto host = qSharedPointerDynamicCast<Host>(obj))
                {
                    if (host->IsValid())
                    {
                        const QString pv = host->PlatformVersion();
                        if (!pv.isEmpty())
                        {
                            // Simple major.minor.patch comparison
                            const QStringList pvParts = pv.split('.');
                            const QStringList minParts = QString("2.2.50").split('.');
                            int cmp = 0;
                            for (int pi = 0; pi < qMax(pvParts.size(), minParts.size()) && cmp == 0; ++pi)
                            {
                                int pa = pi < pvParts.size() ? pvParts.at(pi).toInt() : 0;
                                int pb = pi < minParts.size() ? minParts.at(pi).toInt() : 0;
                                if (pa < pb) cmp = -1;
                                else if (pa > pb) cmp = 1;
                            }
                            isFalconOrGreater = (cmp >= 0);
                        }
                        break; // only need one host in the pool
                    }
                }
            }

            if (isFalconOrGreater)
            {
                QVariantMap otherConfig = vmRecord.value("other_config").toMap();
                const QString taskInRecord = otherConfig.value(ImportVmAction::IMPORT_TASK).toString();
                if (taskInRecord.isEmpty() || taskInRecord != this->importTaskRef_)
                {
                    this->setError(tr("A default template with the same name already exists. "
                                      "The import cannot continue."));
                    this->setState(OperationState::Failed);
                    return;
                }
            }
        }

        // Update VM name to avoid conflicts
        QString currentName = vmRecord.value("name_label").toString();
        QString newName = this->defaultVMName(currentName);
        XenAPI::VM::set_name_label(this->GetSession(), this->vmRef_, newName);

        // Set affinity if specified
        if (!this->hostRef_.isEmpty() && !isTemplate)
        {
            XenAPI::VM::set_affinity(this->GetSession(), this->vmRef_, this->hostRef_);
        }

        // Preserve the VM, VIF and network configuration created by XenServer.
        // Further customization can be performed after the background import completes.
        this->SetDescription(isTemplate ? tr("Template import complete")
                                        : tr("VM import complete"));
        this->setState(OperationState::Completed);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to configure imported VM: %1").arg(e.what()));
        this->setState(OperationState::Failed);
    }
}
