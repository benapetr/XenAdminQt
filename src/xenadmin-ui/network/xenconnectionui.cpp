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
#include "../dialogs/connectingtoserverdialog.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/failure.h"
#include <QtCore/QDateTime>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>

QHash<XenConnection*, QPointer<ConnectingToServerDialog>> XenConnectionUI::s_connectionDialogs;
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
        connection->SetFindingNewCoordinator(true);
        connection->SetFindingNewCoordinatorStartedAt(QDateTime::currentDateTimeUtc());
    }

    RegisterEventHandlers(connection);

    if (interactive)
    {
        if (s_connectionDialogs.contains(connection) && s_connectionDialogs[connection])
        {
            s_connectionDialogs[connection]->raise();
            s_connectionDialogs[connection]->activateWindow();
            return;
        }

        ConnectingToServerDialog* dialog = new ConnectingToServerDialog(connection, owner);
        s_connectionDialogs.insert(connection, dialog);
        dialog->BeginConnect(owner, initiateCoordinatorSearch);
        return;
    }

    if (promptForNewPassword)
        connection->BeginConnect(initiateCoordinatorSearch,
                                 [connection, promptForNewPassword](const QString& oldPassword, QString* newPassword) {
                                     return promptForNewPassword(connection, oldPassword, newPassword);
                                 });
    else
        connection->BeginConnect(initiateCoordinatorSearch);
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
        QObject::connect(connection, &XenConnection::ConnectionResult, connection, [connection](bool connected, const QString& error) {
            HandleConnectionResult(connection, connected, error);
        }));

    s_errorHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::ConnectionStateChanged, connection, [connection]() {
            HandleConnectionStateChanged(connection, connection->IsConnected());
        }));

    s_disconnectedHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::ConnectionClosed, connection, [connection]() {
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
    QPointer<ConnectingToServerDialog> dialog = s_connectionDialogs.value(connection);
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

    QPointer<ConnectingToServerDialog> dialog = s_connectionDialogs.value(connection);
    if (dialog)
    {
        dialog->close();
        dialog->deleteLater();
    }
    s_connectionDialogs.remove(connection);
}

void XenConnectionUI::ShowConnectingDialogError(QWidget* owner, XenConnection* connection, const QString& error)
{
    if (!connection)
        return;

    QStringList failureDescription = connection->GetLastFailureDescription();
    if (!failureDescription.isEmpty())
    {
        Failure failure(failureDescription);
        const QString code = failure.errorCode();

        if (code == Failure::HOST_IS_SLAVE)
        {
            const QString masterHost = failureDescription.size() > 1
                ? failureDescription.at(1)
                : QString();
            if (!masterHost.isEmpty())
            {
                // C# XenConnectionUI: if we're already connected to that pool, say so instead
                const QString oldHost = connection->GetName();
                const QString poolName = XenConnection::ConnectedElsewhere(masterHost);
                if (!poolName.isEmpty())
                {
                    const QString text = poolName == oldHost
                        ? QObject::tr("You are already connected to '%1'").arg(poolName)
                        : QObject::tr("Server '%1' is a member of pool '%2' and is already connected.").arg(oldHost, poolName);
                    QMessageBox::information(owner, QObject::tr("Add New Server"), text);
                    return;
                }

                const QString prompt = QObject::tr("Server '%1' is in a pool.  To connect to a pool, you must connect to the pool coordinator.\n"
                                          "Do you want to connect to the pool coordinator '%2'?").arg(oldHost, masterHost);
                if (QMessageBox::question(owner, QObject::tr("Connect to Server"), prompt) == QMessageBox::Yes)
                {
                    connection->EndConnect(false, false);
                    connection->SetFindingNewCoordinator(false);
                    connection->SetFindingNewCoordinatorStartedAt(QDateTime());
                    connection->SetHostname(masterHost);
                    connection->BeginConnect(false);
                }
                // C#: nothing more to show when the user declines
                return;
            }
        } else if (code == Failure::RBAC_PERMISSION_DENIED)
        {
            QMessageBox::critical(owner, "Connection Failed", "You do not have permission to log in.");
            return;
        } else if (code == Failure::SESSION_AUTHENTICATION_FAILED)
        {
            QMessageBox::critical(owner, "Connection Failed", "User name and password mismatch.");
            return;
        } else if (code == Failure::HOST_STILL_BOOTING)
        {
            QMessageBox::critical(owner, "Connection Failed", "The host is still booting.");
            return;
        } else
        {
            const QString message = failure.message().isEmpty() ? error : failure.message();
            QMessageBox::critical(owner, "Connection Failed", message);
            return;
        }
    }

    // No xapi failure: a transport or other client-side error, shown as it is. C# branches on
    // the exception type the same way (Failure vs WebException etc.) and never parses the text;
    // GetLastFailureDescription is set whenever the server returned a failure, including the
    // HOST_IS_SLAVE redirect, so matching error codes in the message here would only misreport
    // a bare "Authentication failed" as a wrong password.
    QMessageBox::critical(owner, "Connection Failed", error);
}
