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

#include "installcertificatedialog.h"
#include "ui_installcertificatedialog.h"
#include "xenlib/xen/asyncoperation.h"
#include "xenlib/xen/actions/host/installservercertificateaction.h"
#include "xenlib/xen/host.h"
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

InstallCertificateDialog::InstallCertificateDialog(const QSharedPointer<Host>& host, QWidget* parent)
    : QDialog(parent),
      m_host(host),
      ui(new Ui::InstallCertificateDialog)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("Install Server Certificate - %1").arg(host ? host->GetName() : tr("Server")));
    this->m_installButton = this->ui->buttonBox->addButton(tr("Install"), QDialogButtonBox::AcceptRole);
    this->m_closeButton = this->ui->buttonBox->addButton(QDialogButtonBox::Close);
    this->ui->progressBar->setRange(0, 100);
    this->ui->progressBar->setVisible(false);
    this->ui->statusLabel->setVisible(false);

    connect(this->ui->browseKeyButton, &QPushButton::clicked, this, &InstallCertificateDialog::browseKey);
    connect(this->ui->browseCertificateButton, &QPushButton::clicked, this, &InstallCertificateDialog::browseCertificate);
    connect(this->ui->addChainButton, &QPushButton::clicked, this, &InstallCertificateDialog::addChainCertificates);
    connect(this->ui->removeChainButton, &QPushButton::clicked, this, &InstallCertificateDialog::removeChainCertificate);
    connect(this->m_installButton, &QPushButton::clicked, this, &InstallCertificateDialog::installCertificate);
    connect(this->m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(this->ui->keyLineEdit, &QLineEdit::textChanged, this, &InstallCertificateDialog::updateButtons);
    connect(this->ui->certificateLineEdit, &QLineEdit::textChanged, this, &InstallCertificateDialog::updateButtons);
    connect(this->ui->chainListWidget, &QListWidget::itemSelectionChanged, this, &InstallCertificateDialog::updateButtons);

    this->hideErrors();
    this->updateButtons();
}

InstallCertificateDialog::~InstallCertificateDialog()
{
    delete this->ui;
}

void InstallCertificateDialog::browseKey()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select Private Key"),
                                                      QString(),
                                                      tr("Private key files (*.key *.pem);;All files (*)"));
    if (!path.isEmpty())
        this->ui->keyLineEdit->setText(path);
}

void InstallCertificateDialog::browseCertificate()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Select Certificate"),
                                                      QString(),
                                                      tr("Certificate files (*.crt *.cert *.cer *.pem);;All files (*)"));
    if (!path.isEmpty())
        this->ui->certificateLineEdit->setText(path);
}

void InstallCertificateDialog::addChainCertificates()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this,
                                                            tr("Select Chain Certificates"),
                                                            QString(),
                                                            tr("Certificate files (*.crt *.cert *.cer *.pem);;All files (*)"));
    for (const QString& path : paths)
    {
        if (this->ui->chainListWidget->findItems(path, Qt::MatchExactly).isEmpty())
            this->ui->chainListWidget->addItem(path);
    }
    this->hideErrors();
    this->updateButtons();
}

void InstallCertificateDialog::removeChainCertificate()
{
    qDeleteAll(this->ui->chainListWidget->selectedItems());
    this->hideErrors();
    this->updateButtons();
}

void InstallCertificateDialog::installCertificate()
{
    this->hideErrors();

    InstallServerCertificateAction* operation = new InstallServerCertificateAction(
        this->m_host,
        this->ui->keyLineEdit->text().trimmed(),
        this->ui->certificateLineEdit->text().trimmed(),
        this->chainFiles());

    this->m_operation = operation;
    connect(operation, &AsyncOperation::progressChanged, this, &InstallCertificateDialog::onOperationChanged);
    connect(operation, &AsyncOperation::descriptionChanged, this, &InstallCertificateDialog::onOperationChanged);
    connect(operation, &AsyncOperation::completed, this, &InstallCertificateDialog::onOperationCompleted);
    connect(operation, &AsyncOperation::failed, this, &InstallCertificateDialog::onOperationCompleted);
    connect(operation, &AsyncOperation::cancelled, this, &InstallCertificateDialog::onOperationCompleted);

    this->ui->progressBar->setVisible(true);
    this->ui->statusLabel->setVisible(true);
    this->ui->statusLabel->setText(operation->GetDescription());
    this->updateButtons();
    operation->RunAsync();
}

void InstallCertificateDialog::onOperationChanged()
{
    if (!this->m_operation)
        return;

    this->ui->progressBar->setValue(this->m_operation->GetPercentComplete());
    this->ui->statusLabel->setText(this->m_operation->GetDescription());
}

void InstallCertificateDialog::onOperationCompleted()
{
    InstallServerCertificateAction* operation = dynamic_cast<InstallServerCertificateAction*>(this->m_operation);
    if (!operation)
        return;

    this->onOperationChanged();
    const bool succeeded = operation->IsCompleted() && !operation->HasError();

    if (succeeded)
    {
        this->ui->statusLabel->setText(tr("Server certificate installed."));
        this->m_closeButton->setText(tr("Close"));
    } else
    {
        if (!operation->KeyError().isEmpty())
            this->showError(this->ui->keyErrorLabel, operation->KeyError());
        if (!operation->CertificateError().isEmpty())
            this->showError(this->ui->certificateErrorLabel, operation->CertificateError());
        if (!operation->ChainError().isEmpty())
            this->showError(this->ui->chainErrorLabel, operation->ChainError());

        if (operation->KeyError().isEmpty() && operation->CertificateError().isEmpty() && operation->ChainError().isEmpty())
            this->ui->statusLabel->setText(operation->GetErrorMessage().isEmpty()
                ? tr("Certificate installation failed.")
                : tr("Certificate installation failed: %1").arg(operation->GetErrorMessage()));
        else
            this->ui->statusLabel->setText(tr("Certificate installation failed."));
    }

    this->m_operation = nullptr;
    operation->deleteLater();
    this->updateButtons();

    if (succeeded)
        this->m_installButton->setEnabled(false);
}

void InstallCertificateDialog::updateButtons()
{
    const bool running = this->m_operation && this->m_operation->IsRunning();
    this->m_installButton->setEnabled(!running &&
                                      !this->ui->keyLineEdit->text().trimmed().isEmpty() &&
                                      !this->ui->certificateLineEdit->text().trimmed().isEmpty());
    this->ui->removeChainButton->setEnabled(!running && !this->ui->chainListWidget->selectedItems().isEmpty());
    this->m_closeButton->setEnabled(!running);
}

void InstallCertificateDialog::closeEvent(QCloseEvent* event)
{
    if (this->m_operation && this->m_operation->IsRunning())
    {
        event->ignore();
        return;
    }

    QDialog::closeEvent(event);
}

void InstallCertificateDialog::hideErrors()
{
    const QList<QLabel*> labels = { this->ui->keyErrorLabel, this->ui->certificateErrorLabel, this->ui->chainErrorLabel };
    for (QLabel* label : labels)
    {
        if (!label)
            continue;
        label->clear();
        label->setVisible(false);
    }
}

void InstallCertificateDialog::showError(QLabel* label, const QString& message)
{
    if (!label)
        return;

    label->setText(message);
    label->setVisible(!message.isEmpty());
}

QStringList InstallCertificateDialog::chainFiles() const
{
    QStringList paths;
    for (int i = 0; i < this->ui->chainListWidget->count(); ++i)
        paths.append(this->ui->chainListWidget->item(i)->text());
    return paths;
}
