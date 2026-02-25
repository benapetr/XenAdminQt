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

#ifndef MEDDLINGACTION_H
#define MEDDLINGACTION_H

#include <xen/asyncoperation.h>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QSet>

class XenConnection;

/**
 * @brief MeddlingAction - Read-only operation monitoring existing XenAPI tasks
 *
 * Qt equivalent of C# MeddlingAction (XenAdmin/Actions/GUIActions/MeddlingAction.cs).
 * This action doesn't create tasks, it monitors existing tasks created by:
 * - Our own operations that were running during disconnect (task rehydration)
 * - Other XenAdmin instances or CLI tools (external task monitoring)
 *
 * The action is "read-only" - it only polls task state and doesn't modify it.
 *
 * Thread-safety: All public methods are thread-safe.
 */
class MeddlingAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Create a MeddlingAction for an existing task
         * @param taskRef XenAPI task opaque reference
         * @param connection Connection where the task exists
         * @param isOurTask True if this task was created by us (has our UUID)
         * @param parent Parent QObject
         */
        explicit MeddlingAction(const QString& taskRef, XenConnection* connection, bool isOurTask = false, QObject* parent = nullptr);

        ~MeddlingAction() override;

        /**
         * @brief Check if this is our task (created by this XenAdminQt instance)
         */
        bool isOurTask() const
        {
            return m_isOurTask;
        }

        /**
         * @brief True when this task maps to a recognized VM operation (C# IsTaskUnwanted filter parity).
         */
        bool IsRecognizedOperation() const
        {
            return m_isRecognizedOperation;
        }

        /**
         * @brief Update operation state from task record
         * @param taskData Task record as QVariantMap
         * @param taskDeleting True if task is being deleted from server
         */
        void updateFromTask(const QVariantMap& taskData, bool taskDeleting = false);

        /**
         * @brief Determine if a task should be ignored (unwanted)
         *
         * Tasks are unwanted if:
         * - They have our XenAdminQtUUID (we already have an AsyncOperation for them)
         * - They're subtasks of another task
         * - They correspond to operations we don't care about
         *
         * @param taskData Task record from XenAPI
         * @param ourUuid Our application UUID
         * @return true if task should be ignored
         */
        static bool isTaskUnwanted(const QVariantMap& taskData, const QString& ourUuid, bool showAllServerEvents = false);

        /**
         * @brief Determine if a task is suitable for creating a MeddlingOperation
         *
         * Tasks are suitable if:
         * - They have appliesTo set (aware client), OR
         * - Enough time has passed (5 seconds) to give aware clients time to set it
         *
         * This heuristic matches C# MeddlingAction.IsTaskSuitable
         *
         * @param taskData Task record from XenAPI
         * @param serverTimeOffset Server time offset from local time
         * @return true if task is suitable for monitoring
         */
        static bool isTaskSuitable(const QVariantMap& taskData, qint64 serverTimeOffset);

    protected:
        /**
         * @brief Run method - polls existing task instead of creating new one
         */
        void run() override;

        /**
         * @brief Cancel method - only works for our own tasks
         */
        void onCancel() override;

    private:
        static bool isRecognizedOperation(const QVariantMap& taskData);

        /**
         * @brief Extract and set operation title/description from task
         */
        void updateTitleFromTask(const QVariantMap& taskData);

        /**
         * @brief Extract VM operation type from task other_config
         */
        QString getVmOperation(const QVariantMap& taskData) const;

        /**
         * @brief Get human-readable title for VM operation
         */
        QString getVmOperationTitle(const QString& operation) const;

    private:
        bool m_isOurTask; ///< true if this was our task (task rehydration)
        bool m_isRecognizedOperation = false;

        // Heuristic: 5 seconds window for aware clients to set appliesTo
        static const qint64 AWARE_CLIENT_HEURISTIC_MS = 5000;
};

#endif // MEDDLINGACTION_H
