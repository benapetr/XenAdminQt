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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"
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

#include "connectingtoserverdialog.h"
#include "ui_connectingtoserverdialog.h"
#include "addserverdialog.h"
#include "../../xenlib/xen/network/connection.h"
#include <QtCore/QMetaObject>
#include <QtGui/QCloseEvent>

ConnectingToServerDialog::ConnectingToServerDialog(XenConnection* connection, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ConnectingToServerDialog),
      m_connection(connection)
{
    this->ui->setupUi(this);
    this->ui->progressBar1->setRange(0, 0);

    const QString host = this->m_connection ? this->m_connection->GetHostname() : QString();
    this->ui->lblStatus->setText(QString("Attempting to connect to %1...").arg(host));

    connect(this->ui->btnCancel, &QPushButton::clicked, this, &ConnectingToServerDialog::btnCancel_Click);
}

ConnectingToServerDialog::~ConnectingToServerDialog()
{
    this->UnregisterEventHandlers();
    delete this->ui;
}

bool ConnectingToServerDialog::BeginConnect(QWidget* owner, bool initiateCoordinatorSearch)
{
    if (!this->m_connection)
        return false;

    this->m_ownerForm = owner;

    this->RegisterEventHandlers();
    this->m_connection->BeginConnect(initiateCoordinatorSearch, [this](const QString& oldPassword, QString* newPassword) {
        return this->HideAndPromptForNewPassword(oldPassword, newPassword);
    });

    if (this->m_connection->InProgress())
    {
        if (!this->isVisible())
            this->show();
        this->raise();
        this->activateWindow();
        return true;
    }

    return false;
}

QWidget* ConnectingToServerDialog::GetOwnerWidget() const
{
    if (this->m_ownerForm && !this->m_ownerForm->isHidden())
        return this->m_ownerForm;

    return this->parentWidget();
}

void ConnectingToServerDialog::closeEvent(QCloseEvent* event)
{
    if (this->m_connection && this->m_connection->InProgress() && !this->m_connection->IsConnected() && !this->m_endBegun)
    {
        this->m_endBegun = true;
        this->m_connection->EndConnect();
        event->ignore();
        return;
    }

    this->UnregisterEventHandlers();
    QDialog::closeEvent(event);
}

void ConnectingToServerDialog::RegisterEventHandlers()
{
    this->UnregisterEventHandlers();

    if (!this->m_connection)
        return;

    this->m_closedConn = connect(this->m_connection, &XenConnection::ConnectionClosed, this, &ConnectingToServerDialog::Connection_ConnectionClosed);
    this->m_beforeEndConn = connect(this->m_connection, &XenConnection::BeforeConnectionEnd, this, &ConnectingToServerDialog::Connection_BeforeConnectionEnd);
    this->m_messageConn = connect(this->m_connection, &XenConnection::ConnectionMessageChanged, this, &ConnectingToServerDialog::Connection_ConnectionMessageChanged);
}

void ConnectingToServerDialog::UnregisterEventHandlers()
{
    if (this->m_closedConn)
        disconnect(this->m_closedConn);
    if (this->m_beforeEndConn)
        disconnect(this->m_beforeEndConn);
    if (this->m_messageConn)
        disconnect(this->m_messageConn);

    this->m_closedConn = QMetaObject::Connection();
    this->m_beforeEndConn = QMetaObject::Connection();
    this->m_messageConn = QMetaObject::Connection();
}

void ConnectingToServerDialog::Connection_ConnectionClosed()
{
    CloseConnectingDialog();
}

void ConnectingToServerDialog::Connection_BeforeConnectionEnd()
{
    CloseConnectingDialog();
}

void ConnectingToServerDialog::Connection_ConnectionMessageChanged(const QString& message)
{
    QMetaObject::invokeMethod(this, [this, message]() {
        if (this->isVisible())
            this->ui->lblStatus->setText(message);
    }, Qt::QueuedConnection);
}

void ConnectingToServerDialog::CloseConnectingDialog()
{
    this->UnregisterEventHandlers();
    QMetaObject::invokeMethod(this, [this]() {
        this->close();
    }, Qt::QueuedConnection);
}

bool ConnectingToServerDialog::HideAndPromptForNewPassword(const QString& oldPassword, QString* newPassword)
{
    Q_UNUSED(oldPassword);
    bool result = false;

    QMetaObject::invokeMethod(this, [this, &result, newPassword]() {
        const bool wasVisible = this->isVisible();
        if (wasVisible)
            this->hide();

        AddServerDialog dialog(this->m_connection, true, this->GetOwnerWidget());
        if (dialog.exec() == QDialog::Accepted)
        {
            result = true;
            if (newPassword)
                *newPassword = dialog.password();
        }

        if (result && wasVisible)
            this->show();
    }, Qt::BlockingQueuedConnection);

    return result;
}

void ConnectingToServerDialog::btnCancel_Click()
{
    close();
}
