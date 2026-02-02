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

#include "vdipropertiesdialog.h"
#include "actionprogressdialog.h"
#include "ui_verticallytabbeddialog.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "../settingspanels/generaleditpage.h"
#include "../settingspanels/customfieldsdisplaypage.h"
#include "../settingspanels/vdisizelocationpage.h"
#include "../settingspanels/vbdeditpage.h"
#include "mainwindow.h"
#include <stdexcept>

VdiPropertiesDialog::VdiPropertiesDialog(QSharedPointer<VDI> vdi, QWidget* parent) : VerticallyTabbedDialog(vdi, parent), m_vdi(vdi)
{
    const QString name = vdi ? vdi->GetName() : tr("Virtual Disk");
    this->setWindowTitle(tr("'%1' Properties").arg(name));
    this->resize(700, 550);
    this->build();
}

void VdiPropertiesDialog::build()
{
    if (!this->m_vdi)
        return;

    // Match C# PropertiesDialog: General + Custom Fields
    this->showTab(new GeneralEditPage());
    this->showTab(new CustomFieldsDisplayPage());

    // C# PropertiesDialog: VDISizeLocationPage followed by one VBDEditPage per VBD
    this->showTab(new VdiSizeLocationPage());

    QList<VBDEditPage*> vbdPages;
    const QList<QSharedPointer<VBD>> vbds = this->m_vdi->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;
        VBDEditPage* page = new VBDEditPage(vbd);
        vbdPages.append(page);
        this->showTab(page);
    }

    if (!vbdPages.isEmpty())
        this->updateDevicePositions(vbdPages);

    if (!this->m_pages.isEmpty())
        this->ui->verticalTabs->setCurrentRow(0);
}

void VdiPropertiesDialog::updateDevicePositions(const QList<VBDEditPage*>& pages)
{
    if (!this->m_vdi || !this->m_vdi->GetConnection())
        return;

    auto* action = new DelegatedAsyncOperation(
        this->m_vdi->GetConnection(),
        tr("Scanning device positions"),
        tr("Scanning device positions"),
        [pages](DelegatedAsyncOperation* op) {
            XenAPI::Session* session = op->GetSession();
            if (!session)
            {
                throw std::runtime_error("No session");
            }

            for (VBDEditPage* page : pages)
            {
                if (!page)
                    continue;
                page->UpdateDevicePositions(session);
            }
        },
        this);

    auto* dialog = new ActionProgressDialog(action, MainWindow::instance());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setShowCancel(true);
    dialog->show();
}
