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

#ifndef CLOSEXENCENTERWARNINGDIALOG_H
#define CLOSEXENCENTERWARNINGDIALOG_H

#include <QDialog>

class XenConnection;

namespace Ui
{
    class CloseXenCenterWarningDialog;
}

class CloseXenCenterWarningDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit CloseXenCenterWarningDialog(bool fromUpdate = false,
                                             XenConnection* connection = nullptr,
                                             QWidget* parent = nullptr);
        ~CloseXenCenterWarningDialog() override;

    private slots:
        void rebuildList();

    private:
        QString applicationName() const;
        QString stateToText(int state) const;
        bool shouldShowRecord(int state, bool isMeddling) const;

        Ui::CloseXenCenterWarningDialog* ui;
        XenConnection* m_connection = nullptr;
};

#endif // CLOSEXENCENTERWARNINGDIALOG_H
