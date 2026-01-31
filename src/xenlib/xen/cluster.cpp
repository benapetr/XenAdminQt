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

#include "cluster.h"
#include "network/connection.h"
#include "../xencache.h"
#include "clusterhost.h"
#include <QMap>

Cluster::Cluster(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}


QStringList Cluster::GetClusterHostRefs() const
{
    return this->property("cluster_hosts").toStringList();
}

QStringList Cluster::PendingForget() const
{
    return this->property("pending_forget").toStringList();
}

QString Cluster::ClusterToken() const
{
    return this->stringProperty("cluster_token");
}

QString Cluster::ClusterStack() const
{
    return this->stringProperty("cluster_stack");
}

QStringList Cluster::AllowedOperations() const
{
    return this->property("allowed_operations").toStringList();
}

QMap<QString, QString> Cluster::CurrentOperations() const
{
    QVariantMap map = this->property("current_operations").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}

bool Cluster::PoolAutoJoin() const
{
    return this->boolProperty("pool_auto_join", false);
}

double Cluster::TokenTimeout() const
{
    return this->property("token_timeout").toDouble();
}

double Cluster::TokenTimeoutCoefficient() const
{
    return this->property("token_timeout_coefficient").toDouble();
}

QMap<QString, QString> Cluster::ClusterConfig() const
{
    QVariantMap map = this->property("cluster_config").toMap();
    QMap<QString, QString> result;
    for (auto it = map.begin(); it != map.end(); ++it)
        result[it.key()] = it.value().toString();
    return result;
}

QList<QSharedPointer<ClusterHost>> Cluster::GetClusterHosts() const
{
    QList<QSharedPointer<ClusterHost>> clusterHosts;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return clusterHosts;

    XenCache* cache = connection->GetCache();

    const QStringList hostRefs = this->GetClusterHostRefs();
    for (const QString& hostRef : hostRefs)
    {
        QSharedPointer<ClusterHost> clusterHost = cache->ResolveObject<ClusterHost>(hostRef);
        if (clusterHost && clusterHost->IsValid())
            clusterHosts.append(clusterHost);
    }

    return clusterHosts;
}
