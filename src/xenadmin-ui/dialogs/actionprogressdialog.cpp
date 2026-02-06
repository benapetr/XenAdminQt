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

#include "actionprogressdialog.h"
#include "ui_actionprogressdialog.h"
#include "operations/multipleaction.h"
#include <QDebug>
#include <QStyle>
#include <QApplication>

ActionProgressDialog::ActionProgressDialog(const QString& text, QWidget* parent)
    : QDialog(parent), ui(new Ui::ActionProgressDialog), m_operation(nullptr), m_showTryAgainMessage(true), m_showException(true), m_staticMode(true)
{
    this->ui->setupUi(this);

    this->ui->statusLabel->setText(text);
    this->ui->subStatusLabel->setVisible(false);

    // Marquee style (indeterminate progress)
    this->ui->progressBar->setMinimum(0);
    this->ui->progressBar->setMaximum(0);

    this->hideTitleBarIcons();
    this->setWindowTitle(qApp->applicationName());

    // Connect buttons
    connect(this->ui->cancelButton, &QPushButton::clicked, this, &ActionProgressDialog::onCancelClicked);
    connect(this->ui->closeButton, &QPushButton::clicked, this, &ActionProgressDialog::onCloseClicked);
}

ActionProgressDialog::ActionProgressDialog(AsyncOperation* operation, QWidget* parent)
    : QDialog(parent), ui(new Ui::ActionProgressDialog), m_operation(operation), m_showTryAgainMessage(true), m_showException(true), m_staticMode(false)
{
    this->ui->setupUi(this);

    // Connect operation signals
    connect(this->m_operation, &AsyncOperation::completed, this, &ActionProgressDialog::onOperationCompleted);
    connect(this->m_operation, &AsyncOperation::failed, this, &ActionProgressDialog::onOperationCompleted);
    connect(this->m_operation, &AsyncOperation::cancelled, this, &ActionProgressDialog::onOperationCompleted);
    connect(this->m_operation, &AsyncOperation::progressChanged, this, &ActionProgressDialog::onOperationChanged);
    connect(this->m_operation, &AsyncOperation::descriptionChanged, this, &ActionProgressDialog::onOperationChanged);
    connect(this->m_operation, &AsyncOperation::titleChanged, this, &ActionProgressDialog::onOperationChanged);

    // Set initial state
    this->ui->progressBar->setMinimum(0);
    this->ui->progressBar->setMaximum(100);
    this->updateStatusLabel();
    this->ui->cancelButton->setEnabled(this->m_operation->CanCancel());

    this->hideTitleBarIcons();
    this->setWindowTitle(qApp->applicationName());

    // Connect buttons
    connect(this->ui->cancelButton, &QPushButton::clicked, this, &ActionProgressDialog::onCancelClicked);
    connect(this->ui->closeButton, &QPushButton::clicked, this, &ActionProgressDialog::onCloseClicked);
}

ActionProgressDialog::~ActionProgressDialog()
{
    if (this->m_operation)
    {
        disconnect(this->m_operation, nullptr, this, nullptr);
    }
    delete ui;
}

void ActionProgressDialog::setShowCancel(bool show)
{
    this->ui->cancelButton->setVisible(show);
}

void ActionProgressDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // Start operation when dialog is shown
    if (this->m_operation && !this->m_staticMode)
    {
        this->m_operation->RunAsync();
    }
}

void ActionProgressDialog::onOperationChanged()
{
    if (!this->m_operation)
        return;

    // Update progress
    this->ui->progressBar->setValue(this->m_operation->GetPercentComplete());

    // Update status
    this->updateStatusLabel();

    // Update cancel button state
    this->ui->cancelButton->setEnabled(this->m_operation->CanCancel());
}

void ActionProgressDialog::onOperationCompleted()
{
    if (!this->m_operation)
    {
        qWarning() << "[OperationProgressDialog] onOperationCompleted called with null operation";
        return;
    }

    qDebug() << "[OperationProgressDialog] Operation completed:"
             << "Title:" << this->m_operation->GetTitle()
             << "hasError:" << this->m_operation->HasError()
             << "isCancelled:" << this->m_operation->IsCancelled()
             << "errorMessage:" << this->m_operation->GetErrorMessage()
             << "state:" << this->m_operation->GetState();

    // Check operation result
    if (!this->m_operation->HasError() && !this->m_operation->IsCancelled())
    {
        // Success - close dialog
        qDebug() << "[OperationProgressDialog] Operation succeeded, calling accept()";
        this->accept();
        qDebug() << "[OperationProgressDialog] accept() returned, result() =" << result();
        return;
    }

    // Error or cancelled - show error state
    qWarning() << "[OperationProgressDialog] Operation failed or cancelled, calling switchToErrorState()";
    this->switchToErrorState();
}

void ActionProgressDialog::onCancelClicked()
{
    this->ui->cancelButton->setEnabled(false);

    emit cancelClicked();

    if (this->m_operation)
    {
        this->m_operation->Cancel();
    }
}

void ActionProgressDialog::onCloseClicked()
{
    this->reject();
}

void ActionProgressDialog::updateStatusLabel()
{
    if (!this->m_operation)
        return;

    QString text = this->m_operation->GetDescription();
    if (text.isEmpty())
    {
        text = this->m_operation->GetTitle();
    }

    this->ui->statusLabel->setText(text);
    this->updateSubOperationStatusLabel();
}

void ActionProgressDialog::updateSubOperationStatusLabel()
{
    if (!this->m_operation)
        return;

    // Check if this is a MultipleAction with sub-operation details
    MultipleAction* multiOp = qobject_cast<MultipleAction*>(m_operation);

    if (multiOp && multiOp->showSubOperationDetails())
    {
        QString subDesc = multiOp->subOperationDescription();
        QString subTitle = multiOp->subOperationTitle();

        QString text = !subDesc.isEmpty() ? subDesc : subTitle;

        if (!text.isEmpty())
        {
            this->ui->subStatusLabel->setText(text);
            this->ui->subStatusLabel->setVisible(true);
            return;
        }
    }

    this->ui->subStatusLabel->setVisible(false);
}

void ActionProgressDialog::switchToErrorState()
{
    // Hide progress bar
    this->ui->progressBar->setVisible(false);

    // Hide cancel button
    this->ui->cancelButton->setVisible(false);

    // Show close button
    this->ui->closeButton->setVisible(true);
    this->ui->closeButton->setFocus();

    // Re-enable window controls
    this->setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);

    // Show error icon
    this->ui->iconLabel->setVisible(true);
    QPixmap errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(32, 32);
    this->ui->iconLabel->setPixmap(errorIcon);

    // Show exception details if enabled
    if (this->m_showException)
    {
        QString errorText;

        if (this->m_operation && !this->m_operation->GetErrorMessage().isEmpty())
        {
            errorText = this->m_operation->GetErrorMessage();
            QStringList details = this->m_operation->GetErrorDetails();
            if (!details.isEmpty())
                errorText += tr("\n\nDetails:\n- %1").arg(details.join("\n- "));
        } else if (this->m_operation && this->m_operation->IsCancelled())
        {
            errorText = tr("Operation cancelled by user");
        } else
        {
            errorText = tr("An internal error occurred");
        }

        this->ui->exceptionLabel->setText(errorText);
        this->ui->exceptionLabel->setVisible(true);
    }

    // Show "try again" message if enabled
    this->ui->bottomLabel->setVisible(this->m_showTryAgainMessage);

    // Adjust dialog size
    adjustSize();
}

void ActionProgressDialog::hideTitleBarIcons()
{
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
}
