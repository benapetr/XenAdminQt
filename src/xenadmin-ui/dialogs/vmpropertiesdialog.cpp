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
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/cpumemoryeditpage.h"
#include "../settingspanels/bootoptionseditpage.h"
#include "../settingspanels/vmhaeditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"
#include "../settingspanels/homeservereditpage.h"
#include "../settingspanels/vmadvancededitpage.h"
#include "../settingspanels/vmenlightenmenteditpage.h"

VMPropertiesDialog::VMPropertiesDialog(XenConnection* connection,
                                       const QString& vmRef,
                                       QWidget* parent)
    : VerticallyTabbedDialog(connection, vmRef, "vm", parent)
{
    setWindowTitle(tr("VM Properties"));
    resize(700, 550);
    build();
}

void VMPropertiesDialog::build()
{
    // Get VM data to check conditional page requirements
    QVariantMap vmData = objectDataBefore();
    bool isTemplate = vmData.value("is_a_template").toBool();
    bool isHVM = vmData.value("HVM_boot_policy", "").toString() != "";

    // General tab (name, description, folder, tags, IQN)
    showTab(new GeneralEditPage());

    // CPU and Memory tab
    showTab(new CpuMemoryEditPage());

    // Boot Options tab
    showTab(new BootOptionsEditPage());

    // High Availability tab
    showTab(new VMHAEditPage());

    // Custom Fields tab (not shown for templates in some contexts)
    if (!isTemplate)
    {
        showTab(new CustomFieldsDisplayPage());
    }

    // Performance Alerts tab
    showTab(new PerfmonAlertEditPage());

    // Home Server tab - only if WLB not enabled
    // TODO: Check Helpers.WlbEnabledAndConfigured() when available
    // For now, always show it
    showTab(new HomeServerEditPage());

    // Advanced tab - only for HVM VMs
    if (isHVM)
    {
        showTab(new VMAdvancedEditPage());
    }

    // Enlightenment tab - only for VMs that can be enlightened
    // TODO: Check Helpers.ContainerCapability(connection) and VM.CanBeEnlightened()
    // For now, show for HVM VMs as a simplified check
    if (isHVM && !isTemplate)
    {
        showTab(new VMEnlightenmentEditPage());
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
    // - VMEnlightenmentEditPage (line 218-220) - only if container-capable connection && VM.CanBeEnlightened()
    //   Windows guest enlightenment settings
    //   Needs SaveVMEnlightenmentAction
    //
    // - CloudConfigParametersPage (line 222-224) - only if VM.CanHaveCloudConfigDrive()
    //   Cloud-init configuration parameters
    //   Needs SaveCloudConfigAction
    //
    // Notes:
    // - Check VM properties: is_a_template, IsHVM(), CanHaveGpu(), CanBeEnlightened(), CanHaveCloudConfigDrive()
    // - Check connection capabilities: Helpers.WlbEnabledAndConfigured(), Helpers.GpusAvailable(), etc.
    // - Some pages have async Populated events - handle via IEditPage if needed
    // - Follow same two-pattern approach: separate Action class for complex, inline AsyncOperation for simple
}
