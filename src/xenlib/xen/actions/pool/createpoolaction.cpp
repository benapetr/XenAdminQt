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

#include "createpoolaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../xenapi/xenapi_Task.h"
#include "../../api.h"
#include "../../../xencache.h"
#include <stdexcept>
#include <QDateTime>
#include <QDebug>
#include <QThread>

CreatePoolAction::CreatePoolAction(XenConnection* coordinatorConnection,
                                   Host* coordinator,
                                   const QList<XenConnection*>& memberConnections,
                                   const QList<Host*>& members,
                                   const QString& name,
                                   const QString& description,
                                   QObject* parent)
    : AsyncOperation(coordinatorConnection,
                     QString("Creating pool: %1").arg(name),
                     "Creating new pool",
                     parent),
      m_coordinatorConnection(coordinatorConnection),
      m_coordinator(coordinator),
      m_memberConnections(memberConnections),
      m_members(members),
      m_name(name),
      m_description(description)
{
    // Note: We don't use Host* objects in this simplified version
    // The C# version uses them for licensing and AD checks
}

void CreatePoolAction::run()
{
    try
    {
        this->SetPercentComplete(0);
        this->SetDescription("Creating pool...");

        // Get pool reference from cache
        // There should be one pool reference for a standalone coordinator
        QStringList poolRefs = this->m_coordinatorConnection->GetCache()->GetAllRefs(XenObjectType::Pool);
        if (poolRefs.isEmpty())
        {
            throw std::runtime_error("No pool found on coordinator");
        }
        QString poolRef = poolRefs.first();

        this->SetPercentComplete(10);
        this->SetDescription("Setting pool name and description...");

        // Set pool name and description on the coordinator
        XenAPI::Pool::set_name_label(this->GetSession(), poolRef, this->m_name);
        XenAPI::Pool::set_name_description(this->GetSession(), poolRef, this->m_description);

        this->SetPercentComplete(20);

        // If no members to add, we're done
        if (this->m_members.isEmpty())
        {
            this->SetDescription("Pool created successfully");
            return;
        }

        // Add each member sequentially
        // Progress: 20% done, 80% remaining for members
        double progressPerMember = 80.0 / this->m_members.size();

        auto pollTaskWithSession = [this](XenAPI::Session* session,
                                          const QString& taskRef,
                                          double start,
                                          double finish) -> bool
        {
            if (!session || taskRef.isEmpty())
                return false;

            XenRpcAPI api(session);
            QDateTime startTime = QDateTime::currentDateTime();
            int lastDebug = 0;
            qInfo() << "Started polling task" << taskRef;

            constexpr int kTaskPollIntervalMs = 900;
            while (!QThread::currentThread()->isInterruptionRequested())
            {
                qint64 elapsedSecs = startTime.secsTo(QDateTime::currentDateTime());
                int currDebug = static_cast<int>(elapsedSecs / 30);
                if (currDebug > lastDebug)
                {
                    lastDebug = currDebug;
                    qDebug() << "Polling for action:" << GetDescription();
                }

                QVariantMap taskRecord;
                try
                {
                    QVariant recordVariant = api.GetTaskRecord(taskRef);
                    taskRecord = recordVariant.toMap();
                }
                catch (...)
                {
                    qWarning() << "Invalid task handle" << taskRef << "- task is finished";
                    SetPercentComplete(static_cast<int>(finish));
                    return true;
                }

                if (taskRecord.isEmpty())
                {
                    qDebug() << "CreatePoolAction: Task" << taskRef << "not found (might be complete)";
                    SetPercentComplete(static_cast<int>(finish));
                    return true;
                }

                const double taskProgress = taskRecord.value("progress", 0.0).toDouble();
                const QString status = taskRecord.value("status", "pending").toString();
                const int currentPercent = static_cast<int>(start + taskProgress * (finish - start));
                SetPercentComplete(currentPercent);

                if (status == "success")
                {
                    SetPercentComplete(static_cast<int>(finish));
                    return true;
                }
                if (status == "failure")
                {
                    QStringList errorInfo;
                    const QVariantList errorList = taskRecord.value("error_info").toList();
                    for (const QVariant& error : errorList)
                        errorInfo.append(error.toString());

                    const QString errorMsg = errorInfo.isEmpty() ? "Unknown error" : errorInfo.first();
                    setError(errorMsg, errorInfo);
                    return false;
                }
                if (status == "cancelled")
                {
                    setState(Cancelled);
                    return false;
                }

                QThread::msleep(kTaskPollIntervalMs);
            }

            return false;
        };

        for (int i = 0; i < this->m_members.size(); ++i)
        {
            XenConnection* memberConnection = this->m_memberConnections.at(i);
            // Note: Host* not used in simplified version

            int progressStart = 20 + static_cast<int>(i * progressPerMember);
            int progressEnd = 20 + static_cast<int>((i + 1) * progressPerMember);

            SetDescription(QString("Adding member %1 of %2...").arg(i + 1).arg(m_members.size()));

            // Get coordinator credentials from session
            QString coordinatorAddress = m_coordinatorConnection->GetHostname();

            XenAPI::Session* coordinatorSession = m_coordinatorConnection->GetSession();
            if (!coordinatorSession)
            {
                throw std::runtime_error("Coordinator connection has no session");
            }
            QString username = coordinatorSession->GetUsername();
            QString password = coordinatorSession->GetPassword();

            // Call Pool.async_join from the member's session (matches C# NewSession)
            if (!memberConnection)
                throw std::runtime_error("Member connection is null");

            XenAPI::Session* baseMemberSession = memberConnection->GetSession();
            if (!baseMemberSession || !baseMemberSession->IsLoggedIn())
                throw std::runtime_error("Member connection has no active session");

            XenAPI::Session* memberSession = XenAPI::Session::DuplicateSession(baseMemberSession, nullptr);
            if (!memberSession)
                throw std::runtime_error("Failed to create member session");

            QString taskRef;
            try
            {
                taskRef = XenAPI::Pool::async_join(memberSession,
                                                   coordinatorAddress, username, password);

                if (!pollTaskWithSession(memberSession, taskRef, progressStart, progressEnd))
                    throw std::runtime_error("Pool join task failed");
            }
            catch (...)
            {
                if (!taskRef.isEmpty())
                    XenAPI::Task::Destroy(memberSession, taskRef);
                memberSession->Logout();
                memberSession->deleteLater();
                throw;
            }

            if (!taskRef.isEmpty())
                XenAPI::Task::Destroy(memberSession, taskRef);
            memberSession->Logout();
            memberSession->deleteLater();

            SetDescription(QString("Member %1 of %2 joined successfully").arg(i + 1).arg(m_members.size()));

            // TODO: Drop connection and remove from ConnectionsManager
        }

        SetDescription("Pool created successfully");

    } catch (const std::exception& e)
    {
        if (IsCancelled())
        {
            SetDescription("Pool creation cancelled");
        } else
        {
            setError(QString("Failed to create pool: %1").arg(e.what()));
        }
    }
}
