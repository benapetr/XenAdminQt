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
    // General tab (name, description, tags)
    showTab(new GeneralEditPage());

    // Custom Fields tab
    showTab(new CustomFieldsDisplayPage());

    // Performance Alerts tab
    showTab(new PerfmonAlertEditPage());

    // TODO: Add remaining Pool tabs from C# XenAdmin.Dialogs.PropertiesDialog:
    //
    // - PerfmonAlertOptionsPage (line 155) - Alert configuration options
    // - HostPowerONEditPage (line 165) - Power-on settings (if isPool)
    // - PoolGpuEditPage (lines 171-173) - GPU settings (if VGpu capability)
    // - SecurityEditPage (line 176) - SSL/TLS settings (if not forbidden)
    // - LivePatchingEditPage (line 179) - Live patching settings (if not forbidden)
    // - NetworkOptionsEditPage (line 182) - IGMP snooping (if vSwitch backend)
    // - ClusteringEditPage (line 185) - Corosync clustering (if not forbidden)
    // - PoolAdvancedEditPage (line 188) - Advanced pool settings (Cloud or greater)
    //
    // Notes:
    // - Check connection capabilities via Helpers methods
    // - Most pages are conditional based on XenServer version/features
}
