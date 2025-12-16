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

#ifndef EJECTHOSTFROMPOOLACTION_H
#define EJECTHOSTFROMPOOLACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;
class Host;
class Pool;

/// <summary>
/// EjectHostFromPoolAction removes a host from a pool.
/// Matches C# XenModel/Actions/Pool/EjectHostFromPoolAction.cs
///
/// The host must not have any running VMs and must not be the pool coordinator.
/// </summary>
class EjectHostFromPoolAction : public AsyncOperation
{
    Q_OBJECT

public:
    /// <summary>
    /// Constructor for ejecting host from pool
    /// </summary>
    /// <param name="connection">Connection to the pool</param>
    /// <param name="pool">Pool object</param>
    /// <param name="hostToEject">Host object to eject from pool</param>
    /// <param name="parent">Parent QObject</param>
    EjectHostFromPoolAction(XenConnection* connection,
                            Pool* pool,
                            Host* hostToEject,
                            QObject* parent = nullptr);

protected:
    void run() override;

private:
    Pool* m_pool;
    Host* m_hostToEject;
};

#endif // EJECTHOSTFROMPOOLACTION_H
