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

#ifndef OPERATIONPROGRESSDIALOG_H
#define OPERATIONPROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include "xen/asyncoperation.h"
#include "operations/multipleoperation.h"

/**
 * @brief Progress dialog for monitoring AsyncOperation execution
 *
 * Qt equivalent of C# ActionProgressDialog. Monitors an AsyncOperation,
 * showing a progress bar and the operation's description. Shows exceptions
 * to the user if the operation fails. Has optional cancel button.
 *
 * Features:
 * - Real-time progress tracking
 * - Status text updates
 * - Sub-operation status (for MultipleOperation/ParallelOperation)
 * - Exception display on failure
 * - Cancellation support
 *
 * Usage:
 *   auto dialog = new OperationProgressDialog(operation, this);
 *   dialog->setShowCancel(true);
 *   dialog->exec(); // Blocks until operation completes or is cancelled
 */
class OperationProgressDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct with static text (marquee progress)
     * @param text Status text to display
     * @param parent Parent widget
     */
    explicit OperationProgressDialog(const QString& text, QWidget* parent = nullptr);

    /**
     * @brief Construct with operation to monitor
     * @param operation AsyncOperation to monitor
     * @param parent Parent widget
     */
    explicit OperationProgressDialog(AsyncOperation* operation, QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~OperationProgressDialog() override;

    /**
     * @brief Set whether to show "try again" message on error
     */
    void setShowTryAgainMessage(bool show)
    {
        this->m_showTryAgainMessage = show;
    }

    /**
     * @brief Set whether to show exception details on error
     */
    void setShowException(bool show)
    {
        this->m_showException = show;
    }

    /**
     * @brief Set whether to show cancel button
     */
    void setShowCancel(bool show);

    /**
     * @brief Get the monitored operation
     */
    AsyncOperation* operation() const
    {
        return this->m_operation;
    }

signals:
    /**
     * @brief Emitted when user clicks cancel
     */
    void cancelClicked();

protected:
    /**
     * @brief Called when dialog is shown
     */
    void showEvent(QShowEvent* event) override;

private slots:
    /**
     * @brief Handle operation state changes
     */
    void onOperationChanged();

    /**
     * @brief Handle operation completion
     */
    void onOperationCompleted();

    /**
     * @brief Handle cancel button click
     */
    void onCancelClicked();

    /**
     * @brief Handle close button click
     */
    void onCloseClicked();

private:
    /**
     * @brief Initialize UI components
     */
    void setupUi();

    /**
     * @brief Update status label from operation
     */
    void updateStatusLabel();

    /**
     * @brief Update sub-operation status label
     */
    void updateSubOperationStatusLabel();

    /**
     * @brief Switch dialog to error display mode
     */
    void switchToErrorState();

    /**
     * @brief Hide title bar buttons
     */
    void hideTitleBarIcons();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_iconLayout;
    QHBoxLayout* m_buttonLayout;
    QLabel* m_iconLabel;
    QLabel* m_statusLabel;
    QLabel* m_subStatusLabel;
    QLabel* m_exceptionLabel;
    QLabel* m_bottomLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_cancelButton;
    QPushButton* m_closeButton;

    // Operation (QPointer automatically becomes null if operation is deleted)
    QPointer<AsyncOperation> m_operation;

    // Flags
    bool m_showTryAgainMessage;
    bool m_showException;
    bool m_staticMode; // True for text-only mode (no operation)
};

#endif // OPERATIONPROGRESSDIALOG_H
