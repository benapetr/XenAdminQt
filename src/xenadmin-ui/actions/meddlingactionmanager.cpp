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

#include "meddlingactionmanager.h"
#include <QDebug>
#include "meddlingaction.h"
#include <QDebug>
#include <xen/network/connection.h>
#include <xen/session.h>
#include <xen/api.h>
#include <QtCore/QDebug>
#include <QtCore/QUuid>
#include <QtCore/QDateTime>

// Initialize static member
QString MeddlingActionManager::s_applicationUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

MeddlingActionManager::MeddlingActionManager(QObject* parent)
    : QObject(parent)
{
}

MeddlingActionManager::~MeddlingActionManager()
{
    // Clean up matched tasks
    qDeleteAll(this->m_matchedTasks);
    this->m_matchedTasks.clear();
}

QString MeddlingActionManager::applicationUuid()
{
    return s_applicationUuid;
}

void MeddlingActionManager::rehydrateTasks(XenConnection* connection)
{
    if (!connection)
    {
        qWarning() << "MeddlingActionManager: Cannot rehydrate tasks - null connection";
        return;
    }

    XenSession* session = connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        qWarning() << "MeddlingActionManager: Cannot rehydrate tasks - not logged in";
        return;
    }

    qDebug() << "MeddlingActionManager: Rehydrating tasks for connection" << connection->GetHostname();

    // Get all task records (Dictionary<XenRef<Task>, Task> equivalent)
    XenRpcAPI api(session);
    QVariantMap allTaskRecords = api.getAllTaskRecords();

    if (allTaskRecords.isEmpty())
    {
        qDebug() << "MeddlingActionManager: No tasks found for rehydration";
        return;
    }

    qDebug() << "MeddlingActionManager: Found" << allTaskRecords.count() << "tasks";

    // Process each task (key = opaque_ref, value = task record)
    for (auto it = allTaskRecords.constBegin(); it != allTaskRecords.constEnd(); ++it)
    {
        QString taskRef = it.key();
        QVariantMap taskData = it.value().toMap();

        if (taskRef.isEmpty() || taskData.isEmpty())
            continue;

        // Categorize and potentially create MeddlingAction
        categorizeTask(connection, taskRef, taskData);
    }

    qDebug() << "MeddlingActionManager: Rehydration complete."
             << this->m_matchedTasks.count() << "meddling operations created,"
             << this->m_unmatchedTasks.count() << "tasks unmatched";
}

void MeddlingActionManager::handleTaskAdded(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData)
{
    if (taskRef.isEmpty() || taskData.isEmpty())
        return;

    // Skip if already tracked
    if (this->m_matchedTasks.contains(taskRef) || this->m_unmatchedTasks.contains(taskRef))
        return;

    qDebug() << "MeddlingActionManager: New task added:" << taskRef;

    categorizeTask(connection, taskRef, taskData);
}

void MeddlingActionManager::handleTaskUpdated(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData)
{
    if (taskRef.isEmpty() || taskData.isEmpty())
        return;

    // Check if this is an unmatched task that's now suitable
    if (this->m_unmatchedTasks.contains(taskRef))
    {
        qint64 serverTimeOffset = this->getServerTimeOffset(connection);

        if (MeddlingAction::isTaskUnwanted(taskData, s_applicationUuid))
        {
            qDebug() << "MeddlingActionManager: Unmatched task is now unwanted:" << taskRef;
            this->m_unmatchedTasks.remove(taskRef);
        } else if (MeddlingAction::isTaskSuitable(taskData, serverTimeOffset))
        {
            qDebug() << "MeddlingActionManager: Unmatched task is now suitable:" << taskRef;
            this->m_unmatchedTasks.remove(taskRef);
            this->categorizeTask(connection, taskRef, taskData);
        }
        return;
    }

    // Update matched task
    if (this->m_matchedTasks.contains(taskRef))
    {
        MeddlingAction* op = this->m_matchedTasks.value(taskRef);
        if (op)
        {
            op->updateFromTask(taskData, false);

            // Remove from tracking if completed
            QString status = taskData.value("status").toString();
            if (status == "success" || status == "failure" || status == "cancelled")
            {
                qDebug() << "MeddlingActionManager: Task completed:" << taskRef << status;
                this->m_matchedTasks.remove(taskRef);
                // Don't delete - OperationManager owns it
            }
        }
    }
}

void MeddlingActionManager::handleTaskRemoved(XenConnection* connection, const QString& taskRef)
{
    Q_UNUSED(connection);

    if (taskRef.isEmpty())
        return;

    this->m_unmatchedTasks.remove(taskRef);

    if (this->m_matchedTasks.contains(taskRef))
    {
        MeddlingAction* op = this->m_matchedTasks.take(taskRef);
        if (op)
        {
            qDebug() << "MeddlingActionManager: Task removed from server:" << taskRef;

            // Mark as completed (server destroyed it)
            QVariantMap emptyData;
            op->updateFromTask(emptyData, true);

            // Don't delete - OperationManager owns it
        }
    }
}

void MeddlingActionManager::categorizeTask(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData)
{
    qint64 serverTimeOffset = this->getServerTimeOffset(connection);

    // Check if task is unwanted (our own task, subtask, etc.)
    if (MeddlingAction::isTaskUnwanted(taskData, s_applicationUuid))
    {
        qDebug() << "MeddlingActionManager: Task is unwanted (our own or subtask):" << taskRef;
        return;
    }

    // Check if task is suitable for MeddlingAction
    if (!MeddlingAction::isTaskSuitable(taskData, serverTimeOffset))
    {
        qDebug() << "MeddlingActionManager: Task not yet suitable (waiting for appliesTo):" << taskRef;
        this->m_unmatchedTasks[taskRef] = taskData;
        return;
    }

    // Check if this is one of our tasks (has our UUID in other_config)
    QVariantMap otherConfig = taskData.value("other_config").toMap();
    QString taskUuid = otherConfig.value("XenAdminQtUUID").toString();
    bool isOurTask = (taskUuid == s_applicationUuid && !taskUuid.isEmpty());

    qDebug() << "MeddlingActionManager: Creating MeddlingAction for task:" << taskRef
             << "isOurTask:" << isOurTask;

    // Create MeddlingAction
    MeddlingAction* op = new MeddlingAction(taskRef, connection, isOurTask, this);
    op->updateFromTask(taskData, false);

    // Track it
    this->m_matchedTasks[taskRef] = op;

    // Notify (OperationManager will register it)
    emit meddlingOperationCreated(op);
}

qint64 MeddlingActionManager::getServerTimeOffset(XenConnection* connection) const
{
    Q_UNUSED(connection);

    // TODO: XenConnection should expose server time offset
    // For now, return 0 (assume clocks are synchronized)
    // C# gets this from Connection.ServerTimeOffset
    return 0;
}
