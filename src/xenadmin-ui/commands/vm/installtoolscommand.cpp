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

#include "installtoolscommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/actions/vm/installpvtoolsaction.h"
#include "xenlib/xen/vm.h"
#include <QMessageBox>
#include <QPointer>

InstallToolsCommand::InstallToolsCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool InstallToolsCommand::CanInstallTools(const QSharedPointer<VM>& vm)
{
    // The old C# InstallToolsCommand.CanRun, plus a CD drive to put the ISO in
    return vm && vm->IsValid() && !vm->IsTemplate() && !vm->IsSnapshot() && !vm->IsControlDomain()
        && !vm->IsLocked() && vm->IsRunning() && vm->FindVMCDROM();
}

bool InstallToolsCommand::CanRun() const
{
    return CanInstallTools(this->getVM());
}

void InstallToolsCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!CanInstallTools(vm))
        return;

    MainWindow* mainWindow = MainWindow::instance();
    const QString message = tr("The Guest Tools ISO will be inserted into the CD drive of '%1', replacing any disk "
                               "in it. You can then install the tools from within the VM.\n\nDo you want to continue?")
                                .arg(vm->GetName());
    if (QMessageBox::question(mainWindow, tr("Install Guest Tools"), message,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    InstallPVToolsAction* action = new InstallPVToolsAction(vm, mainWindow);

    // Linux guests run the installer by hand (C# showed the same instructions)
    if (!vm->IsWindows())
    {
        QPointer<MainWindow> window = mainWindow;
        const QString vmName = vm->GetName();
        connect(action, &AsyncOperation::completed, mainWindow, [window, vmName]()
        {
            if (!window)
                return;
            QMessageBox::information(window, tr("Install Guest Tools"),
                                     tr("The Guest Tools ISO is in the CD drive of '%1'. To install the tools, run "
                                        "as root in the VM:\n\n"
                                        "mount /dev/cdrom /mnt\n"
                                        "bash /mnt/Linux/install.sh\n"
                                        "umount /mnt\n\n"
                                        "then restart the VM.").arg(vmName));
        });
    }

    action->RunAsync(true);
}

QString InstallToolsCommand::MenuText() const
{
    return tr("&Install Guest Tools");
}
