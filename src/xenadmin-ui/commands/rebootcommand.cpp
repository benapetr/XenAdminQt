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

#include "rebootcommand.h"
#include "../mainwindow.h"
#include "host/reboothostcommand.h"
#include "vm/restartvmcommand.h"
#include <QMessageBox>

RebootCommand::RebootCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool RebootCommand::CanRun() const
{
    // Try host first, then VM (matches C# pattern)
    QString objectType = this->getSelectedObjectType();

    if (objectType == "host")
    {
        RebootHostCommand hostCmd(this->mainWindow(), nullptr);
        return hostCmd.CanRun();
    } else if (objectType == "vm")
    {
        RestartVMCommand vmCmd(this->mainWindow(), nullptr);
        return vmCmd.CanRun();
    }

    return false;
}

void RebootCommand::Run()
{
    QString objectType = this->getSelectedObjectType();

    if (objectType == "host")
    {
        RebootHostCommand* hostCmd = new RebootHostCommand(this->mainWindow(), this);
        if (hostCmd->CanRun())
        {
            hostCmd->Run();
            hostCmd->deleteLater();
            return;
        }
        hostCmd->deleteLater();
    }

    if (objectType == "vm")
    {
        RestartVMCommand* vmCmd = new RestartVMCommand(this->mainWindow(), this);
        if (vmCmd->CanRun())
        {
            vmCmd->Run();
            vmCmd->deleteLater();
            return;
        }
        vmCmd->deleteLater();
    }

    QMessageBox::warning(this->mainWindow(), "Cannot Reboot",
                         "The selected object cannot be rebooted.");
}

QString RebootCommand::MenuText() const
{
    return "Reboot";
}
