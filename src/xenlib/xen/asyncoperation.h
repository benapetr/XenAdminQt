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

        explicit AsyncOperation(XenConnection* connection, const QString& title,
                                const QString& description = QString(), QObject* parent = nullptr);
        explicit AsyncOperation(const QString& title, const QString& description = QString(),
                                QObject* parent = nullptr);
        virtual ~AsyncOperation();

        // Core properties
        QString title() const;
        void setTitle(const QString& title);

        QString description() const;
        void setDescription(const QString& description);

        XenConnection* connection() const;
        void setConnection(XenConnection* connection);

        XenAPI::Session* session() const;

        // Progress tracking
        int percentComplete() const;
        void setPercentComplete(int percent);

        // State management
        OperationState state() const;
        bool isRunning() const;
        bool isCompleted() const;
        bool isCancelled() const;
        bool isFailed() const;

        // Error handling
        QString errorMessage() const;
        QString shortErrorMessage() const;
        QStringList errorDetails() const;
        bool hasError() const;

        // Cancellation
        bool canCancel() const;
        void setCanCancel(bool canCancel);

        // Result
        QString result() const;
        void setResult(const QString& result);

        // Timing
        QDateTime startTime() const;
        QDateTime endTime() const;
        qint64 elapsedTime() const; // in milliseconds

        // RBAC (Role-Based Access Control) support
        QStringList apiMethodsToRoleCheck() const;
        void addApiMethodToRoleCheck(const QString& method);

        // Task management (for XenAPI tasks)
        QString relatedTaskRef() const;
        void setRelatedTaskRef(const QString& taskRef);

        // Operation UUID (for task rehydration after reconnect)
        QString operationUuid() const;
        void setOperationUuid(const QString& uuid);

        // Cleanup for shutdown/reconnect - removes UUID from task.other_config
        void prepareForEventReloadAfterRestart();

        // Object context (matches C# ActionBase Pool/Host/VM/SR/Template properties)
        Pool* pool() const;
        void setPool(Pool* pool);

        Host* host() const;
        void setHost(Host* host);

        VM* vm() const;
        void setVM(VM* vm);

        SR* sr() const;
        void setSR(SR* sr);

        VM* vmTemplate() const;
        void setVMTemplate(VM* vmTemplate);

        // AppliesTo list - all object refs this operation applies to
        QStringList appliesTo() const;
        void addAppliesTo(const QString& opaqueRef);
        void clearAppliesTo();

        // History suppression (for internal/composite operations)
        bool suppressHistory() const;
        void setSuppressHistory(bool suppress);

        // Safe exit flag (can XenCenter exit while this operation is running?)
        bool safeToExit() const;
        void setSafeToExit(bool safe);

    public slots:
        // Execution control
        virtual void runAsync();
        virtual void runSync(XenAPI::Session* session = nullptr);
        virtual void cancel();

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
        bool pollTask(const QString& taskRef, double start, double finish);
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
        void setAppliesToFromObject(QObject* xenObject);

    private:
        // Worker thread management
        void runOnWorkerThread();
        void startWorkerThread();
        void waitForWorkerCompletion();

        // Private data
        QString m_title;
        QString m_description;
        QPointer<XenConnection> m_connection;
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
        QPointer<Pool> m_pool;
        QPointer<Host> m_host;
        QPointer<VM> m_vm;
        QPointer<SR> m_sr;
        QPointer<VM> m_vmTemplate;
        QStringList m_appliesTo;

        // Thread management
        mutable QRecursiveMutex m_mutex;
        QWaitCondition m_waitCondition;
        bool m_syncExecution;

        // Session ownership
        bool m_ownsSession;

        static const int TASK_POLL_INTERVAL_MS = 900;
        static const int DEFAULT_TIMEOUT_MS = 300000; // 5 minutes
};

#endif // ASYNCOPERATION_H
