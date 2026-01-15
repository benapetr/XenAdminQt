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

#include "setdefaultsrcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xen/pool.h"
#include "xen/sr.h"
#include "xen/actions/pool/setsrasdefaultaction.h"
#include "xencache.h"
#include <QMessageBox>

SetDefaultSRCommand::SetDefaultSRCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

bool SetDefaultSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    XenConnection* connection = sr->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    // C# SR.HasPBDs()
    if (sr->GetPBDs().isEmpty())
        return false;

    // C# SR.IsDefaultSr(sr)
    QStringList poolRefs = cache ? cache->GetAllRefs("pool") : QStringList();
    if (!poolRefs.isEmpty())
    {
        QSharedPointer<Pool> pool = cache->ResolveObject<Pool>("pool", poolRefs.first());
        if (pool && pool->GetDefaultSRRef() == sr->OpaqueRef())
            return false;
    }

    // C# SR.SupportsVdiCreate() (ISO SRs disallowed)
    if (sr->ContentType() == "iso")
        return false;
    if (!sr->AllowedOperations().contains("vdi_create"))
        return false;

    // C# (sr.shared || HostCount <= 1)
    int hostCount = cache->GetAllRefs("host").size();
    if (!sr->IsShared() && hostCount > 1)
        return false;

    // C# !HelpersGUI.GetActionInProgress(sr)
    if (!sr->CurrentOperations().isEmpty())
        return false;
    if (sr->IsLocked())
        return false;

    return true;
}

void SetDefaultSRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Set as Default Storage Repository",
                                    QString("Set storage repository '%1' as the default storage repository?\n\n"
                                            "This will be used as the default location for new virtual disks.")
                                        .arg(srName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->ShowStatusMessage(QString("Setting '%1' as default storage repository...").arg(srName));

        XenConnection* connection = sr->GetConnection();
        if (!connection || !connection->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Set Default Storage Repository Failed",
                                 "Not connected to XenServer.");
            return;
        }

        QStringList poolRefs = connection->GetCache()->GetAllRefs("pool");
        if (poolRefs.isEmpty())
        {
            QMessageBox::warning(this->mainWindow(), "Set Default Storage Repository Failed",
                                 "No pool found.");
            return;
        }

        QSharedPointer<Pool> pool = connection->GetCache()->ResolveObject<Pool>("pool", poolRefs.first());
        if (!pool || !pool->IsValid())
        {
            QMessageBox::warning(this->mainWindow(), "Set Default Storage Repository Failed",
                                 "Invalid pool object.");
            return;
        }

        SetSrAsDefaultAction* action = new SetSrAsDefaultAction(pool, srRef, nullptr);
        OperationManager::instance()->RegisterOperation(action);

        this->m_pendingSrName = srName;
        connect(action, &AsyncOperation::completed, this, &SetDefaultSRCommand::onSetDefaultCompleted);
        connect(action, &AsyncOperation::failed, this, &SetDefaultSRCommand::onSetDefaultFailed);

        action->RunAsync();
    }
}

QString SetDefaultSRCommand::MenuText() const
{
    return "Set as Default";
}

void SetDefaultSRCommand::onSetDefaultCompleted()
{
    if (this->mainWindow())
    {
        this->mainWindow()->ShowStatusMessage(QString("Storage repository '%1' set as default successfully").arg(m_pendingSrName), 5000);
    }

    if (auto* op = qobject_cast<AsyncOperation*>(sender()))
        op->deleteLater();
}

void SetDefaultSRCommand::onSetDefaultFailed(const QString& error)
{
    if (this->mainWindow())
    {
        QMessageBox::warning(this->mainWindow(), "Set Default Storage Repository Failed",
                             QString("Failed to set storage repository '%1' as default: %2")
                                 .arg(m_pendingSrName, error));
        this->mainWindow()->ShowStatusMessage("Set default SR failed", 5000);
    }

    if (auto* op = qobject_cast<AsyncOperation*>(sender()))
        op->deleteLater();
}
