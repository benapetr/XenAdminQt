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

#ifndef MOVEVMDIALOG_H
#define MOVEVMDIALOG_H

#include <QDialog>
#include <QSharedPointer>

namespace Ui
{
    class MoveVMDialog;
}

class VM;
class XenConnection;

/**
 * @brief Dialog for moving a VM's virtual disks to a different storage repository
 *
 * Qt port of C# XenAdmin.Dialogs.VMDialogs.MoveVMDialog
 *
 * This dialog allows the user to select a target SR and move all of a VM's
 * virtual disks to that SR. It uses the VMMoveAction to perform the move.
 *
 * Key features:
 * - Shows SR picker with appropriate filtering
 * - Validates SR selection before enabling Move button
 * - Supports SR rescanning
 * - Automatically closes on successful move operation
 *
 * C# location: XenAdmin/Dialogs/VMDialogs/MoveVMDialog.cs
 */
class MoveVMDialog : public QDialog
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param vm VM to move
         * @param parent Parent widget
         */
        explicit MoveVMDialog(QSharedPointer<VM> vm, QWidget* parent = nullptr);
        ~MoveVMDialog() override;

        /**
         * @brief Get the selected SR reference
         * @return Selected SR opaque reference
         */
        QString getSelectedSR() const;

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void onSrPickerSelectionChanged();
        void onSrPickerDoubleClicked();
        void onSrPickerCanBeScannedChanged();
        void onRescanClicked();
        void accept() override;

    private:
        void initialize();
        void enableMoveButton();

        Ui::MoveVMDialog* ui;
        QSharedPointer<VM> m_vm;
        XenConnection* m_connection;
};

#endif // MOVEVMDIALOG_H
