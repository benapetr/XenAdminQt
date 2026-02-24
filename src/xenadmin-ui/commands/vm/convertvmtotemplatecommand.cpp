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

#include "convertvmtotemplatecommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/setvmotherconfigaction.h"
#include "xenlib/xen/actions/vm/vmtotemplateaction.h"
#include <QMessageBox>

ConvertVMToTemplateCommand::ConvertVMToTemplateCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool ConvertVMToTemplateCommand::CanRun() const
{
    const QList<QSharedPointer<XenObject>> selectedObjects = this->getSelectedObjects();
    if (selectedObjects.size() != 1)
        return false;

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return this->canConvertToTemplate();
}

void ConvertVMToTemplateCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    QString vmName = this->getSelectedVMName();
    if (vmName.isEmpty())
        return;

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        MainWindow::instance(),
        tr("Convert to Template"),
        tr("Are you sure you want to convert VM '%1' to a template?\n\n"
           "The VM will be shut down and converted to a template. "
           "Templates cannot be started directly but can be used to create new VMs.")
            .arg(vmName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Get XenConnection from VM
        XenConnection* conn = vm->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(MainWindow::instance(), "Not Connected", "Not connected to XenServer");
            return;
        }

        QList<AsyncOperation*> actions;
        actions.append(new SetVMOtherConfigAction(vm, "instant", "true"));
        actions.append(new VMToTemplateAction(vm));

        this->RunMultipleActions(actions,
                                 tr("Templatizing VM '%1'").arg(vmName),
                                 tr("Converting VM to template..."),
                                 tr("VM converted to template"),
                                 true,
                                 true);
    }
}

QString ConvertVMToTemplateCommand::MenuText() const
{
    return "Convert to Template";
}

bool ConvertVMToTemplateCommand::canConvertToTemplate() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (vm->IsTemplate())
        return false;

    if (vm->IsLocked())
        return false;

    return vm->GetAllowedOperations().contains("make_into_template");
}
