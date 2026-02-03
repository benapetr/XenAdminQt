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

#include "vdisizelocationpage.h"
#include "ui_vdisizelocationpage.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/sm.h"
#include "xenlib/xen/xenapi/xenapi_VDI.h"
#include "xenlib/utils/misc.h"
#include <QIcon>
#include <QDebug>
#include <stdexcept>

namespace
{
    const qint64 kSizeDeltaThreshold = 10LL * 1024 * 1024; // 10 MB
}

VdiSizeLocationPage::VdiSizeLocationPage(QWidget* parent)
    : IEditPage(parent)
    , ui(new Ui::VdiSizeLocationPage)
    , m_originalSize(0)
    , m_originalSizeGb(0.0)
    , m_canResize(false)
    , m_validToSave(true)
{
    this->ui->setupUi(this);
    connect(this->ui->sizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VdiSizeLocationPage::onSizeChanged);
}

VdiSizeLocationPage::~VdiSizeLocationPage()
{
    delete this->ui;
}

QString VdiSizeLocationPage::GetText() const
{
    return tr("Size and Location");
}

QString VdiSizeLocationPage::GetSubText() const
{
    return this->m_subText;
}

QIcon VdiSizeLocationPage::GetImage() const
{
    return QIcon(":/icons/virtual_storage.png");
}

void VdiSizeLocationPage::SetXenObjects(const QString& objectRef, const QString& objectType, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectRef);
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    this->m_vdi = qSharedPointerDynamicCast<VDI>(this->m_object);
    if (!this->m_vdi || !this->m_vdi->IsValid())
        return;

    this->m_sr = this->m_vdi->GetSR();
    this->repopulate();
    emit populated();
}

AsyncOperation* VdiSizeLocationPage::SaveSettings()
{
    if (!this->HasChanged() || !this->m_vdi)
        return nullptr;

    const qint64 newSize = this->selectedSizeBytes();
    const QString vdiRef = this->m_vdi->OpaqueRef();
    const bool canResizeOffline = this->m_vdi->AllowedOperations().contains("resize");

    return new DelegatedAsyncOperation(
        this->connection(),
        tr("Change disk size"),
        tr("Changing disk size"),
        [vdiRef, newSize, canResizeOffline](DelegatedAsyncOperation* op) {
            XenAPI::Session* session = op->GetSession();
            if (!session)
            {
                throw std::runtime_error("No session");
            }
            if (canResizeOffline)
                XenAPI::VDI::resize(session, vdiRef, newSize);
            else
                XenAPI::VDI::resize_online(session, vdiRef, newSize);
        },
        this);
}

bool VdiSizeLocationPage::IsValidToSave() const
{
    return this->m_validToSave;
}

void VdiSizeLocationPage::ShowLocalValidationMessages()
{
    this->ui->warningLabel->setVisible(true);
}

void VdiSizeLocationPage::HideLocalValidationMessages()
{
    if (this->m_validToSave)
        this->ui->warningLabel->clear();
}

void VdiSizeLocationPage::Cleanup()
{
}

bool VdiSizeLocationPage::HasChanged() const
{
    if (!this->m_canResize)
        return false;

    const double deltaGb = this->ui->sizeSpinBox->value() - this->m_originalSizeGb;
    return (deltaGb * 1024.0 * 1024.0 * 1024.0) > kSizeDeltaThreshold;
}

void VdiSizeLocationPage::onSizeChanged(double value)
{
    Q_UNUSED(value);
    this->validateSize();
    this->updateSubText();
    emit populated();
}

void VdiSizeLocationPage::repopulate()
{
    this->ui->sizeSpinBox->blockSignals(true);

    this->m_originalSize = this->m_vdi->VirtualSize();
    const double sizeGb = this->m_originalSize / (1024.0 * 1024.0 * 1024.0);

    this->ui->currentSizeValueLabel->setText(Misc::FormatSize(this->m_originalSize));
    this->ui->sizeSpinBox->setValue(sizeGb);
    this->m_originalSizeGb = this->ui->sizeSpinBox->value();

    const QString location = this->m_sr ? this->m_sr->NameWithoutHost() : tr("Unknown");
    this->ui->locationValueLabel->setText(QString("'%1'").arg(location));

    const QStringList allowedOps = this->m_vdi->AllowedOperations();
    this->m_canResize = allowedOps.contains("resize") || allowedOps.contains("resize_online");

    this->ui->sizeSpinBox->setEnabled(this->m_canResize);
    this->ui->newSizeLabel->setEnabled(this->m_canResize);

    if (!this->m_canResize)
    {
        this->ui->warningLabel->setText(tr("Resizing is not available for this virtual disk."));
    } else
    {
        this->ui->warningLabel->clear();
    }

    this->ui->sizeSpinBox->blockSignals(false);

    this->validateSize();
    this->updateSubText();
}

void VdiSizeLocationPage::validateSize()
{
    this->m_validToSave = true;

    if (!this->m_canResize)
        return;

    const qint64 newSize = this->selectedSizeBytes();
    if (newSize < this->m_originalSize)
    {
        this->ui->warningLabel->setText(tr("Cannot decrease virtual disk size. Only increases are supported."));
        this->m_validToSave = false;
        return;
    }

    QSharedPointer<SM> sm = this->m_sr ? this->m_sr->GetSM() : QSharedPointer<SM>();
    const bool vdiSizeUnlimited = sm && sm->Capabilities().contains("LARGE_VDI");

    if (!vdiSizeUnlimited && newSize > SR::DISK_MAX_SIZE)
    {
        this->ui->warningLabel->setText(
            tr("Disk size cannot be more than %1.").arg(Misc::FormatSize(SR::DISK_MAX_SIZE)));
        this->m_validToSave = false;
        return;
    }

    const bool isThinlyProvisioned = sm && sm->Capabilities().contains("THIN_PROVISIONING");
    if (!isThinlyProvisioned && this->m_sr)
    {
        const qint64 sizeDiff = newSize - this->m_originalSize;
        const qint64 freeSpace = this->m_sr->FreeSpace();
        if (sizeDiff > freeSpace)
        {
            this->ui->warningLabel->setText(tr("There is not enough available space for this disk"));
            this->m_validToSave = false;
            return;
        }
    }

    this->ui->warningLabel->clear();
}

void VdiSizeLocationPage::updateSubText()
{
    if (!this->m_vdi)
        return;

    const qint64 size = this->m_canResize ? this->selectedSizeBytes() : this->m_originalSize;
    const QString location = this->m_sr ? this->m_sr->NameWithoutHost() : tr("Unknown");
    this->m_subText = tr("%1, %2").arg(Misc::FormatSize(size)).arg(location);
}

qint64 VdiSizeLocationPage::selectedSizeBytes() const
{
    const double sizeGb = this->ui->sizeSpinBox->value();
    return static_cast<qint64>(sizeGb * 1024.0 * 1024.0 * 1024.0);
}
