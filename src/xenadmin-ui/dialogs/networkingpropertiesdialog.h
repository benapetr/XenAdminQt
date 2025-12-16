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

#ifndef NETWORKINGPROPERTIESDIALOG_H
#define NETWORKINGPROPERTIESDIALOG_H

#include <QDialog>
#include <QVariantMap>

namespace Ui
{
    class NetworkingPropertiesDialog;
}

class XenLib;

class NetworkingPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkingPropertiesDialog(XenLib* xenLib, const QString& pifRef, QWidget* parent = nullptr);
    ~NetworkingPropertiesDialog();

    void accept() override;

private slots:
    void onIPModeChanged();
    void onInputChanged();
    void validateAndUpdateUI();

private:
    void loadPIFData();
    bool validateIP(const QString& ip, bool allowEmpty = false);
    bool validateSubnetMask(const QString& mask);
    void applyChanges();

    Ui::NetworkingPropertiesDialog* ui;
    XenLib* m_xenLib;
    QString m_pifRef;
    QVariantMap m_pifData;
};

#endif // NETWORKINGPROPERTIESDIALOG_H
