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
    ui->setupUi(this);
    ui->progressBar1->setRange(0, 0);

    const QString host = m_connection ? m_connection->GetHostname() : QString();
    ui->lblStatus->setText(QString("Attempting to connect to %1...").arg(host));

    connect(ui->btnCancel, &QPushButton::clicked, this, &ConnectingToServerDialog::btnCancel_Click);
}

ConnectingToServerDialog::~ConnectingToServerDialog()
{
    UnregisterEventHandlers();
    delete ui;
}

bool ConnectingToServerDialog::BeginConnect(QWidget* owner, bool initiateCoordinatorSearch)
{
    if (!m_connection)
        return false;

    m_ownerForm = owner;

    RegisterEventHandlers();
    m_connection->BeginConnect(initiateCoordinatorSearch, [this](const QString& oldPassword, QString* newPassword) {
        return HideAndPromptForNewPassword(oldPassword, newPassword);
    });

    if (m_connection->InProgress())
    {
        if (!isVisible())
            show();
        raise();
        activateWindow();
        return true;
    }

    return false;
}

QWidget* ConnectingToServerDialog::GetOwnerWidget() const
{
    if (m_ownerForm && !m_ownerForm->isHidden())
        return m_ownerForm;

    return parentWidget();
}

void ConnectingToServerDialog::closeEvent(QCloseEvent* event)
{
    if (m_connection && m_connection->InProgress() && !m_connection->IsConnected() && !m_endBegun)
    {
        m_endBegun = true;
        m_connection->EndConnect();
        event->ignore();
        return;
    }

    UnregisterEventHandlers();
    QDialog::closeEvent(event);
}

void ConnectingToServerDialog::RegisterEventHandlers()
{
    UnregisterEventHandlers();

    if (!m_connection)
        return;

    m_closedConn = connect(m_connection, &XenConnection::connectionClosed,
                           this, &ConnectingToServerDialog::Connection_ConnectionClosed);
    m_beforeEndConn = connect(m_connection, &XenConnection::beforeConnectionEnd,
                              this, &ConnectingToServerDialog::Connection_BeforeConnectionEnd);
    m_messageConn = connect(m_connection, &XenConnection::connectionMessageChanged,
                            this, &ConnectingToServerDialog::Connection_ConnectionMessageChanged);
}

void ConnectingToServerDialog::UnregisterEventHandlers()
{
    if (m_closedConn)
        disconnect(m_closedConn);
    if (m_beforeEndConn)
        disconnect(m_beforeEndConn);
    if (m_messageConn)
        disconnect(m_messageConn);

    m_closedConn = QMetaObject::Connection();
    m_beforeEndConn = QMetaObject::Connection();
    m_messageConn = QMetaObject::Connection();
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
        if (isVisible())
            ui->lblStatus->setText(message);
    }, Qt::QueuedConnection);
}

void ConnectingToServerDialog::CloseConnectingDialog()
{
    UnregisterEventHandlers();
    QMetaObject::invokeMethod(this, [this]() {
        close();
    }, Qt::QueuedConnection);
}

bool ConnectingToServerDialog::HideAndPromptForNewPassword(const QString& oldPassword, QString* newPassword)
{
    Q_UNUSED(oldPassword);
    bool result = false;

    QMetaObject::invokeMethod(this, [this, &result, newPassword]() {
        const bool wasVisible = isVisible();
        if (wasVisible)
            hide();

        AddServerDialog dialog(m_connection, true, GetOwnerWidget());
        if (dialog.exec() == QDialog::Accepted)
        {
            result = true;
            if (newPassword)
                *newPassword = dialog.password();
        }

        if (result && wasVisible)
            show();
    }, Qt::BlockingQueuedConnection);

    return result;
}

void ConnectingToServerDialog::btnCancel_Click()
{
    close();
}
