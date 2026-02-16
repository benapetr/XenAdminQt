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

#include "vmpropertiesdialog.h"
#include "ui_verticallytabbeddialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/cpumemoryeditpage.h"
#include "../settingspanels/bootoptionseditpage.h"
#include "../settingspanels/vmhaeditpage.h"
#include "../settingspanels/gpueditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"
#include "../settingspanels/homeservereditpage.h"
#include "../settingspanels/vmadvancededitpage.h"
#include "../settingspanels/vmenlightenmenteditpage.h"
#include "xen/pool.h"
#include "xen/host.h"
#include "xen/poolupdate.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xencache.h"

namespace
{
    bool containerCapability(XenConnection* connection)
    {
        if (!connection || !connection->GetCache())
            return false;

        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        if (!pool || !pool->IsValid())
            return false;

        QSharedPointer<Host> master = pool->GetMasterHost();
        if (!master || !master->IsValid())
            return false;

        if (master->ProductBrand().compare(QStringLiteral("XCP-ng"), Qt::CaseInsensitive) == 0)
            return true;

        const QList<QSharedPointer<PoolUpdate>> updates = master->AppliedUpdates();
        for (const QSharedPointer<PoolUpdate>& update : updates)
        {
            if (update && update->GetName().startsWith(QStringLiteral("xscontainer"), Qt::CaseInsensitive))
                return true;
        }

        const QList<Host::SuppPack> packs = master->SuppPacks();
        for (const Host::SuppPack& pack : packs)
        {
            if (pack.IsValid && pack.Name.startsWith(QStringLiteral("xscontainer"), Qt::CaseInsensitive))
                return true;
        }

        return false;
    }
}

VMPropertiesDialog::VMPropertiesDialog(QSharedPointer<VM> vm, QWidget* parent) : VerticallyTabbedDialog(vm, parent)
{
    this->m_vm = vm;
    this->setWindowTitle(tr("'%1' Properties").arg(vm->GetName()));
    this->resize(700, 550);
    this->build();
}

void VMPropertiesDialog::build()
{
    // Match C# XenAdmin.Dialogs.PropertiesDialog.Build() tab order for VM objects
    // Reference: xenadmin/XenAdmin/Dialogs/PropertiesDialog.cs lines 133-234

    // Get VM data to check conditional page requirements
    if (!this->m_vm)
        return;
    bool isSnapshot = this->m_vm->IsSnapshot();
    bool isVm = !isSnapshot;
    bool isHVM = this->m_vm->IsHVM();

    // Tab 1: General
    this->showTab(new GeneralEditPage());

    // Tab 2: Custom Fields
    this->showTab(new CustomFieldsDisplayPage());

    if (isVm)
    {
        // Tab 3: CPU and Memory
        this->showTab(new CpuMemoryEditPage());

        // Tab 4: Boot Options
        this->showTab(new BootOptionsEditPage());

        // Tab 5: High Availability
        this->showTab(new VMHAEditPage());
    }

    if (isVm)
    {
        // Tab 6: Performance Alerts
        this->showTab(new PerfmonAlertEditPage());

        // Tab 7: Home Server (only if WLB not enabled/configured)
        bool wlbEnabled = false;
        QSharedPointer<Pool> pool = this->m_vm && this->m_vm->GetConnection()
            ? this->m_vm->GetConnection()->GetCache()->GetPoolOfOne()
            : QSharedPointer<Pool>();
        if (pool)
            wlbEnabled = pool->IsWLBEnabled() && !pool->WLBUrl().isEmpty();

        if (!wlbEnabled)
        {
            this->showTab(new HomeServerEditPage());
        }

        // Tab 8: GPU (C# GpuEditPage parity)
        if (this->m_vm->CanHaveGpu() && GpuHelpers::GpusAvailable(this->m_vm->GetConnection()))
            this->showTab(new GpuEditPage());

        // TODO: Add remaining conditional VM tabs from C# XenAdmin.Dialogs.PropertiesDialog:
        // - USBEditPage (if !template and USB passthrough supported and PUSBs exist)
        // - CloudConfigParametersPage (if VM.CanHaveCloudConfigDrive())
        if (isHVM)
        {
            this->showTab(new VMAdvancedEditPage());
        }

        if (containerCapability(this->m_vm->GetConnection()) && this->m_vm->CanBeEnlightened())
        {
            this->showTab(new VMEnlightenmentEditPage());
        }
    }

    if (!this->m_pages.isEmpty())
    {
        this->ui->verticalTabs->setCurrentRow(0);
    }
}
