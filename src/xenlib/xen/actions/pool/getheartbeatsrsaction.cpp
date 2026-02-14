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

#include "getheartbeatsrsaction.h"
#include "../../pool.h"
#include "../../sr.h"
#include "../../failure.h"
#include "../../api.h"
#include "../../jsonrpcclient.h"
#include "../../../xencache.h"
#include "../../xenapi/xenapi_SR.h"
#include <QThread>

namespace
{
QString decodeUnsuitableReason(const QStringList& errorInfo)
{
    if (!errorInfo.isEmpty())
    {
        const QString code = errorInfo.first();
        if (code == QLatin1String(Failure::SR_HAS_NO_PBDS))
            return QObject::tr("SR is detached.");
        if (code == QLatin1String(Failure::RBAC_PERMISSION_DENIED))
            return Failure(errorInfo).message().section('\n', 0, 0).trimmed();

        return Failure(errorInfo).message();
    }

    return QObject::tr("This SR does not support this operation.");
}

QString decodeUnsuitableReason(const QString& rawError)
{
    const QString message = rawError.trimmed();
    if (message.contains("SR_HAS_NO_PBDS", Qt::CaseInsensitive))
        return QObject::tr("SR is detached.");
    if (message.contains("RBAC_PERMISSION_DENIED", Qt::CaseInsensitive))
        return message.section('\n', 0, 0).trimmed();
    if (!message.isEmpty())
        return message;
    return QObject::tr("This SR does not support this operation.");
}

QStringList taskErrorInfo(const QVariantMap& taskRecord)
{
    QStringList errors;
    const QVariantList errorInfo = taskRecord.value("error_info").toList();
    for (const QVariant& part : errorInfo)
        errors.append(part.toString());
    return errors;
}
} // namespace

GetHeartbeatSRsAction::GetHeartbeatSRsAction(QSharedPointer<Pool> pool, QObject* parent)
    : AsyncOperation(pool ? pool->GetConnection() : nullptr,
                     QObject::tr("Scanning Storage Repositories"),
                     QObject::tr("Scanning storage repositories for HA heartbeat suitability"),
                     true,
                     parent),
      m_pool(pool)
{
}

void GetHeartbeatSRsAction::run()
{
    this->SetCanCancel(true);
    this->SetPercentComplete(0);
    this->m_srs.clear();

    XenCache* cache = this->m_pool ? this->m_pool->GetCache() : nullptr;
    if (!cache)
        return;

    const QList<QSharedPointer<SR>> srs = cache->GetAll<SR>(XenObjectType::SR);
    if (srs.isEmpty())
        return;

    const double increment = 100.0 / static_cast<double>(srs.size());
    int index = 0;

    for (const QSharedPointer<SR>& sr : srs)
    {
        if (this->IsCancelled())
            return;

        if (!sr || !sr->IsValid() || !sr->IsShared() || sr->IsToolsSR())
        {
            ++index;
            this->SetPercentComplete(static_cast<int>(index * increment));
            continue;
        }

        SRWrapper wrapper;
        wrapper.sr = sr;

        try
        {
            this->SetDescription(QObject::tr("Checking %1...").arg(sr->GetName()));

            const QString taskRef = XenAPI::SR::async_assert_can_host_ha_statefile(this->GetSession(), sr->OpaqueRef());
            if (taskRef.isEmpty())
            {
                wrapper.enabled = false;
                wrapper.reasonUnsuitable = decodeUnsuitableReason(Xen::JsonRpcClient::lastError());
            } else
            {
                XenRpcAPI api(this->GetSession());
                this->SetRelatedTaskRef(taskRef);

                QVariantMap taskRecord;
                QString status;

                while (!this->IsCancelled())
                {
                    taskRecord = api.GetTaskRecord(taskRef).toMap();
                    if (taskRecord.isEmpty())
                    {
                        wrapper.enabled = false;
                        wrapper.reasonUnsuitable = decodeUnsuitableReason(Xen::JsonRpcClient::lastError());
                        break;
                    }

                    status = taskRecord.value("status").toString();
                    if (status == "success")
                    {
                        wrapper.enabled = true;
                        break;
                    }
                    if (status == "failure")
                    {
                        wrapper.enabled = false;
                        wrapper.reasonUnsuitable = decodeUnsuitableReason(taskErrorInfo(taskRecord));
                        break;
                    }
                    if (status == "cancelled")
                    {
                        wrapper.enabled = false;
                        wrapper.reasonUnsuitable = QObject::tr("Operation was cancelled.");
                        break;
                    }

                    QThread::msleep(250);
                }

                this->destroyTask();
                if (this->IsCancelled())
                    return;
            }
        } catch (const std::exception& ex)
        {
            wrapper.enabled = false;
            wrapper.reasonUnsuitable = decodeUnsuitableReason(QString::fromUtf8(ex.what()));
        }

        this->m_srs.append(wrapper);
        ++index;
        this->SetPercentComplete(static_cast<int>(index * increment));
    }

    this->SetDescription(QObject::tr("Heartbeat SR scan completed"));
    this->SetPercentComplete(100);
}
