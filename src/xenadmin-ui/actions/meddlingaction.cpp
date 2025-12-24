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

#include "meddlingaction.h"
#include <QDebug>
#include <xen/api.h>
#include <xen/network/connection.h>
#include <xen/session.h>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>

MeddlingAction::MeddlingAction(const QString& taskRef,
                               XenConnection* connection,
                               bool isOurTask,
                               QObject* parent)
    : AsyncOperation(connection, "Task", QString(), parent), m_isOurTask(isOurTask)
{
    this->setRelatedTaskRef(taskRef);
    this->setSuppressHistory(false); // Meddling operations appear in history
    this->setCanCancel(isOurTask);   // Can only cancel our own tasks
    this->setSafeToExit(true);       // Safe to exit - we're just monitoring

    // Mark as already running since we're monitoring existing task
    this->setState(Running);
    this->setPercentComplete(0);
}

MeddlingAction::~MeddlingAction()
{
}

void MeddlingAction::run()
{
    // MeddlingAction doesn't create tasks, it monitors existing ones
    // Poll the task that was passed to constructor
    QString taskRef = this->relatedTaskRef();

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
        qWarning() << "Cannot cancel task that doesn't belong to us:" << this->relatedTaskRef();
        return;
    }

    // Cancel the XenAPI task
    XenSession* sess = this->session();
    if (!sess || !sess->IsLoggedIn())
    {
        qWarning() << "Cannot cancel task - no valid session";
        return;
    }

    XenRpcAPI api(sess);
    QString taskRef = this->relatedTaskRef();

    if (!taskRef.isEmpty())
    {
        qDebug() << "Cancelling meddling task:" << taskRef;
        api.cancelTask(taskRef);
    }
}

void MeddlingAction::updateFromTask(const QVariantMap& taskData, bool taskDeleting)
{
    if (taskData.isEmpty())
        return;

    // Update title/description from task
    this->updateTitleFromTask(taskData);

    // Update progress
    if (taskData.contains("progress"))
    {
        double progress = taskData.value("progress").toDouble();
        this->setPercentComplete(static_cast<int>(progress * 100));
    }

    // Update state
    QString status = taskData.value("status").toString();

    if (taskDeleting || status == "success")
    {
        this->setPercentComplete(100);
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
    } else if (status == "cancelled")
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
        this->setTitle(nameLabel);
    }

    if (!nameDescription.isEmpty())
    {
        this->setDescription(nameDescription);
    }

    // Try to enhance title with VM operation if available
    QString vmOp = this->getVmOperation(taskData);
    if (!vmOp.isEmpty())
    {
        QString enhancedTitle = this->getVmOperationTitle(vmOp);
        if (!enhancedTitle.isEmpty())
        {
            this->setTitle(enhancedTitle);
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

bool MeddlingAction::isTaskUnwanted(const QVariantMap& taskData, const QString& ourUuid)
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
    if (!subtaskOf.isEmpty() && subtaskOf != "OpaqueRef:NULL")
    {
        return true;
    }

    // Check if this is a recognized VM operation
    QString vmOp = otherConfig.value("vm_operation").toString();
    if (vmOp == "unknown" || vmOp.isEmpty())
    {
        // Not a recognized operation - might be internal task
        // For now, allow it (C# has more complex logic here)
        // TODO: Add full operation recognition logic
    }

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
    QDateTime created = taskData.value("created").toDateTime();
    if (!created.isValid())
        return false;

    // Apply server time offset
    QDateTime createdLocal = created.addMSecs(serverTimeOffset);
    QDateTime now = QDateTime::currentDateTime();

    qint64 ageMs = createdLocal.msecsTo(now);

    // If task is older than heuristic window, assume non-aware client
    return ageMs >= AWARE_CLIENT_HEURISTIC_MS;
}
