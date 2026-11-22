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

#include "vmappliancepropertiesdialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/vmgroupvmspage.h"
#include "../settingspanels/vmappliancestartorderpage.h"
#include "ui_verticallytabbeddialog.h"
#include "../widgets/verticaltabwidget.h"
#include "xenlib/xen/vmappliance.h"

VMAppliancePropertiesDialog::VMAppliancePropertiesDialog(QSharedPointer<VMAppliance> appliance, QWidget* parent)
    : VerticallyTabbedDialog(appliance, parent), m_appliance(appliance)
{
    this->setWindowTitle(tr("vApp Properties"));
    this->resize(700, 550);
    this->build();
}

void VMAppliancePropertiesDialog::build()
{
    // C# PropertiesDialog for a VM_appliance: General, Custom Fields, VMs, VM Startup Sequence
    this->showTab(new GeneralEditPage());
    this->showTab(new CustomFieldsDisplayPage());
    this->m_vmsPage = new VMGroupVMsPage(VMGroup::Kind::VMAppliance);
    this->showTab(this->m_vmsPage);
    this->m_startOrderPage = new VMApplianceStartOrderPage();
    this->showTab(this->m_startOrderPage);

    // C# verticalTabs_SelectedIndexChanged: the startup sequence lists the VMs chosen on the VMs page
    connect(this->ui->verticalTabs, &VerticalTabWidget::currentRowChanged, this, [this]()
    {
        if (this->ui->ContentPanel->currentWidget() == this->m_startOrderPage)
            this->m_startOrderPage->SetSelectedVMs(this->m_vmsPage->SelectedVMs());
    });
}
