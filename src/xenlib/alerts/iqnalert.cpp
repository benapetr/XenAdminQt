/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "iqnalert.h"
#include "alertmanager.h"
#include "../xen/host.h"
#include "../xen/sr.h"
#include "../xen/network/connection.h"
#include "../xencache.h"

using namespace XenLib;

namespace
{
    // C# Host.GetIscsiIqn
    QString iscsiIqn(const QSharedPointer<Host>& host)
    {
        return host ? host->GetOtherConfig().value("iscsi_iqn").toString() : QString();
    }
}

IqnAlert::IqnAlert(const QSharedPointer<Host>& host) : Alert(host ? host->GetConnection() : nullptr), m_host(host)
{
    if (host)
        this->m_hostUuid = host->GetUUID();
}

QString IqnAlert::AppliesTo() const
{
    return this->m_host ? this->m_host->GetName() : QString();
}

void IqnAlert::Dismiss()
{
    // C# Alert.Dismiss for client-side alerts: just remove it
    AlertManager::instance()->RemoveAlert(this);
}

bool IqnAlert::IsIscsiUsed(const QList<XenConnection*>& connections)
{
    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->GetCache())
            continue;
        for (const QSharedPointer<SR>& sr : connection->GetCache()->GetAll<SR>())
        {
            if (sr && sr->GetType() == "lvmoiscsi")
                return true;
        }
    }
    return false;
}

void IqnAlert::PerformCheck(const QList<XenConnection*>& connections)
{
    // C# HelpersGUI.CheckHostIQNsExist and CheckHostIQNsDiffer
    QList<QSharedPointer<Host>> missing;
    QList<QPair<QSharedPointer<Host>, QString>> seen;   // first server with each IQN
    QList<QPair<QSharedPointer<Host>, QSharedPointer<Host>>> duplicates;

    for (XenConnection* connection : connections)
    {
        if (!connection || !connection->GetCache())
            continue;
        for (const QSharedPointer<Host>& host : connection->GetCache()->GetAll<Host>())
        {
            if (!host)
                continue;
            const QString iqn = iscsiIqn(host);
            if (iqn.isEmpty())
            {
                if (host->IsLive())
                    missing.append(host);
                continue;
            }

            QSharedPointer<Host> first;
            for (const auto& entry : seen)
            {
                if (entry.second == iqn)
                {
                    first = entry.first;
                    break;
                }
            }
            if (first)
                duplicates.append(qMakePair(host, first));
            else
                seen.append(qMakePair(host, iqn));
        }
    }

    AlertManager* manager = AlertManager::instance();
    for (const QSharedPointer<Host>& host : missing)
    {
        const QString ref = host->OpaqueRef();
        const bool shown = manager->FindAlert([ref](Alert* a)
        {
            auto* existing = qobject_cast<MissingIqnAlert*>(a);
            return existing && existing->GetHost() && existing->GetHost()->OpaqueRef() == ref;
        });
        if (!shown)
            manager->AddAlert(new MissingIqnAlert(host));
    }

    for (const auto& pair : duplicates)
    {
        const QString ref = pair.first->OpaqueRef();
        const bool shown = manager->FindAlert([ref](Alert* a)
        {
            auto* existing = qobject_cast<DuplicateIqnAlert*>(a);
            return existing && existing->GetHost() && existing->GetHost()->OpaqueRef() == ref;
        });
        if (!shown)
            manager->AddAlert(new DuplicateIqnAlert(pair.first, pair.second));
    }
}

QString MissingIqnAlert::GetTitle() const
{
    return tr("iSCSI-IQN Missing");
}

QString MissingIqnAlert::GetDescription() const
{
    return tr("%1 has no iSCSI-IQN. This could cause problems with iSCSI storage on this network.").arg(this->AppliesTo());
}

DuplicateIqnAlert::DuplicateIqnAlert(const QSharedPointer<Host>& host, const QSharedPointer<Host>& other)
    : IqnAlert(host), m_other(other)
{
}

QString DuplicateIqnAlert::GetTitle() const
{
    return tr("iSCSI-IQN Exists");
}

QString DuplicateIqnAlert::GetDescription() const
{
    return tr("%1 has an iSCSI-IQN (%2) which already exists on %3. This could cause problems with iSCSI storage on this network.")
        .arg(this->AppliesTo(), iscsiIqn(this->m_host), this->m_other ? this->m_other->GetName() : QString());
}
