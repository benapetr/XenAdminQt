/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "vmsspropertiesdialog.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/vmgroupvmspage.h"
#include "../settingspanels/policysnapshottypepage.h"
#include "../settingspanels/policysnapshotfrequencypage.h"
#include "xenlib/xen/vmss.h"

VMSSPropertiesDialog::VMSSPropertiesDialog(QSharedPointer<VMSS> policy, QWidget* parent)
    : VerticallyTabbedDialog(policy, parent), m_policy(policy)
{
    this->setWindowTitle(tr("Snapshot Schedule Properties"));
    this->resize(760, 600);
    this->build();
}

void VMSSPropertiesDialog::build()
{
    this->showTab(new GeneralEditPage());
    this->showTab(new CustomFieldsDisplayPage());
    this->showTab(new VMGroupVMsPage(VMGroup::Kind::VMSS));
    this->showTab(new PolicySnapshotTypePage());
    this->showTab(new PolicySnapshotFrequencyPage());
}
