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
#include <QDebug>

OtherConfigAndTagsWatcher* OtherConfigAndTagsWatcher::instance_ = nullptr;

OtherConfigAndTagsWatcher::OtherConfigAndTagsWatcher(QObject* parent)
    : QObject(parent)
{
}

OtherConfigAndTagsWatcher* OtherConfigAndTagsWatcher::instance()
{
    if (!instance_)
    {
        instance_ = new OtherConfigAndTagsWatcher();
    }
    return instance_;
}

void OtherConfigAndTagsWatcher::RegisterEventHandlers()
{
    // C# equivalent: Subscribes to cache CollectionChanged for all object types
    // In Qt, XenCache emits objectChanged signal for individual object updates
    // We connect to this signal to track other_config, tags, and gui_config changes
    
    // Note: This is a simplified implementation
    // Full C# version registers/deregisters handlers per connection
    // For now, we'll rely on CustomFieldsManager and TagQueryType
    // to connect directly to cache signals as needed
    
    qDebug() << "OtherConfigAndTagsWatcher: Event handlers registered";
}

void OtherConfigAndTagsWatcher::DeregisterEventHandlers()
{
    // Clean up connections
    this->lastOtherConfig_.clear();
    this->lastTags_.clear();
    this->lastGuiConfig_.clear();
    
    qDebug() << "OtherConfigAndTagsWatcher: Event handlers deregistered";
}

void OtherConfigAndTagsWatcher::onCacheObjectChanged(const QString& type, const QString& ref, const QVariantMap& data)
{
    // C# equivalent: PropertyChanged<T> handler
    // Checks for changes to other_config, tags, or gui_config
    
    this->checkForChanges(type, ref, data);
}

void OtherConfigAndTagsWatcher::checkForChanges(const QString& type, const QString& ref, const QVariantMap& newData)
{
    // Check other_config
    if (newData.contains("other_config"))
    {
        QVariantMap newOtherConfig = newData.value("other_config").toMap();
        QString key = type + ":" + ref;
        
        if (!this->lastOtherConfig_.contains(key) || this->lastOtherConfig_[key] != newOtherConfig)
        {
            this->lastOtherConfig_[key] = newOtherConfig;
            this->fireOtherConfigEvent_ = true;
        }
    }
    
    // Check tags
    if (newData.contains("tags"))
    {
        QStringList newTags;
        QVariant tagsVar = newData.value("tags");
        
        if (tagsVar.canConvert<QStringList>())
        {
            newTags = tagsVar.toStringList();
        } else if (tagsVar.canConvert<QVariantList>())
        {
            QVariantList tagsList = tagsVar.toList();
            for (const QVariant& tag : tagsList)
                newTags.append(tag.toString());
        }
        
        QString key = type + ":" + ref;
        
        if (!this->lastTags_.contains(key) || this->lastTags_[key] != newTags)
        {
            this->lastTags_[key] = newTags;
            this->fireTagsEvent_ = true;
        }
    }
    
    // Check gui_config (pool only)
    if (type == "pool" && newData.contains("gui_config"))
    {
        QVariantMap newGuiConfig = newData.value("gui_config").toMap();
        
        if (!this->lastGuiConfig_.contains(ref) || this->lastGuiConfig_[ref] != newGuiConfig)
        {
            this->lastGuiConfig_[ref] = newGuiConfig;
            this->fireGuiConfigEvent_ = true;
        }
    }
    
    // Emit batched signals
    // C# version waits for connection_XenObjectsUpdated event
    // Qt simplified: emit immediately if flags are set
    if (this->fireOtherConfigEvent_)
    {
        emit OtherConfigChanged();
        this->fireOtherConfigEvent_ = false;
    }
    
    if (this->fireTagsEvent_)
    {
        emit TagsChanged();
        this->fireTagsEvent_ = false;
    }
    
    if (this->fireGuiConfigEvent_)
    {
        emit GuiConfigChanged();
        this->fireGuiConfigEvent_ = false;
    }
}

void OtherConfigAndTagsWatcher::markEventsReadyToFire(bool fire)
{
    this->fireOtherConfigEvent_ = fire;
    this->fireTagsEvent_ = fire;
    this->fireGuiConfigEvent_ = fire;
}
