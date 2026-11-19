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

#include "searchaction.h"
#include "../../network/connection.h"
#include "../../pool.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include "../../../xensearch/searchmanager.h"

SearchAction::SearchAction(XenConnection* connection, const QString& uuid, const QString& name,
                           const QString& xml, Operation operation, QObject* parent)
    : AsyncOperation(connection,
                     operation == Operation::Save ? tr("Save Search '%1'").arg(name) : tr("Delete Search '%1'").arg(name),
                     operation == Operation::Save ? tr("Saving search '%1'...").arg(name) : tr("Deleting search '%1'...").arg(name),
                     parent),
      m_uuid(uuid),
      m_xml(xml),
      m_operation(operation)
{
    QSharedPointer<Pool> pool = connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
    if (pool)
        this->SetPool(pool);

    this->AddApiMethodToRoleCheck("pool.remove_from_gui_config");
    if (operation == Operation::Save)
        this->AddApiMethodToRoleCheck("pool.add_to_gui_config");
}

void SearchAction::run()
{
    try
    {
        XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
        const QString key = SearchManager::GuiConfigKey(this->m_uuid);

        // C# Search.Save uses the first pool; Search.Delete removes the key from every pool
        const QList<QSharedPointer<Pool>> pools = cache ? cache->GetAll<Pool>(XenObjectType::Pool) : QList<QSharedPointer<Pool>>();
        if (pools.isEmpty())
            throw std::runtime_error(tr("The server has no pool to save the search on.").toStdString());

        if (this->m_operation == Operation::Save)
        {
            const QString poolRef = pools.first()->OpaqueRef();
            XenAPI::Pool::remove_from_gui_config(this->GetSession(), poolRef, key);
            XenAPI::Pool::add_to_gui_config(this->GetSession(), poolRef, key, this->m_xml);
        } else
        {
            for (const QSharedPointer<Pool>& pool : pools)
                XenAPI::Pool::remove_from_gui_config(this->GetSession(), pool->OpaqueRef(), key);
        }
    } catch (const std::exception& e)
    {
        this->setError(QString::fromUtf8(e.what()));
    }
}
