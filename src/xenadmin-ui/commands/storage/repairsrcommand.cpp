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
#include "../../operations/operationmanager.h"
#include "xen/sr.h"
#include "xen/actions/sr/srrefreshaction.h"
#include <QMessageBox>
#include <QTimer>

RepairSRCommand::RepairSRCommand(MainWindow* mainWindow, QObject* parent)
    : SRCommand(mainWindow, parent)
{
}

bool RepairSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    return sr != nullptr;
}

void RepairSRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Repair Storage Repository",
                                    QString("This will attempt to repair storage repository '%1'.\n\n"
                                            "Are you sure you want to continue?")
                                        .arg(srName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Repairing storage repository '%1'...").arg(srName));

        XenConnection* connection = sr->GetConnection();
        if (!connection || !connection->IsConnected())
        {
            QMessageBox::warning(this->mainWindow(), "Repair Storage Repository Failed",
                                 "Not connected to XenServer.");
            return;
        }

        SrRefreshAction* action = new SrRefreshAction(connection, srRef, this);
        OperationManager::instance()->registerOperation(action);

        connect(action, &AsyncOperation::completed, [this, srName, action]() {
            this->mainWindow()->showStatusMessage(
                QString("Storage repository '%1' repaired successfully").arg(srName), 5000);
            QTimer::singleShot(2000, this->mainWindow(), &MainWindow::refreshServerTree);
            action->deleteLater();
        });

        connect(action, &AsyncOperation::failed, [this, srName, action](const QString& error) {
            QMessageBox::warning(this->mainWindow(), "Repair Storage Repository Failed",
                                 QString("Failed to repair storage repository '%1': %2").arg(srName, error));
            this->mainWindow()->showStatusMessage("SR repair failed", 5000);
            action->deleteLater();
        });

        action->runAsync();
    }
}

QString RepairSRCommand::MenuText() const
{
    return "Repair";
}
