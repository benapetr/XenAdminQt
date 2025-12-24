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

#include "joinpoolcommand.h"
#include "../../mainwindow.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/host.h"
#include "xen/xenapi/xenapi_Pool.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>

JoinPoolCommand::JoinPoolCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool JoinPoolCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host || !host->connection() || !host->connection()->isConnected())
        return false;

    // A standalone host can always attempt to join a pool
    // The actual join operation will validate pool membership
    return true;
}

void JoinPoolCommand::Run()
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host)
        return;

    QString hostRef = host->opaqueRef();

    if (hostRef.isEmpty())
        return;

    // Create a dialog to get pool master details
    QDialog dialog(this->mainWindow());
    dialog.setWindowTitle("Join Resource Pool");
    dialog.setMinimumWidth(400);

    QFormLayout* layout = new QFormLayout(&dialog);

    QLineEdit* addressEdit = new QLineEdit(&dialog);
    addressEdit->setPlaceholderText("e.g., 192.168.1.100");
    layout->addRow("Pool Master Address:", addressEdit);

    QLineEdit* usernameEdit = new QLineEdit(&dialog);
    usernameEdit->setText("root");
    layout->addRow("Username:", usernameEdit);

    QLineEdit* passwordEdit = new QLineEdit(&dialog);
    passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addRow("Password:", passwordEdit);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttonBox);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QString masterAddress = addressEdit->text().trimmed();
    QString username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text();

    if (masterAddress.isEmpty() || username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), "Join Pool",
                             "Please provide all required information.");
        return;
    }

    // Confirm the operation
    int result = QMessageBox::question(this->mainWindow(), "Join Resource Pool",
                                       QString("This will join the current host to the pool managed by %1.\n\n"
                                               "The host will be rebooted and all VMs will be shut down.\n\n"
                                               "Do you want to continue?")
                                           .arg(masterAddress),
                                       QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    // Get the current connection (host being joined)
    XenConnection* hostConnection = host->connection();
    if (!hostConnection)
    {
        QMessageBox::critical(this->mainWindow(), "Join Pool", "No active connection to the host.");
        return;
    }

    XenSession* session = hostConnection->getSession();
    if (!session || !session->isLoggedIn())
    {
        QMessageBox::critical(this->mainWindow(), "Join Pool",
                              "Host session is not active.");
        return;
    }

    this->mainWindow()->showStatusMessage(QString("Joining pool at %1...").arg(masterAddress), 0);

    try
    {
        // Call Pool.async_join directly from the current host's session
        // This is the correct pattern - the joining host calls async_join with the
        // coordinator's address and credentials
        QString taskRef = XenAPI::Pool::async_join(session, masterAddress, username, password);

        // Note: After successful join, the host will reboot and reconnect to the coordinator
        // The connection will be dropped, so we just show success message
        QMessageBox::information(this->mainWindow(), "Join Pool",
                                 QString("Successfully initiated join pool operation.\n"
                                         "Task: %1\n\n"
                                         "The host will now be rebooted and join the pool.")
                                     .arg(taskRef));

    } catch (const std::exception& e)
    {
        QMessageBox::critical(this->mainWindow(), "Join Pool", QString("Failed to join pool:\n%1").arg(e.what()));
    }
}

QString JoinPoolCommand::MenuText() const
{
    return "Join Pool...";
}

QSharedPointer<Host> JoinPoolCommand::getSelectedHost() const
{
    return qSharedPointerCast<Host>(this->GetObject());
}
