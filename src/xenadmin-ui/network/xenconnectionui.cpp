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

#include "xenconnectionui.h"
#include "../dialogs/addserverdialog.h"
#include "../../xenlib/xen/network/connection.h"
#include <QtCore/QDateTime>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QDialog>

QHash<XenConnection*, QPointer<QProgressDialog>> XenConnectionUI::s_connectionDialogs;
QHash<XenConnection*, QMetaObject::Connection> XenConnectionUI::s_connectedHandlers;
QHash<XenConnection*, QMetaObject::Connection> XenConnectionUI::s_errorHandlers;
QHash<XenConnection*, QMetaObject::Connection> XenConnectionUI::s_disconnectedHandlers;

void XenConnectionUI::BeginConnect(XenConnection* connection,
                                   bool interactive,
                                   QWidget* owner,
                                   bool initiateCoordinatorSearch,
                                   const PasswordPrompt& promptForNewPassword)
{
    if (!connection)
        return;

    if (initiateCoordinatorSearch)
    {
        connection->setFindingNewCoordinator(true);
        connection->setFindingNewCoordinatorStartedAt(QDateTime::currentDateTimeUtc());
    }

    if (interactive)
    {
        if (s_connectionDialogs.contains(connection) && s_connectionDialogs[connection])
        {
            s_connectionDialogs[connection]->raise();
            s_connectionDialogs[connection]->activateWindow();
            return;
        }

        QProgressDialog* dialog = new QProgressDialog("Connecting to server...", "Cancel", 0, 0, owner);
        dialog->setWindowTitle("Connect to Server");
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setAutoClose(false);
        dialog->setAutoReset(false);
        dialog->show();

        s_connectionDialogs.insert(connection, dialog);
        QObject::connect(dialog, &QProgressDialog::canceled, connection, [connection]() {
            connection->Disconnect();
        });
    }

    RegisterEventHandlers(connection);

    if (promptForNewPassword)
    {
        QString newPassword;
        if (promptForNewPassword(connection, connection->GetPassword(), &newPassword))
            connection->ConnectToHost(connection->GetHostname(),
                                      connection->GetPort(),
                                      connection->GetUsername(),
                                      newPassword);
        return;
    }

    connection->ConnectToHost(connection->GetHostname(),
                              connection->GetPort(),
                              connection->GetUsername(),
                              connection->GetPassword());
}

bool XenConnectionUI::PromptForNewPassword(XenConnection* connection,
                                           const QString& oldPassword,
                                           QWidget* owner,
                                           QString* newPassword)
{
    if (!connection)
        return false;

    Q_UNUSED(oldPassword);

    AddServerDialog dialog(connection, true, owner);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    if (newPassword)
        *newPassword = dialog.password();

    return true;
}

void XenConnectionUI::RegisterEventHandlers(XenConnection* connection)
{
    UnregisterEventHandlers(connection);

    s_connectedHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::connected, connection, [connection]() {
            HandleConnectionResult(connection, true, QString());
        }));

    s_errorHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::error, connection, [connection](const QString& error) {
            HandleConnectionResult(connection, false, error);
        }));

    s_disconnectedHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::disconnected, connection, [connection]() {
            HandleConnectionStateChanged(connection, false);
        }));
}

void XenConnectionUI::UnregisterEventHandlers(XenConnection* connection)
{
    if (!connection)
        return;

    if (s_connectedHandlers.contains(connection))
        QObject::disconnect(s_connectedHandlers.take(connection));
    if (s_errorHandlers.contains(connection))
        QObject::disconnect(s_errorHandlers.take(connection));
    if (s_disconnectedHandlers.contains(connection))
        QObject::disconnect(s_disconnectedHandlers.take(connection));
}

void XenConnectionUI::HandleConnectionResult(XenConnection* connection, bool connected, const QString& error)
{
    QPointer<QProgressDialog> dialog = s_connectionDialogs.value(connection);
    if (dialog)
    {
        dialog->close();
        dialog->deleteLater();
    }
    s_connectionDialogs.remove(connection);

    if (!connected && !error.isEmpty())
        ShowConnectingDialogError(dialog ? dialog->parentWidget() : nullptr, connection, error);

    HandleConnectionStateChanged(connection, connected);
}

void XenConnectionUI::HandleConnectionStateChanged(XenConnection* connection, bool connected)
{
    if (connected || !connection)
        return;

    QPointer<QProgressDialog> dialog = s_connectionDialogs.value(connection);
    if (dialog)
    {
        dialog->close();
        dialog->deleteLater();
    }
    s_connectionDialogs.remove(connection);
}

void XenConnectionUI::ShowConnectingDialogError(QWidget* owner, XenConnection* connection, const QString& error)
{
    Q_UNUSED(connection);
    QMessageBox::critical(owner, "Connection Failed", error);
}
