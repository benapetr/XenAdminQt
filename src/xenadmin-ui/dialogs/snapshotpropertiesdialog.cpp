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

#include "snapshotpropertiesdialog.h"
#include "settingspanels/customfieldsdisplaypage.h"
#include "settingspanels/generaleditpage.h"
#include "ui_verticallytabbeddialog.h"
#include "xenlib/xen/vm.h"

SnapshotPropertiesDialog::SnapshotPropertiesDialog(QSharedPointer<VM> snapshot, QWidget* parent) : VerticallyTabbedDialog(snapshot, parent)
{
    QString snapshotName = objectDataBefore().value("name_label", tr("Snapshot")).toString();
    this->setWindowTitle(tr("'%1' Properties").arg(snapshotName));
    this->resize(700, 550);
    this->build();
}

void SnapshotPropertiesDialog::build()
{
    this->showTab(new GeneralEditPage());
    this->showTab(new CustomFieldsDisplayPage());

    if (!this->m_pages.isEmpty())
        this->ui->verticalTabs->setCurrentRow(0);
}
