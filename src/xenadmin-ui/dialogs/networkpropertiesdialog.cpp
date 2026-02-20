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

#include "networkpropertiesdialog.h"
#include "ui_verticallytabbeddialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/networkgeneraleditpage.h"
#include "xenlib/xen/network.h"

NetworkPropertiesDialog::NetworkPropertiesDialog(QSharedPointer<Network> network, QWidget* parent)
    : VerticallyTabbedDialog(network, parent)
{
    this->setWindowTitle(tr("'%1' Properties").arg(network->GetName()));
    this->resize(700, 550);
    this->build();
}

void NetworkPropertiesDialog::build()
{
    // Match C# XenAdmin.Dialogs.PropertiesDialog.Build() for Network objects
    // Reference: xenadmin/XenAdmin/Dialogs/PropertiesDialog.cs line 122 (isNetwork check)

    // Tab 1: General (GeneralEditPage - universal for all object types)
    // Shows: Name, Description, Folder, Tags
    this->showTab(new GeneralEditPage());

    // Tab 2: Custom Fields
    // Allows editing custom fields on the network object
    this->showTab(new CustomFieldsDisplayPage());

    // Tab 3: Network Settings (NetworkGeneralEditPage - network-specific)
    // Equivalent to C# EditNetworkPage
    // Shows: NIC, VLAN, MTU, Auto-add to VMs, Bond mode
    this->showTab(new NetworkGeneralEditPage());

    // TODO: Add SR-IOV settings page when SR-IOV networks are more commonly used
    // Would show SR-IOV-specific configuration options
    // C# doesn't have a separate page for this either

    // Select first tab by default
    if (!this->m_pages.isEmpty())
    {
        this->ui->verticalTabs->setCurrentRow(0);
    }
}
