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

#include <QMessageBox>
#include <QDialog>
#include "restarttoolstackcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/commanderrordialog.h"
#include "xenlib/xen/actions/host/restarttoolstackaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/xenobject.h"

RestartToolstackCommand::RestartToolstackCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool RestartToolstackCommand::CanRun() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (hosts.isEmpty())
        return false;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsLive())
            return true;
    }

    return false;
}

void RestartToolstackCommand::Run()
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    QList<AsyncOperation*> actions;
    QHash<QSharedPointer<XenObject>, QString> cantRunReasons;
    QString firstRunnableHostName;

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsLive())
        {
            if (firstRunnableHostName.isEmpty())
                firstRunnableHostName = host->GetName();
            actions.append(new RestartToolstackAction(host, nullptr));
        } else if (host)
        {
            cantRunReasons.insert(host, tr("Server is not live."));
        }
    }

    if (!cantRunReasons.isEmpty())
    {
        CommandErrorDialog::DialogMode mode =
            actions.isEmpty() ? CommandErrorDialog::DialogMode::Close
                              : CommandErrorDialog::DialogMode::OKCancel;
        CommandErrorDialog dialog(tr("Restart Toolstack"),
                                  tr("Some servers cannot restart the toolstack."),
                                  cantRunReasons,
                                  mode,
                                  MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted || actions.isEmpty())
            return;
    }

    if (actions.isEmpty())
        return;

    const int count = actions.count();
    const QString confirmTitle = "Restart Toolstack";
    const QString confirmText = count == 1
        ? QString("Are you sure you want to restart the toolstack on '%1'?\n\n"
                  "The management interface will restart. This may take a minute.")
              .arg(firstRunnableHostName)
        : "Are you sure you want to restart the toolstack on the selected hosts?\n\n"
          "The management interface will restart. This may take a minute.";

    int ret = QMessageBox::warning(MainWindow::instance(), confirmTitle, confirmText, QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    this->RunMultipleActions(actions,
                             QString(),
                             tr("Restarting toolstack..."),
                             tr("Toolstack restarted"),
                             true);
}

QString RestartToolstackCommand::MenuText() const
{
    return "Restart Toolstack";
}
