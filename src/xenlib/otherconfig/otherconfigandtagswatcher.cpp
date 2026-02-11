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

#include "otherconfigandtagswatcher.h"
#include "../xencache.h"
#include "../xen/network/connection.h"
#include "../xen/network/connectionsmanager.h"

OtherConfigAndTagsWatcher* OtherConfigAndTagsWatcher::instance_ = nullptr;

OtherConfigAndTagsWatcher::OtherConfigAndTagsWatcher(QObject* parent) : QObject(parent)
{
}

OtherConfigAndTagsWatcher* OtherConfigAndTagsWatcher::instance()
{
    if (!instance_)
        instance_ = new OtherConfigAndTagsWatcher();
    return instance_;
}

void OtherConfigAndTagsWatcher::RegisterEventHandlers()
{
    if (this->handlersRegistered_)
        return;

    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    connect(manager, &Xen::ConnectionsManager::connectionAdded, this, &OtherConfigAndTagsWatcher::onConnectionAdded);
    connect(manager, &Xen::ConnectionsManager::connectionRemoved, this, &OtherConfigAndTagsWatcher::onConnectionRemoved);

    const QList<XenConnection*> connections = manager->GetAllConnections();
    for (XenConnection* connection : connections)
        this->onConnectionAdded(connection);

    this->markEventsReadyToFire(true);
    this->handlersRegistered_ = true;
}

void OtherConfigAndTagsWatcher::DeregisterEventHandlers()
{
    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    disconnect(manager, &Xen::ConnectionsManager::connectionAdded, this, &OtherConfigAndTagsWatcher::onConnectionAdded);
    disconnect(manager, &Xen::ConnectionsManager::connectionRemoved, this, &OtherConfigAndTagsWatcher::onConnectionRemoved);

    for (auto it = this->handlers_.begin(); it != this->handlers_.end(); ++it)
    {
        disconnect(it->cacheObjectChanged);
        disconnect(it->xenObjectsUpdated);
        disconnect(it->stateChanged);
    }
    this->handlers_.clear();
    this->handlersRegistered_ = false;
}

void OtherConfigAndTagsWatcher::onConnectionAdded(XenConnection* connection)
{
    if (!connection || this->handlers_.contains(connection))
        return;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return;

    ConnectionHandlers handlers;
    handlers.cacheObjectChanged = connect(cache, &XenCache::objectChanged, this, &OtherConfigAndTagsWatcher::onCacheObjectChanged);
    handlers.xenObjectsUpdated = connect(connection, &XenConnection::XenObjectsUpdated, this, &OtherConfigAndTagsWatcher::onConnectionXenObjectsUpdated);
    handlers.stateChanged = connect(connection, &XenConnection::ConnectionStateChanged, this, &OtherConfigAndTagsWatcher::onConnectionStateChanged);
    this->handlers_.insert(connection, handlers);

    // On initial connection registration emit all on next batch (C# MarkEventsReadyToFire(true)).
    this->markEventsReadyToFire(true);
}

void OtherConfigAndTagsWatcher::onConnectionRemoved(XenConnection* connection)
{
    if (!connection || !this->handlers_.contains(connection))
        return;

    const ConnectionHandlers handlers = this->handlers_.take(connection);
    disconnect(handlers.cacheObjectChanged);
    disconnect(handlers.xenObjectsUpdated);
    disconnect(handlers.stateChanged);
}

void OtherConfigAndTagsWatcher::onConnectionXenObjectsUpdated()
{
    if (this->fireOtherConfigEvent_)
        emit OtherConfigChanged();
    if (this->fireTagsEvent_)
        emit TagsChanged();
    if (this->fireGuiConfigEvent_)
        emit GuiConfigChanged();

    this->markEventsReadyToFire(false);
}

void OtherConfigAndTagsWatcher::onConnectionStateChanged()
{
    // C# parity: on state change, fire all and reset pending flags.
    emit OtherConfigChanged();
    emit TagsChanged();
    emit GuiConfigChanged();
    this->markEventsReadyToFire(false);
}

void OtherConfigAndTagsWatcher::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    Q_UNUSED(connection);

    if (type == "pool")
        this->fireGuiConfigEvent_ = true;

    if (type == "pool" || type == "host" || type == "vm" || type == "sr" || type == "vdi" || type == "network")
    {
        this->fireOtherConfigEvent_ = true;
        this->fireTagsEvent_ = true;
    }
}

void OtherConfigAndTagsWatcher::markEventsReadyToFire(bool fire)
{
    this->fireOtherConfigEvent_ = fire;
    this->fireTagsEvent_ = fire;
    this->fireGuiConfigEvent_ = fire;
}
