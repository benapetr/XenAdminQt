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

#include "vmpropertiescommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include "../../dialogs/vmpropertiesdialog.h"
#include <QtWidgets>
#include "xenlib/xen/vm.h"

VMPropertiesCommand::VMPropertiesCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

VMPropertiesCommand::VMPropertiesCommand(QSharedPointer<VM> vm, MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
    if (vm)
        this->SetSelectionOverride({vm});
}

void VMPropertiesCommand::Run()
{
    if (!this->CanRun())
    {
        qWarning() << "VMPropertiesCommand: Cannot execute - no VM selected or invalid state";
        QMessageBox::warning(nullptr, tr("Cannot Show Properties"), tr("No VM is selected or the VM is in an invalid state."));
        return;
    }

    this->showPropertiesDialog();
}

bool VMPropertiesCommand::CanRun() const
{
    if (this->getSelectedObjects().size() != 1)
        return false;

    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    if (!vm->GetConnection() || !vm->GetConnection()->IsConnected())
        return false;

    return !vm->IsTemplate() && !vm->IsLocked();
}

QString VMPropertiesCommand::MenuText() const
{
    return tr("Properties...");
}

QString VMPropertiesCommand::toolTip() const
{
    return tr("Show virtual machine properties and configuration");
}

QIcon VMPropertiesCommand::icon() const
{
    // Use standard Qt icon for now - should be replaced with proper properties icon
    return QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView);
}

void VMPropertiesCommand::showPropertiesDialog()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    VMPropertiesDialog dialog(vm, mainWindow());

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "VMPropertiesCommand: VM Properties dialog completed with changes";
        mainWindow()->RefreshServerTree();
        qDebug() << "VMPropertiesCommand: VM properties updated successfully";
    } else
    {
        qDebug() << "VMPropertiesCommand: VM Properties dialog cancelled";
    }
}
