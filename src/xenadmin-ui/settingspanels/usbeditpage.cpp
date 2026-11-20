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

#include "usbeditpage.h"
#include "ui_usbeditpage.h"
#include "../dialogs/attachusbdialog.h"
#include "xenlib/xen/actions/usb/deletevusbaction.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pusb.h"
#include "xenlib/xen/usbgroup.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vusb.h"
#include "xenlib/xencache.h"
#include <QHeaderView>
#include <QMessageBox>

namespace
{
    const int VUSB_ROLE = Qt::UserRole + 1;
}

USBEditPage::USBEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::USBEditPage)
{
    this->ui->setupUi(this);
    this->ui->dataGridViewUsbList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->ui->dataGridViewUsbList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    this->ui->dataGridViewUsbList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(this->ui->dataGridViewUsbList, &QTableWidget::itemSelectionChanged, this, &USBEditPage::updateDetachButton);
    connect(this->ui->buttonAttach, &QPushButton::clicked, this, &USBEditPage::attach);
    connect(this->ui->buttonDetach, &QPushButton::clicked, this, &USBEditPage::detach);
}

USBEditPage::~USBEditPage()
{
    delete this->ui;
}

QString USBEditPage::GetText() const
{
    return tr("USB");
}

QString USBEditPage::GetSubText() const
{
    if (!this->m_vm)
        return QString();
    const int count = this->m_vm->VUSBRefs().size();
    if (count == 0)
        return tr("No devices attached");
    if (count == 1)
        return tr("1 device attached");
    return tr("%1 devices attached").arg(count);
}

QIcon USBEditPage::GetImage() const
{
    return QIcon(":/icons/usb_16.png");
}

void USBEditPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    this->m_vm = qSharedPointerDynamicCast<VM>(object);
    if (!this->m_vm)
        return;

    // C# listens to the VM's VUSBs and each VUSB's properties; both arrive as cache updates
    if (XenCache* cache = this->m_vm->GetCache())
        connect(cache, &XenCache::objectChanged, this, &USBEditPage::onCacheObjectChanged, Qt::UniqueConnection);

    this->buildList();
    this->showHideWarnings();
    this->updateDetachButton();
}

void USBEditPage::onCacheObjectChanged(QSharedPointer<XenObject> object)
{
    if (!object || !this->m_vm)
        return;

    if (object->GetObjectType() == XenObjectType::VM && object->OpaqueRef() == this->m_vm->OpaqueRef())
    {
        // C# rebuilds when VM.VUSBs changes; the warnings follow the power state
        this->showHideWarnings();
        if (this->m_vm->VUSBRefs() != this->m_vusbRefs)
        {
            this->buildList();
            emit populated();
        }
    } else if (object->GetObjectType() == XenObjectType::VUSB && this->m_vusbRefs.contains(object->OpaqueRef()))
    {
        this->buildList();
    }
}

void USBEditPage::buildList()
{
    QTableWidget* table = this->ui->dataGridViewUsbList;
    const QString selectedRef = table->selectedItems().isEmpty()
        ? QString() : table->item(table->selectedItems().first()->row(), 0)->data(VUSB_ROLE).toString();

    table->setSortingEnabled(false);
    table->setRowCount(0);

    this->m_vusbRefs = this->m_vm->VUSBRefs();
    for (const QSharedPointer<VUSB>& vusb : this->m_vm->GetVUSBs())
    {
        if (!vusb)
            continue;

        // C# VMUsbRow: location and description come from the group's first PUSB
        const QSharedPointer<USBGroup> group = vusb->GetUSBGroup();
        const QList<QSharedPointer<PUSB>> pusbs = group ? group->GetPUSBs() : QList<QSharedPointer<PUSB>>();
        const QSharedPointer<PUSB> pusb = pusbs.isEmpty() ? QSharedPointer<PUSB>() : pusbs.first();

        const int row = table->rowCount();
        table->insertRow(row);
        auto* location = new QTableWidgetItem(pusb ? pusb->Path() : QString());
        location->setData(VUSB_ROLE, vusb->OpaqueRef());
        table->setItem(row, 0, location);
        table->setItem(row, 1, new QTableWidgetItem(pusb ? pusb->Description() : QString()));
        table->setItem(row, 2, new QTableWidgetItem(vusb->CurrentlyAttached() ? tr("Yes") : tr("No")));

        if (vusb->OpaqueRef() == selectedRef)
            table->selectRow(row);
    }

    table->setSortingEnabled(true);
    this->updateDetachButton();
}

void USBEditPage::showHideWarnings()
{
    // C#: attaching needs a halted VM that HA would not restart
    QSharedPointer<Pool> pool = this->m_vm->GetCache() ? this->m_vm->GetCache()->GetPool() : QSharedPointer<Pool>();
    const bool haEnabled = pool && pool->HAEnabled() && this->m_vm->HARestartPriority() == "restart";

    if (this->m_vm->GetPowerState() != "Halted")
    {
        this->ui->labelWarning->setText(tr("USB devices can only be attached when the VM is shut down."));
        this->ui->buttonAttach->setEnabled(false);
    } else if (haEnabled)
    {
        this->ui->labelWarning->setText(tr("Cannot attach USB devices because the VM is currently protected by HA."));
        this->ui->buttonAttach->setEnabled(false);
    } else
    {
        this->ui->buttonAttach->setEnabled(true);
    }
    this->ui->flowLayoutPanelWarning->setVisible(!this->ui->buttonAttach->isEnabled());
}

void USBEditPage::updateDetachButton()
{
    this->ui->buttonDetach->setEnabled(!this->ui->dataGridViewUsbList->selectedItems().isEmpty());
}

void USBEditPage::attach()
{
    if (!this->m_vm)
        return;
    AttachUsbDialog dialog(this->m_vm, this);
    dialog.exec();
}

void USBEditPage::detach()
{
    QTableWidget* table = this->ui->dataGridViewUsbList;
    if (!this->m_vm || table->selectedItems().isEmpty())
        return;

    const QString vusbRef = table->item(table->selectedItems().first()->row(), 0)->data(VUSB_ROLE).toString();
    QSharedPointer<VUSB> vusb = this->m_vm->GetCache() ? this->m_vm->GetCache()->ResolveObject<VUSB>(vusbRef) : QSharedPointer<VUSB>();
    if (!vusb)
        return;

    QMessageBox box(QMessageBox::NoIcon, tr("Detach USB device"),
                    tr("Are you sure you want to detach the selected USB device?"),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    (new DeleteVUSBAction(vusb, this->m_vm))->RunAsync(true);
}

AsyncOperation* USBEditPage::SaveSettings()
{
    return nullptr;
}

bool USBEditPage::IsValidToSave() const
{
    return true;
}

void USBEditPage::ShowLocalValidationMessages()
{
}

void USBEditPage::HideLocalValidationMessages()
{
}

void USBEditPage::Cleanup()
{
    if (this->m_vm && this->m_vm->GetCache())
        disconnect(this->m_vm->GetCache(), nullptr, this, nullptr);
}

bool USBEditPage::HasChanged() const
{
    // Attach and Detach run straight away, so there is nothing left to save
    return false;
}
