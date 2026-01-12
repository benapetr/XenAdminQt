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

#ifndef ASYNCOPERATION_H
#define ASYNCOPERATION_H

#include "../xenlib_global.h"
#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

namespace XenAPI
{
    class Session;
}

class XenObject;
class XenConnection;
class Pool;
class Host;
class VM;
class SR;
class NetworkingActionHelpers; // Forward declaration for friend access

class XENLIB_EXPORT AsyncOperation : public QObject
{
    Q_OBJECT
    friend class NetworkingActionHelpers; // Allow access to pollToCompletion

    public:
        enum OperationState
        {
            NotStarted,
            Running,
            Completed,
            Cancelled,
            Failed
        };
        Q_ENUM(OperationState)

        explicit AsyncOperation(XenConnection* connection, const QString& title, const QString& description = QString(), QObject* parent = nullptr);
        explicit AsyncOperation(const QString& title, const QString& description = QString(), QObject* parent = nullptr);
        virtual ~AsyncOperation();

        // Core properties
        QString GetTitle() const;
        void SetTitle(const QString& title);

        QString GetDescription() const;
        void SetDescription(const QString& description);

        XenConnection* GetConnection() const;
        void SetConnection(XenConnection* connection);

        XenAPI::Session* GetSession() const;

        // Progress tracking
        int GetPercentComplete() const;
        void SetPercentComplete(int percent);

        // State management
        OperationState GetState() const;
        bool IsRunning() const;
        bool IsCompleted() const;
        bool IsCancelled() const;
        bool IsFailed() const;

        // Error handling
        QString GetErrorMessage() const;
        QString GetShortErrorMessage() const;
        QStringList GetErrorDetails() const;
        bool HasError() const;

        // Cancellation
        bool CanCancel() const;
        void SetCanCancel(bool canCancel);

        // Result
        QString GetResult() const;
        void SetResult(const QString& result);

        // Timing
        QDateTime GetStartTime() const;
        QDateTime GetEndTime() const;
        qint64 GetElapsedTime() const; // in milliseconds

        // RBAC (Role-Based Access Control) support
        QStringList GetApiMethodsToRoleCheck() const;
        void AddApiMethodToRoleCheck(const QString& method);

        // Task management (for XenAPI tasks)
        QString GetRelatedTaskRef() const;
        void SetRelatedTaskRef(const QString& taskRef);

        // Operation UUID (for task rehydration after reconnect)
        QString GetOperationUUID() const;
        void SetOperationUUID(const QString& uuid);

        // Cleanup for shutdown/reconnect - removes UUID from task.other_config
        void PrepareForEventReloadAfterRestart();

        // Object context (matches C# ActionBase Pool/Host/VM/SR/Template properties)
        QSharedPointer<Pool> GetPool() const;
        void SetPool(QSharedPointer<Pool> pool);

        QSharedPointer<Host> GetHost() const;
        void SetHost(QSharedPointer<Host> host);

        QSharedPointer<VM> GetVM() const;
        void SetVM(QSharedPointer<VM> vm);

        QSharedPointer<SR> GetSR() const;
        void SetSR(QSharedPointer<SR> sr);

        QSharedPointer<VM> GetTemplate() const;
        void SetTemplate(QSharedPointer<VM> vmTemplate);

        // GetAppliesToList list - all object refs this operation applies to
        QStringList GetAppliesToList() const;
        void AddAppliesTo(const QString& opaqueRef);
        void ClearAppliesTo();

        // History suppression (for internal/composite operations)
        bool SuppressHistory() const;
        void SetSuppressHistory(bool suppress);

        // Safe exit flag (can XenCenter exit while this operation is running?)
        bool IsSafeToExit() const;
        void SetSafeToExit(bool safe);

    public slots:
        // Execution control
        virtual void RunAsync(bool auto_delete = false);
        virtual void RunSync(XenAPI::Session* session = nullptr);
        virtual void Cancel();

    signals:
        // State change signals
        void started();
        void progressChanged(int percent);
        void completed();
        void cancelled();
        void failed(const QString& error);
        void stateChanged(AsyncOperation::OperationState state);

        // Progress detail signals
        void titleChanged(const QString& title);
        void descriptionChanged(const QString& description);

    protected:
        // Virtual methods to be implemented by subclasses
        virtual void run() = 0;
        virtual void onCancel()
        {
        }

        // Helper methods for subclasses
        void setState(OperationState newState);
        void setError(const QString& message, const QStringList& details = QStringList());
        void clearError();

        // Session management
        XenAPI::Session* createSession();
        void destroySession();

        // Task polling (for XenAPI async calls)
        void pollToCompletion(const QString& taskRef, double start = 0, double finish = 100, bool suppressFailures = false);
        bool pollTask(const QString& taskRef, double start, double finish, bool suppressFailures = false);
        void destroyTask();

        // Task cancellation (matches C# CancellingAction.CancelRelatedTask)
        virtual void cancelRelatedTask();

        // Task tagging (for rehydration after reconnect)
        void tagTaskWithUuid(const QString& taskRef);
        void removeUuidFromTask(const QString& taskRef);

        // Audit logging
        void auditLogSuccess();
        void auditLogCancelled();
        void auditLogFailure(const QString& error);

        // Thread-safe property access
        void setTitleSafe(const QString& title);
        void setDescriptionSafe(const QString& description);
        void setPercentCompleteSafe(int percent);

        // AppliesTo helper - automatically adds object refs (matches C# SetAppliesTo)
        void setAppliesToFromObject(QSharedPointer<XenObject> xenObject);

        QPointer<XenConnection> m_connection;

    private:
        // Worker thread management
        void runOnWorkerThread();

        // Private data
        QString m_title;
        QString m_description;
        QPointer<XenAPI::Session> m_session;
        int m_percentComplete;
        OperationState m_state;
        QString m_errorMessage;
        QString m_shortErrorMessage;
        QStringList m_errorDetails;
        bool m_canCancel;
        QString m_result;
        QDateTime m_startTime;
        QDateTime m_endTime;
        QStringList m_apiMethodsToRoleCheck;
        QString m_relatedTaskRef;
        QString m_operationUuid; ///< UUID for task rehydration (XenAdminQtUUID)
        bool m_suppressHistory;
        bool m_safeToExit;

        // Object context (C# ActionBase equivalents)
        // TODO we can probably use single shared pointer for XenObject here instead
        QSharedPointer<Pool> m_pool;
        QSharedPointer<Host> m_host;
        QSharedPointer<VM> m_vm;
        QSharedPointer<SR> m_sr;
        QSharedPointer<VM> m_vmTemplate;
        QStringList m_appliesTo;

        // Thread management
        mutable QRecursiveMutex m_mutex;
        QWaitCondition m_waitCondition;
        bool m_syncExecution;

        // Session ownership
        bool m_ownsSession;

        // Most of async ops are actions that are run by commands async which makes them hard to manage from memory perspective
        // parenting them to command leads to segfaults because command is usually deleted earlier than async op it launched

        // parenting them to anything persistent leads to memory leaks, so we can run these with auto delete which means the op
        // will delete itself once it's finished
        bool m_autoDelete = false;

        static const int TASK_POLL_INTERVAL_MS = 900;
        static const int DEFAULT_TIMEOUT_MS = 300000; // 5 minutes
};

#endif // ASYNCOPERATION_H
