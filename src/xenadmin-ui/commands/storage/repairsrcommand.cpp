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
#include "xenlib.h"
#include "xen/api.h"
#include <QMessageBox>
#include <QTimer>

RepairSRCommand::RepairSRCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool RepairSRCommand::canRun() const
{
    QString srRef = this->getSelectedSRRef();
    return !srRef.isEmpty();
}

void RepairSRCommand::run()
{
    QString srRef = this->getSelectedSRRef();
    QString srName = this->getSelectedSRName();

    if (srRef.isEmpty() || srName.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Repair Storage Repository",
                                    QString("This will attempt to repair storage repository '%1'.\n\n"
                                            "Are you sure you want to continue?")
                                        .arg(srName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Repairing storage repository '%1'...").arg(srName));

        bool success = this->mainWindow()->xenLib()->getAPI()->repairSR(srRef);
        if (success)
        {
            this->mainWindow()->showStatusMessage(QString("Storage repository '%1' repaired successfully").arg(srName), 5000);
            // Refresh the tree to update status
            QTimer::singleShot(2000, this->mainWindow(), &MainWindow::refreshServerTree);
        } else
        {
            QMessageBox::warning(this->mainWindow(), "Repair Storage Repository Failed",
                                 QString("Failed to repair storage repository '%1'. Check the error log for details.").arg(srName));
            this->mainWindow()->showStatusMessage("SR repair failed", 5000);
        }
    }
}

QString RepairSRCommand::menuText() const
{
    return "Repair";
}

QString RepairSRCommand::getSelectedSRRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "storage")
        return QString();

    return this->getSelectedObjectRef();
}

QString RepairSRCommand::getSelectedSRName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "storage")
        return QString();

    return item->text(0);
}