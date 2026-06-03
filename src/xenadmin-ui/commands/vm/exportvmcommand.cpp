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

#include "exportvmcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/exportwizard.h"
#include "xenlib/xen/vm.h"
#include <QDebug>

ExportVMCommand::ExportVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool ExportVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->isVMExportable();
}

void ExportVMCommand::Run()
{
    if (!this->CanRun())
    {
        qDebug() << "ExportVMCommand: Run ignored because CanRun() is false" << this;
        return;
    }

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
    {
        qDebug() << "ExportVMCommand: Run ignored because no VM resolved" << this;
        return;
    }

    // Create wizard with connection context and preselect the current VM
    ExportWizard* wizard = new ExportWizard(vm->GetConnection(), MainWindow::instance());
    wizard->SetPreselectedVMs({vm});
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->show();
    wizard->raise();
    wizard->activateWindow();
}

QString ExportVMCommand::MenuText() const
{
    return "Export...";
}

bool ExportVMCommand::isVMExportable() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->IsTemplate() || vm->IsLocked())
        return false;

    for (const QString& op : vm->GetAllowedOperations())
    {
        if (op == "export")
            return true;
    }

    return false;
}
