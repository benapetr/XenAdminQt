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

#include "migratevirtualdiskdialog.h"
#include "xencache.h"
#include "xen/vdi.h"
#include "xen/sr.h"
#include "xen/actions/vdi/migratevirtualdiskaction.h"
#include "../operations/operationmanager.h"
#include "../controls/srpicker.h"

MigrateVirtualDiskDialog::MigrateVirtualDiskDialog(QSharedPointer<VDI> vdi, QWidget* parent)
    : MoveVirtualDiskDialog(vdi, parent)
{
    // Update window title to reflect migration
    this->setWindowTitle(tr("Migrate Virtual Disk"));
}

MigrateVirtualDiskDialog::MigrateVirtualDiskDialog(const QList<QSharedPointer<VDI>>& vdis, QWidget* parent)
    : MoveVirtualDiskDialog(vdis, parent)
{
    // Update window title to reflect migration
    this->setWindowTitle(tr("Migrate Virtual Disks"));
}

SrPicker::SRPickerType MigrateVirtualDiskDialog::srPickerType() const
{
    // C# MigrateVirtualDiskDialog uses SrPickerType.Migrate
    return SrPicker::Migrate;
}

void MigrateVirtualDiskDialog::createAndRunActions(const QString& targetSRRef, const QString& targetSRName)
{
    QSharedPointer<SR> targetSR = this->m_connection && this->m_connection->GetCache()
        ? this->m_connection->GetCache()->ResolveObject<SR>(targetSRRef)
        : QSharedPointer<SR>();
    QString resolvedTargetName = targetSR ? targetSR->GetName() : targetSRName;

    // Create migrate operations using MigrateVirtualDiskAction
    // This uses VDI.async_pool_migrate instead of copy+delete
    OperationManager* opManager = OperationManager::instance();

    if (this->m_vdiRefs.count() == 1)
    {
        // Single VDI migration
        QSharedPointer<VDI> vdi = this->m_connection && this->m_connection->GetCache()
            ? this->m_connection->GetCache()->ResolveObject<VDI>(this->m_vdiRefs.first())
            : QSharedPointer<VDI>();
        QString vdiName = vdi ? vdi->GetName() : QString("Virtual Disk");

        MigrateVirtualDiskAction* action = new MigrateVirtualDiskAction(this->m_connection, this->m_vdiRefs.first(), targetSRRef);

        action->SetTitle(QString("Migrating virtual disk '%1' to '%2'") .arg(vdiName) .arg(resolvedTargetName));
        action->SetDescription(QString("Migrating '%1'...").arg(vdiName));

        opManager->RegisterOperation(action);
        action->RunAsync(true);
    } else
    {
        // Multiple VDI migration - batch in parallel (max 3 at a time)
        // C# uses ParallelAction with BATCH_SIZE=3
        for (const QString& vdiRef : this->m_vdiRefs)
        {
            QSharedPointer<VDI> vdi = this->m_connection && this->m_connection->GetCache()
                ? this->m_connection->GetCache()->ResolveObject<VDI>(vdiRef)
                : QSharedPointer<VDI>();
            QString vdiName = vdi ? vdi->GetName() : QString("Virtual Disk");

            MigrateVirtualDiskAction* action = new MigrateVirtualDiskAction(this->m_connection, vdiRef, targetSRRef);

            action->SetTitle(QString("Migrating virtual disk '%1' to '%2'").arg(vdiName).arg(resolvedTargetName));
            action->SetDescription(QString("Migrating '%1'...").arg(vdiName));

            opManager->RegisterOperation(action);
            action->RunAsync(true);
        }
    }
}
