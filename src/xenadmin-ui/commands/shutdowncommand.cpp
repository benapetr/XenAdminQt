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

#include "shutdowncommand.h"
#include "../mainwindow.h"
#include "host/shutdownhostcommand.h"
#include "vm/stopvmcommand.h"
#include <QMessageBox>

ShutdownCommand::ShutdownCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ShutdownCommand::canRun() const
{
    // Matches C# ShutDownCommand.CanRunCore: try host first, then VM
    // This allows the button to be enabled if EITHER a shutdownable host OR VM is selected

    QString objectType = this->getSelectedObjectType();

    if (objectType == "host")
    {
        // Check if host can be shut down
        ShutdownHostCommand hostCmd(this->mainWindow(), nullptr);
        return hostCmd.canRun();
    } else if (objectType == "vm")
    {
        // Check if VM can be shut down
        StopVMCommand vmCmd(this->mainWindow(), nullptr);
        return vmCmd.canRun();
    }

    return false;
}

void ShutdownCommand::run()
{
    // Matches C# ShutDownCommand.RunCore: try host first, then VM
    QString objectType = this->getSelectedObjectType();

    if (objectType == "host")
    {
        ShutdownHostCommand* hostCmd = new ShutdownHostCommand(this->mainWindow(), this);
        if (hostCmd->canRun())
        {
            hostCmd->run();
            hostCmd->deleteLater();
            return;
        }
        hostCmd->deleteLater();
    }

    if (objectType == "vm")
    {
        StopVMCommand* vmCmd = new StopVMCommand(this->mainWindow(), this);
        if (vmCmd->canRun())
        {
            vmCmd->run();
            vmCmd->deleteLater();
            return;
        }
        vmCmd->deleteLater();
    }

    // If we get here, neither command could run
    QMessageBox::warning(this->mainWindow(), "Cannot Shutdown",
                         "The selected object cannot be shut down.");
}

QString ShutdownCommand::menuText() const
{
    return "Shutdown";
}
