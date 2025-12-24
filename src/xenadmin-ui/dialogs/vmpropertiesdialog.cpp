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
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"
#include "../settingspanels/homeservereditpage.h"
#include "../settingspanels/vmadvancededitpage.h"
#include "../settingspanels/vmenlightenmenteditpage.h"
#include "xen/network/connection.h"
#include "xencache.h"

VMPropertiesDialog::VMPropertiesDialog(XenConnection* connection, const QString& vmRef, QWidget* parent)
    : VerticallyTabbedDialog(connection, vmRef, "vm", parent)
{
    QString vmName = objectDataBefore().value("name_label", tr("VM")).toString();
    this->setWindowTitle(tr("'%1' Properties").arg(vmName));
    this->resize(700, 550);
    this->build();
}

void VMPropertiesDialog::build()
{
    // Match C# XenAdmin.Dialogs.PropertiesDialog.Build() tab order for VM objects
    // Reference: xenadmin/XenAdmin/Dialogs/PropertiesDialog.cs lines 133-234

    // Get VM data to check conditional page requirements
    QVariantMap vmData = objectDataBefore();
    bool isSnapshot = vmData.value("is_a_snapshot").toBool();
    bool isVm = !isSnapshot;
    bool isTemplate = vmData.value("is_a_template").toBool();
    bool isHVM = vmData.value("HVM_boot_policy", "").toString() != "";

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

    // Tab 6: Performance Alerts
    this->showTab(new PerfmonAlertEditPage());

    // Tab 7: Home Server (only if WLB not enabled/configured)
    bool wlbEnabled = false;
    if (connection())
    {
        QStringList poolRefs = connection()->getCache()->GetAllRefs("pool");
        if (!poolRefs.isEmpty())
        {
            QVariantMap poolData = connection()->getCache()->ResolveObjectData("pool", poolRefs.first());
            QString wlbUrl = poolData.value("wlb_url").toString();
            wlbEnabled = poolData.value("wlb_enabled").toBool() && !wlbUrl.isEmpty();
        }
    }

    if (!wlbEnabled)
    {
        this->showTab(new HomeServerEditPage());
    }

    // TODO: Add remaining conditional VM tabs from C# XenAdmin.Dialogs.PropertiesDialog:
    //
    // - GpuEditPage (lines 198-203) - only if VM.CanHaveGpu() and GPUs available
    //   GPU passthrough configuration
    //   Needs SaveGpuAction with VGPU creation/destruction
    //
    // - USBEditPage (lines 205-212) - only if !template && USB passthrough not restricted && PUSBs exist
    //   USB device passthrough configuration
    //   Has async Populated event - may need data fetching
    //   Needs SaveUSBAction with PUSB/VUSB creation
    //
    // - USBEditPage (lines 205-212) - only if !template && USB passthrough not restricted && PUSBs exist
    //
    // - VMAdvancedEditPage (line 214) - only for HVM VMs
    //
    // - VMEnlightenmentEditPage (line 218-220) - only if container-capable connection && VM.CanBeEnlightened()
    //
    // - CloudConfigParametersPage (line 222-224) - only if VM.CanHaveCloudConfigDrive()

    if (isHVM)
    {
        this->showTab(new VMAdvancedEditPage());
    }

    // TODO: Implement Helpers.ContainerCapability() and VM.CanBeEnlightened().
    // For now, mirror previous behavior: show Enlightenment only for HVM non-templates.
    if (isHVM && !isTemplate)
    {
        this->showTab(new VMEnlightenmentEditPage());
    }

    if (!this->m_pages.isEmpty())
    {
        this->ui->verticalTabs->setCurrentRow(0);
    }
}
