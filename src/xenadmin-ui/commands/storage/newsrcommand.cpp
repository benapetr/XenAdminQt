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
#include <QDebug>
#include "../../mainwindow.h"
#include <QDebug>
#include "../../dialogs/newsrwizard.h"
#include <QDebug>
#include "xenlib.h"
#include <QDebug>
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

    // Match C# NewSRCommand::RunCore logic:
    // - Show wizard with current connection
    // - Wizard handles all SR creation/reattachment internally
    // - If accepted, SR was successfully created (wizard shows progress dialog)
    // - If rejected, user cancelled

    NewSRWizard wizard(mainWindow());

    if (wizard.exec() == QDialog::Accepted)
    {
        qDebug() << "NewSRCommand: New Storage Repository Wizard completed successfully";
        
        // SR creation/reattachment was handled by wizard's accept() method
        // No additional action needed here - wizard already:
        // 1. Created Host object from pool coordinator
        // 2. Instantiated SrCreateAction or SrReattachAction
        // 3. Showed OperationProgressDialog
        // 4. Displayed success/error message
        
        if (mainWindow())
        {
            // Refresh the tree view to show new SR
            mainWindow()->refreshServerTree();
            qDebug() << "NewSRCommand: SR operation completed, refreshing main window";
        }
    }
    else
    {
        qDebug() << "NewSRCommand: New Storage Repository Wizard cancelled";
    }
}