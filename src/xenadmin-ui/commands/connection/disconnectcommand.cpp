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

#include "disconnectcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/warningdialogs/closexencenterwarningdialog.h"
#include "xen/network/connection.h"
#include "operations/operationmanager.h"
#include "actions/meddlingaction.h"
#include <QMetaObject>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QElapsedTimer>

namespace
{
    bool allActionsFinished(XenConnection* connection)
    {
        const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->records();
        for (OperationManager::OperationRecord* record : records)
        {
            AsyncOperation* operation = record ? record->operation.data() : nullptr;
            if (!operation)
                continue;
            if (qobject_cast<MeddlingAction*>(operation))
                continue;
            if (operation->connection() != connection)
                continue;
            if (record->state != AsyncOperation::Completed)
                return false;
        }
        return true;
    }

    void cancelAllActions(XenConnection* connection)
    {
        const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->records();
        for (OperationManager::OperationRecord* record : records)
        {
            AsyncOperation* operation = record ? record->operation.data() : nullptr;
            if (!operation)
                continue;
            if (qobject_cast<MeddlingAction*>(operation))
                continue;
            if (operation->connection() != connection)
                continue;
            if (operation->canCancel())
                operation->cancel();
        }
    }

    void waitForCancel(MainWindow* mainWindow, XenConnection* connection)
    {
        QProgressDialog progress(mainWindow);
        progress.setWindowTitle(QObject::tr("Canceling Tasks"));
        progress.setLabelText(QObject::tr("Canceling..."));
        progress.setCancelButton(nullptr);
        progress.setRange(0, 0);
        progress.setMinimumDuration(0);
        progress.show();

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 6000)
        {
            if (allActionsFinished(connection))
                break;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
        }
    }
}

DisconnectCommand::DisconnectCommand(MainWindow* mainWindow,
                                     XenConnection* connection,
                                     bool prompt,
                                     QObject* parent)
    : Command(mainWindow, parent),
      m_connection(connection),
      m_prompt(prompt)
{
}

bool DisconnectCommand::CanRun() const
{
    return m_connection && (m_connection->IsConnected() || m_connection->InProgress());
}

void DisconnectCommand::Run()
{
    if (!CanRun())
        return;

    if (m_prompt && !confirmDisconnect())
        return;

    if (!m_prompt)
        cancelAllActions(m_connection);

    doDisconnect();
}

QString DisconnectCommand::MenuText() const
{
    return "Disconnect";
}

bool DisconnectCommand::confirmDisconnect() const
{
    if (!m_connection)
        return false;

    if (!allActionsFinished(m_connection))
    {
        CloseXenCenterWarningDialog dlg(false, m_connection, this->mainWindow());
        if (dlg.exec() != QDialog::Accepted)
            return false;

        cancelAllActions(m_connection);
        waitForCancel(this->mainWindow(), m_connection);
        return true;
    }

    return true;
}

void DisconnectCommand::doDisconnect()
{
    if (!m_connection)
        return;

    this->mainWindow()->showStatusMessage("Disconnecting...");

    m_connection->EndConnect(true, false);
    QMetaObject::invokeMethod(this->mainWindow(), "SaveServerList", Qt::QueuedConnection);
}
