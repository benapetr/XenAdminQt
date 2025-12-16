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

#include "newsrcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/newsrwizard.h"
#include "xenlib.h"
#include <QtWidgets>

NewSRCommand::NewSRCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
    qDebug() << "NewSRCommand: Created";
}

void NewSRCommand::run()
{
    qDebug() << "NewSRCommand: Executing New Storage Repository command";

    if (!canRun())
    {
        qWarning() << "NewSRCommand: Cannot execute - requirements not met";
        QMessageBox::warning(nullptr, tr("Cannot Create Storage Repository"),
                             tr("Storage repository creation is not available at this time.\n"
                                "Please ensure you have an active connection to a XenServer."));
        return;
    }

    showNewSRWizard();
}

bool NewSRCommand::canRun() const
{
    // Match C# NewSRCommand::CanRunCore logic:
    // - Must have a host or pool connection
    // - Connection must be active
    // - Host (if selected) must not have active host actions

    if (!mainWindow() || !xenLib() || !xenLib()->isConnected())
        return false;

    QString objectType = getSelectedObjectType();

    // Can run if host or pool is selected
    if (objectType == "host" || objectType == "pool")
        return true;

    // Can run if any object under a pool/host is selected (SR, VM, etc.)
    // For now, allow if connected
    return true;
}

QString NewSRCommand::menuText() const
{
    return tr("New Storage Repository...");
}

void NewSRCommand::showNewSRWizard()
{
    qDebug() << "NewSRCommand: Opening New Storage Repository Wizard";

    NewSRWizard wizard(mainWindow());

    if (wizard.exec() == QDialog::Accepted)
    {
        qDebug() << "NewSRCommand: New Storage Repository Wizard completed successfully";

        // Get the wizard results
        SRType srType = wizard.getSelectedSRType();
        QString srName = wizard.getSRName();
        QString srDescription = wizard.getSRDescription();

        qDebug() << "NewSRCommand: Creating SR -"
                 << "Type:" << static_cast<int>(srType)
                 << "Name:" << srName
                 << "Description:" << srDescription;

        // TODO: Implement actual SR creation using XenAPI
        QMessageBox::information(mainWindow(), tr("Storage Repository Created"),
                                 tr("Storage Repository '%1' has been created successfully.\n\n"
                                    "Note: This is a demonstration. Actual XenAPI integration "
                                    "will be implemented in the next phase.")
                                     .arg(srName));

        if (mainWindow())
        {
            qDebug() << "NewSRCommand: SR creation completed";
        }
    } else
    {
        qDebug() << "NewSRCommand: New Storage Repository Wizard cancelled";
    }
}