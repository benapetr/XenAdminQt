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

#include "destroypoolaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include "../../pool.h"
#include <stdexcept>

DestroyPoolAction::DestroyPoolAction(QSharedPointer<Pool> pool,
                                     QObject* parent)
    : AsyncOperation(pool ? pool->GetConnection() : nullptr,
                     QString("Destroying pool"),
                     "Destroying pool",
                     parent),
      m_pool(pool)
{
    if (!this->m_pool || !this->m_pool->IsValid())
    {
        throw std::invalid_argument("Invalid pool object");
    }

    // RBAC dependencies (matches C# DestroyPoolAction)
    this->AddApiMethodToRoleCheck("pool.set_name_label");
    this->AddApiMethodToRoleCheck("pool.set_name_description");
}

void DestroyPoolAction::run()
{
    try
    {
        this->SetPercentComplete(0);
        this->SetDescription("Checking pool state...");

        // Check that pool has only one host
        QStringList hostRefs = this->GetConnection()->GetCache()->GetAllRefs(XenObjectType::Host);
        if (hostRefs.size() > 1)
        {
            throw std::runtime_error("Cannot destroy pool with multiple hosts. Remove all hosts except coordinator first.");
        }

        this->SetPercentComplete(20);
        this->SetDescription("Destroying pool...");

        // Clear pool name and description to "destroy" it
        // This effectively converts the pool back to a standalone host
        XenAPI::Pool::set_name_label(this->GetSession(), this->m_pool->OpaqueRef(), "");

        this->SetPercentComplete(70);
        XenAPI::Pool::set_name_description(this->GetSession(), this->m_pool->OpaqueRef(), "");

        this->SetPercentComplete(100);
        this->SetDescription("Pool destroyed successfully");

    } catch (const std::exception& e)
    {
        if (this->IsCancelled())
        {
            this->SetDescription("Destroy cancelled");
        } else
        {
            this->setError(QString("Failed to destroy pool: %1").arg(e.what()));
        }
    }
}
