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

#ifndef XENCONNECTIONUI_H
#define XENCONNECTIONUI_H

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <functional>

class QWidget;
class QProgressDialog;
class XenConnection;

class XenConnectionUI
{
    public:
        using PasswordPrompt = std::function<bool(XenConnection* connection, const QString& oldPassword, QString* newPassword)>;

        static void BeginConnect(XenConnection* connection, bool interactive, QWidget* owner, bool initiateCoordinatorSearch, const PasswordPrompt& promptForNewPassword = PasswordPrompt());

        static bool PromptForNewPassword(XenConnection* connection, const QString& oldPassword, QWidget* owner, QString* newPassword);

    private:
        static void RegisterEventHandlers(XenConnection* connection);
        static void UnregisterEventHandlers(XenConnection* connection);
        static void HandleConnectionResult(XenConnection* connection, bool connected, const QString& error);
        static void HandleConnectionStateChanged(XenConnection* connection, bool connected);
        static void ShowConnectingDialogError(QWidget* owner, XenConnection* connection, const QString& error);

        static QHash<XenConnection*, QPointer<QProgressDialog>> s_connectionDialogs;
        static QHash<XenConnection*, QMetaObject::Connection> s_connectedHandlers;
        static QHash<XenConnection*, QMetaObject::Connection> s_errorHandlers;
        static QHash<XenConnection*, QMetaObject::Connection> s_disconnectedHandlers;
};

#endif // XENCONNECTIONUI_H
