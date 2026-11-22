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

#include "policysnapshottypepage.h"
#include "ui_policysnapshottypepage.h"
#include "xenlib/xen/vmss.h"

PolicySnapshotTypePage::PolicySnapshotTypePage(QWidget* parent) : IEditPage(parent), ui(new Ui::PolicySnapshotTypePage)
{
    this->ui->setupUi(this);
    this->ui->labelWarning->setText(this->ui->labelWarning->text().arg(tr("XCP-ng VM Tools")));
    this->ui->labelQuiesceRemoved->setVisible(false);
    connect(this->ui->radioButtonDiskOnly, &QRadioButton::clicked, this, &PolicySnapshotTypePage::onTypeChosen);
    connect(this->ui->radioButtonDiskAndMemory, &QRadioButton::clicked, this, &PolicySnapshotTypePage::onTypeChosen);
}

PolicySnapshotTypePage::~PolicySnapshotTypePage()
{
    delete this->ui;
}

QString PolicySnapshotTypePage::BackupType() const
{
    if (this->m_keepQuiesce)
        return "snapshot_with_quiesce";
    return this->ui->radioButtonDiskAndMemory->isChecked() ? "checkpoint" : "snapshot";
}

QString PolicySnapshotTypePage::TypeToString(const QString& type)
{
    if (type == "snapshot")
        return tr("Disks only");
    if (type == "checkpoint")
        return tr("Disks and memory");
    if (type == "snapshot_with_quiesce")
        return tr("Quiesced snapshots");
    return tr("Unknown");
}

QString PolicySnapshotTypePage::BackupTypeToString() const
{
    return TypeToString(this->BackupType());
}

void PolicySnapshotTypePage::onTypeChosen()
{
    // Choosing a type replaces an old quiesced one
    this->m_keepQuiesce = false;
    this->ui->labelQuiesceRemoved->setVisible(false);
}

QString PolicySnapshotTypePage::GetText() const
{
    return tr("Snapshot Type");
}

QString PolicySnapshotTypePage::GetSubText() const
{
    return this->BackupTypeToString();
}

QIcon PolicySnapshotTypePage::GetImage() const
{
    return QIcon(":/template-icons/snapshot.png");
}

void PolicySnapshotTypePage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    this->m_object = object;
    QSharedPointer<VMSS> policy = qSharedPointerDynamicCast<VMSS>(object);
    this->m_originalType = objectDataCopy.contains("type") ? objectDataCopy.value("type").toString()
                                                           : (policy ? policy->Type() : QString("snapshot"));

    // C# EnableShapshotTypes for an existing policy
    this->m_keepQuiesce = this->m_originalType == "snapshot_with_quiesce";
    this->ui->radioButtonDiskAndMemory->setChecked(this->m_originalType == "checkpoint");
    this->ui->radioButtonDiskOnly->setChecked(this->m_originalType != "checkpoint");
    this->ui->labelQuiesceRemoved->setVisible(this->m_keepQuiesce);
}

AsyncOperation* PolicySnapshotTypePage::SaveSettings()
{
    // C#: sets the copy's type; SaveChangesAction applies it
    return nullptr;
}

QVariantMap PolicySnapshotTypePage::GetModifiedObjectData() const
{
    return QVariantMap{{"type", this->BackupType()}};
}

bool PolicySnapshotTypePage::IsValidToSave() const
{
    return true;
}

void PolicySnapshotTypePage::ShowLocalValidationMessages()
{
}

void PolicySnapshotTypePage::HideLocalValidationMessages()
{
}

void PolicySnapshotTypePage::Cleanup()
{
    this->ui->radioButtonDiskOnly->setChecked(true);
}

bool PolicySnapshotTypePage::HasChanged() const
{
    return this->BackupType() != this->m_originalType;
}
