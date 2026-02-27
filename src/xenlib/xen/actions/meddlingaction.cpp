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

#include <QtCore/QDateTime>
#include <QDebug>
#include "meddlingaction.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/api.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/utils/misc.h"

namespace
{
bool taskFlowDebugEnabled()
{
    // TODO(petr): remove temporary always-on task flow diagnostics after Linux stuck-task issue is resolved.
    return true;
}

QDateTime parseTaskDateTime(const QVariant& value)
{
    const QDateTime direct = value.toDateTime();
    if (direct.isValid())
        return direct.toUTC();

    return Misc::ParseXenDateTime(value.toString());
}
}

MeddlingAction::MeddlingAction(const QString& taskRef, XenConnection* connection, bool isOurTask, QObject* parent) : AsyncOperation(connection, "Task", QString(), false, parent), m_isOurTask(isOurTask)
{
    this->SetRelatedTaskRef(taskRef);
    this->SetCanCancel(isOurTask);   // Can only cancel our own tasks
    this->SetSafeToExit(true);       // Safe to exit - we're just monitoring

    // Mark as already running since we're monitoring existing task
    this->setState(Running);
    this->SetPercentComplete(0);
}

MeddlingAction::~MeddlingAction()
{
}

void MeddlingAction::run()
{
    // MeddlingAction doesn't create tasks, it monitors existing ones
    // Poll the task that was passed to constructor
    QString taskRef = this->GetRelatedTaskRef();

    if (taskRef.isEmpty())
    {
        this->setError("No task reference provided for meddling operation");
        return;
    }

    // Poll until completion (or cancellation)
    this->pollToCompletion(taskRef, 0, 100);
}

void MeddlingAction::onCancel()
{
    if (!this->m_isOurTask)
    {
        qWarning() << "Cannot cancel task that doesn't belong to us:" << this->GetRelatedTaskRef();
        return;
    }

    // Cancel the XenAPI task
    XenAPI::Session* sess = this->GetSession();
    if (!sess || !sess->IsLoggedIn())
    {
        qWarning() << "Cannot cancel task - no valid session";
        return;
    }

    XenRpcAPI api(sess);
    QString taskRef = this->GetRelatedTaskRef();

    if (!taskRef.isEmpty())
    {
        qDebug() << "Cancelling meddling task:" << taskRef;
        api.CancelTask(taskRef);
    }
}

void MeddlingAction::updateFromTask(const QVariantMap& taskData, bool taskDeleting)
{
    // When the task object is deleted from server, EventPoller only provides taskRef,
    // so manager calls us with empty taskData + taskDeleting=true.
    // In that case we must still transition to Completed.
    if (taskData.isEmpty())
    {
        if (taskDeleting)
        {
            this->SetPercentComplete(100);
            this->setState(Completed);
        }
        return;
    }

    // Update title/description from task
    this->updateTitleFromTask(taskData);
    this->m_isRecognizedOperation = MeddlingAction::isRecognizedOperation(taskData);

    // Update progress
    if (taskData.contains("progress"))
    {
        double progress = taskData.value("progress").toDouble();
        this->SetPercentComplete(static_cast<int>(progress * 100));
    }

    // Update state
    const QString status = taskData.value("status").toString().trimmed().toLower();
    const QDateTime finished = parseTaskDateTime(taskData.value("finished"));
    const bool hasFinishedTimestamp = finished.isValid();

    if (taskFlowDebugEnabled())
    {
        qDebug().noquote()
            << QString("[TaskFlow][Action] ref=%1 deleting=%2 status=%3 finishedRaw=%4 finishedValid=%5 progress=%6")
                   .arg(this->GetRelatedTaskRef(),
                        taskDeleting ? "true" : "false",
                        status,
                        taskData.value("finished").toString(),
                        hasFinishedTimestamp ? "true" : "false",
                        taskData.value("progress").toString());
    }

    if (taskDeleting || status == "success" || (hasFinishedTimestamp && status != "failure" && status != "cancelled" && status != "cancelling"))
    {
        this->SetPercentComplete(100);
        this->setState(Completed);
    } else if (status == "failure")
    {
        QVariantList errorInfo = taskData.value("error_info").toList();
        QStringList errors;
        for (const QVariant& v : errorInfo)
            errors.append(v.toString());

        QString errorMsg = errors.isEmpty() ? "Unknown error" : errors.first();
        this->setError(errorMsg, errors);
        this->setState(Failed);
    } else if (status == "cancelled" || status == "cancelling")
    {
        this->setState(Cancelled);
    }
    // else status == "pending" - keep running
}

void MeddlingAction::updateTitleFromTask(const QVariantMap& taskData)
{
    QString nameLabel = taskData.value("name_label").toString();
    QString nameDescription = taskData.value("name_description").toString();

    if (!nameLabel.isEmpty())
    {
        this->SetTitle(nameLabel);
    }

    if (!nameDescription.isEmpty())
    {
        this->SetDescription(nameDescription);
    }

    // Try to enhance title with VM operation if available
    QString vmOp = this->getVmOperation(taskData);
    if (!vmOp.isEmpty())
    {
        QString enhancedTitle = this->getVmOperationTitle(vmOp);
        if (!enhancedTitle.isEmpty())
        {
            this->SetTitle(enhancedTitle);
        }
    }
}

QString MeddlingAction::getVmOperation(const QVariantMap& taskData) const
{
    QVariantMap otherConfig = taskData.value("other_config").toMap();
    return otherConfig.value("vm_operation").toString();
}

QString MeddlingAction::getVmOperationTitle(const QString& operation) const
{
    // Map VM operations to human-readable titles
    static const QHash<QString, QString> operationTitles = {
        {"clean_reboot", "Rebooting VM"},
        {"clean_shutdown", "Shutting down VM"},
        {"clone", "Cloning VM"},
        {"hard_reboot", "Force rebooting VM"},
        {"hard_shutdown", "Force shutting down VM"},
        {"migrate_send", "Migrating VM"},
        {"pool_migrate", "Migrating VM"},
        {"resume", "Resuming VM"},
        {"resume_on", "Resuming VM"},
        {"start", "Starting VM"},
        {"start_on", "Starting VM"},
        {"suspend", "Suspending VM"},
        {"checkpoint", "Checkpointing VM"},
        {"snapshot", "Snapshotting VM"},
        {"export", "Exporting VM"},
        {"import", "Importing VM"}};

    return operationTitles.value(operation, QString());
}

bool MeddlingAction::isTaskUnwanted(const QVariantMap& taskData, const QString& ourUuid, bool showAllServerEvents)
{
    // Check if this is our task (already have an AsyncOperation for it)
    QVariantMap otherConfig = taskData.value("other_config").toMap();
    QString taskUuid = otherConfig.value("XenAdminQtUUID").toString();

    if (taskUuid == ourUuid && !ourUuid.isEmpty())
    {
        return true; // Our own task - we already have the real AsyncOperation
    }

    // Check if this is a subtask (we monitor parent task instead)
    QString subtaskOf = taskData.value("subtask_of").toString();
    if (!subtaskOf.isEmpty() && subtaskOf != XENOBJECT_NULL)
    {
        return true;
    }

    // C# parity: hide unrecognized server tasks by default.
    if (!showAllServerEvents && !MeddlingAction::isRecognizedOperation(taskData))
        return true;

    return false;
}

bool MeddlingAction::isRecognizedOperation(const QVariantMap& taskData)
{
    QVariantMap otherConfig = taskData.value("other_config").toMap();
    QString vmOp = otherConfig.value("vm_operation").toString();
    if (vmOp.isEmpty())
        vmOp = otherConfig.value("vm-operation").toString();

    // Match C# RecognisedVmOperations list.
    static const QSet<QString> recognized = {
        "clean_reboot",
        "clean_shutdown",
        "clone",
        "hard_reboot",
        "hard_shutdown",
        "migrate_send",
        "pool_migrate",
        "resume",
        "resume_on",
        "start",
        "start_on",
        "suspend",
        "checkpoint",
        "snapshot",
        "export",
        "import"
    };

    if (recognized.contains(vmOp))
        return true;

    // C# fallback: infer from task.name_label when other_config doesn't provide vm_operation.
    QString nameLabel = taskData.value("name_label").toString();
    QString normalized = nameLabel;
    if (normalized.startsWith("Async."))
        normalized = normalized.mid(QStringLiteral("Async.").size());

    if (normalized.startsWith("VM."))
    {
        QString op = normalized.mid(QStringLiteral("VM.").size()).trimmed();
        return recognized.contains(op);
    }

    // C# special cases
    if (nameLabel == "VM import" || nameLabel.startsWith("Export of VM: "))
        return true;

    return false;
}

bool MeddlingAction::isTaskSuitable(const QVariantMap& taskData, qint64 serverTimeOffset)
{
    // Check if task has appliesTo set (aware client)
    QVariantMap otherConfig = taskData.value("other_config").toMap();
    QString appliesTo = otherConfig.value("applies_to").toString();

    if (!appliesTo.isEmpty())
    {
        return true; // Aware client - suitable immediately
    }

    // Give clients time to set appliesTo (5 second window)
    QDateTime created = parseTaskDateTime(taskData.value("created"));
    if (!created.isValid())
    {
        if (taskFlowDebugEnabled())
        {
            qDebug().noquote()
                << QString("[TaskFlow][Action] suitable=false reason=invalid-created raw=%1")
                       .arg(taskData.value("created").toString());
        }
        return false;
    }

    // Apply server time offset
    QDateTime createdLocal = created.addMSecs(serverTimeOffset);
    QDateTime now = QDateTime::currentDateTime();

    qint64 ageMs = createdLocal.msecsTo(now);

    // If task is older than heuristic window, assume non-aware client
    const bool suitable = ageMs >= AWARE_CLIENT_HEURISTIC_MS;
    if (taskFlowDebugEnabled())
    {
        qDebug().noquote()
            << QString("[TaskFlow][Action] suitable=%1 createdRaw=%2 ageMs=%3 heuristicMs=%4")
                   .arg(suitable ? "true" : "false",
                        taskData.value("created").toString(),
                        QString::number(ageMs),
                        QString::number(AWARE_CLIENT_HEURISTIC_MS));
    }
    return suitable;
}

bool MeddlingAction::isTaskTerminal(const QVariantMap& taskData)
{
    const QString status = taskData.value("status").toString().trimmed().toLower();
    if (status == "success" || status == "failure" || status == "cancelled" || status == "cancelling")
        return true;

    return parseTaskDateTime(taskData.value("finished")).isValid();
}
