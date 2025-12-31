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

#include "poolpropertiesdialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"
#include "../settingspanels/hostpoweroneditpage.h"
#include "../settingspanels/pooladvancededitpage.h"
#include "../settingspanels/securityeditpage.h"
#include "../settingspanels/livepatchingeditpage.h"
#include "../settingspanels/networkoptionseditpage.h"

PoolPropertiesDialog::PoolPropertiesDialog(XenConnection* connection,
                                           const QString& poolRef,
                                           QWidget* parent)
    : VerticallyTabbedDialog(connection, poolRef, "pool", parent)
{
    setWindowTitle(tr("Pool Properties"));
    resize(700, 550);
    build();
}

void PoolPropertiesDialog::build()
{
    // Match C# XenAdmin.Dialogs.PropertiesDialog.Build() for Pool objects
    // Reference: xenadmin/XenAdmin/Dialogs/PropertiesDialog.cs lines 154-188
    
    // General tab (name, description, tags)
    // C# line 135 - always shown
    this->showTab(new GeneralEditPage());

    // Custom Fields tab
    // C# line 137 - shown for all objects
    this->showTab(new CustomFieldsDisplayPage());

    // Performance Alerts tab
    // C# line 149 - shown for VM, Host, SR
    this->showTab(new PerfmonAlertEditPage());

    // Performance Alert Options tab
    // C# line 155: if (isPoolOrStandalone)
    // TODO: Create PerfmonAlertOptionsPage class
    // Reference: xenadmin/XenAdmin/SettingsPanels/PerfmonAlertOptionsPage.cs
    // Condition: Pool or standalone host
    // this->showTab(new PerfmonAlertOptionsPage());

    // Power On tab
    // C# line 165: if (isHost || isPool)
    // Allows configuring remote power management (WOL/DRAC/iLO)
    this->showTab(new HostPowerONEditPage());

    // GPU Settings tab
    // C# lines 171-173: if ((isPoolOrStandalone && Helpers.VGpuCapability(connection))
    //                       || (isHost && host.CanEnableDisableIntegratedGpu()))
    // TODO: Create PoolGpuEditPage class - more complex, requires GPU group management
    // Reference: xenadmin/XenAdmin/SettingsPanels/PoolGpuEditPage.cs
    // Condition: VGpu capability or integrated GPU support
    // TODO: Implement version/capability checking (need Helpers class)
    // this->showTab(new PoolGpuEditPage());

    // Security tab (SSL/TLS settings)
    // C# line 176: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictSslLegacySwitch)
    //                  && !Helpers.StockholmOrGreater(connection))
    // Shown unconditionally for now (TODO: add version check later)
    this->showTab(new SecurityEditPage());

    // Live Patching tab
    // C# line 179: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictLivePatching)
    //                  && !Helpers.CloudOrGreater(connection))
    // Shown unconditionally for now (TODO: add version check later)
    this->showTab(new LivePatchingEditPage());

    // Network Options tab (IGMP Snooping)
    // C# line 182: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictIGMPSnooping)
    //                  && Helpers.GetCoordinator(pool).vSwitchNetworkBackend())
    // Shown unconditionally for now (TODO: add version check later)
    this->showTab(new NetworkOptionsEditPage());

    // Clustering tab (Corosync)
    // C# line 185: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictCorosync))
    // TODO: Create ClusteringEditPage class - more complex, requires network picker
    // Reference: xenadmin/XenAdmin/SettingsPanels/ClusteringEditPage.cs
    // Condition: Not forbidden
    // TODO: Implement version/capability checking (need Helpers class)
    // this->showTab(new ClusteringEditPage());

    // Advanced Pool Settings tab (Migration Compression)
    // C# line 188: if (isPool && Helpers.CloudOrGreater(connection)
    //                  && Helpers.XapiEqualOrGreater_22_33_0(connection))
    // Shown unconditionally for now (TODO: add version check later)
    this->showTab(new PoolAdvancedEditPage());
}
