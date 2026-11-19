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
#include "../settingspanels/perfmonalertoptionspage.h"
#include "../settingspanels/securityeditpage.h"
#include "../settingspanels/livepatchingeditpage.h"
#include "../settingspanels/networkoptionseditpage.h"
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

    // C#: isPoolOrStandalone = isPool || (isHost && Helpers.GetPool(connection) == null)
    QSharedPointer<Host> host = this->m_host;
    XenConnection* connection = host ? host->GetConnection() : nullptr;
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    const bool isStandalone = cache && cache->GetPool().isNull();
    QSharedPointer<Host> coordinator = Host::GetCoordinator(connection);

    // General (name, description, folder, tags), always shown
    this->showTab(new GeneralEditPage());

    // Custom Fields, shown for all objects except vApps and snapshot schedules
    this->showTab(new CustomFieldsDisplayPage());

    // Performance Alerts, shown for VMs, servers and SRs
    this->showTab(new PerfmonAlertEditPage());

    // Email notifications for the alerts, for pools and standalone servers
    if (isStandalone)
        this->showTab(new PerfmonAlertOptionsPage());

    // Multipath
    this->showTab(new HostMultipathPage());

    // Log Destination
    this->showTab(new LogDestinationEditPage());

    // Power On (remote power management: WOL/DRAC/iLO)
    this->showTab(new HostPowerONEditPage());

    // VM Startup Options (autostart)
    this->showTab(new HostAutostartEditPage());

    // GPU: standalone server with vGPU capability, or a server that can toggle integrated GPU
    if ((isStandalone && GpuHelpers::VGpuCapability(connection)) || (host && host->CanEnableDisableIntegratedGpu()))
        this->showTab(new PoolGpuEditPage());

    // The pool-wide pages below appear for a standalone server; for a pool member they are in
    // Pool Properties. Licence restrictions (C# Helpers.FeatureForbidden) are not ported.

    // Security (SSL legacy switch), before Stockholm only
    if (isStandalone && !Host::StockholmOrGreater(connection))
        this->showTab(new SecurityEditPage());

    // Live Patching, before Cloud only
    if (isStandalone && !Host::CloudOrGreater(connection))
        this->showTab(new LivePatchingEditPage());

    // Networking (IGMP snooping), with the vSwitch network backend
    if (isStandalone && coordinator && coordinator->vSwitchNetworkBackend())
        this->showTab(new NetworkOptionsEditPage());

    // Clustering (Corosync) is not ported yet
    // TODO: Create ClusteringEditPage class (C# line 185, isPoolOrStandalone)

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
