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

#include "asyncoperation.h"
#include "session.h"
#include "connection.h"
#include "api.h"
#include "pool.h"
#include "host.h"
#include "vm.h"
#include "sr.h"
#include "failure.h"
#include "../xenlib.h"
#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QRunnable>
#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>

// Define static const members
const int AsyncOperation::TASK_POLL_INTERVAL_MS;
const int AsyncOperation::DEFAULT_TIMEOUT_MS;

// AsyncOperation Implementation

AsyncOperation::AsyncOperation(XenConnection* connection, const QString& title,
                               const QString& description, QObject* parent)
    : QObject(parent), m_title(title), m_description(description), m_connection(connection), m_session(nullptr), m_percentComplete(0), m_state(NotStarted), m_canCancel(true), m_suppressHistory(false), m_safeToExit(true), m_pool(nullptr), m_host(nullptr), m_vm(nullptr), m_sr(nullptr), m_vmTemplate(nullptr), m_syncExecution(false), m_ownsSession(false)
{
}

AsyncOperation::AsyncOperation(const QString& title, const QString& description, QObject* parent)
    : AsyncOperation(nullptr, title, description, parent)
{
}

AsyncOperation::~AsyncOperation()
{
    // Clean up session if we own it
    destroySession();
}

// Property getters
QString AsyncOperation::title() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_title;
}

void AsyncOperation::setTitle(const QString& title)
{
    QMutexLocker locker(&this->m_mutex);
    if (this->m_title != title)
    {
        this->m_title = title;
        locker.unlock();
        emit this->titleChanged(title);
    }
}

QString AsyncOperation::description() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_description;
}

void AsyncOperation::setDescription(const QString& description)
{
    QMutexLocker locker(&this->m_mutex);
    if (this->m_description != description)
    {
        this->m_description = description;
        locker.unlock();
        emit this->descriptionChanged(description);
    }
}

XenConnection* AsyncOperation::connection() const
{
    return this->m_connection;
}

void AsyncOperation::setConnection(XenConnection* connection)
{
    this->m_connection = connection;
}

XenSession* AsyncOperation::session() const
{
    return this->m_session;
}

int AsyncOperation::percentComplete() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_percentComplete;
}

void AsyncOperation::setPercentComplete(int percent)
{
    QMutexLocker locker(&this->m_mutex);
    percent = qBound(0, percent, 100);
    if (this->m_percentComplete != percent)
    {
        this->m_percentComplete = percent;
        locker.unlock();
        emit this->progressChanged(percent);
    }
}

// State management
AsyncOperation::OperationState AsyncOperation::state() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_state;
}

bool AsyncOperation::isRunning() const
{
    return this->state() == Running;
}

bool AsyncOperation::isCompleted() const
{
    return this->state() == Completed;
}

bool AsyncOperation::isCancelled() const
{
    return this->state() == Cancelled;
}

bool AsyncOperation::isFailed() const
{
    return this->state() == Failed;
}

// Error handling
QString AsyncOperation::errorMessage() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_errorMessage;
}

QString AsyncOperation::shortErrorMessage() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_shortErrorMessage;
}

QStringList AsyncOperation::errorDetails() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_errorDetails;
}

bool AsyncOperation::hasError() const
{
    QMutexLocker locker(&this->m_mutex);
    return !this->m_errorMessage.isEmpty();
}

// Cancellation
bool AsyncOperation::canCancel() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_canCancel && this->isRunning();
}

void AsyncOperation::setCanCancel(bool canCancel)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_canCancel = canCancel;
}

// Result
QString AsyncOperation::result() const
{
    QMutexLocker locker(&this->m_mutex);
    if (this->hasError())
    {
        return QString();
    }
    return this->m_result;
}

void AsyncOperation::setResult(const QString& result)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_result = result;
}

// Timing
QDateTime AsyncOperation::startTime() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_startTime;
}

QDateTime AsyncOperation::endTime() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_endTime;
}

qint64 AsyncOperation::elapsedTime() const
{
    QMutexLocker locker(&this->m_mutex);
    if (!this->m_startTime.isValid())
    {
        return 0;
    }

    QDateTime end = this->m_endTime.isValid() ? this->m_endTime : QDateTime::currentDateTime();
    return this->m_startTime.msecsTo(end);
}

// RBAC support
QStringList AsyncOperation::apiMethodsToRoleCheck() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_apiMethodsToRoleCheck;
}

void AsyncOperation::addApiMethodToRoleCheck(const QString& method)
{
    QMutexLocker locker(&this->m_mutex);
    if (!this->m_apiMethodsToRoleCheck.contains(method))
    {
        this->m_apiMethodsToRoleCheck.append(method);
    }
}

// Task management
QString AsyncOperation::relatedTaskRef() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_relatedTaskRef;
}

void AsyncOperation::setRelatedTaskRef(const QString& taskRef)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_relatedTaskRef = taskRef;
}

QString AsyncOperation::operationUuid() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_operationUuid;
}

void AsyncOperation::setOperationUuid(const QString& uuid)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_operationUuid = uuid;
}

Pool* AsyncOperation::pool() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_pool;
}

void AsyncOperation::setPool(Pool* pool)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_pool = pool;
    if (pool)
    {
        // Inline add to m_appliesTo - already have mutex lock
        QString ref = pool->opaqueRef();
        if (!ref.isEmpty() && !this->m_appliesTo.contains(ref))
        {
            this->m_appliesTo.append(ref);
        }
    }
}

Host* AsyncOperation::host() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_host;
}

void AsyncOperation::setHost(Host* host)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_host = host;
    if (host)
    {
        // Inline add to m_appliesTo - already have mutex lock
        QString ref = host->opaqueRef();
        if (!ref.isEmpty() && !this->m_appliesTo.contains(ref))
        {
            this->m_appliesTo.append(ref);
        }

        // Set pool if in pool-of-one (matches C# ActionBase.Host setter)
        // TODO: Check if host is in pool-of-one and set pool accordingly
    }
}

VM* AsyncOperation::vm() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_vm;
}

void AsyncOperation::setVM(VM* vm)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_vm = vm;
    if (vm)
    {
        QString ref;
        // If this is a snapshot, add the parent VM instead (matches C# ActionBase.VM setter)
        if (vm->isSnapshot())
        {
            QString parentRef = vm->snapshotOfRef();
            if (!parentRef.isEmpty())
            {
                ref = parentRef;
            } else
            {
                ref = vm->opaqueRef();
            }
        } else
        {
            ref = vm->opaqueRef();
        }

        // Inline add to m_appliesTo - already have mutex lock
        if (!ref.isEmpty() && !this->m_appliesTo.contains(ref))
        {
            this->m_appliesTo.append(ref);
        }

        // Add home host if available
        QString homeRef = vm->homeRef();
        if (!homeRef.isEmpty() && !this->m_appliesTo.contains(homeRef))
        {
            this->m_appliesTo.append(homeRef);
        }
    }
}

SR* AsyncOperation::sr() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_sr;
}

void AsyncOperation::setSR(SR* sr)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_sr = sr;
    if (sr)
    {
        // Inline add to m_appliesTo - already have mutex lock
        QString ref = sr->opaqueRef();
        if (!ref.isEmpty() && !this->m_appliesTo.contains(ref))
        {
            this->m_appliesTo.append(ref);
        }

        // Add home host if available and host is null (matches C# ActionBase.SR setter)
        if (!this->m_host)
        {
            QString homeRef = sr->homeRef();
            if (!homeRef.isEmpty() && !this->m_appliesTo.contains(homeRef))
            {
                this->m_appliesTo.append(homeRef);
            }
        }
    }
}

VM* AsyncOperation::vmTemplate() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_vmTemplate;
}

void AsyncOperation::setVMTemplate(VM* vmTemplate)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_vmTemplate = vmTemplate;
    if (vmTemplate)
    {
        // Inline add to m_appliesTo - already have mutex lock
        QString ref = vmTemplate->opaqueRef();
        if (!ref.isEmpty() && !this->m_appliesTo.contains(ref))
        {
            this->m_appliesTo.append(ref);
        }
    }
}

// AppliesTo list management
QStringList AsyncOperation::appliesTo() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_appliesTo;
}

void AsyncOperation::addAppliesTo(const QString& opaqueRef)
{
    QMutexLocker locker(&this->m_mutex);
    if (!opaqueRef.isEmpty() && !this->m_appliesTo.contains(opaqueRef))
    {
        this->m_appliesTo.append(opaqueRef);
    }
}

void AsyncOperation::clearAppliesTo()
{
    QMutexLocker locker(&this->m_mutex);
    this->m_appliesTo.clear();
}

// History suppression
bool AsyncOperation::suppressHistory() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_suppressHistory;
}

void AsyncOperation::setSuppressHistory(bool suppress)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_suppressHistory = suppress;
}

// Safe exit flag
bool AsyncOperation::safeToExit() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_safeToExit;
}

void AsyncOperation::setSafeToExit(bool safe)
{
    QMutexLocker locker(&this->m_mutex);
    this->m_safeToExit = safe;
}

// Execution control
void AsyncOperation::runAsync()
{
    QMutexLocker locker(&this->m_mutex);

    if (this->m_state != NotStarted)
    {
        qWarning() << "AsyncOperation: Cannot start operation that is already running or completed";
        return;
    }

    this->m_syncExecution = false;
    this->setState(Running);
    this->m_startTime = QDateTime::currentDateTime();
    locker.unlock();

    emit this->started();

    // Run on QThreadPool (like C# ThreadPool.QueueUserWorkItem)
    //
    // IMPORTANT: This ensures run() executes on a background thread, NOT the UI thread.
    // The action thread will block when calling XenAPI methods (via sendRequest()),
    // but this is EFFICIENT blocking using QWaitCondition - the thread sleeps (zero CPU)
    // while ConnectionWorker handles network I/O on a separate thread.
    //
    // See docs/ASYNCOPERATION_THREADING.md for full architecture explanation.
    QThreadPool::globalInstance()->start([this]() {
        this->runOnWorkerThread();
    });
}

void AsyncOperation::runOnWorkerThread()
{
    qDebug() << "[AsyncOperation] Starting runOnWorkerThread for:" << m_title;

    try
    {
        if (!this->m_session)
        {
            qDebug() << "[AsyncOperation] No session, creating new session...";
            this->m_session = this->createSession();
            if (!this->m_session)
            {
                qCritical() << "[AsyncOperation] Failed to create session!";
                this->setError("Unable to acquire XenAPI session");
                this->setState(Failed);
                this->auditLogFailure(this->m_errorMessage);
                return;
            }
            qDebug() << "[AsyncOperation] Session created successfully";
        }

        qDebug() << "[AsyncOperation] Calling run() for:" << this->m_title;
        this->run();
        qDebug() << "[AsyncOperation] run() completed for:" << this->m_title;

        QMutexLocker locker(&this->m_mutex);
        if (this->m_state == Running)
        {
            qDebug() << "[AsyncOperation] Setting state to Completed for:" << m_title;
            this->setState(Completed);
            this->auditLogSuccess();
        } else
        {
            qWarning() << "[AsyncOperation] State is not Running after run():" << m_state << "for:" << m_title;
        }
        this->m_endTime = QDateTime::currentDateTime();
    } catch (const std::exception& e)
    {
        qCritical() << "[AsyncOperation] Exception caught in runOnWorkerThread for:" << m_title << ":" << e.what();
        this->setError(QString::fromLocal8Bit(e.what()));
        this->setState(Failed);
        this->auditLogFailure(QString::fromLocal8Bit(e.what()));
        QMutexLocker locker(&this->m_mutex);
        this->m_endTime = QDateTime::currentDateTime();
    }

    // Clean up session if we created it
    if (this->m_ownsSession)
    {
        qDebug() << "[AsyncOperation] Cleaning up owned session for:" << m_title;
        this->destroySession();
    }

    qDebug() << "[AsyncOperation] runOnWorkerThread completed for:" << m_title;
}

void AsyncOperation::runSync(XenSession* session)
{
    QMutexLocker locker(&this->m_mutex);

    if (this->m_state != NotStarted)
    {
        qWarning() << "AsyncOperation: Cannot start operation that is already running or completed";
        return;
    }

    if (session)
    {
        this->m_session = session;
        this->m_ownsSession = false;
    }

    this->m_syncExecution = true;
    this->setState(Running);
    this->m_startTime = QDateTime::currentDateTime();
    locker.unlock();

    emit this->started();

    try
    {
        if (!this->m_session)
        {
            this->m_session = this->createSession();
            if (!this->m_session)
            {
                this->setError("Unable to acquire XenAPI session");
                this->setState(Failed);
                this->auditLogFailure(this->m_errorMessage);
                return;
            }
        }

        this->run();

        locker.relock();
        if (this->m_state == Running)
        {
            this->setState(Completed);
            this->auditLogSuccess();
        }
    } catch (const std::exception& e)
    {
        this->setError(QString::fromLocal8Bit(e.what()));
        this->setState(Failed);
        this->auditLogFailure(QString::fromLocal8Bit(e.what()));
    }

    //locker.relock();
    this->m_endTime = QDateTime::currentDateTime();
    locker.unlock();

    // Clean up session if we created it
    if (this->m_ownsSession)
    {
        this->destroySession();
    }
}

void AsyncOperation::cancel()
{
    QMutexLocker locker(&this->m_mutex);

    if (!this->m_canCancel || this->m_state != Running)
    {
        return;
    }

    locker.unlock();

    // Cancel related XenAPI task first (matches C# CancellingAction.Cancel)
    cancelRelatedTask();

    // Call subclass-specific cancellation logic
    this->onCancel();
    this->setState(Cancelled);
    this->auditLogCancelled();
}

// Protected helper methods
void AsyncOperation::setState(OperationState newState)
{
    QMutexLocker locker(&this->m_mutex);
    if (this->m_state != newState)
    {
        this->m_state = newState;
        locker.unlock();

        emit this->stateChanged(newState);

        switch (newState)
        {
        case Running:
            emit this->started();
            break;
        case Completed:
            emit this->completed();
            break;
        case Cancelled:
            emit this->cancelled();
            break;
        case Failed:
            emit this->failed(this->m_errorMessage);
            break;
        default:
            break;
        }
    }
}

void AsyncOperation::setError(const QString& message, const QStringList& details)
{
    QString resolvedMessage = message;
    QString resolvedShort;

    if (!details.isEmpty())
    {
        Failure failure(details);
        QString friendly = failure.message();
        if (!friendly.isEmpty())
            resolvedMessage = friendly;
        resolvedShort = failure.shortMessage();
    }

    if (resolvedMessage.isEmpty())
        resolvedMessage = message;

    QMutexLocker locker(&this->m_mutex);
    this->m_errorMessage = resolvedMessage;
    this->m_shortErrorMessage = resolvedShort;
    this->m_errorDetails = details;

    // Mark the action as failed when an error is set
    // This matches C# behavior where Exception property determines success/failure
    if (this->m_state == Running)
    {
        this->setState(Failed);
    }
}

void AsyncOperation::clearError()
{
    QMutexLocker locker(&this->m_mutex);
    this->m_errorMessage.clear();
    this->m_shortErrorMessage.clear();
    this->m_errorDetails.clear();
}

// Session management
XenSession* AsyncOperation::createSession()
{
    if (!this->m_connection)
    {
        return nullptr;
    }

    XenSession* baseSession = this->m_connection->getSession();
    if (!baseSession || !baseSession->isLoggedIn())
    {
        qWarning() << "AsyncOperation::createSession: Base session unavailable";
        return nullptr;
    }

    // Don't pass 'this' as parent - session will be created in worker thread
    // We manage lifetime manually via destroySession()
    XenSession* duplicate = XenSession::duplicateSession(baseSession, nullptr);
    if (!duplicate)
    {
        qWarning() << "AsyncOperation::createSession: Failed to duplicate session";
        return nullptr;
    }

    this->m_ownsSession = true;
    return duplicate;
}

void AsyncOperation::destroySession()
{
    if (this->m_session && this->m_ownsSession)
    {
        this->m_session->logout();
        // TODO investigate why this sometimes instantly becomes nullptr
        if (this->m_session)
        {
            this->m_session->deleteLater();
            this->m_session = nullptr;
        }
        this->m_ownsSession = false;
    }
}

// Task polling
void AsyncOperation::pollToCompletion(const QString& taskRef, double start, double finish, bool suppressFailures)
{
    // Matches C# AsyncAction.PollToCompletion()
    // Null or empty task ref can happen during RBAC dry-run
    if (taskRef.isEmpty())
    {
        qDebug() << "AsyncOperation::pollToCompletion: Empty task reference (RBAC dry-run?)";
        return;
    }

    this->setRelatedTaskRef(taskRef);

    // Tag task with our UUID for rehydration after reconnect
    tagTaskWithUuid(taskRef);

    QDateTime startTime = QDateTime::currentDateTime();
    int lastDebug = 0;
    qInfo() << "Started polling task" << taskRef;
    qDebug() << "Polling for action:" << description();

    try
    {
        while (!QThread::currentThread()->isInterruptionRequested())
        {
            // Log progress every 30 seconds
            qint64 elapsedSecs = startTime.secsTo(QDateTime::currentDateTime());
            int currDebug = static_cast<int>(elapsedSecs / 30);
            if (currDebug > lastDebug)
            {
                lastDebug = currDebug;
                qDebug() << "Polling for action:" << description();
            }

            try
            {
                if (this->pollTask(taskRef, start, finish))
                {
                    break; // Task completed
                }
            } catch (...)
            {
                if (suppressFailures)
                {
                    qDebug() << "Task polling failed but suppressFailures=true, breaking";
                    break;
                }
                throw; // Re-throw if not suppressing failures
            }

            QThread::msleep(this->TASK_POLL_INTERVAL_MS);
        }
    } catch (...)
    {
        // Make sure we clean up task even if polling throws
        this->destroyTask();
        throw; // Re-throw after cleanup
    }

    // Always destroy task when polling completes (matches C# finally block)
    this->destroyTask();
}

bool AsyncOperation::pollTask(const QString& taskRef, double start, double finish)
{
    // Matches C# AsyncAction.Poll()
    if (taskRef.isEmpty())
    {
        qWarning() << "AsyncOperation::pollTask: Empty task reference";
        return true;
    }

    XenSession* session = this->session();
    if (!session || !session->isLoggedIn())
    {
        setError("Not connected to XenServer", QStringList());
        return true;
    }

    // Create XenAPI instance to call task methods
    XenRpcAPI api(session);

    // Get task record
    QVariantMap taskRecord;
    try
    {
        QVariant recordVariant = api.getTaskRecord(taskRef);
        taskRecord = recordVariant.toMap();
    } catch (...)
    {
        // Check if this is a HANDLE_INVALID error (task finished and destroyed)
        // This matches C# catching Failure.HANDLE_INVALID
        qWarning() << "Invalid task handle" << taskRef << "- task is finished";
        setPercentComplete(static_cast<int>(finish));
        return true;
    }

    if (taskRecord.isEmpty())
    {
        // Task not found - might have been destroyed already
        qDebug() << "AsyncOperation::pollTask: Task" << taskRef << "not found (might be complete)";
        setPercentComplete(static_cast<int>(finish));
        return true;
    }

    // Get task progress and status
    double taskProgress = taskRecord.value("progress", 0.0).toDouble();
    QString status = taskRecord.value("status", "pending").toString();

    // Update percent complete
    int currentPercent = static_cast<int>(start + taskProgress * (finish - start));
    setPercentComplete(currentPercent);

    // Check task status
    if (status == "success")
    {
        qDebug() << "AsyncOperation::pollTask: Task" << taskRef << "completed successfully";
        setPercentComplete(static_cast<int>(finish));

        // Get result if available
        QVariant resultVariant = taskRecord.value("result");
        if (!resultVariant.isNull())
        {
            QString resultStr = resultVariant.toString();

            // Task result may contain XML-wrapped values like "<value>OpaqueRef:...</value>"
            // Strip XML tags to get the actual reference (matches C# behavior)
            if (resultStr.contains("<value>") && resultStr.contains("</value>"))
            {
                int startIdx = resultStr.indexOf("<value>") + 7; // length of "<value>"
                int endIdx = resultStr.indexOf("</value>");
                if (startIdx >= 7 && endIdx > startIdx)
                {
                    resultStr = resultStr.mid(startIdx, endIdx - startIdx);
                }
            }

            setResult(resultStr);
        }

        return true;
    } else if (status == "failure")
    {
        qWarning() << "AsyncOperation::pollTask: Task" << taskRef << "failed";

        // Get error info from task record
        QStringList errorInfo;
        QVariantList errorList = taskRecord.value("error_info").toList();
        for (const QVariant& error : errorList)
        {
            errorInfo.append(error.toString());
        }

        QString errorMsg = errorInfo.isEmpty() ? "Unknown error" : errorInfo.first();
        setError(errorMsg, errorInfo);
        return true;
    } else if (status == "cancelled")
    {
        qDebug() << "AsyncOperation::pollTask: Task" << taskRef << "was cancelled";
        setState(Cancelled);
        return true;
    } else if (status == "pending")
    {
        // Task still running
        return false;
    } else
    {
        qWarning() << "AsyncOperation::pollTask: Unknown task status:" << status;
        return false;
    }
}

void AsyncOperation::destroyTask()
{
    // Matches C# AsyncAction.DestroyTask()
    // Null or empty task ref can happen during RBAC dry-run
    QString taskRef = relatedTaskRef();

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn() || taskRef.isEmpty())
    {
        // Clear task ref even if we can't destroy it
        if (!taskRef.isEmpty())
        {
            this->setRelatedTaskRef(QString());
        }
        return;
    }

    try
    {
        // Remove our UUID from task before destroying
        removeUuidFromTask(taskRef);

        // Destroy the task on the server
        XenRpcAPI api(sess);
        bool destroyed = api.destroyTask(taskRef);

        if (destroyed)
        {
            qDebug() << "Successfully destroyed task" << taskRef;
        } else
        {
            qDebug() << "Failed to destroy task" << taskRef << "(might already be destroyed)";
        }
    } catch (...)
    {
        // Task might already be destroyed or HANDLE_INVALID - not critical
        qDebug() << "Exception destroying task" << taskRef << "(task might be invalid)";
    }

    // Always clear task reference
    this->setRelatedTaskRef(QString());
}

void AsyncOperation::cancelRelatedTask()
{
    // Matches C# CancellingAction.CancelRelatedTask()
    // Creates new session to cancel task since main session may be in use by worker thread

    QString taskRef = relatedTaskRef();
    if (taskRef.isEmpty())
    {
        return;
    }

    XenConnection* conn = connection();
    if (!conn || !conn->isConnected())
    {
        qDebug() << "AsyncOperation::cancelRelatedTask: No connection available";
        return;
    }

    // Get current session - if we don't have one, we can't cancel
    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
    {
        qDebug() << "AsyncOperation::cancelRelatedTask: No session available";
        return;
    }

    try
    {
        XenRpcAPI api(sess);
        bool cancelled = api.cancelTask(taskRef);

        if (cancelled)
        {
            qDebug() << "Successfully cancelled task" << taskRef;
        } else
        {
            qDebug() << "Failed to cancel task" << taskRef;
        }
    } catch (...)
    {
        // Task might already be finished or invalid - not critical
        qDebug() << "Exception cancelling task" << taskRef << "(task might be finished)";
    }
}

void AsyncOperation::tagTaskWithUuid(const QString& taskRef)
{
    if (taskRef.isEmpty() || m_operationUuid.isEmpty())
        return;

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
        return;

    XenRpcAPI api(sess);

    try
    {
        // Remove old UUID if exists
        api.removeFromTaskOtherConfig(taskRef, "XenAdminQtUUID");

        // Add our UUID to task.other_config
        api.addToTaskOtherConfig(taskRef, "XenAdminQtUUID", m_operationUuid);

        qDebug() << "Tagged task" << taskRef << "with UUID" << m_operationUuid;
    } catch (...)
    {
        // RBAC permission denied - read-only user can't modify other_config
        // Just ignore - not critical
        qDebug() << "Could not tag task with UUID (permission denied)";
    }
}

void AsyncOperation::removeUuidFromTask(const QString& taskRef)
{
    if (taskRef.isEmpty())
        return;

    XenSession* sess = session();
    if (!sess || !sess->isLoggedIn())
        return;

    XenRpcAPI api(sess);

    try
    {
        api.removeFromTaskOtherConfig(taskRef, "XenAdminQtUUID");
        qDebug() << "Removed UUID from task" << taskRef;
    } catch (...)
    {
        // Permission denied or task already destroyed - ignore
    }
}

// Audit logging
void AsyncOperation::auditLogSuccess()
{
    qDebug() << "Operation completed successfully:" << this->m_title;
}

void AsyncOperation::auditLogCancelled()
{
    qDebug() << "Operation cancelled:" << this->m_title;
}

void AsyncOperation::auditLogFailure(const QString& error)
{
    qWarning() << "Operation failed:" << this->m_title << "Error:" << error;
}

// Thread-safe property setters
void AsyncOperation::setTitleSafe(const QString& title)
{
    // Use QMetaObject::invokeMethod to safely call setTitle from any thread
    QMetaObject::invokeMethod(this, "setTitle", Qt::QueuedConnection,
                              Q_ARG(QString, title));
}

void AsyncOperation::setDescriptionSafe(const QString& description)
{
    QMetaObject::invokeMethod(this, "setDescription", Qt::QueuedConnection,
                              Q_ARG(QString, description));
}

void AsyncOperation::setPercentCompleteSafe(int percent)
{
    QMetaObject::invokeMethod(this, "setPercentComplete", Qt::QueuedConnection,
                              Q_ARG(int, percent));
}

// AppliesTo helper - automatically adds object refs (matches C# SetAppliesTo)
void AsyncOperation::setAppliesToFromObject(QObject* xenObject)
{
    if (!xenObject)
        return;

    // Check object type and call appropriate setter
    if (Pool* pool = qobject_cast<Pool*>(xenObject))
    {
        setPool(pool);
    } else if (Host* host = qobject_cast<Host*>(xenObject))
    {
        setHost(host);
    } else if (VM* vm = qobject_cast<VM*>(xenObject))
    {
        // Check if it's a template
        if (vm->isTemplate())
        {
            setVMTemplate(vm);
        } else
        {
            setVM(vm);
        }
    } else if (SR* sr = qobject_cast<SR*>(xenObject))
    {
        setSR(sr);
    } else
    {
        qWarning() << "AsyncOperation::setAppliesToFromObject: Unknown object type"
                   << xenObject->metaObject()->className();
    }
}

// Cleanup for shutdown/reconnect - removes UUID from task.other_config
// Matches C# AsyncAction.PrepareForEventReloadAfterRestart()
void AsyncOperation::prepareForEventReloadAfterRestart()
{
    if (!m_relatedTaskRef.isEmpty() && !m_operationUuid.isEmpty())
    {
        qDebug() << "AsyncOperation::prepareForEventReloadAfterRestart: Removing UUID from task" << m_relatedTaskRef;
        removeUuidFromTask(m_relatedTaskRef);
    }
}

// Private worker thread management - simplified
void AsyncOperation::startWorkerThread()
{
    // TODO
}

void AsyncOperation::waitForWorkerCompletion()
{
    // TODO
}
