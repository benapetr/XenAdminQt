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

#include "crosspoolmigratecommand.h"
#include "../../dialogs/crosspoolmigratewizard.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"

CrossPoolMigrateCommand::CrossPoolMigrateCommand(MainWindow* mainWindow, CrossPoolMigrateWizard::WizardMode mode, QObject* parent) : VMCommand(mainWindow, parent), m_mode(mode)
{
}

bool CrossPoolMigrateCommand::CanRun() const
{
    QList<QSharedPointer<VM>> vms;
    QStringList selection = this->GetSelection();
    QSharedPointer<VM> baseVm = this->getVM();
    if (selection.isEmpty())
    {
        if (baseVm)
            vms.append(baseVm);
    } else
    {
        XenConnection* conn = baseVm ? baseVm->GetConnection() : nullptr;
        XenCache* cache = conn ? conn->GetCache() : nullptr;
        if (cache)
        {
            for (const QString& ref : selection)
            {
                QSharedPointer<VM> vm = cache->ResolveObject<VM>("vm", ref);
                if (vm)
                    vms.append(vm);
            }
        }
    }

    if (vms.isEmpty())
        return false;

    XenConnection* connection = vms.first()->GetConnection();
    if (!connection || !connection->IsConnected())
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    for (const QSharedPointer<VM>& vmItem : vms)
    {
        if (!vmItem)
            return false;

        if (!vmItem->GetAllowedOperations().contains("migrate_send"))
            return false;

        QStringList vbdRefs = vmItem->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
            QString vdiRef = vbdData.value("VDI").toString();
            if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
                continue;

            QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
            QString srRef = vdiData.value("SR").toString();
            if (srRef.isEmpty() || srRef == "OpaqueRef:NULL")
                continue;

            QSharedPointer<SR> srObj = cache->ResolveObject<SR>("sr", srRef);
            if (srObj && srObj->HBALunPerVDI())
                return false;
        }
    }

    return true;
}

void CrossPoolMigrateCommand::Run()
{
    QList<QSharedPointer<VM>> vms;
    QStringList selection = this->GetSelection();
    QSharedPointer<VM> baseVm = this->getVM();
    if (selection.isEmpty())
    {
        if (baseVm)
            vms.append(baseVm);
    }
    else
    {
        XenConnection* conn = baseVm ? baseVm->GetConnection() : nullptr;
        XenCache* cache = conn ? conn->GetCache() : nullptr;
        if (cache)
        {
            for (const QString& ref : selection)
            {
                QSharedPointer<VM> vm = cache->ResolveObject<VM>("vm", ref);
                if (vm)
                    vms.append(vm);
            }
        }
    }

    if (vms.isEmpty())
        return;

    CrossPoolMigrateWizard wizard(this->mainWindow(), vms, m_mode);
    wizard.exec();
}

QString CrossPoolMigrateCommand::MenuText() const
{
    if (m_mode == CrossPoolMigrateWizard::WizardMode::Copy)
        return tr("Cross Pool Copy...");
    if (m_mode == CrossPoolMigrateWizard::WizardMode::Move)
        return tr("Cross Pool Move...");
    return tr("Cross Pool Migrate...");
}
