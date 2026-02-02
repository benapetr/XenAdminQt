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

#include "vbdeditpage.h"
#include "ui_vbdeditpage.h"
#include "xenlib/xen/actions/vbd/vbdeditaction.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xen/xenapi/xenapi_VBD.h"
#include "xenlib/xen/session.h"
#include "../dialogs/warningdialogs/warningdialog.h"
#include <QIcon>
#include <QMessageBox>
#include <QMetaObject>
#include <QDebug>

namespace
{
    const int kPriorityMin = 0;
    const int kPriorityMax = 7;
}

VBDEditPage::VBDEditPage(QSharedPointer<VBD> vbd, QWidget* parent)
    : IEditPage(parent)
    , ui(new Ui::VBDEditPage)
    , m_vbd(vbd)
    , m_validToSave(true)
{
    this->ui->setupUi(this);

    this->ui->modeComboBox->addItem(tr("Read/Write"));
    this->ui->modeComboBox->addItem(tr("Read Only"));

    this->ui->prioritySlider->setMinimum(kPriorityMin);
    this->ui->prioritySlider->setMaximum(kPriorityMax);

    connect(this->ui->modeComboBox, &QComboBox::currentIndexChanged, this, &VBDEditPage::onInputsChanged);
    connect(this->ui->positionComboBox, &QComboBox::currentIndexChanged, this, &VBDEditPage::onInputsChanged);
    connect(this->ui->prioritySlider, &QSlider::valueChanged, this, &VBDEditPage::onInputsChanged);

    if (this->m_vbd && this->m_vbd->IsValid())
    {
        this->m_vdi = this->m_vbd->GetVDI();
        this->m_vm = this->m_vbd->GetVM();
        this->m_sr = this->m_vdi ? this->m_vdi->GetSR() : QSharedPointer<SR>();
    }
}

VBDEditPage::~VBDEditPage()
{
    delete this->ui;
}

QString VBDEditPage::GetText() const
{
    return this->m_vm ? this->m_vm->GetName() : tr("VM");
}

QString VBDEditPage::GetSubText() const
{
    return this->m_subText;
}

QIcon VBDEditPage::GetImage() const
{
    return QIcon(":/tree-icons/vm_generic.png");
}

void VBDEditPage::SetXenObjects(const QString& objectRef, const QString& objectType, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectRef);
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    if (!this->m_vbd || !this->m_vbd->IsValid())
        return;

    this->m_vdi = this->m_vbd->GetVDI();
    this->m_vm = this->m_vbd->GetVM();
    this->m_sr = this->m_vdi ? this->m_vdi->GetSR() : QSharedPointer<SR>();

    this->repopulate();
    emit populated();
}

AsyncOperation* VBDEditPage::SaveSettings()
{
    if (!this->HasChanged() || !this->m_vbd)
        return nullptr;

    const QString devicePosition = this->selectedDevicePosition();
    const QString vbdMode = this->ui->modeComboBox->currentIndex() == 0 ? QStringLiteral("RW") : QStringLiteral("RO");
    const bool deviceChanged = devicePosition != this->m_vbd->GetUserdevice();

    if (deviceChanged && this->m_vdi && this->m_vdi->GetType() == "system")
    {
        const auto result = WarningDialog::ShowYesNo(
            tr("This will change the device position of one of this VM's system disks and may leave the VM unbootable. Are you sure you want to continue?"),
            tr("Edit Storage Settings"),
            this);
        if (result != WarningDialog::Result::Yes)
            return nullptr;
    }

    bool changeDevicePosition = false;
    QString otherVbdRef;

    if (deviceChanged)
    {
        QSharedPointer<VBD> other = this->findOtherVbdWithPosition(devicePosition);
        if (other && other->IsValid())
        {
            const auto result = WarningDialog::ShowThreeButton(
                tr("Position %1 is already in use. Your VM will not boot with two disks in the same position. Do you want to swap the disk at '%1' with this disk?")
                    .arg(devicePosition),
                tr("Warning"),
                tr("&Swap these disks"),
                tr("&Configure just this disk anyway"),
                tr("Cancel"),
                this);

            if (result == WarningDialog::Result::Cancel)
                return nullptr;

            changeDevicePosition = true;
            if (result == WarningDialog::Result::Yes)
            {
                otherVbdRef = other->OpaqueRef();
                this->warnSwapRequiresRestart(other);
            }
            // Result::No => configure existing: keep otherVbdRef empty, but proceed.
        }
        else
        {
            changeDevicePosition = true;
        }
    }

    int priority = this->m_vbd->GetIoNice();
    if (this->ui->priorityGroup->isVisible())
        priority = this->ui->prioritySlider->value();

    return new VbdEditAction(this->m_vbd->OpaqueRef(),
                             vbdMode,
                             priority,
                             changeDevicePosition,
                             otherVbdRef,
                             devicePosition,
                             true,
                             this);
}

bool VBDEditPage::IsValidToSave() const
{
    return this->m_validToSave;
}

void VBDEditPage::ShowLocalValidationMessages()
{
    this->ui->warningLabel->setVisible(true);
}

void VBDEditPage::HideLocalValidationMessages()
{
    if (this->m_validToSave)
        this->ui->warningLabel->clear();
}

void VBDEditPage::Cleanup()
{
}

bool VBDEditPage::HasChanged() const
{
    if (!this->m_vbd)
        return false;

    const bool modeChanged = (this->ui->modeComboBox->currentIndex() == 1) != this->m_vbd->IsReadOnly();
    const bool deviceChanged = this->selectedDevicePosition() != this->m_vbd->GetUserdevice();
    const bool priorityChanged = this->ui->priorityGroup->isVisible() && this->ui->prioritySlider->value() != this->m_vbd->GetIoNice();

    return modeChanged || deviceChanged || priorityChanged;
}

void VBDEditPage::UpdateDevicePositions(XenAPI::Session* session)
{
    if (!session || !this->m_vm)
        return;

    QVariant allowedVariant = XenAPI::VM::get_allowed_VBD_devices(session, this->m_vm->OpaqueRef());
    const QVariantList allowedList = allowedVariant.toList();

    QStringList devices;
    for (const QVariant& v : allowedList)
        devices.append(v.toString());

    // Never allow DVD drive unless already in use (C# logic).
    devices.removeAll("3");

    const QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;
        const QString device = vbd->GetUserdevice();
        if (!devices.contains(device))
            devices.append(device);
    }

    if (!devices.contains(this->m_vbd->GetUserdevice()))
        devices.append(this->m_vbd->GetUserdevice());

    std::sort(devices.begin(), devices.end(), [](const QString& a, const QString& b) {
        bool okA = false;
        bool okB = false;
        int ia = a.toInt(&okA);
        int ib = b.toInt(&okB);
        if (okA && okB)
            return ia < ib;
        return a < b;
    });

    QMetaObject::invokeMethod(this, [this, devices, vbds]()
    {
        this->ui->positionComboBox->blockSignals(true);
        this->ui->positionComboBox->clear();

        for (const QString& device : devices)
        {
            QString display = device;
            for (const QSharedPointer<VBD>& other : vbds)
            {
                if (!other || !other->IsValid())
                    continue;
                if (other->GetUserdevice() != device)
                    continue;

                if (other->OpaqueRef() == this->m_vbd->OpaqueRef())
                    break;

                QSharedPointer<VDI> otherVdi = other->GetVDI();
                if (otherVdi && otherVdi->IsValid())
                {
                    QString name = otherVdi->GetName();
                    if (name.length() > 30)
                        name = name.left(27) + "...";
                    display = tr("%1 (in use by %2)").arg(device).arg(name);
                }
                else
                    display = tr("%1 (in use)").arg(device);
                break;
            }

            this->ui->positionComboBox->addItem(display, device);
        }

        const int index = this->ui->positionComboBox->findData(this->m_vbd->GetUserdevice());
        if (index >= 0)
            this->ui->positionComboBox->setCurrentIndex(index);

        this->ui->positionComboBox->setEnabled(true);
        this->ui->positionComboBox->blockSignals(false);
        this->updateSubText();
        emit populated();
    }, Qt::QueuedConnection);
}

void VBDEditPage::onInputsChanged()
{
    this->updateSubText();
    emit populated();
}

void VBDEditPage::repopulate()
{
    if (!this->m_vbd)
        return;

    if (this->m_vbd->CurrentlyAttached())
    {
        this->ui->modeComboBox->setEnabled(false);
        this->ui->warningLabel->setText(tr("Disk is currently attached."));
    }
    else
    {
        const bool vdiReadOnly = this->m_vdi && this->m_vdi->ReadOnly();
        this->ui->modeComboBox->setEnabled(!vdiReadOnly);
        this->ui->warningLabel->clear();
    }

    const bool isReadOnly = (this->m_vdi && this->m_vdi->ReadOnly()) || this->m_vbd->IsReadOnly();
    this->ui->modeComboBox->setCurrentIndex(isReadOnly ? 1 : 0);

    this->ui->prioritySlider->setValue(this->m_vbd->GetIoNice());

    bool showPriority = false;
    if (this->m_sr)
    {
        QVariantMap otherConfig = this->m_sr->GetOtherConfig();
        showPriority = otherConfig.value("scheduler").toString() == "cfq";
    }

    this->ui->priorityGroup->setVisible(showPriority);

    this->ui->positionComboBox->setEnabled(false);
    this->updateSubText();
}

void VBDEditPage::updateSubText()
{
    const QString position = this->selectedDevicePosition();
    const QString modeText = this->ui->modeComboBox->currentText();
    if (!position.isEmpty())
        this->m_subText = tr("Position %1, %2").arg(position).arg(modeText);
    else
        this->m_subText = modeText;
}

QString VBDEditPage::selectedDevicePosition() const
{
    const int index = this->ui->positionComboBox->currentIndex();
    if (index < 0)
        return QString();
    return this->ui->positionComboBox->currentData().toString();
}

QSharedPointer<VBD> VBDEditPage::findOtherVbdWithPosition(const QString& position) const
{
    if (!this->m_vm)
        return QSharedPointer<VBD>();

    const QList<QSharedPointer<VBD>> vbds = this->m_vm->GetVBDs();
    for (const QSharedPointer<VBD>& vbd : vbds)
    {
        if (!vbd || !vbd->IsValid())
            continue;
        if (vbd->OpaqueRef() == this->m_vbd->OpaqueRef())
            continue;
        if (vbd->GetUserdevice() == position)
            return vbd;
    }
    return QSharedPointer<VBD>();
}

void VBDEditPage::warnSwapRequiresRestart(const QSharedPointer<VBD>& other) const
{
    if (!this->m_vm || !other)
        return;

    if (this->m_vm->IsHalted())
        return;

    const bool thisAttached = this->m_vbd->CurrentlyAttached() && !this->m_vbd->CanUnplug();
    const bool otherAttached = other->CurrentlyAttached() && !other->CanUnplug();

    if (thisAttached || otherAttached)
        QMessageBox::information(nullptr,
                                 tr("Restart required"),
                                 tr("You will have to restart the VM for changes in device position to take effect."));
}
