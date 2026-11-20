/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

// C# equivalent: xenadmin/XenAdmin/Dialogs/AttachUsbDialog.cs

#ifndef ATTACHUSBDIALOG_H
#define ATTACHUSBDIALOG_H

#include <QDialog>
#include <QSharedPointer>

namespace Ui
{
    class AttachUsbDialog;
}

class Host;
class QPushButton;
class VM;

//! Lists the free passthrough-enabled USB devices on the hosts a VM can run on, and attaches one
class AttachUsbDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit AttachUsbDialog(QSharedPointer<VM> vm, QWidget* parent = nullptr);
        ~AttachUsbDialog() override;

    private slots:
        void updateAttachButton();
        void attach();

    private:
        void buildList();

        Ui::AttachUsbDialog* ui;
        QPushButton* m_attachButton;
        QSharedPointer<VM> m_vm;
        QList<QSharedPointer<Host>> m_possibleHosts;
};

#endif // ATTACHUSBDIALOG_H
