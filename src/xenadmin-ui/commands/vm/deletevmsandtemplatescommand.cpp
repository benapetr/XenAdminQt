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

#include "deletevmsandtemplatescommand.h"
#include "../../mainwindow.h"
#include "../../../xenlib/xenlib.h"

DeleteVMsAndTemplatesCommand::DeleteVMsAndTemplatesCommand(MainWindow* mainWindow, QObject* parent)
    : DeleteVMCommand(mainWindow, parent)
{
}

bool DeleteVMsAndTemplatesCommand::canRun() const
{
    QString objectType = this->getSelectedObjectType();

    // Allow both VMs and templates
    if (objectType != "vm" && objectType != "template")
        return false;

    QString vmRef = this->getSelectedObjectRef();
    return this->canRunForVM(vmRef);
}

bool DeleteVMsAndTemplatesCommand::canRunForVM(const QString& vmRef) const
{
    if (vmRef.isEmpty())
        return false;

    QVariantMap vmData = this->xenLib()->getCachedObjectData("vm", vmRef);
    if (vmData.isEmpty())
        return false;

    // Check if VM/template is locked
    if (vmData.value("locked", false).toBool())
        return false;

    // Don't allow deletion of snapshots
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    // Check allowed operations
    QVariantList allowedOps = vmData.value("allowed_operations").toList();
    return allowedOps.contains("destroy");
}

QString DeleteVMsAndTemplatesCommand::menuText() const
{
    return tr("&Delete");
}
