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
#include "../../xenapi/xenapi_VIF.h"
#include "../../xenapi/xenapi_Network.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../../xencache.h"
#include "../../failure.h"
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
    , wizardDone_(false)
    , startAutomatically_(false)
    , httpClient_(nullptr)
{
    this->SetSafeToExit(false);
    this->SetCanCancel(false); // upload is in progress; cancel enabled only once wizard has finished
    
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
    this->AddApiMethodToRoleCheck("network.destroy");
    this->AddApiMethodToRoleCheck("vif.create");
    this->AddApiMethodToRoleCheck("vif.destroy");
    this->AddApiMethodToRoleCheck("http/put_import");
    if (!this->hostRef_.isEmpty())
        this->AddApiMethodToRoleCheck("vm.set_affinity");
}

ImportVmAction::~ImportVmAction()
{
    if (this->httpClient_)
        delete this->httpClient_;
}

void ImportVmAction::endWizard(bool startAutomatically, const QStringList& vifRefs)
{
    QMutexLocker locker(&this->mutex_);
    this->startAutomatically_ = startAutomatically;
    this->vifRefs_ = vifRefs;
    this->wizardDone_ = true;
    this->SetCanCancel(true); // safe to cancel once wizard configuration is complete
    this->waitCondition_.wakeAll();
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

    // Create HTTP client
    this->httpClient_ = new HttpClient(this);

    // Upload file with progress tracking
    QFileInfo fileInfo(this->filename_);
    bool success = this->httpClient_->putFile(
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
        this->setError(this->httpClient_->lastError());
        this->pollToCompletion(this->importTaskRef_);
        return QString();
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

void ImportVmAction::updateNetworks(const QString& vmRef, const QString& importTaskRef)
{
    if (this->vifRefs_.isEmpty())
        return;

    this->SetDescription(tr("Updating network configuration..."));

    XenAPI::Session* session = this->GetSession();

    // Get the list of VIFs on the imported VM
    QStringList vifList;
    try
    {
        vifList = XenAPI::VM::get_VIFs(session, vmRef);
    }
    catch (const std::exception& e)
    {
        qWarning() << "ImportVmAction: Failed to get VIFs:" << e.what();
        return;
    }

    if (vifList.isEmpty())
        return;

    // Collect the original network for each VIF before any remapping, so we can clean up
    // temporary import networks at the end (matches C# ImportVmAction pattern).
    QList<QPair<QString, QVariantMap>> vifOriginals; // (vifRef, record)
    QStringList originalNetworks;
    for (const QString& vifRef : vifList)
    {
        try
        {
            const QString netRef = XenAPI::VIF::get_network(session, vifRef);
            const QVariantMap rec = XenAPI::VIF::get_record(session, vifRef);
            vifOriginals.append({vifRef, rec});
            if (!netRef.isEmpty() && !originalNetworks.contains(netRef))
                originalNetworks << netRef;
        }
        catch (const std::exception& e)
        {
            qWarning() << "ImportVmAction: Could not read VIF" << vifRef << ":" << e.what();
            vifOriginals.append({vifRef, QVariantMap()});
        }
    }

    // Remap VIFs to the target networks.
    // vifRefs_ holds target network refs: one per VIF (matched by device field when available,
    // otherwise by index), or a single entry applied to all VIFs.
    for (int i = 0; i < vifOriginals.size(); ++i)
    {
        const QString& vifRef = vifOriginals.at(i).first;
        const QVariantMap& vifRecord = vifOriginals.at(i).second;
        if (vifRecord.isEmpty())
            continue;

        // Match target network by device number first, then fall back to positional index.
        const QString deviceStr = vifRecord.value("device").toString();
        QString targetNetwork;
        for (const QString& candidate : this->vifRefs_)
        {
            // vifRefs_ may later carry device:network pairs; for now treat as plain network refs
            Q_UNUSED(candidate);
        }
        // Plain network-refs list: use index if available, else first
        if (i < this->vifRefs_.size())
            targetNetwork = this->vifRefs_.at(i);
        else if (!this->vifRefs_.isEmpty())
            targetNetwork = this->vifRefs_.first();

        if (targetNetwork.isEmpty())
            continue;

        const QString currentNetwork = XenAPI::VIF::get_network(session, vifRef);
        if (currentNetwork == targetNetwork)
        {
            qDebug() << "ImportVmAction: VIF" << deviceStr << "already on target network, skipping";
            continue;
        }

        this->SetDescription(tr("Remapping network interface %1...").arg(deviceStr.isEmpty() ? QString::number(i + 1) : deviceStr));

        // Try VIF.move first (available from XenServer Ely / 7.2+, platform_version >= 2.1.1)
        bool moved = false;
        try
        {
            QString moveTask = XenAPI::VIF::async_move(session, vifRef, targetNetwork);
            this->pollToCompletion(moveTask);
            moved = true;
            qDebug() << "ImportVmAction: Moved VIF device" << deviceStr << "via VIF.move";
        }
        catch (const std::exception& e)
        {
            qDebug() << "ImportVmAction: VIF.move not available, falling back to destroy+recreate:" << e.what();
        }

        if (!moved)
        {
            // Fall back: destroy old VIF, create new one on target network
            try
            {
                XenAPI::VIF::destroy(session, vifRef);

                QVariantMap newRecord = vifRecord;
                newRecord["network"] = targetNetwork;
                newRecord.remove("uuid");
                newRecord.remove("ref");
                newRecord.remove("current_operations");
                newRecord.remove("allowed_operations");
                newRecord.remove("status_code");
                newRecord.remove("status_detail");
                newRecord.remove("runtime_properties");
                if (newRecord.value("MAC_autogenerated").toBool())
                    newRecord["MAC"] = QString(); // let server autogenerate

                XenAPI::VIF::create(session, newRecord);
                qDebug() << "ImportVmAction: Recreated VIF device" << deviceStr << "on target network";
            }
            catch (const std::exception& e)
            {
                qWarning() << "ImportVmAction: Failed to recreate VIF device" << deviceStr << ":" << e.what();
            }
        }
    }

    // Destroy any temporary networks created by this import task that no longer have VIFs or PIFs
    // (matches C# ImportVmAction network cleanup)
    if (!importTaskRef.isEmpty())
    {
        for (const QString& netRef : originalNetworks)
        {
            try
            {
                QVariantMap netRecord = XenAPI::Network::get_record(session, netRef);
                const QVariantMap otherConfig = netRecord.value("other_config").toMap();
                if (otherConfig.value(IMPORT_TASK).toString() != importTaskRef)
                    continue;

                // Only destroy if no VIFs and no PIFs remain
                if (!netRecord.value("VIFs").toList().isEmpty() || !netRecord.value("PIFs").toList().isEmpty())
                {
                    qDebug() << "ImportVmAction: Temp network" << netRef << "still has VIFs/PIFs, keeping";
                    continue;
                }

                qDebug() << "ImportVmAction: Destroying empty temporary import network" << netRef;
                XenAPI::Network::destroy(session, netRef);
            }
            catch (const std::exception& e)
            {
                qDebug() << "ImportVmAction: Temp network cleanup skipped for" << netRef << ":" << e.what();
            }
        }
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

        // Notify the wizard that the VM has been created on the server.
        // The wizard will close the upload progress dialog and advance to
        // the network configuration page.  The signal is emitted from the
        // worker thread; the wizard connects with Qt::QueuedConnection so
        // the slot runs on the UI thread.
        emit this->vmDiscovered(this->vmRef_);

        // Wait for wizard to finish
        this->SetDescription(isTemplate ? tr("Waiting for template configuration...")
                                        : tr("Waiting for VM configuration..."));

        QMutexLocker locker(&this->mutex_);
        while (!this->wizardDone_ && !this->IsCancelled())
        {
            this->waitCondition_.wait(&this->mutex_, 1000);
        }
        locker.unlock();

        if (this->IsCancelled())
        {
            this->setState(OperationState::Cancelled);
            return;
        }

        // Update networks
        if (!this->vifRefs_.isEmpty())
        {
            this->updateNetworks(this->vmRef_, this->importTaskRef_);
        }

        this->SetDescription(isTemplate ? tr("Template import complete")
                                        : tr("VM import complete"));

        // Start the VM automatically if requested and it is not a template
        if (this->startAutomatically_ && !isTemplate && !this->IsCancelled())
        {
            this->SetDescription(tr("Starting imported VM..."));
            try
            {
                // Check power state — use resume for suspended VMs, start for halted
                QString powerState = XenAPI::VM::get_power_state(this->GetSession(), this->vmRef_);
                QString startTaskRef;
                if (powerState.compare("Suspended", Qt::CaseInsensitive) == 0)
                {
                    startTaskRef = XenAPI::VM::async_resume(this->GetSession(), this->vmRef_, false, false);
                }
                else
                {
                    startTaskRef = XenAPI::VM::async_start(this->GetSession(), this->vmRef_, false, false);
                }
                this->pollToCompletion(startTaskRef);
                this->SetDescription(tr("VM started"));
            }
            catch (const Failure& f)
            {
                // Collect per-host boot reasons for NO_HOSTS_AVAILABLE
                // (mirrors C# VMOperationCommand::StartDiagnosisForm)
                QMap<QString, QString> hostReasons;
                if (f.errorCode() == QLatin1String(Failure::NO_HOSTS_AVAILABLE))
                {
                    auto* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
                    if (cache)
                    {
                        for (const auto& obj : cache->GetAll(XenObjectType::Host))
                        {
                            if (auto host = qSharedPointerDynamicCast<Host>(obj))
                            {
                                try
                                {
                                    XenAPI::VM::assert_can_boot_here(this->GetSession(),
                                                                     this->vmRef_,
                                                                     host->OpaqueRef());
                                }
                                catch (const Failure& hf)
                                {
                                    hostReasons[host->GetName()] = hf.message();
                                }
                                catch (const std::exception& he)
                                {
                                    hostReasons[host->GetName()] = QString::fromUtf8(he.what());
                                }
                            }
                        }
                    }
                }
                emit this->vmStartFailed(this->vmRef_, f.errorCode(), hostReasons);
                qWarning() << "ImportVmAction: Failed to auto-start VM:" << f.message();
            }
            catch (const std::exception& e)
            {
                // Non-fatal: report but don't fail the whole import
                emit this->vmStartFailed(this->vmRef_, QString(), {});
                qWarning() << "ImportVmAction: Failed to auto-start VM:" << e.what();
            }
        }

        this->setState(OperationState::Completed);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to configure imported VM: %1").arg(e.what()));
        this->setState(OperationState::Failed);
    }
}
