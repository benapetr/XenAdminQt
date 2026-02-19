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
    
    QFileInfo fileInfo(filename);
    QString poolName = "XenServer"; // Default
    
    if (this->GetConnection())
    {
        QSharedPointer<Pool> pool = this->GetConnection()->GetCache()->GetPoolOfOne();
        if (pool && pool->IsValid())
            poolName = pool->GetName();
    }
    
    this->SetTitle(tr("Importing '%1' to %2").arg(fileInfo.fileName(), poolName));
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

    // TODO: Implement network VIF updates once XenAPI::VM::get_VIFs, XenAPI::VIF, and XenAPI::Network methods are fully implemented
    // This requires: VM::get_VIFs, VIF::get_network, VIF::destroy, VIF::create, Network::get_record, Network::destroy
    qDebug() << "ImportVmAction: Network updates deferred (VIF XenAPI methods not yet implemented)";
}

void ImportVmAction::run()
{
    this->SetSafeToExit(false);
    this->SetDescription(tr("Preparing import..."));

    // Upload file
    QString vmRef = this->uploadFile();
    if (vmRef.isEmpty())
    {
        this->setState(OperationState::Failed);
        return;
    }

    // Wait for VM object to appear in cache
    this->SetDescription(tr("Waiting for VM to be created..."));
    int attempts = 0;
    while (attempts < 100 && !this->IsCancelled())
    {
        QSharedPointer<VM> vm = this->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
        if (vm && vm->IsValid())
        {
            this->vmRef_ = vmRef;
            break;
        }
        QThread::msleep(100);
        attempts++;
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

        // Update VM name to avoid conflicts
        QString currentName = vmRecord.value("name_label").toString();
        QString newName = this->defaultVMName(currentName);
        XenAPI::VM::set_name_label(this->GetSession(), this->vmRef_, newName);

        // Set affinity if specified
        if (!this->hostRef_.isEmpty() && !isTemplate)
        {
            XenAPI::VM::set_affinity(this->GetSession(), this->vmRef_, this->hostRef_);
        }

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

        // TODO: If startAutomatically_ is true and VM is not a template, start the VM
        // This requires implementing VMStartAction first

        this->setState(OperationState::Completed);
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to configure imported VM: %1").arg(e.what()));
        this->setState(OperationState::Failed);
    }
}
