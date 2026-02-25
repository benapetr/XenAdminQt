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

#include "movevirtualdiskdialog.h"
#include "ui_movevirtualdiskdialog.h"
#include "commands/command.h"
#include "commands/storage/migratevirtualdiskcommand.h"
#include "commands/storage/movevirtualdiskcommand.h"
#include "xen/network/connection.h"
#include "xen/vdi.h"
#include "xen/sr.h"
#include "xencache.h"
#include "xen/actions/vdi/movevirtualdiskaction.h"
#include "../controls/srpicker.h"
#include <QMessageBox>

static QStringList buildVdiRefs(const QList<QSharedPointer<VDI>>& vdis)
{
    QStringList refs;
    refs.reserve(vdis.size());
    for (const QSharedPointer<VDI>& vdi : vdis)
    {
        if (vdi && vdi->IsValid())
            refs.append(vdi->OpaqueRef());
    }
    return refs;
}

Command* MoveVirtualDiskDialog::MoveMigrateCommand(MainWindow* mainWindow, const QList<QSharedPointer<XenObject>>& selection, QObject* parent)
{
    MigrateVirtualDiskCommand* migrateCmd = new MigrateVirtualDiskCommand(mainWindow, parent);
    migrateCmd->SetSelectionOverride(selection);
    if (migrateCmd->CanRun())
        return migrateCmd;

    delete migrateCmd;
    MoveVirtualDiskCommand* moveCmd = new MoveVirtualDiskCommand(mainWindow, parent);
    moveCmd->SetSelectionOverride(selection);
    return moveCmd;
}

MoveVirtualDiskDialog::MoveVirtualDiskDialog(QSharedPointer<VDI> vdi, QWidget* parent) : QDialog(parent), ui(new Ui::MoveVirtualDiskDialog), m_connection(vdi ? vdi->GetConnection() : nullptr)
{
    if (vdi && vdi->IsValid())
    {
        this->m_vdis.append(vdi);
        this->m_vdiRefs << vdi->OpaqueRef();
    }
    this->setupUI();
}

MoveVirtualDiskDialog::MoveVirtualDiskDialog(const QList<QSharedPointer<VDI>>& vdis, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::MoveVirtualDiskDialog),
      m_connection(vdis.isEmpty() || !vdis.first() ? nullptr : vdis.first()->GetConnection()),
      m_vdis(vdis),
      m_vdiRefs(buildVdiRefs(vdis))
{
    this->setupUI();
}

MoveVirtualDiskDialog::~MoveVirtualDiskDialog()
{
    delete this->ui;
}

void MoveVirtualDiskDialog::setupUI()
{
    this->ui->setupUi(this);

    // Connect SrPicker signals (C# MoveVirtualDiskDialog pattern)
    connect(this->ui->srPicker1, &SrPicker::selectedIndexChanged, this, &MoveVirtualDiskDialog::onSRSelectionChanged);
    connect(this->ui->srPicker1, &SrPicker::doubleClickOnRow, this, &MoveVirtualDiskDialog::onSRDoubleClicked);
    connect(this->ui->srPicker1, &SrPicker::canBeScannedChanged, this, &MoveVirtualDiskDialog::onCanBeScannedChanged);
    connect(this->ui->rescanButton, &QPushButton::clicked, this, &MoveVirtualDiskDialog::onRescanButtonClicked);
    connect(this->ui->moveButton, &QPushButton::clicked, this, &MoveVirtualDiskDialog::onMoveButtonClicked);

    // Update window title for multiple VDIs
    if (this->m_vdiRefs.count() > 1)
    {
        this->setWindowTitle(QString("Move %1 Virtual Disks").arg(this->m_vdiRefs.count()));
    }

    // Populate SR picker (C# OnLoad pattern)
    this->ui->srPicker1->Populate(this->srPickerType(), this->m_connection, QString(), QString(), this->m_vdiRefs);

    // Update button states
    this->updateMoveButton();
    this->onCanBeScannedChanged();
}

SrPicker::SRPickerType MoveVirtualDiskDialog::srPickerType() const
{
    // C# MoveVirtualDiskDialog uses SrPickerType.Move
    return SrPicker::Move;
}

void MoveVirtualDiskDialog::onSRSelectionChanged()
{
    this->updateMoveButton();
}

void MoveVirtualDiskDialog::onSRDoubleClicked()
{
    // C# srPicker1_DoubleClickOnRow pattern
    if (this->ui->moveButton->isEnabled())
    {
        this->ui->moveButton->click();
    }
}

void MoveVirtualDiskDialog::onRescanButtonClicked()
{
    // C# buttonRescan_Click - delegate to SrPicker.ScanSRs()
    this->ui->srPicker1->ScanSRs();
}

void MoveVirtualDiskDialog::onCanBeScannedChanged()
{
    // C# srPicker1_CanBeScannedChanged - update Rescan button state
    this->ui->rescanButton->setEnabled(this->ui->srPicker1->CanBeScanned());
    this->updateMoveButton();
}

void MoveVirtualDiskDialog::updateMoveButton()
{
    // C# UpdateMoveButton - enable Move button only if SR is selected
    this->ui->moveButton->setEnabled(!this->ui->srPicker1->GetSelectedSR().isEmpty());
}

void MoveVirtualDiskDialog::onMoveButtonClicked()
{
    // Get selected SR from SrPicker
    QString targetSRRef = this->ui->srPicker1->GetSelectedSR();
    if (targetSRRef.isEmpty())
        return;

    // Get target SR name
    QSharedPointer<SR> targetSR = this->m_connection && this->m_connection->GetCache()
        ? this->m_connection->GetCache()->ResolveObject<SR>(targetSRRef)
        : QSharedPointer<SR>();
    QString targetSRName = targetSR ? targetSR->GetName() : QString();

    // Close dialog
    this->accept();

    // Create actions (virtual method - can be overridden)
    this->createAndRunActions(targetSRRef, targetSRName);
} 

void MoveVirtualDiskDialog::createAndRunActions(const QString& targetSRRef, const QString& targetSRName)
{
    // Create move operations

    if (this->m_vdiRefs.count() == 1)
    {
        // Single VDI move
        QSharedPointer<VDI> vdi = !this->m_vdis.isEmpty() && this->m_vdis.first() && this->m_vdis.first()->IsValid()
            ? this->m_vdis.first()
            : (this->m_connection && this->m_connection->GetCache()
                ? this->m_connection->GetCache()->ResolveObject<VDI>(this->m_vdiRefs.first())
                : QSharedPointer<VDI>());
        QString vdiName = vdi ? vdi->GetName() : QString("Virtual Disk");

        MoveVirtualDiskAction* action = new MoveVirtualDiskAction(this->m_connection, this->m_vdiRefs.first(), targetSRRef);

        action->SetTitle(QString("Moving virtual disk '%1' to '%2'").arg(vdiName).arg(targetSRName));
        action->SetDescription(QString("Moving '%1'...").arg(vdiName));

        action->RunAsync(true);
    } else
    {
        // Multiple VDI move - batch in parallel (max 3 at a time)
        // C# uses ParallelAction with BATCH_SIZE=3
        for (const QString& vdiRef : this->m_vdiRefs)
        {
            QSharedPointer<VDI> vdi = this->m_connection && this->m_connection->GetCache()
                ? this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef)
                : QSharedPointer<VDI>();
            QString vdiName = vdi ? vdi->GetName() : QString("Virtual Disk");

            MoveVirtualDiskAction* action = new MoveVirtualDiskAction(this->m_connection, vdiRef, targetSRRef);

            action->SetTitle(QString("Moving virtual disk '%1' to '%2'").arg(vdiName).arg(targetSRName));
            action->SetDescription(QString("Moving '%1'...").arg(vdiName));

            action->RunAsync(true);
        }
    }
}
