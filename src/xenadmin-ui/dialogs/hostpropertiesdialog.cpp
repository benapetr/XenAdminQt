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

#include "hostpropertiesdialog.h"
#include "ui_verticallytabbeddialog.h" // Need this to access ui->verticalTabs
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/perfmonalerteditpage.h"
#include "../settingspanels/hostmultipathpage.h"
#include "../settingspanels/hostpoweroneditpage.h"
#include "../settingspanels/hostautostarteditpage.h"
#include "../settingspanels/logdestinationeditpage.h"
#include "../settingspanels/poolgpueditpage.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

HostPropertiesDialog::HostPropertiesDialog(QSharedPointer<Host> host, QWidget* parent)
    : VerticallyTabbedDialog(host, parent), m_host(host)
{
    this->setWindowTitle(tr("Host Properties"));
    this->resize(700, 550);

    // Build tabs - must be called after base constructor completes
    this->build();
}

void HostPropertiesDialog::build()
{
    // Match C# XenAdmin.Dialogs.PropertiesDialog.Build() tab order for Host objects
    // Reference: xenadmin/XenAdmin/Dialogs/PropertiesDialog.cs lines 115-290

    // TODO: Add conditional logic to determine if this is standalone host
    // bool isStandalone = (pool == null || pool.Connection.Cache.Hosts.Count == 1);
    // For now, assume pool/standalone and show all applicable tabs

    // Tab 1: General (name, description, folder, tags)
    // Always shown for all object types (C# line 135)
    this->showTab(new GeneralEditPage());

    // Tab 2: Custom Fields
    // Shown for all objects except VM appliances and VMSS (C# line 137)
    this->showTab(new CustomFieldsDisplayPage());

    // Tab 3: Performance Alerts
    // Shown for VMs, Hosts, and SRs (C# line 149)
    this->showTab(new PerfmonAlertEditPage());

    // Tab 4: Performance Alert Options
    // Shown only for pool coordinators and standalone hosts (C# line 154)
    // TODO: Create PerfmonAlertOptionsPage class
    // TODO: Add conditional: if (isPoolOrStandalone)
    // this->showTab(new PerfmonAlertOptionsPage());

    // Tab 5: Multipath
    // Shown for all hosts to configure multipath storage (C# line 158-161)
    this->showTab(new HostMultipathPage());

    // Tab 6: Log Destination
    // Shown for all hosts (C# line 161)
    this->showTab(new LogDestinationEditPage());

    // Tab 7: Power On
    // Shown for hosts and pools to configure remote power management (WOL/DRAC/iLO) (C# line 164-165)
    this->showTab(new HostPowerONEditPage());

    // Tab 8: VM Startup Options
    // Shown for all hosts to configure VM autostart behavior (C# line 167-168)
    this->showTab(new HostAutostartEditPage());

    // Tab 9: GPU
    // Shown if standalone host has vGPU capability or host can enable/disable integrated GPU.
    QSharedPointer<Host> host = this->m_host;
    XenConnection* connection = host ? host->GetConnection() : nullptr;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    const bool standalone = cache && cache->GetPoolOfOne().isNull();
    if ((standalone && GpuHelpers::VGpuCapability(connection)) || (host && host->CanEnableDisableIntegratedGpu()))
        this->showTab(new PoolGpuEditPage());

    // Tab 10: Security (SSL Legacy Switch)
    // Shown for pool/standalone if not forbidden and pre-Stockholm (C# line 174-175)
    // TODO: Create SecurityEditPage class
    // TODO: Add conditional: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictSslLegacySwitch)
    //                           && !Helpers.StockholmOrGreater(connection))
    // this->showTab(new SecurityEditPage());

    // Tab 11: Live Patching
    // Shown for pool/standalone if not forbidden and pre-Cloud (C# line 177-178)
    // TODO: Create LivePatchingEditPage class
    // TODO: Add conditional: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictLivePatching)
    //                           && !Helpers.CloudOrGreater(connection))
    // this->showTab(new LivePatchingEditPage());

    // Tab 12: Networking (IGMP Snooping)
    // Shown for pool/standalone if not forbidden and using vSwitch backend (C# line 180-181)
    // TODO: Create NetworkOptionsEditPage class
    // TODO: Add conditional: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictIGMPSnooping)
    //                           && coordinator.vSwitchNetworkBackend())
    // this->showTab(new NetworkOptionsEditPage());

    // Tab 13: Clustering (Corosync)
    // Shown for pool/standalone if not forbidden (C# line 183-184)
    // TODO: Create ClusteringEditPage class
    // TODO: Add conditional: if (isPoolOrStandalone && !Helpers.FeatureForbidden(connection, Host.RestrictCorosync))
    // this->showTab(new ClusteringEditPage());

    // Tab 14: NRPE (Monitoring)
    // Shown for pool/standalone if XCP-ng 8.3+ / XenServer 23.27.0+ and user has permission (C# line 283-287)
    // TODO: Create NRPEEditPage class
    // TODO: Add conditional: if (isPoolOrStandalone && Helpers.XapiEqualOrGreater_23_27_0(connection)
    //                           && (session.IsLocalSuperuser || session.Roles.Any(r => r.name_label == Role.MR_ROLE_POOL_ADMIN)))
    // this->showTab(new NRPEEditPage());

    // Select first tab
    if (!this->m_pages.isEmpty())
    {
        this->ui->verticalTabs->setCurrentRow(0);
    }
}

void HostPropertiesDialog::SelectPoolGpuEditPage()
{
    for (IEditPage* page : this->m_pages)
    {
        if (qobject_cast<PoolGpuEditPage*>(page))
        {
            this->selectPage(page);
            return;
        }
    }
}
