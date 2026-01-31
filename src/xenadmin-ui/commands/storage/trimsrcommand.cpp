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

#include "trimsrcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xen/pbd.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/actions/sr/srtrimaction.h"
#include <QMessageBox>
#include <QDebug>

TrimSRCommand::TrimSRCommand(MainWindow* mainWindow, QObject* parent) : SRCommand(mainWindow, parent)
{
}

void TrimSRCommand::setTargetSR(const QString& srRef, XenConnection* connection)
{
    this->m_overrideSRRef = srRef;
    this->m_overrideConnection = connection;
}

static QSharedPointer<SR> resolveOverrideSR(const QString& srRef, XenConnection* connection)
{
    if (srRef.isEmpty() || !connection || !connection->GetCache())
        return QSharedPointer<SR>();
    return connection->GetCache()->ResolveObject<SR>(XenObjectType::SR, srRef);
}

bool TrimSRCommand::CanRun() const
{
    QSharedPointer<SR> sr = this->m_overrideSRRef.isEmpty()
        ? this->getSR()
        : resolveOverrideSR(this->m_overrideSRRef, this->m_overrideConnection);
    if (!sr)
        return false;

    // Can trim if SR supports it and is attached to a host
    return this->supportsTrim(sr) && this->isAttachedToHost(sr);
}

void TrimSRCommand::Run()
{
    QSharedPointer<SR> sr = this->m_overrideSRRef.isEmpty()
        ? this->getSR()
        : resolveOverrideSR(this->m_overrideSRRef, this->m_overrideConnection);
    if (!sr)
        return;

    QString srRef = sr->OpaqueRef();
    QString srName = sr->GetName();

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Trim Storage Repository");
    msgBox.setText(QString("Are you sure you want to trim storage repository '%1'?").arg(srName));
    msgBox.setInformativeText("Trimming will reclaim freed space from the storage repository.\n\nThis operation may take some time depending on the amount of space to reclaim.\n\nDo you want to continue?");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "TrimSRCommand: Trimming SR" << srName << "(" << srRef << ")";

    // Get GetConnection from SR object for multi-GetConnection support
    XenConnection* conn = sr->GetConnection();
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), "Not Connected", "Not connected to XenServer");
        return;
    }

    // Create and run trim action
    SrTrimAction* action = new SrTrimAction(conn, sr, nullptr);

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [srName, action, sr]()
    {
        MainWindow* mainWindow = MainWindow::instance();
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            if (mainWindow)
                mainWindow->ShowStatusMessage(QString("Successfully trimmed SR '%1'").arg(srName), 5000);

            QMessageBox::information(
                mainWindow,
                "Trim Completed",
                QString("Successfully reclaimed freed space from storage repository '%1'.\n\n"
                        "The storage has been trimmed and space returned to the underlying storage.")
                    .arg(srName));
        } else
        {
            QMessageBox::warning(mainWindow, "Trim Failed", QString("Failed to trim SR '%1'.\n\n%2").arg(srName, action->GetErrorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
        sr->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
}

QString TrimSRCommand::MenuText() const
{
    return "Trim SR...";
}

bool TrimSRCommand::supportsTrim(const QSharedPointer<SR> &sr) const
{
    return sr && sr->SupportsTrim();
}

bool TrimSRCommand::isAttachedToHost(const QSharedPointer<SR> &sr) const
{
    if (!sr)
        return false;

    // Check if any PBD is currently attached
    QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();

    foreach (QSharedPointer<PBD> pbd, pbds)
    {
        if (pbd->IsCurrentlyAttached())
        {
            // At least one PBD is attached
            return true;
        }
    }

    return false;
}
