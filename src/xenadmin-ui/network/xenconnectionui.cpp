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
        connection->setFindingNewCoordinator(true);
        connection->setFindingNewCoordinatorStartedAt(QDateTime::currentDateTimeUtc());
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
        QObject::connect(connection, &XenConnection::connectionResult, connection, [connection](bool connected, const QString& error) {
            HandleConnectionResult(connection, connected, error);
        }));

    s_errorHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::connectionStateChanged, connection, [connection]() {
            HandleConnectionStateChanged(connection, connection->IsConnected());
        }));

    s_disconnectedHandlers.insert(connection,
        QObject::connect(connection, &XenConnection::connectionClosed, connection, [connection]() {
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
                const QString prompt = QString("This server is a pool member. Connect to the pool coordinator at %1 instead?")
                                           .arg(masterHost);
                if (QMessageBox::question(owner, "Connect to Server", prompt) == QMessageBox::Yes)
                {
                    connection->Disconnect();
                    connection->setFindingNewCoordinator(false);
                    connection->setFindingNewCoordinatorStartedAt(QDateTime());
                    connection->SetHostname(masterHost);
                    connection->BeginConnect(false);
                    return;
                }
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

    //! TODO this fallback is probably not needed, review and eventually delete it

    if (error.startsWith("HOST_IS_SLAVE:"))
    {
        const QString masterHost = error.section(':', 1);
        const QString prompt = QString("This server is a pool member. Connect to the pool coordinator at %1 instead?")
                                   .arg(masterHost);
        if (QMessageBox::question(owner, "Connect to Server", prompt) == QMessageBox::Yes)
        {
            connection->Disconnect();
            connection->setFindingNewCoordinator(false);
            connection->setFindingNewCoordinatorStartedAt(QDateTime());
            connection->SetHostname(masterHost);
            connection->BeginConnect(false);
            return;
        }
    }

    if (error.contains("RBAC_PERMISSION_DENIED", Qt::CaseInsensitive))
    {
        QMessageBox::critical(owner, "Connection Failed", "You do not have permission to log in.");
        return;
    }

    if (error.contains("SESSION_AUTHENTICATION_FAILED", Qt::CaseInsensitive) ||
        error.contains("Authentication failed", Qt::CaseInsensitive))
    {
        QMessageBox::critical(owner, "Connection Failed", "User name and password mismatch.");
        return;
    }

    if (error.contains("HOST_STILL_BOOTING", Qt::CaseInsensitive))
    {
        QMessageBox::critical(owner, "Connection Failed", "The host is still booting.");
        return;
    }

    QMessageBox::critical(owner, "Connection Failed", error);
}
