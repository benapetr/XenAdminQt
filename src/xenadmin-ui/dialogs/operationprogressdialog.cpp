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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QApplication>

OperationProgressDialog::OperationProgressDialog(const QString& text, QWidget* parent)
    : QDialog(parent), m_operation(nullptr), m_showTryAgainMessage(true), m_showException(true), m_staticMode(true)
{
    setupUi();

    m_statusLabel->setText(text);
    m_subStatusLabel->setVisible(false);

    // Marquee style (indeterminate progress)
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(0);

    hideTitleBarIcons();
    setWindowTitle(qApp->applicationName());
}

OperationProgressDialog::OperationProgressDialog(AsyncOperation* operation, QWidget* parent)
    : QDialog(parent), m_operation(operation), m_showTryAgainMessage(true), m_showException(true), m_staticMode(false)
{
    setupUi();

    // Connect operation signals
    connect(m_operation, &AsyncOperation::completed,
            this, &OperationProgressDialog::onOperationCompleted);
    connect(m_operation, &AsyncOperation::progressChanged,
            this, &OperationProgressDialog::onOperationChanged);
    connect(m_operation, &AsyncOperation::descriptionChanged,
            this, &OperationProgressDialog::onOperationChanged);
    connect(m_operation, &AsyncOperation::titleChanged,
            this, &OperationProgressDialog::onOperationChanged);

    // Set initial state
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    updateStatusLabel();
    m_cancelButton->setEnabled(m_operation->canCancel());

    hideTitleBarIcons();
    setWindowTitle(qApp->applicationName());
}

OperationProgressDialog::~OperationProgressDialog()
{
    if (m_operation)
    {
        disconnect(m_operation, nullptr, this, nullptr);
    }
}

void OperationProgressDialog::setupUi()
{
    setModal(true);
    setMinimumWidth(450);

    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    m_mainLayout->setSpacing(12);

    // Icon and status layout
    m_iconLayout = new QHBoxLayout();

    // Icon (hidden by default, shown on error)
    m_iconLabel = new QLabel(this);
    m_iconLabel->setVisible(false);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLayout->addWidget(m_iconLabel);

    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_iconLayout->addWidget(m_statusLabel, 1);

    m_mainLayout->addLayout(m_iconLayout);

    // Sub-operation status label
    m_subStatusLabel = new QLabel(this);
    m_subStatusLabel->setWordWrap(true);
    m_subStatusLabel->setVisible(false);
    m_subStatusLabel->setStyleSheet("color: gray; font-size: 90%;");
    m_mainLayout->addWidget(m_subStatusLabel);

    // Exception label (hidden by default)
    m_exceptionLabel = new QLabel(this);
    m_exceptionLabel->setWordWrap(true);
    m_exceptionLabel->setVisible(false);
    m_exceptionLabel->setStyleSheet("color: red;");
    m_mainLayout->addWidget(m_exceptionLabel);

    // Bottom label (hidden by default)
    m_bottomLabel = new QLabel(tr("Please correct the issue and try again."), this);
    m_bottomLabel->setWordWrap(true);
    m_bottomLabel->setVisible(false);
    m_mainLayout->addWidget(m_bottomLabel);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_mainLayout->addWidget(m_progressBar);

    // Buttons layout
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->addStretch();

    // Cancel button
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setVisible(false);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &OperationProgressDialog::onCancelClicked);
    m_buttonLayout->addWidget(m_cancelButton);

    // Close button (hidden by default, shown on error)
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setVisible(false);
    connect(m_closeButton, &QPushButton::clicked,
            this, &OperationProgressDialog::onCloseClicked);
    m_buttonLayout->addWidget(m_closeButton);

    m_mainLayout->addLayout(m_buttonLayout);
}

void OperationProgressDialog::setShowCancel(bool show)
{
    m_cancelButton->setVisible(show);
}

void OperationProgressDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // Start operation when dialog is shown
    if (m_operation && !m_staticMode)
    {
        m_operation->runAsync();
    }
}

void OperationProgressDialog::onOperationChanged()
{
    if (!m_operation)
        return;

    // Update progress
    m_progressBar->setValue(m_operation->percentComplete());

    // Update status
    updateStatusLabel();

    // Update cancel button state
    m_cancelButton->setEnabled(m_operation->canCancel());
}

void OperationProgressDialog::onOperationCompleted()
{
    if (!m_operation)
    {
        qWarning() << "[OperationProgressDialog] onOperationCompleted called with null operation";
        return;
    }

    qDebug() << "[OperationProgressDialog] Operation completed:"
             << "Title:" << m_operation->title()
             << "hasError:" << m_operation->hasError()
             << "isCancelled:" << m_operation->isCancelled()
             << "errorMessage:" << m_operation->errorMessage()
             << "state:" << m_operation->state();

    // Check operation result
    if (!m_operation->hasError() && !m_operation->isCancelled())
    {
        // Success - close dialog
        qDebug() << "[OperationProgressDialog] Operation succeeded, calling accept()";
        accept();
        qDebug() << "[OperationProgressDialog] accept() returned, result() =" << result();
        return;
    }

    // Error or cancelled - show error state
    qWarning() << "[OperationProgressDialog] Operation failed or cancelled, calling switchToErrorState()";
    switchToErrorState();
}

void OperationProgressDialog::onCancelClicked()
{
    m_cancelButton->setEnabled(false);

    emit cancelClicked();

    if (m_operation)
    {
        m_operation->cancel();
    }
}

void OperationProgressDialog::onCloseClicked()
{
    reject();
}

void OperationProgressDialog::updateStatusLabel()
{
    if (!m_operation)
        return;

    QString text = m_operation->description();
    if (text.isEmpty())
    {
        text = m_operation->title();
    }

    m_statusLabel->setText(text);
    updateSubOperationStatusLabel();
}

void OperationProgressDialog::updateSubOperationStatusLabel()
{
    if (!m_operation)
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
            m_subStatusLabel->setText(text);
            m_subStatusLabel->setVisible(true);
            return;
        }
    }

    m_subStatusLabel->setVisible(false);
}

void OperationProgressDialog::switchToErrorState()
{
    // Hide progress bar
    m_progressBar->setVisible(false);

    // Hide cancel button
    m_cancelButton->setVisible(false);

    // Show close button
    m_closeButton->setVisible(true);
    m_closeButton->setFocus();

    // Re-enable window controls
    setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);

    // Show error icon
    m_iconLabel->setVisible(true);
    QPixmap errorIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(32, 32);
    m_iconLabel->setPixmap(errorIcon);

    // Show exception details if enabled
    if (m_showException)
    {
        QString errorText;

        if (m_operation && !m_operation->errorMessage().isEmpty())
        {
            errorText = m_operation->errorMessage();
        } else if (m_operation && m_operation->isCancelled())
        {
            errorText = tr("Operation cancelled by user");
        } else
        {
            errorText = tr("An internal error occurred");
        }

        m_exceptionLabel->setText(errorText);
        m_exceptionLabel->setVisible(true);
    }

    // Show "try again" message if enabled
    m_bottomLabel->setVisible(m_showTryAgainMessage);

    // Adjust dialog size
    adjustSize();
}

void OperationProgressDialog::hideTitleBarIcons()
{
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
}
