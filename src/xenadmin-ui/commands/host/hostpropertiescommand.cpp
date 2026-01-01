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

#include "hostpropertiescommand.h"
#include "../../dialogs/hostpropertiesdialog.h"
#include "../../mainwindow.h"
#include "xen/host.h"
#include <QMessageBox>

HostPropertiesCommand::HostPropertiesCommand(MainWindow* mainWindow, QObject* parent)
    : HostCommand(mainWindow, parent)
{
}

bool HostPropertiesCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return false;

    return host->GetConnection() && host->GetConnection()->IsConnected();
}

void HostPropertiesCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    // New signature: GetConnection, hostRef, parent
    HostPropertiesDialog* dialog = new HostPropertiesDialog(host->GetConnection(), host->OpaqueRef(), this->mainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->exec();
}

QString HostPropertiesCommand::MenuText() const
{
    return "Properties";
}
