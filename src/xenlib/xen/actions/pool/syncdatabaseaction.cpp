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

#include "syncdatabaseaction.h"
#include "../../xenapi/xenapi_Pool.h"
#include "xen/pool.h"
#include <QDebug>
#include <stdexcept>

SyncDatabaseAction::SyncDatabaseAction(QSharedPointer<Pool> pool, QObject* parent)
    : AsyncOperation("Synchronizing Database", "Synchronizing pool database across all members...", parent), m_pool(pool)
{
    if (!this->m_pool || !this->m_pool->IsValid())
        qWarning() << "SyncDatabaseAction: Invalid pool object";
    this->m_connection = this->m_pool->GetConnection();
}

void SyncDatabaseAction::run()
{
    try
    {
        if (!this->m_pool || !this->m_pool->IsValid())
        {
            throw std::runtime_error("Invalid pool object");
        }

        if (!this->GetSession())
        {
            throw std::runtime_error("Not connected to XenServer");
        }

        this->SetPercentComplete(0);
        this->SetDescription("Synchronizing database across pool members...");

        // Kick off async database synchronization task
        QString taskRef = XenAPI::Pool::async_sync_database(this->GetSession());

        // Poll the task from 0% to 100%
        this->pollToCompletion(taskRef, 0, 100);

        this->SetPercentComplete(100);
        this->SetDescription("Database synchronized successfully");

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to synchronize database: %1").arg(e.what()));
        throw;
    }
}
