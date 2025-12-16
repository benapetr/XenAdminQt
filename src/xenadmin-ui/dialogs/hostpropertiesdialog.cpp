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
#include "../settingspanels/hostautostarteditpage.h"
#include "../settingspanels/logdestinationeditpage.h"
#include <QMessageBox>

HostPropertiesDialog::HostPropertiesDialog(XenConnection* connection,
                                           const QString& hostRef,
                                           QWidget* parent)
    : VerticallyTabbedDialog(connection, hostRef, "host", parent)
{
    setWindowTitle(tr("Host Properties"));
    resize(700, 550);

    // Build tabs - must be called after base constructor completes
    build();
}

void HostPropertiesDialog::build()
{
    // General tab (name, description, folder, tags)
    showTab(new GeneralEditPage());

    // Host-specific tabs
    showTab(new LogDestinationEditPage());
    showTab(new HostAutostartEditPage());

    // TODO: Add remaining host tabs from C# XenAdmin.Dialogs.PropertiesDialog:
    // - HostMultipathPage (multipath configuration)
    // - HostPowerONEditPage (WOL/DRAC/iLO configuration) - needs SavePowerOnSettingsAction
    // - PoolGpuEditPage (if pool/standalone and GPU capable)
    // - PerfmonAlertEditPage (performance alerts)
    // - PerfmonAlertOptionsEditPage (if pool/standalone)
    // - SecurityEditPage (SSL legacy switch, if applicable)
    // - LivePatchingEditPage (if applicable)
    // - NetworkOptionsEditPage (IGMP snooping)
    // - ClusteringEditPage (Corosync clustering)
    // - NRPEEditPage (NRPE monitoring, XCP-ng 8.3+)

    // Select first tab
    if (!m_pages.isEmpty())
    {
        ui->verticalTabs->setCurrentRow(0);
    }
}
