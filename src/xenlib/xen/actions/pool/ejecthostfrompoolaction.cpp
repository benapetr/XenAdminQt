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

#include "ejecthostfrompoolaction.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Pool.h"
#include <QDebug>
#include <stdexcept>

EjectHostFromPoolAction::EjectHostFromPoolAction(XenConnection* connection,
                                                 Pool* pool,
                                                 Host* hostToEject,
                                                 QObject* parent)
    : AsyncOperation(connection,
                     QString("Removing %1 from pool %2")
                         .arg(hostToEject ? hostToEject->nameLabel() : "host")
                         .arg(pool ? pool->nameLabel() : ""),
                     QString("Removing host from pool"),
                     parent),
      m_pool(pool), m_hostToEject(hostToEject)
{
    if (!m_pool)
        throw std::invalid_argument("Pool cannot be null");
    if (!m_hostToEject)
        throw std::invalid_argument("Host to eject cannot be null");

    setPool(m_pool);
    setHost(m_hostToEject);
}

void EjectHostFromPoolAction::run()
{
    try
    {
        setDescription("Removing host from pool...");

        qDebug() << "EjectHostFromPoolAction: Ejecting" << m_hostToEject->nameLabel()
                 << "from pool" << m_pool->nameLabel();

        // Call Pool.eject to remove the host
        XenAPI::Pool::eject(session(), m_hostToEject->opaqueRef());

        setDescription("Host removed from pool");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to eject host from pool: %1").arg(e.what()));
    }
}
