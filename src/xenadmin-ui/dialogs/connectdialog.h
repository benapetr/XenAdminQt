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

#ifndef CONNECTDIALOG_H
#define CONNECTDIALOG_H

#include <QDialog>
#include "../connectionprofile.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ConnectDialog;
}
QT_END_NAMESPACE

class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget* parent = nullptr);
    // Constructor for retry mode with pre-filled credentials
    explicit ConnectDialog(const QString& hostname, int port, const QString& username,
                           const QString& errorMessage, QWidget* parent = nullptr);
    ~ConnectDialog();

    QString getHostname() const;
    int getPort() const;
    QString getUsername() const;
    QString getPassword() const;
    bool useSSL() const;
    bool saveProfile() const;
    QString getProfileName() const;
    ConnectionProfile getConnectionProfile() const;

private slots:
    void validateInput();
    void onProfileSelected(int index);
    void onSaveProfileChanged(int state);
    void onDeleteProfile();

private:
    void loadProfiles();
    void fillFromProfile(const ConnectionProfile& profile);

    Ui::ConnectDialog* ui;
    QList<ConnectionProfile> m_profiles;
};

#endif // CONNECTDIALOG_H
