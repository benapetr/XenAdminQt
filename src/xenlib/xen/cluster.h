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

#ifndef CLUSTER_H
#define CLUSTER_H

#include "xenobject.h"
#include <QMap>
#include <QStringList>

class ClusterHost;

/**
 * @brief Cluster - Cluster-wide cluster metadata
 *
 * Qt equivalent of C# XenAPI.Cluster class. Represents a cluster configuration.
 * First published in XenServer 7.6.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - cluster_hosts: List of cluster host references
 * - pending_forget: Hosts pending removal from cluster
 * - cluster_token: Cluster authentication token
 * - cluster_stack: Cluster stack identifier
 * - allowed_operations: Operations allowed on this cluster
 * - current_operations: Currently executing operations
 * - pool_auto_join: Whether pool auto-joins cluster
 * - token_timeout: Token timeout in seconds
 * - token_timeout_coefficient: Token timeout coefficient
 * - cluster_config: Cluster configuration parameters
 * - other_config: Additional configuration
 */
class XENLIB_EXPORT Cluster : public XenObject
{
    Q_OBJECT

    public:
        explicit Cluster(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Cluster() override = default;

        QString GetObjectType() const override;

        // Property accessors (read from cache)
        QStringList GetClusterHostRefs() const;
        QStringList PendingForget() const;
        QString ClusterToken() const;
        QString ClusterStack() const;
        QStringList AllowedOperations() const;
        QMap<QString, QString> CurrentOperations() const;
        bool PoolAutoJoin() const;
        double TokenTimeout() const;
        double TokenTimeoutCoefficient() const;
        QMap<QString, QString> ClusterConfig() const;

        //! Get list of ClusterHost shared pointers
        QList<QSharedPointer<ClusterHost>> GetClusterHosts() const;
};

#endif // CLUSTER_H
