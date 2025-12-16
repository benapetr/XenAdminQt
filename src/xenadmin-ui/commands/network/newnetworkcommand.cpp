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

#include "newnetworkcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/newnetworkwizard.h"
#include <QtWidgets>

NewNetworkCommand::NewNetworkCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
    qDebug() << "NewNetworkCommand: Created with MainWindow";
}

void NewNetworkCommand::run()
{
    qDebug() << "NewNetworkCommand: Executing New Network command";

    if (!this->canRun())
    {
        qWarning() << "NewNetworkCommand: Cannot execute - requirements not met";
        QMessageBox::warning(nullptr, tr("Cannot Create Network"),
                             tr("Network creation is not available at this time.\n"
                                "Please ensure you have an active connection to a XenServer."));
        return;
    }

    this->showNewNetworkWizard();
}

bool NewNetworkCommand::canRun() const
{
    // For now, always allow network creation
    // TODO: Check if we have proper XenAPI connection and permissions
    return true;
}

QString NewNetworkCommand::menuText() const
{
    return tr("New Network...");
}

void NewNetworkCommand::showNewNetworkWizard()
{
    qDebug() << "NewNetworkCommand: Opening New Network Wizard";

    NewNetworkWizard wizard(this->mainWindow());

    if (wizard.exec() == QDialog::Accepted)
    {
        qDebug() << "NewNetworkCommand: New Network Wizard completed successfully";

        if (this->mainWindow())
        {
            qDebug() << "NewNetworkCommand: Network creation completed";
        }
    } else
    {
        qDebug() << "NewNetworkCommand: New Network Wizard cancelled";
    }
}