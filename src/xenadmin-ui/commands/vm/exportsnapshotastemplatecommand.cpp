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

#include "exportsnapshotastemplatecommand.h"
#include "exportvmcommand.h"
#include "../../../xenlib/xenlib.h"
#include "../../../xenlib/xencache.h"
#include "../../mainwindow.h"
#include <QMessageBox>

ExportSnapshotAsTemplateCommand::ExportSnapshotAsTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ExportSnapshotAsTemplateCommand::CanRun() const
{
    QString vmRef = this->getSelectedObjectRef();
    QString type = this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != "vm")
    {
        return false;
    }

    // Get VM data from cache
    QVariantMap vmData = this->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return false;
    }

    // VM must be a snapshot
    bool isSnapshot = vmData.value("is_a_snapshot").toBool();
    return isSnapshot;
}

void ExportSnapshotAsTemplateCommand::Run()
{
    QString vmRef = this->getSelectedObjectRef();
    QString type = this->getSelectedObjectType();

    if (vmRef.isEmpty() || type != "vm")
    {
        return;
    }

    // Get VM data to verify it's a snapshot
    QVariantMap vmData = this->xenLib()->getCache()->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return;
    }

    bool isSnapshot = vmData.value("is_a_snapshot").toBool();
    if (!isSnapshot)
    {
        QMessageBox::warning(this->mainWindow(), tr("Not a Snapshot"),
                             tr("Selected item is not a VM snapshot"));
        return;
    }

    // Reuse ExportVMCommand - snapshots are exported just like VMs
    ExportVMCommand* exportCmd = new ExportVMCommand(this->mainWindow(), this);
    exportCmd->Run();
    exportCmd->deleteLater();
}

QString ExportSnapshotAsTemplateCommand::MenuText() const
{
    return tr("E&xport Snapshot as Template...");
}
