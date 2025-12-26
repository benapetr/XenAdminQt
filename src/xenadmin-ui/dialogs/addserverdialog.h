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

#ifndef ADDSERVERDIALOG_H
#define ADDSERVERDIALOG_H

#include <QtGui/QShowEvent>
#include <QtWidgets/QDialog>

class XenConnection;

namespace Ui
{
    class AddServerDialog;
}

class AddServerDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit AddServerDialog(XenConnection* connection, bool changedPass, QWidget* parent = nullptr);
        ~AddServerDialog() override;

        QString serverInput() const;
        QString username() const;
        QString password() const;

    signals:
        void cachePopulated(XenConnection* connection);

    protected:
        void showEvent(QShowEvent* event) override;

    private slots:
        void AddServerDialog_Load();
        void AddServerDialog_Shown();
        void AddButton_Click();
        void CancelButton2_Click();
        void TextFields_TextChanged();
        void labelError_TextChanged();

    private:
        void UpdateText();
        void UpdateButtons();
        bool OKButtonEnabled() const;
        QString hostnameWithPort() const;

        Ui::AddServerDialog* ui;
        XenConnection* m_connection;
        bool m_changedPass;
};

#endif // ADDSERVERDIALOG_H
