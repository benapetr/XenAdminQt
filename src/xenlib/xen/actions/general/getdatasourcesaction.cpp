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

#include "getdatasourcesaction.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../failure.h"
#include "../../vm.h"
#include "../../../xencache.h"

namespace
{
    bool isVmBadPowerStateError(const QString& error)
    {
        return error.contains(QLatin1String(Failure::VM_BAD_POWER_STATE), Qt::CaseInsensitive);
    }
}

GetDataSourcesAction::GetDataSourcesAction(XenConnection* connection,
                                           XenObjectType objectType,
                                           const QString& objectRef,
                                           QObject* parent)
    : AsyncOperation(connection, "Get data sources", "Getting performance data sources...", true, parent),
      m_objectType(objectType),
      m_objectRef(objectRef)
{
    if (this->m_objectType == XenObjectType::VM)
        this->AddApiMethodToRoleCheck("VM.get_data_sources");
    else if (this->m_objectType == XenObjectType::Host)
        this->AddApiMethodToRoleCheck("host.get_data_sources");
}

void GetDataSourcesAction::run()
{
    this->m_dataSources.clear();

    if (this->m_objectRef.isEmpty())
        return;

    if (this->m_objectType == XenObjectType::VM)
    {
        XenConnection* connection = this->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (cache)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(this->m_objectRef);
            if (vm && vm->IsHalted())
                return;
        }

        try
        {
            this->m_dataSources = XenAPI::VM::get_data_sources(this->GetSession(), this->m_objectRef);
        }
        catch (const std::exception& ex)
        {
            const QString error = QString::fromUtf8(ex.what());
            if (isVmBadPowerStateError(error))
            {
                this->m_dataSources.clear();
                return;
            }
            throw;
        }
    } else if (this->m_objectType == XenObjectType::Host)
    {
        this->m_dataSources = XenAPI::Host::get_data_sources(this->GetSession(), this->m_objectRef);
    }
}
