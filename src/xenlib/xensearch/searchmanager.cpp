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

#include "searchmanager.h"
#include "search.h"
#include "query.h"
#include "queryscope.h"
#include "queries.h"
#include "grouping.h"
#include "searchmarshalling.h"
#include "../xencache.h"
#include "../xen/network/connection.h"
#include "../xen/network/connectionsmanager.h"
#include "../xen/pool.h"
#include <algorithm>

using namespace XenSearch;

SearchManager* SearchManager::instance()
{
    static SearchManager* manager = new SearchManager();
    return manager;
}

SearchManager::SearchManager()
{
    // Pools change often; resynchronise once per burst
    this->synchroniseTimer_.setSingleShot(true);
    this->synchroniseTimer_.setInterval(0);
    connect(&this->synchroniseTimer_, &QTimer::timeout, this, &SearchManager::synchronise);

    this->initDefaultSearches();

    Xen::ConnectionsManager* connections = Xen::ConnectionsManager::instance();
    connect(connections, &Xen::ConnectionsManager::connectionAdded, this, &SearchManager::onConnectionAdded);
    connect(connections, &Xen::ConnectionsManager::connectionRemoved, this, &SearchManager::scheduleSynchronise);
    for (XenConnection* connection : connections->GetAllConnections())
        this->watch(connection);

    this->synchronise();
}

QString SearchManager::SearchPrefix()
{
    // C# BrandManager.ExtensionSearch + ".Search-" (the same for XenCenter and XCP-ng Center)
    return QStringLiteral("xensearch.Search-");
}

QString SearchManager::GuiConfigKey(const QString& uuid)
{
    return SearchPrefix() + uuid;
}

void SearchManager::onConnectionAdded(XenConnection* connection)
{
    this->watch(connection);
    this->scheduleSynchronise();
}

void SearchManager::watch(XenConnection* connection)
{
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return;

    // C# RegisterBatchCollectionChanged<Pool>: follow changes of the pools' gui_config
    connect(cache, &XenCache::itemChanged, this, [this](XenConnection*, XenObjectType type, const QString&) {
        if (type == XenObjectType::Pool)
            this->scheduleSynchronise();
    });
    connect(cache, &XenCache::itemRemoved, this, [this](XenConnection*, XenObjectType type, const QString&) {
        if (type == XenObjectType::Pool)
            this->scheduleSynchronise();
    });
    connect(cache, &XenCache::bulkUpdateComplete, this, [this](XenObjectType type, int) {
        if (type == XenObjectType::Pool)
            this->scheduleSynchronise();
    });
    connect(cache, &XenCache::cacheCleared, this, &SearchManager::scheduleSynchronise);
}

void SearchManager::scheduleSynchronise()
{
    this->synchroniseTimer_.start();
}

// C# SynchroniseSearches: the searches saved on all pools
void SearchManager::synchronise()
{
    QList<Entry> saved;
    for (XenConnection* connection : Xen::ConnectionsManager::instance()->GetAllConnections())
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;

        for (const QSharedPointer<Pool>& pool : cache->GetAll<Pool>(XenObjectType::Pool))
        {
            const QVariantMap guiConfig = pool->GUIConfig();
            for (auto it = guiConfig.constBegin(); it != guiConfig.constEnd(); ++it)
            {
                if (!it.key().startsWith(SearchPrefix()))
                    continue;

                const QString xml = it.value().toString();
                std::unique_ptr<Search> search(SearchMarshalling::LoadSearch(xml));
                if (!search)
                    continue;

                Entry entry;
                entry.uuid = it.key().mid(SearchPrefix().length());
                entry.name = search->GetName();
                entry.connection = connection;
                entry.xml = xml;

                // The same search may be on several pools; list it once (C# keys by UUID)
                auto existing = std::find_if(saved.begin(), saved.end(),
                                             [&](const Entry& e) { return e.uuid == entry.uuid; });
                if (existing == saved.end())
                    saved.append(entry);
            }
        }
    }

    std::sort(saved.begin(), saved.end(), [](const Entry& a, const Entry& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    auto same = [](const QList<Entry>& a, const QList<Entry>& b) {
        if (a.size() != b.size())
            return false;
        for (int i = 0; i < a.size(); ++i)
        {
            if (a[i].uuid != b[i].uuid || a[i].xml != b[i].xml || a[i].connection != b[i].connection)
                return false;
        }
        return true;
    };

    if (same(saved, this->saved_))
        return;

    this->saved_ = saved;
    emit this->SearchesChanged();
}

QList<SearchManager::Entry> SearchManager::GetSearches() const
{
    return this->defaults_ + this->saved_;
}

bool SearchManager::Contains(const QString& uuid) const
{
    const QList<Entry> searches = this->GetSearches();
    return std::any_of(searches.begin(), searches.end(), [&](const Entry& e) { return e.uuid == uuid; });
}

Search* SearchManager::CreateSearch(const QString& uuid) const
{
    for (const Entry& entry : this->GetSearches())
    {
        if (entry.uuid != uuid)
            continue;
        Search* search = SearchMarshalling::LoadSearch(entry.xml);
        if (!search)
            return nullptr;
        if (entry.isDefault)
        {
            // Built-in searches keep their flag (the XML has none)
            Search* builtIn = new Search(search->GetQuery(), search->GetGrouping(), search->GetName(), search->GetUUID(),
                                         true, search->GetColumns(), search->GetSorting(), true, true);
            search->ReleaseOwnership();
            delete search;
            search = builtIn;
        }
        search->SetConnection(entry.connection);
        return search;
    }
    return nullptr;
}

QList<XenConnection*> SearchManager::ConnectionsHolding(const QString& uuid) const
{
    QList<XenConnection*> holding;
    for (XenConnection* connection : Xen::ConnectionsManager::instance()->GetAllConnections())
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            continue;
        for (const QSharedPointer<Pool>& pool : cache->GetAll<Pool>(XenObjectType::Pool))
        {
            if (pool->GUIConfig().contains(GuiConfigKey(uuid)) && !holding.contains(connection))
                holding.append(connection);
        }
    }
    return holding;
}

// C# InitDefaultSearches
void SearchManager::initDefaultSearches()
{
    auto add = [this](Search* search) {
        Entry entry;
        entry.uuid = search->GetUUID();
        entry.name = search->GetName();
        entry.isDefault = true;
        entry.xml = SearchMarshalling::SearchToXml(search);
        this->defaults_.append(entry);
        delete search;
    };

    add(new Search(new Query(new QueryScope(ObjectTypes::VM), nullptr),
                   new PropertyGrouping(PropertyNames::os_name), tr("VMs by Operating System"),
                   "dead-beef-1234-vmsbyos", true));

    add(new Search(new Query(new QueryScope(ObjectTypes::VM),
                             new GroupQuery(GroupQuery::GroupQueryType::And, {
                                 new EnumQuery(PropertyNames::power_state, "Running", false),
                                 new EnumQuery(PropertyNames::virtualisation_status, "IoDriversInstalled, ManagementInstalled", true)})),
                   new PropertyGrouping(PropertyNames::virtualisation_status),
                   tr("VMs without %1").arg(tr("XCP-ng VM Tools")), "dead-beef-1234-vmswotools", true));

    add(new Search(new Query(new QueryScope(ObjectTypes::VM), nullptr),
                   new PropertyGrouping(PropertyNames::power_state), tr("VMs by Power State"),
                   "dead-beef-1234-vmsbyps", true));

    add(new Search(new Query(new QueryScope(ObjectTypes::VM), nullptr),
                   new XenModelObjectPropertyGrouping(PropertyNames::networks), tr("VMs by Network"),
                   "dead-beef-1234-vmsbynet", true));

    add(new Search(new Query(new QueryScope(ObjectTypes::VM), new BoolQuery(PropertyNames::in_any_appliance, true)),
                   new VAppGrouping(), tr("VMs by vApp"), "dead-beef-1234-vmsbyappliance", true));

    add(new Search(new Query(nullptr, new ListEmptyQuery(PropertyNames::tags, false)),
                   new TagsGrouping(), tr("Resources by Tag"), "dead-beef-1234-objbytags", true));

    add(new Search(new Query(new QueryScope(ObjectTypes::Snapshot | ObjectTypes::VM), nullptr),
                   new PoolGrouping(new HostGrouping(new XenModelObjectPropertyGrouping(PropertyNames::vm))),
                   tr("VMs and Snapshots"), "dead-beef-1234-snapshotsbyvm", true));

    std::sort(this->defaults_.begin(), this->defaults_.end(), [](const Entry& a, const Entry& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}
