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

#ifndef MEDDLINGACTIONMANAGER_H
#define MEDDLINGACTIONMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QVariantMap>

class XenConnection;
class AsyncOperation;
class MeddlingAction;

/**
 * @brief MeddlingActionManager - Manages task rehydration after reconnect
 *
 * Qt equivalent of C# MeddlingActionManager. Handles:
 * 1. Task rehydration - Finding our tasks after reconnect via XenAdminQtUUID
 * 2. External task monitoring - Creating MeddlingActions for other clients' tasks
 *
 * The manager listens to connection state changes and task cache updates.
 * When a connection is re-established, it queries all tasks and creates
 * MeddlingActions for:
 * - Our own tasks (isOurTask=true) - task rehydration
 * - External tasks (isOurTask=false) - external monitoring
 *
 * Thread-safety: All methods are thread-safe and can be called from any thread.
 */
class MeddlingActionManager : public QObject
{
    Q_OBJECT

    public:
        explicit MeddlingActionManager(QObject* parent = nullptr);
        ~MeddlingActionManager() override;

        /**
         * @brief Get the application UUID for task tagging
         *
         * This UUID is written to task.other_config["XenAdminQtUUID"] and used
         * to identify our tasks after reconnect.
         */
        static QString applicationUuid();

        /**
         * @brief Rehydrate tasks for a connection after reconnect
         *
         * Called after XenLib reports connectionStateChanged(true). Queries all
         * tasks and creates MeddlingActions for:
         * - Tasks with our XenAdminQtUUID (rehydration)
         * - Tasks from other clients (external monitoring)
         *
         * @param connection The connection that just reconnected
         */
        void rehydrateTasks(XenConnection* connection);

        /**
         * @brief Handle task added event from cache
         *
         * Called when EventPoller sees a new task. Categorizes and creates
         * MeddlingAction if needed.
         *
         * @param connection Connection where task appeared
         * @param taskRef Task opaque reference
         * @param taskData Task record
         */
        void handleTaskAdded(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);

        /**
         * @brief Handle task updated event from cache
         *
         * Updates existing MeddlingAction with new task state.
         *
         * @param connection Connection where task was updated
         * @param taskRef Task opaque reference
         * @param taskData Updated task record
         */
        void handleTaskUpdated(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);

        /**
         * @brief Handle task removed event from cache
         *
         * Removes MeddlingAction when task is destroyed on server.
         *
         * @param connection Connection where task was removed
         * @param taskRef Task opaque reference
         */
        void handleTaskRemoved(XenConnection* connection, const QString& taskRef);

    signals:
        /**
         * @brief Emitted when a meddling operation is created
         *
         * Connect to this signal to register the operation with OperationManager
         * so it appears in the history/events UI.
         */
        void meddlingOperationCreated(MeddlingAction* operation);

    private:
        /**
         * @brief Categorize a task and create MeddlingAction if suitable
         *
         * Tasks are categorized as:
         * - Unwanted: Our own tasks, subtasks, unrecognized operations
         * - Unmatched: New tasks waiting for appliesTo to be set
         * - Matched: Suitable tasks that get a MeddlingAction
         *
         * @param connection Connection where task exists
         * @param taskRef Task opaque reference
         * @param taskData Task record
         */
        void categorizeTask(XenConnection* connection, const QString& taskRef, const QVariantMap& taskData);

        /**
         * @brief Get server time offset for a connection
         */
        qint64 getServerTimeOffset(XenConnection* connection) const;

    private:
        /// Tasks we've seen but haven't categorized yet (waiting for appliesTo)
        QHash<QString, QVariantMap> m_unmatchedTasks;

        /// Tasks we've categorized and created MeddlingActions for
        QHash<QString, MeddlingAction*> m_matchedTasks;

        /// Application UUID (generated once at startup)
        static QString s_applicationUuid;
};

#endif // MEDDLINGACTIONMANAGER_H
