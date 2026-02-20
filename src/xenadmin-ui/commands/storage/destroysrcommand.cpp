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

#include "destroysrcommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/sr/destroysraction.h"
#include <QPointer>
#include <QMessageBox>
#include <QDebug>

DestroySRCommand::DestroySRCommand(MainWindow* mainWindow, QObject* parent) : SRCommand(mainWindow, parent)
{
}

bool DestroySRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    return this->canSRBeDestroyed();
}

void DestroySRCommand::Run()
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    // Show critical warning dialog (double confirmation)
    QMessageBox msgBox(mainWindow());
    msgBox.setWindowTitle("Destroy Storage Repository");
    msgBox.setText(QString("WARNING: Are you about to DESTROY storage repository '%1'!").arg(srName));
    msgBox.setInformativeText(
        "This will PERMANENTLY DELETE all data on the storage repository!\n\n"
        "This action CANNOT be undone!\n\n"
        "Are you absolutely sure you want to continue?");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    qDebug() << "DestroySRCommand: Destroying SR" << srName << "(" << srRef << ")";

    // Get GetConnection from SR object for multi-GetConnection support
    XenConnection* conn = sr->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(MainWindow::instance(), "Not Connected", "Not connected to XenServer");
        return;
    }

    // Create and run destroy action
    DestroySrAction* action = new DestroySrAction(conn, srRef, srName, nullptr);

    QPointer<MainWindow> mainWindow = MainWindow::instance();
    if (!mainWindow)
    {
        action->deleteLater();
        return;
    }

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, mainWindow, [srName, action, mainWindow]()
    {
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            if (mainWindow)
                mainWindow->ShowStatusMessage(QString("Successfully destroyed SR '%1'").arg(srName), 5000);
        } else
        {
            QMessageBox::warning(
                mainWindow,
                "Destroy SR Failed",
                QString("Failed to destroy SR '%1'.\n\n%2").arg(srName, action->GetErrorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    }, Qt::QueuedConnection);

    // Run action asynchronously
    action->RunAsync();
}

QString DestroySRCommand::MenuText() const
{
    return "Destroy Storage Repository";
}

bool DestroySRCommand::canSRBeDestroyed() const
{
    QSharedPointer<SR> sr = this->getSR();
    if (!sr)
        return false;

    // Cannot destroy if SR has VDIs (contains data)
    if (!sr->GetVDIRefs().isEmpty())
        return false;

    // Cannot destroy shared SRs (must be detached first)
    if (sr->IsShared())
    {
        QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        foreach (QSharedPointer<PBD> pbd, pbds)
        {
            if (pbd->IsCurrentlyAttached())
                return false; // SR is still attached
        }
    }

    // Cannot destroy ISO or tools SR
    QString contentType = sr->ContentType();
    if (contentType == "iso" || contentType == "tools")
        return false;

    return true;
}
