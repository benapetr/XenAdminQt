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

// copyvmdialog.h - Dialog for copying VMs and templates
// Matches C# XenAdmin.Dialogs.CopyVMDialog
#ifndef COPYVMDIALOG_H
#define COPYVMDIALOG_H

#include <QDialog>
#include <QSharedPointer>

namespace Ui
{
    class CopyVMDialog;
}

class VM;
class XenConnection;

class CopyVMDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit CopyVMDialog(QSharedPointer<VM> vm, QWidget* parent = nullptr);
        ~CopyVMDialog();

        QString getName() const;
        QString getDescription() const;
        bool isFastClone() const;
        QString getSelectedSR() const;

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void onNameTextChanged();
        void onDescriptionTextChanged();
        void onCloneRadioToggled(bool checked);
        void onCopyRadioToggled(bool checked);
        void onRescanClicked();
        void onSrPickerSelectionChanged();
        void onSrPickerCanBeScannedChanged();
        void accept() override;

    private:
        Ui::CopyVMDialog* ui;
        QSharedPointer<VM> m_vm;
        XenConnection* m_connection;
        
        void initialize();
        void enableMoveButton();
        void enableRescanButton();
        QString getDefaultCopyName(QSharedPointer<VM> vmToCopy);
};

#endif // COPYVMDIALOG_H
