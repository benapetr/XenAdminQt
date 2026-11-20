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

#ifndef CLUSTERINGACTIONS_H
#define CLUSTERINGACTIONS_H

#include "../../asyncoperation.h"

class Network;
class Pool;

//! Creates a corosync cluster on a pool (C# XenModel/Actions/Pool/EnableClusteringAction.cs)
class XENLIB_EXPORT EnableClusteringAction : public AsyncOperation
{
    Q_OBJECT

    public:
        EnableClusteringAction(QSharedPointer<Pool> pool, QSharedPointer<Network> network, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_networkRef;
        QStringList m_pifRefs;
        QString m_poolName;
};

//! Destroys a pool's cluster (C# XenModel/Actions/Pool/DisableClusteringAction.cs)
class XENLIB_EXPORT DisableClusteringAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit DisableClusteringAction(QSharedPointer<Pool> pool, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_poolName;
};

#endif // CLUSTERINGACTIONS_H
