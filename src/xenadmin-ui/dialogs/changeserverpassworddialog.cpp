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

#include "changeserverpassworddialog.h"
#include "ui_changeserverpassworddialog.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/host/changehostpasswordaction.h"
#include "operations/operationmanager.h"
#include <QPushButton>
#include <QStyle>

ChangeServerPasswordDialog::ChangeServerPasswordDialog(QSharedPointer<Host> host, QWidget* parent) : QDialog(parent),
      ui(new Ui::ChangeServerPasswordDialog), m_host(host), m_connection(host ? host->GetConnection() : nullptr)
{
    this->ui->setupUi(this);

    connect(this->ui->oldPassBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->newPassBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->confirmBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &ChangeServerPasswordDialog::onAccepted);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    this->updateTitle();
    this->updateButtons();
    this->updateInfoRows();
}

ChangeServerPasswordDialog::ChangeServerPasswordDialog(QSharedPointer<Pool> pool, QWidget* parent) : QDialog(parent),
      ui(new Ui::ChangeServerPasswordDialog), m_pool(pool), m_connection(pool ? pool->GetConnection() : nullptr)
{
    this->ui->setupUi(this);

    connect(this->ui->oldPassBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->newPassBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->confirmBox, &QLineEdit::textChanged, this, &ChangeServerPasswordDialog::onTextChanged);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &ChangeServerPasswordDialog::onAccepted);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    this->updateTitle();
    this->updateButtons();
    this->updateInfoRows();
}

ChangeServerPasswordDialog::~ChangeServerPasswordDialog()
{
    delete this->ui;
}

void ChangeServerPasswordDialog::onTextChanged()
{
    this->ui->currentPasswordError->setVisible(false);
    this->ui->newPasswordError->setVisible(false);
    this->updateButtons();
}

void ChangeServerPasswordDialog::onAccepted()
{
    const bool isOldPasswordCorrect = this->m_connection && this->ui->oldPassBox->text() == this->m_connection->GetPassword();

    if (!isOldPasswordCorrect)
    {
        this->ui->currentPasswordError->setVisible(true);
        this->ui->oldPassBox->setFocus();
        this->ui->oldPassBox->selectAll();
        return;
    }

    if (this->ui->newPassBox->text() != this->ui->confirmBox->text())
    {
        this->ui->newPasswordError->setText(tr("The new passwords do not match."));
        this->ui->newPasswordError->setVisible(true);
        this->ui->newPassBox->setFocus();
        this->ui->newPassBox->selectAll();
        return;
    }

    ChangeHostPasswordAction* action = new ChangeHostPasswordAction(this->m_connection, this->ui->oldPassBox->text(), this->ui->newPassBox->text(), nullptr);
    OperationManager::instance()->RegisterOperation(action);
    action->RunAsync(true);

    this->accept();
}

void ChangeServerPasswordDialog::updateTitle()
{
    QString targetName;

    if (this->m_host)
    {
        QSharedPointer<Pool> pool = this->m_host->GetPoolOfOne();
        targetName = pool ? pool->GetName() : this->m_host->GetName();
    } else if (this->m_pool)
    {
        targetName = this->m_pool->GetName();
    }

    if (targetName.isEmpty())
        targetName = tr("Server");

    this->setWindowTitle(tr("Change Password - %1").arg(targetName));
}

void ChangeServerPasswordDialog::updateButtons()
{
    const bool enabled = !this->ui->oldPassBox->text().isEmpty() && !this->ui->newPassBox->text().isEmpty() && !this->ui->confirmBox->text().isEmpty();

    QPushButton* okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton)
        okButton->setEnabled(enabled);
}

void ChangeServerPasswordDialog::updateInfoRows()
{
    const QIcon infoIcon = this->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    this->ui->noteIconLabel->setPixmap(infoIcon.pixmap(16, 16));
    this->ui->poolSecretIconLabel->setPixmap(infoIcon.pixmap(16, 16));

    const bool showPoolSecretReminder =
        this->m_connection &&
        this->stockholmOrGreater(this->m_connection) &&
        !this->hasPoolSecretRotationRestriction(this->m_connection);

    this->ui->poolSecretRowWidget->setVisible(showPoolSecretReminder);
}

bool ChangeServerPasswordDialog::stockholmOrGreater(XenConnection* connection) const
{
    if (!connection || !connection->GetCache())
        return true;

    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    if (!coordinator)
    {
        const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        coordinator = hosts.isEmpty() ? QSharedPointer<Host>() : hosts.first();
    }

    if (!coordinator)
        return true;

    const QString platformVersion = coordinator->PlatformVersion();
    return this->compareVersion(platformVersion, "3.1.50") >= 0;
}

bool ChangeServerPasswordDialog::hasPoolSecretRotationRestriction(XenConnection* connection) const
{
    if (!connection || !connection->GetCache())
        return false;

    const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->IsValid() && host->RestrictPoolSecretRotation())
            return true;
    }

    return false;
}

int ChangeServerPasswordDialog::compareVersion(const QString& lhs, const QString& rhs)
{
    const QStringList lhsParts = lhs.split('.');
    const QStringList rhsParts = rhs.split('.');
    const int partCount = qMax(lhsParts.size(), rhsParts.size());

    for (int i = 0; i < partCount; ++i)
    {
        const int left = (i < lhsParts.size()) ? lhsParts.at(i).toInt() : 0;
        const int right = (i < rhsParts.size()) ? rhsParts.at(i).toInt() : 0;
        if (left < right)
            return -1;
        if (left > right)
            return 1;
    }

    return 0;
}
