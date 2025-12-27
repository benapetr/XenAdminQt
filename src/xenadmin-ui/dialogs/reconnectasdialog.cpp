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

#include "reconnectasdialog.h"
#include "ui_reconnectasdialog.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include <QPushButton>
#include <QtWidgets/QDialogButtonBox>

namespace
{
    const char* kReconnectAsBlurb =
        "You are currently logged in to '%1' as '%2'.\n\n"
        "To log out of this server and log in again using a different user account, "
        "enter the account credentials below and click OK.";
}

ReconnectAsDialog::ReconnectAsDialog(XenConnection* connection, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ReconnectAsDialog),
      m_connection(connection)
{
    ui->setupUi(this);
    updateBlurb();
    updateButtonState();

    QPixmap pix(":/tree-icons/template_user.png");
    if (!pix.isNull())
        ui->pictureLabel->setPixmap(pix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    connect(ui->textBoxUsername, &QLineEdit::textChanged, this, &ReconnectAsDialog::updateButtonState);
    connect(ui->textBoxPassword, &QLineEdit::textChanged, this, &ReconnectAsDialog::updateButtonState);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->textBoxUsername->setFocus();
}

ReconnectAsDialog::~ReconnectAsDialog()
{
    delete ui;
}

QString ReconnectAsDialog::username() const
{
    return ui->textBoxUsername->text().trimmed();
}

QString ReconnectAsDialog::password() const
{
    return ui->textBoxPassword->text();
}

void ReconnectAsDialog::updateButtonState()
{
    const bool enabled = !ui->textBoxUsername->text().trimmed().isEmpty() &&
        !ui->textBoxPassword->text().isEmpty();

    if (auto* okButton = ui->buttonBox->button(QDialogButtonBox::Ok))
        okButton->setEnabled(enabled);
}

void ReconnectAsDialog::updateBlurb()
{
    QString hostname;
    QString username;

    if (m_connection)
    {
        hostname = m_connection->GetHostname();
        if (auto* session = m_connection->GetSession())
            username = session->getUsername();
        if (username.isEmpty())
            username = m_connection->GetUsername();
    }

    if (hostname.isEmpty())
        hostname = tr("server");
    if (username.isEmpty())
        username = tr("unknown user");

    ui->labelBlurb->setText(QString(kReconnectAsBlurb).arg(hostname, username));
}
