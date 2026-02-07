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

#include "closexencenterwarningdialog.h"
#include "ui_closexencenterwarningdialog.h"
#include "operations/operationmanager.h"
#include "xenlib/xen/actions/meddlingaction.h"
#include "xenlib/xen/network/connection.h"
#include <QCoreApplication>

CloseXenCenterWarningDialog::CloseXenCenterWarningDialog(bool fromUpdate,
                                                         XenConnection* connection,
                                                         QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::CloseXenCenterWarningDialog)
    , m_connection(connection)
{
    ui->setupUi(this);

    const QString appName = applicationName();
    ui->label2->setText(tr("Unfinished tasks may not complete successfully if you exit %1 before they finish.")
                            .arg(appName));
    ui->label1->setText(tr("%1 is still performing the following tasks:").arg(appName));
    ui->label3->setVisible(fromUpdate);
    ui->label3->setText(tr("In order to update, %1 will be closed.").arg(appName));

    if (m_connection)
    {
        ui->label2->setText(tr("Unfinished tasks will be canceled if you disconnect from '%1' before they finish.")
                                .arg(m_connection->GetHostname()));
        ui->ExitButton->setText(tr("&Disconnect anyway"));
        ui->DontExitButton->setText(tr("Do&n't disconnect"));
    } else
    {
        ui->ExitButton->setText(tr("E&xit %1 anyway").arg(appName));
        ui->DontExitButton->setText(tr("&Don't Exit"));
    }

    connect(ui->ExitButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->DontExitButton, &QPushButton::clicked, this, &QDialog::reject);

    OperationManager* manager = OperationManager::instance();
    connect(manager, &OperationManager::recordAdded, this, &CloseXenCenterWarningDialog::rebuildList);
    connect(manager, &OperationManager::recordUpdated, this, &CloseXenCenterWarningDialog::rebuildList);
    connect(manager, &OperationManager::recordRemoved, this, &CloseXenCenterWarningDialog::rebuildList);

    rebuildList();
}

CloseXenCenterWarningDialog::~CloseXenCenterWarningDialog()
{
    delete ui;
}

QString CloseXenCenterWarningDialog::applicationName() const
{
    const QString appName = QCoreApplication::applicationName();
    return appName.isEmpty() ? QStringLiteral("XenAdmin") : appName;
}

QString CloseXenCenterWarningDialog::stateToText(int state) const
{
    using State = AsyncOperation::OperationState;
    switch (static_cast<State>(state))
    {
    case State::Running:
        return tr("Running");
    case State::Failed:
        return tr("Failed");
    case State::Cancelled:
        return tr("Cancelled");
    case State::Completed:
        return tr("Completed");
    case State::NotStarted:
        return tr("Pending");
    default:
        return tr("Unknown");
    }
}

bool CloseXenCenterWarningDialog::shouldShowRecord(int state, bool isMeddling) const
{
    using State = AsyncOperation::OperationState;
    if (isMeddling)
        return false;

    State opState = static_cast<State>(state);
    return opState == State::Running || opState == State::NotStarted;
}

void CloseXenCenterWarningDialog::rebuildList()
{
    ui->actionsTable->setRowCount(0);

    OperationManager* manager = OperationManager::instance();
    const QList<OperationManager::OperationRecord*>& records = manager->GetRecords();

    int row = 0;
    for (OperationManager::OperationRecord* record : records)
    {
        AsyncOperation* operation = record ? record->operation.data() : nullptr;
        if (!operation)
            continue;

        const bool isMeddling = qobject_cast<MeddlingAction*>(operation) != nullptr;
        if (!shouldShowRecord(record->state, isMeddling))
            continue;

        if (m_connection && operation->GetConnection() != m_connection)
            continue;

        ui->actionsTable->insertRow(row);

        QTableWidgetItem* statusItem = new QTableWidgetItem(stateToText(record->state));
        QTableWidgetItem* messageItem = new QTableWidgetItem(record->title);
        QTableWidgetItem* locationItem = new QTableWidgetItem(
            operation->GetConnection() ? operation->GetConnection()->GetHostname() : QString());
        QTableWidgetItem* dateItem = new QTableWidgetItem(
            QLocale::system().toString(record->started, QLocale::ShortFormat));

        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        messageItem->setFlags(messageItem->flags() & ~Qt::ItemIsEditable);
        locationItem->setFlags(locationItem->flags() & ~Qt::ItemIsEditable);
        dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);

        ui->actionsTable->setItem(row, 0, statusItem);
        ui->actionsTable->setItem(row, 1, messageItem);
        ui->actionsTable->setItem(row, 2, locationItem);
        ui->actionsTable->setItem(row, 3, dateItem);

        ++row;
    }

    ui->actionsTable->resizeColumnsToContents();
    ui->actionsTable->horizontalHeader()->setStretchLastSection(true);
}
