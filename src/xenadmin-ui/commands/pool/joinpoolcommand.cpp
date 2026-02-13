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
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/asyncoperation.h"
#include "xenlib/xen/xenapi/xenapi_Pool.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>

namespace
{
    class JoinPoolAsyncAction : public AsyncOperation
    {
        public:
            JoinPoolAsyncAction(XenConnection* connection,
                                const QString& masterAddress,
                                const QString& username,
                                const QString& password,
                                QObject* parent = nullptr)
                : AsyncOperation(connection,
                                 QObject::tr("Join Resource Pool"),
                                 QObject::tr("Joining resource pool..."),
                                 parent),
                  m_masterAddress(masterAddress),
                  m_username(username),
                  m_password(password)
            {
            }

        protected:
            void run() override
            {
                this->SetPercentComplete(5);
                this->SetDescription(QObject::tr("Starting pool join task..."));

                const QString taskRef = XenAPI::Pool::async_join(this->GetSession(), m_masterAddress, m_username, m_password);

                this->SetPercentComplete(10);
                this->SetDescription(QObject::tr("Joining resource pool..."));
                this->pollToCompletion(taskRef, 10, 100);
            }

        private:
            QString m_masterAddress;
            QString m_username;
            QString m_password;
    };
}

JoinPoolCommand::JoinPoolCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool JoinPoolCommand::CanRun() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    if (!host || !host->GetConnection() || !host->GetConnection()->IsConnected())
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

    // Create a dialog to get pool master details
    QDialog dialog(MainWindow::instance());
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
        QMessageBox::warning(MainWindow::instance(), "Join Pool", "Please provide all required information.");
        return;
    }

    // Confirm the operation
    int result = QMessageBox::question(MainWindow::instance(), "Join Resource Pool",
                                       QString("This will join the current host to the pool managed by %1.\n\n"
                                               "The host will be rebooted and all VMs will be shut down.\n\n"
                                               "Do you want to continue?")
                                           .arg(masterAddress),
                                       QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    // Get the current GetConnection (host being joined)
    XenConnection* hostConnection = host->GetConnection();
    if (!hostConnection)
    {
        QMessageBox::critical(MainWindow::instance(), "Join Pool", "No active connection to the host.");
        return;
    }

    XenAPI::Session* session = hostConnection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        QMessageBox::critical(MainWindow::instance(), "Join Pool", "Host session is not active.");
        return;
    }

    auto* action = new JoinPoolAsyncAction(hostConnection, masterAddress, username, password, this->mainWindow());
    action->SetHost(host);

    this->RunMultipleActions({ action }, QObject::tr("Join Resource Pool"),  QObject::tr("Joining resource pool..."), QObject::tr("Joined"), true, true);
}

QString JoinPoolCommand::MenuText() const
{
    return "Join Pool...";
}

QSharedPointer<Host> JoinPoolCommand::getSelectedHost() const
{
    return qSharedPointerDynamicCast<Host>(this->GetObject());
}
