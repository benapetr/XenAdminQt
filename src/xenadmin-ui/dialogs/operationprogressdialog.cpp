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

#include "operationprogressdialog.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QApplication>

OperationProgressDialog::OperationProgressDialog(const QString& text, QWidget* parent)
    : QDialog(parent), m_operation(nullptr), m_showTryAgainMessage(true), m_showException(true), m_staticMode(true)
{
    this->setupUi();

    this->m_statusLabel->setText(text);
    this->m_subStatusLabel->setVisible(false);

    // Marquee style (indeterminate progress)
    this->m_progressBar->setMinimum(0);
    this->m_progressBar->setMaximum(0);

    this->hideTitleBarIcons();
    this->setWindowTitle(qApp->applicationName());
}

OperationProgressDialog::OperationProgressDialog(AsyncOperation* operation, QWidget* parent)
    : QDialog(parent), m_operation(operation), m_showTryAgainMessage(true), m_showException(true), m_staticMode(false)
{
    this->setupUi();

    // Connect operation signals
    connect(this->m_operation, &AsyncOperation::completed, this, &OperationProgressDialog::onOperationCompleted);
    connect(this->m_operation, &AsyncOperation::progressChanged, this, &OperationProgressDialog::onOperationChanged);
    connect(this->m_operation, &AsyncOperation::descriptionChanged, this, &OperationProgressDialog::onOperationChanged);
    connect(this->m_operation, &AsyncOperation::titleChanged, this, &OperationProgressDialog::onOperationChanged);

    // Set initial state
    this->m_progressBar->setMinimum(0);
    this->m_progressBar->setMaximum(100);
    this->updateStatusLabel();
    this->m_cancelButton->setEnabled(this->m_operation->CanCancel());

    this->hideTitleBarIcons();
    this->setWindowTitle(qApp->applicationName());
}

OperationProgressDialog::~OperationProgressDialog()
{
    if (this->m_operation)
    {
        disconnect(this->m_operation, nullptr, this, nullptr);
    }
}

void OperationProgressDialog::setupUi()
{
    setModal(true);
    setMinimumWidth(450);

    // Main layout
    this->m_mainLayout = new QVBoxLayout(this);
    this->m_mainLayout->setContentsMargins(12, 12, 12, 12);
    this->m_mainLayout->setSpacing(12);

    // Icon and status layout
    this->m_iconLayout = new QHBoxLayout();

    // Icon (hidden by default, shown on error)
    this->m_iconLabel = new QLabel(this);
    this->m_iconLabel->setVisible(false);
    this->m_iconLabel->setFixedSize(32, 32);
    this->m_iconLayout->addWidget(m_iconLabel);

    // Status label
    this->m_statusLabel = new QLabel(this);
    this->m_statusLabel->setWordWrap(true);
    this->m_iconLayout->addWidget(this->m_statusLabel, 1);

    this->m_mainLayout->addLayout(m_iconLayout);

    // Sub-operation status label
    this->m_subStatusLabel = new QLabel(this);
    this->m_subStatusLabel->setWordWrap(true);
    this->m_subStatusLabel->setVisible(false);
    this->m_subStatusLabel->setStyleSheet("color: gray; font-size: 90%;");
    this->m_mainLayout->addWidget(this->m_subStatusLabel);

    // Exception label (hidden by default)
    this->m_exceptionLabel = new QLabel(this);
    this->m_exceptionLabel->setWordWrap(true);
    this->m_exceptionLabel->setVisible(false);
    this->m_exceptionLabel->setStyleSheet("color: red;");
    this->m_mainLayout->addWidget(this->m_exceptionLabel);

    // Bottom label (hidden by default)
    this->m_bottomLabel = new QLabel(tr("Please correct the issue and try again."), this);
    this->m_bottomLabel->setWordWrap(true);
    this->m_bottomLabel->setVisible(false);
    this->m_mainLayout->addWidget(this->m_bottomLabel);

    // Progress bar
    this->m_progressBar = new QProgressBar(this);
    this->m_mainLayout->addWidget(this->m_progressBar);

    // Buttons layout
    this->m_buttonLayout = new QHBoxLayout();
    this->m_buttonLayout->addStretch();

    // Cancel button
    this->m_cancelButton = new QPushButton(tr("Cancel"), this);
    this->m_cancelButton->setVisible(false);
    connect(this->m_cancelButton, &QPushButton::clicked,
            this, &OperationProgressDialog::onCancelClicked);
    this->m_buttonLayout->addWidget(this->m_cancelButton);

    // Close button (hidden by default, shown on error)
    this->m_closeButton = new QPushButton(tr("Close"), this);
    this->m_closeButton->setVisible(false);
    connect(this->m_closeButton, &QPushButton::clicked,
            this, &OperationProgressDialog::onCloseClicked);
    this->m_buttonLayout->addWidget(this->m_closeButton);

    this->m_mainLayout->addLayout(this->m_buttonLayout);
}

void OperationProgressDialog::setShowCancel(bool show)
{
    this->m_cancelButton->setVisible(show);
}

void OperationProgressDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // Start operation when dialog is shown
    if (this->m_operation && !this->m_staticMode)
    {
        this->m_operation->RunAsync();
    }
}

void OperationProgressDialog::onOperationChanged()
{
    if (!this->m_operation)
        return;

    // Update progress
    this->m_progressBar->setValue(this->m_operation->GetPercentComplete());

    // Update status
    this->updateStatusLabel();

    // Update cancel button state
    this->m_cancelButton->setEnabled(this->m_operation->CanCancel());
}

void OperationProgressDialog::onOperationCompleted()
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

void OperationProgressDialog::onCancelClicked()
{
    this->m_cancelButton->setEnabled(false);

    emit cancelClicked();

    if (this->m_operation)
    {
        this->m_operation->Cancel();
    }
}

void OperationProgressDialog::onCloseClicked()
{
    this->reject();
}

void OperationProgressDialog::updateStatusLabel()
{
    if (!this->m_operation)
        return;

    QString text = this->m_operation->GetDescription();
    if (text.isEmpty())
    {
        text = this->m_operation->GetTitle();
    }

    this->m_statusLabel->setText(text);
    this->updateSubOperationStatusLabel();
}

void OperationProgressDialog::updateSubOperationStatusLabel()
{
    if (!this->m_operation)
        return;

    // Check if this is a MultipleOperation with sub-operation details
    MultipleOperation* multiOp = qobject_cast<MultipleOperation*>(m_operation);

    if (multiOp && multiOp->showSubOperationDetails())
    {
        QString subDesc = multiOp->subOperationDescription();
        QString subTitle = multiOp->subOperationTitle();

        QString text = !subDesc.isEmpty() ? subDesc : subTitle;

        if (!text.isEmpty())
        {
            this->m_subStatusLabel->setText(text);
            this->m_subStatusLabel->setVisible(true);
            return;
        }
    }

    this->m_subStatusLabel->setVisible(false);
}

void OperationProgressDialog::switchToErrorState()
{
    // Hide progress bar
    this->m_progressBar->setVisible(false);

    // Hide cancel button
    this->m_cancelButton->setVisible(false);

    // Show close button
    this->m_closeButton->setVisible(true);
    this->m_closeButton->setFocus();

    // Re-enable window controls
    this->setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);

    // Show error icon
    this->m_iconLabel->setVisible(true);
    QPixmap errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(32, 32);
    this->m_iconLabel->setPixmap(errorIcon);

    // Show exception details if enabled
    if (this->m_showException)
    {
        QString errorText;

        if (this->m_operation && !this->m_operation->GetErrorMessage().isEmpty())
        {
            errorText = this->m_operation->GetErrorMessage();
        } else if (this->m_operation && this->m_operation->IsCancelled())
        {
            errorText = tr("Operation cancelled by user");
        } else
        {
            errorText = tr("An internal error occurred");
        }

        this->m_exceptionLabel->setText(errorText);
        this->m_exceptionLabel->setVisible(true);
    }

    // Show "try again" message if enabled
    this->m_bottomLabel->setVisible(this->m_showTryAgainMessage);

    // Adjust dialog size
    adjustSize();
}

void OperationProgressDialog::hideTitleBarIcons()
{
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
}
