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

#include "detachsrcommand.h"
#include "xenlib/xen/actions/sr/detachsraction.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../dialogs/commanderrordialog.h"
#include "xenlib/operations/multipleaction.h"
#include <QMessageBox>
#include <QDebug>

DetachSRCommand::DetachSRCommand(MainWindow* mainWindow, QObject* parent) : SRCommand(mainWindow, parent)
{
}

void DetachSRCommand::setTargetSR(const QString& srRef)
{
    this->m_overrideSRRef = srRef;
}

QList<QSharedPointer<SR>> DetachSRCommand::selectedSRs() const
{
    QList<QSharedPointer<SR>> srs;
    QStringList selection = this->GetSelection();
    QSharedPointer<SR> baseSr = this->getSR();

    if (selection.isEmpty())
    {
        if (baseSr)
            srs.append(baseSr);
        return srs;
    }

    XenConnection* conn = baseSr ? baseSr->GetConnection() : nullptr;
    XenCache* cache = conn ? conn->GetCache() : nullptr;
    if (!cache)
        return srs;

    for (const QString& ref : selection)
    {
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(XenObjectType::SR, ref);
        if (sr)
            srs.append(sr);
    }

    return srs;
}

bool DetachSRCommand::canRunForSr(const QSharedPointer<SR>& sr, QString* reason) const
{
    if (!sr)
        return false;

    if (sr->IsDetached())
    {
        if (reason)
            *reason = "Storage repository is already detached.";
        return false;
    }

    if (sr->HasRunningVMs())
    {
        if (reason)
            *reason = "Storage repository has running VMs.";
        return false;
    }

    if (!sr->CurrentOperations().isEmpty() || sr->IsLocked())
    {
        if (reason)
            *reason = "An action is already in progress for this storage repository.";
        return false;
    }

    return true;
}

bool DetachSRCommand::CanRun() const
{
    QList<QSharedPointer<SR>> srs = selectedSRs();
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (canRunForSr(sr))
            return true;
    }

    return false;
}

void DetachSRCommand::Run()
{
    QList<QSharedPointer<SR>> srs = selectedSRs();
    if (srs.isEmpty())
    {
        qWarning() << "DetachSRCommand: Cannot run";
        return;
    }

    QList<QSharedPointer<SR>> runnable;
    QHash<QSharedPointer<XenObject>, QString> cantRunReasons;
    for (const QSharedPointer<SR>& sr : srs)
    {
        QString reason;
        if (canRunForSr(sr, &reason))
        {
            runnable.append(sr);
        } else if (sr)
        {
            cantRunReasons.insert(sr, reason);
        }
    }

    if (!cantRunReasons.isEmpty())
    {
        CommandErrorDialog::DialogMode mode =
            runnable.isEmpty() ? CommandErrorDialog::DialogMode::Close
                               : CommandErrorDialog::DialogMode::OKCancel;
        CommandErrorDialog dialog("Detach Storage Repository",
                                  "Some storage repositories cannot be detached.",
                                  cantRunReasons,
                                  mode,
                                  MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted || runnable.isEmpty())
            return;
    }

    if (runnable.isEmpty())
        return;

    const int count = runnable.count();
    QString confirmTitle = count == 1 ? "Detach Storage Repository" : "Detach Storage Repositories";
    QString confirmText = count == 1
        ? QString("Are you sure you want to detach SR '%1'?").arg(runnable.first()->GetName())
        : "Are you sure you want to detach the selected storage repositories?";

    // Show confirmation dialog
    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle(confirmTitle);
    msgBox.setText(confirmText);
    msgBox.setInformativeText("This will disconnect the storage repository from all hosts in the pool.\n"
                              "No data will be deleted, and the SR can be re-attached later.");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
    {
        return;
    }

    if (runnable.count() == 1)
    {
        QSharedPointer<SR> sr = runnable.first();
        QString srRef = sr->OpaqueRef();
        QString srName = sr->GetName().isEmpty() ? srRef : sr->GetName();

        qDebug() << "DetachSRCommand: Detaching SR" << srName << "(" << srRef << ")";

        XenConnection* conn = sr->GetConnection();
        if (!conn || !conn->IsConnected())
        {
            QMessageBox::warning(MainWindow::instance(), "Not Connected",
                                 "Not connected to XenServer");
            return;
        }

        DetachSrAction* action = new DetachSrAction(conn, srRef, srName, false, nullptr);

        QPointer<MainWindow> mainWindow = MainWindow::instance();
        connect(action, &AsyncOperation::completed, mainWindow, [mainWindow, srName, action]()
        {
            if (!mainWindow)
            {
                action->deleteLater();
                return;
            }

            if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
            {
                mainWindow->ShowStatusMessage(QString("Successfully detached SR '%1'").arg(srName), 5000);
            } else
            {
                QMessageBox::warning(
                    mainWindow,
                    "Detach SR Failed",
                    QString("Failed to detach SR '%1'").arg(srName));
            }
            action->deleteLater();
        }, Qt::QueuedConnection);

        action->RunAsync();
        return;
    }

    QList<AsyncOperation*> actions;
    for (const QSharedPointer<SR>& sr : runnable)
    {
        QString srRef = sr->OpaqueRef();
        QString srName = sr->GetName().isEmpty() ? srRef : sr->GetName();
        DetachSrAction* action = new DetachSrAction(sr->GetConnection(), srRef, srName, false, MainWindow::instance());
        actions.append(action);
    }

    MultipleAction* multi = new MultipleAction(
        nullptr,
        "Detach Storage Repositories",
        "Detaching storage repositories...",
        "Storage repositories detached successfully",
        actions,
        true,
        false,
        false,
        nullptr);
    connect(multi, &AsyncOperation::completed, multi, &QObject::deleteLater);
    connect(multi, &AsyncOperation::failed, multi, &QObject::deleteLater);
    multi->RunAsync();
}

QString DetachSRCommand::currentSR() const
{
    if (!this->m_overrideSRRef.isEmpty())
    {
        return this->m_overrideSRRef;
    }

    if (this->getSelectedObjectType() != XenObjectType::SR)
    {
        return QString();
    }

    return this->getSelectedObjectRef();
}
