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

#include "repairsrcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/repairsrdialog.h"
#include "xen/sr.h"
#include <QMessageBox>

RepairSRCommand::RepairSRCommand(MainWindow* mainWindow, QObject* parent) : SRCommand(mainWindow, parent)
{
}

bool RepairSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    XenConnection* connection = sr->GetConnection();
    if (!connection || !connection->IsConnected())
        return false;

    const bool noOps = sr->CurrentOperations().isEmpty();
    return sr->HasPBDs() && (sr->IsBroken() || !sr->MultipathAOK()) && noOps &&
           sr->CanRepairAfterUpgradeFromLegacySL();
}

void RepairSRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    XenConnection* connection = sr->GetConnection();
    if (!connection || !connection->IsConnected())
    {
        QMessageBox::warning(MainWindow::instance(), "Repair Storage Repository Failed", "Not connected to XenServer.");
        return;
    }

    if (!sr->MultipathAOK())
    {
        QMessageBox::warning(MainWindow::instance(), "Multipathing", "Multipathing has failed on this storage repository.");
    }

    // Show the RepairSRDialog which will handle the repair operation
    RepairSRDialog* dialog = new RepairSRDialog(sr, true, MainWindow::instance());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

QString RepairSRCommand::MenuText() const
{
    return "Repair";
}

QIcon RepairSRCommand::GetIcon() const
{
    return QIcon(":/icons/storage_broken.png");
}
