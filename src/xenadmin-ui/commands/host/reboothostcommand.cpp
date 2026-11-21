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

#include "reboothostcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/commanderrordialog.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/host/reboothostaction.h"
#include <QMessageBox>
#include <QPushButton>

RebootHostCommand::RebootHostCommand(MainWindow* mainWindow, QObject* parent) : HostCommand(mainWindow, parent)
{
}

bool RebootHostCommand::CanRun() const
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

void RebootHostCommand::Run()
{
    QList<QSharedPointer<Host>> runnable;
    QHash<QSharedPointer<XenObject>, QString> cantRunReasons;

    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;

        if (host->IsLive())
            runnable.append(host);
        else
            cantRunReasons.insert(host, tr("Server is not reachable"));
    }

    if (runnable.isEmpty() && cantRunReasons.isEmpty())
        return;

    if (!cantRunReasons.isEmpty())
    {
        CommandErrorDialog::DialogMode mode =
            runnable.isEmpty() ? CommandErrorDialog::DialogMode::Close
                               : CommandErrorDialog::DialogMode::OKCancel;
        CommandErrorDialog dialog(tr("Reboot Server"), tr("Some servers cannot be rebooted."), cantRunReasons, mode, MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted || runnable.isEmpty())
            return;
    }

    // C# RebootHostCommand.ConfirmationDialogText
    bool hasRunningVMs = false;
    QStringList hciHosts;
    for (const QSharedPointer<Host>& host : runnable)
    {
        if (host->HasRunningVMs())
            hasRunningVMs = true;

        // VM.HciWarnBeforeShutdown: the VM is a storage virtual appliance
        for (const QSharedPointer<VM>& vm : host->GetResidentVMs())
        {
            if (vm && vm->GetOtherConfig().contains("hci-warn-before-shutdown"))
            {
                hciHosts.append(host->GetName());
                break;
            }
        }
    }

    const int count = runnable.count();
    QString confirmText;
    if (!hciHosts.isEmpty())
    {
        confirmText = hciHosts.count() == 1
            ? tr("Rebooting a server will also shut down all the VMs running on it. '%1' is running a storage "
                 "virtual appliance and rebooting it might impact the availability of your storage. "
                 "Are you sure you want to continue?").arg(hciHosts.first())
            : tr("Rebooting a server will also shut down all the VMs running on it. The following servers are "
                 "running a storage virtual appliance and rebooting them might impact the availability of your "
                 "storage:\n\n%1\n\nAre you sure you want to continue?").arg(hciHosts.join("\n"));
    } else if (hasRunningVMs)
    {
        confirmText = count == 1
            ? tr("Are you sure you want to reboot '%1'?\n\nThis will also shut down all the VMs currently running on it.")
                  .arg(runnable.first()->GetName())
            : tr("Are you sure you want to reboot the selected servers?\n\nThis will also shut down all the VMs currently running on them.");
    } else
    {
        confirmText = count == 1
            ? tr("Are you sure you want to reboot '%1'?").arg(runnable.first()->GetName())
            : tr("Are you sure you want to reboot the selected servers?");
    }

    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle(count == 1 ? tr("Reboot Server") : tr("Reboot Multiple Servers"));
    msgBox.setText(confirmText);
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton* yesButton = msgBox.addButton(tr("&Yes, Reboot"), QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(noButton);
    msgBox.exec();
    if (msgBox.clickedButton() != yesButton)
        return;

    auto ntolPrompt = [](QSharedPointer<Pool> pool, qint64 current, qint64 target)
    {
        const QString poolName = pool ? pool->GetName() : QString();
        const QString poolLabel = poolName.isEmpty() ? tr("pool") : tr("pool '%1'").arg(poolName);
        const QString text = tr("HA is enabled for %1.\n\n"
                                "To reboot this server, the pool's server failures to tolerate must be "
                                "reduced from %2 to %3.\n\n"
                                "Do you want to continue?")
                                 .arg(poolLabel)
                                 .arg(current)
                                 .arg(target);

        return QMessageBox::question(MainWindow::instance(),
                                     tr("Adjust HA Failures to Tolerate"),
                                     text,
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No) != QMessageBox::Yes;
    };

    // C# runs these through RunMultipleActions; failures are reported in the Events
    // history like any other action, without a separate message box
    QList<AsyncOperation*> actions;
    for (const QSharedPointer<Host>& host : runnable)
        actions.append(new RebootHostAction(host, ntolPrompt, nullptr));

    this->RunMultipleActions(actions, QString(), tr("Rebooting servers"), tr("Servers rebooted"), true, true);
}

QString RebootHostCommand::MenuText() const
{
    return "Reboot Host";
}

QIcon RebootHostCommand::GetIcon() const
{
    return QIcon(":/icons/reboot.png");
}
