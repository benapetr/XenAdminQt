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


#include "installpvtoolsaction.h"
#include "../sr/srrepairaction.h"
#include "../../network/connection.h"
#include "../../sr.h"
#include "../../vbd.h"
#include "../../vdi.h"
#include "../../vm.h"
#include "../../xenapi/xenapi_VBD.h"
#include "../../../xencache.h"

InstallPVToolsAction::InstallPVToolsAction(QSharedPointer<VM> vm, QObject* parent)
    : AsyncOperation(vm ? vm->GetConnection() : nullptr,
                     tr("Installing Guest Tools on VM %1").arg(vm ? vm->GetName() : QString()),
                     tr("Installing Guest Tools..."), parent),
      m_vm(vm)
{
    if (vm)
        this->SetVM(vm);

    // C# RBAC dependencies
    XenCache* cache = vm && vm->GetConnection() ? vm->GetConnection()->GetCache() : nullptr;
    if (cache)
    {
        for (const QSharedPointer<SR>& sr : cache->GetAll<SR>())
        {
            if (sr && sr->IsToolsSR() && sr->IsBroken())
            {
                this->AddApiMethodToRoleCheck("pbd.plug");
                this->AddApiMethodToRoleCheck("pbd.create");
                break;
            }
        }
    }
    this->AddApiMethodToRoleCheck("vbd.eject");
    this->AddApiMethodToRoleCheck("vbd.insert");
}

QSharedPointer<VDI> InstallPVToolsAction::FindToolsIso(XenConnection* connection)
{
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return QSharedPointer<VDI>();

    for (const QSharedPointer<SR>& sr : cache->GetAll<SR>())
    {
        if (!sr || sr->ContentType() != "iso")
            continue;
        for (const QSharedPointer<VDI>& vdi : sr->GetVDIs())
        {
            if (vdi && vdi->IsToolsIso())
                return vdi;
        }
    }
    return QSharedPointer<VDI>();
}

void InstallPVToolsAction::run()
{
    try
    {
        XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
        if (!this->m_vm || !cache)
            throw std::runtime_error(tr("The VM is no longer available.").toStdString());

        // Repair the tools SR if it is unplugged
        for (const QSharedPointer<SR>& sr : cache->GetAll<SR>())
        {
            if (!sr || !sr->IsToolsSR() || !sr->IsBroken())
                continue;

            SrRepairAction repair(sr, false);
            repair.RunSync(this->GetSession());
            if (repair.IsFailed())
                throw std::runtime_error(tr("The Guest Tools SR could not be found.").toStdString());
        }

        QSharedPointer<VBD> cdrom = this->m_vm->FindVMCDROM();
        if (!cdrom)
            throw std::runtime_error(tr("Could not find a CD drive on the selected VM.").toStdString());

        QSharedPointer<VDI> toolsIso = FindToolsIso(this->GetConnection());
        if (!toolsIso)
            throw std::runtime_error(tr("Could not find the Guest Tools install disk.").toStdString());

        this->SetDescription(tr("Launching Guest Tools installation."));

        if (!cdrom->IsEmpty())
            XenAPI::VBD::eject(this->GetSession(), cdrom->OpaqueRef());
        XenAPI::VBD::insert(this->GetSession(), cdrom->OpaqueRef(), toolsIso->OpaqueRef());

        // The installation continues inside the VM
        this->SetDescription(tr("Guest Tools installation launched."));
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
