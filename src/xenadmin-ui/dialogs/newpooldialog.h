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

#ifndef NEWPOOLDIALOG_H
#define NEWPOOLDIALOG_H

#include <QDialog>
#include <QListWidgetItem>

class XenLib;
class XenConnection;
class Session;

namespace Ui
{
    class NewPoolDialog;
}

/**
 * @brief Dialog for creating a new resource pool
 *
 * Port of C# XenAdmin.Dialogs.NewPoolDialog
 *
 * Allows user to:
 * - Enter pool name and optional description
 * - Select a coordinator (master) host from connected standalone servers
 * - Select supporter (slave) hosts to join the pool
 *
 * Only standalone servers (not already in a pool) can be selected.
 */
class NewPoolDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewPoolDialog(QWidget* parent = nullptr);
    ~NewPoolDialog();

private slots:
    void onCoordinatorChanged(int index);
    void onServerItemChanged(QListWidgetItem* item);
    void onPoolNameChanged(const QString& text);
    void onAddServerClicked();
    void onCreateClicked();

private:
    void populateConnections();
    void updateServerList();
    void updateCreateButton();
    bool isStandaloneConnection(XenConnection* connection) const;
    XenConnection* getCoordinatorConnection() const;
    QList<XenConnection*> getSupporterConnections() const;
    void createPool();

    Ui::NewPoolDialog* ui;
    QList<XenConnection*> m_connections;
};

#endif // NEWPOOLDIALOG_H
