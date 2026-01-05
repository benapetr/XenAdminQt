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

#include "exportvmaction.h"
#include "../../network/httpclient.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../vm.h"
#include "../../pool.h"
#include "../../host.h"
#include "../../../xencache.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

ExportVmAction::ExportVmAction(XenConnection* connection,
                               const QString& hostRef,
                               const QString& vmRef,
                               const QString& filename,
                               bool verify,
                               QObject* parent)
    : AsyncOperation(connection, tr("Exporting VM"), tr("Preparing export..."), parent)
    , hostRef_(hostRef)
    , vmRef_(vmRef)
    , filename_(filename)
    , verify_(verify)
    , httpClient_(nullptr)
    , progressThread_(nullptr)
{
    this->setSafeToExit(false);
    
    // Get VM name for title
    QString vmName = "VM";
    
    if (this->connection() && this->connection()->GetCache())
    {
        QSharedPointer<VM> vm = this->connection()->GetCache()->ResolveObject<VM>("vm", vmRef);
        if (vm && vm->IsValid())
            vmName = vm->GetName();
    }
    
    this->setTitle(tr("Export %1 to backup file").arg(vmName));
}

ExportVmAction::~ExportVmAction()
{
    if (this->httpClient_)
        delete this->httpClient_;
        
    if (this->progressThread_)
    {
        this->progressThread_->quit();
        this->progressThread_->wait();
        delete this->progressThread_;
    }
}

void ExportVmAction::progressPoll()
{
    try
    {
        // Poll task progress
        this->pollToCompletion(0, this->verify_ ? 50 : 95);
    }
    catch (const std::exception& e)
    {
        if (this->exception_.isEmpty())
            this->exception_ = e.what();
    }
}

void ExportVmAction::run()
{
    this->setSafeToExit(false);
    this->setDescription(tr("Export in progress..."));

    if (!this->connection() || !this->session())
    {
        this->setError("No connection or session available");
        this->setState(OperationState::Failed);
        return;
    }

    // Get VM name and UUID
    QString vmName = "VM";
    QString vmUuid;
    
    try
    {
        QVariantMap vmRecord = XenAPI::VM::get_record(this->session(), this->vmRef_);
        vmName = vmRecord.value("name_label").toString();
        vmUuid = vmRecord.value("uuid").toString();
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to get VM details: %1").arg(e.what()));
        this->setState(OperationState::Failed);
        return;
    }

    // Determine target host
    QString targetHost = this->connection()->GetHostname();
    if (!this->hostRef_.isEmpty())
    {
        QSharedPointer<Host> host = this->connection()->GetCache()->ResolveObject<Host>("host", this->hostRef_);
        if (host && host->IsValid())
            targetHost = host->GetAddress();
    }

    qDebug() << "ExportVmAction: Downloading from" << targetHost;

    // Create export task
    QString taskRef;
    try
    {
        taskRef = XenAPI::Task::Create(this->session(),
                                       "export",
                                       QString("Exporting %1 to backup file").arg(vmName));
        this->setRelatedTaskRef(taskRef);
        
        qDebug() << "ExportVmAction: Created task" << taskRef;
    }
    catch (const std::exception& e)
    {
        this->setError(QString("Failed to create export task: %1").arg(e.what()));
        this->setState(OperationState::Failed);
        return;
    }

    // Start progress polling thread
    this->progressThread_ = QThread::create([this]() { this->progressPoll(); });
    this->progressThread_->start();

    // Prepare query parameters
    QMap<QString, QString> params;
    params["task_id"] = taskRef;
    params["session_id"] = this->session()->getSessionId();
    params["uuid"] = vmUuid;

    // Create HTTP client
    this->httpClient_ = new HttpClient(this);

    // Download file with progress tracking
    QString tmpFile = this->filename_ + ".tmp";
    
    bool success = this->httpClient_->getFile(
        targetHost,
        "/export",
        params,
        tmpFile,
        [this, vmName](qint64 bytes) {
            // Convert bytes to megabytes for display
            double mb = bytes / (1024.0 * 1024.0);
            this->setDescription(tr("Downloading %1 (%2 MB)")
                                 .arg(vmName)
                                 .arg(mb, 0, 'f', 1));
        },
        [this]() -> bool {
            return this->isCancelled();
        }
    );

    // Wait for progress thread to finish
    this->progressThread_->quit();
    this->progressThread_->wait();

    // Check for errors
    if (!success)
    {
        QFile::remove(tmpFile);
        
        if (!this->exception_.isEmpty())
            this->setError(this->exception_);
        else
            this->setError(this->httpClient_->lastError());
        
        this->setState(OperationState::Failed);
        return;
    }

    if (this->isCancelled())
    {
        QFile::remove(tmpFile);
        this->setState(OperationState::Cancelled);
        return;
    }

    // Verify file if requested
    if (this->verify_)
    {
        this->setDescription(tr("Verifying export..."));
        this->setPercentComplete(50);
        
        // TODO: Implement TAR verification (requires archive library)
        // For now, just check file exists and has size > 0
        QFileInfo fileInfo(tmpFile);
        if (!fileInfo.exists() || fileInfo.size() == 0)
        {
            QFile::remove(tmpFile);
            this->setError("Export verification failed: file is empty or missing");
            this->setState(OperationState::Failed);
            return;
        }
        
        this->setPercentComplete(95);
    }

    // Rename temp file to final name
    if (QFile::exists(this->filename_))
        QFile::remove(this->filename_);
    
    if (!QFile::rename(tmpFile, this->filename_))
    {
        QFile::remove(tmpFile);
        this->setError("Failed to rename temporary file");
        this->setState(OperationState::Failed);
        return;
    }

    this->setDescription(tr("Export successful"));
    this->setPercentComplete(100);
    this->setState(OperationState::Completed);
}
