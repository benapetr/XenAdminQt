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
#include "xenlib.h"
#include "xencache.h"
#include "xen/actions/vdi/migratevirtualdiskaction.h"
#include "../operations/operationmanager.h"

MigrateVirtualDiskDialog::MigrateVirtualDiskDialog(XenLib* xenLib, const QString& vdiRef, QWidget* parent)
    : MoveVirtualDiskDialog(xenLib, vdiRef, parent)
{
    // Update window title to reflect migration
    setWindowTitle(tr("Migrate Virtual Disk"));
}

MigrateVirtualDiskDialog::MigrateVirtualDiskDialog(XenLib* xenLib, const QStringList& vdiRefs, QWidget* parent)
    : MoveVirtualDiskDialog(xenLib, vdiRefs, parent)
{
    // Update window title to reflect migration
    setWindowTitle(tr("Migrate Virtual Disks"));
}

void MigrateVirtualDiskDialog::createAndRunActions(const QString& targetSRRef, const QString& targetSRName)
{
    // Create migrate operations using MigrateVirtualDiskAction
    // This uses VDI.async_pool_migrate instead of copy+delete
    OperationManager* opManager = OperationManager::instance();

    if (m_vdiRefs.count() == 1)
    {
        // Single VDI migration
        QVariantMap vdiData = m_xenLib->getCache()->ResolveObjectData("vdi", m_vdiRefs.first());
        QString vdiName = vdiData.value("name_label", "Virtual Disk").toString();

        MigrateVirtualDiskAction* action = new MigrateVirtualDiskAction(
            m_xenLib->getConnection(),
            m_vdiRefs.first(),
            targetSRRef);

        action->setTitle(QString("Migrating virtual disk '%1' to '%2'")
                             .arg(vdiName)
                             .arg(targetSRName));
        action->setDescription(QString("Migrating '%1'...").arg(vdiName));

        opManager->registerOperation(action);
        action->runAsync();
    } else
    {
        // Multiple VDI migration - batch in parallel (max 3 at a time)
        // C# uses ParallelAction with BATCH_SIZE=3
        for (const QString& vdiRef : m_vdiRefs)
        {
            QVariantMap vdiData = m_xenLib->getCache()->ResolveObjectData("vdi", vdiRef);
            QString vdiName = vdiData.value("name_label", "Virtual Disk").toString();

            MigrateVirtualDiskAction* action = new MigrateVirtualDiskAction(
                m_xenLib->getConnection(),
                vdiRef,
                targetSRRef);

            action->setTitle(QString("Migrating virtual disk '%1' to '%2'")
                                 .arg(vdiName)
                                 .arg(targetSRName));
            action->setDescription(QString("Migrating '%1'...").arg(vdiName));

            opManager->registerOperation(action);
            action->runAsync();
        }
    }
}
