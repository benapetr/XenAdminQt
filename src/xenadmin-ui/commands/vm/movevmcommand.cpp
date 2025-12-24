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

#include "movevmcommand.h"
#include "../../mainwindow.h"
#include "xen/vm.h"
#include "xencache.h"
#include <QMessageBox>

MoveVMCommand::MoveVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool MoveVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->canVMBeMoved();
}

void MoveVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    // TODO: Launch Move/Migrate VM wizard
    QMessageBox::information(this->mainWindow(), "Not Implemented",
                             "Move VM functionality will be implemented in future phase.\n\n"
                             "This allows moving or migrating a VM to different storage or hosts.");
}

QString MoveVMCommand::MenuText() const
{
    return "Move VM...";
}

bool MoveVMCommand::canVMBeMoved() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    QVariantMap vmData = vm->data();

    // Check if VM is a template or snapshot
    if (vmData.value("is_a_template", false).toBool())
        return false;
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    // Check if VM has CBT (Changed Block Tracking) disabled
    // VMs with CBT enabled cannot be moved
    XenCache* cache = vm->connection()->getCache();

    QVariantList vbds = vmData.value("VBDs", QVariantList()).toList();
    for (const QVariant& vbdRefVar : vbds)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
        QString vdiRef = vbdData.value("VDI", "").toString();

        if (!vdiRef.isEmpty())
        {
            QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
            if (vdiData.value("cbt_enabled", false).toBool())
                return false; // CBT enabled, cannot move
        }
    }

    return true;
}
