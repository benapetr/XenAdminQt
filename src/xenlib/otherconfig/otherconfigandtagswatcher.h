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

#ifndef OTHERCONFIGANDTAGSWATCHER_H
#define OTHERCONFIGANDTAGSWATCHER_H

#include "../xenlib_global.h"
#include <QObject>
#include <QMap>
#include <QHash>
#include <QStringList>
#include <QVariantMap>

class XenConnection;

/**
 * @brief Watches for changes to other_config, tags, and gui_config across all objects
 *
 * C# equivalent: XenAdmin.OtherConfigAndTagsWatcher
 * 
 * This singleton monitors all XenObjects for changes to:
 * - other_config (custom key-value pairs)
 * - tags (user-defined labels)
 * - gui_config (pool GUI settings like custom field definitions)
 *
 * It batches property change events from multiple objects and emits aggregated
 * signals when connections finish updating. This prevents excessive UI updates
 * when many objects change simultaneously (e.g., during initial cache population).
 *
 * Usage:
 *   OtherConfigAndTagsWatcher::instance()->RegisterEventHandlers();
 *   connect(OtherConfigAndTagsWatcher::instance(), &OtherConfigAndTagsWatcher::TagsChanged,
 *           this, &MyClass::onTagsChanged);
 */
class XENLIB_EXPORT OtherConfigAndTagsWatcher : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     */
    static OtherConfigAndTagsWatcher* instance();

    /**
     * @brief Register event handlers for all connections
     * 
     * Call this once at application startup after ConnectionsManager is ready.
     * Subscribes to cache changes for Pool, Host, VM, SR, VDI, Network objects.
     */
    void RegisterEventHandlers();

    /**
     * @brief Deregister all event handlers
     * 
     * Call before application shutdown to clean up connections.
     */
    void DeregisterEventHandlers();

signals:
    /**
     * @brief Emitted when other_config changes on any XenObject
     * 
     * C# equivalent: OtherConfigChanged event
     * Batched - only fires once per connection update cycle
     */
    void OtherConfigChanged();

    /**
     * @brief Emitted when tags change on any XenObject
     * 
     * C# equivalent: TagsChanged event
     * Used by TagQueryType to refresh available tag list
     */
    void TagsChanged();

    /**
     * @brief Emitted when pool gui_config changes
     * 
     * C# equivalent: GuiConfigChanged event
     * Used by CustomFieldsManager to reload custom field definitions
     */
    void GuiConfigChanged();

private slots:
    void onConnectionAdded(XenConnection* connection);
    void onConnectionRemoved(XenConnection* connection);
    void onConnectionXenObjectsUpdated();
    void onConnectionStateChanged();
    void onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref);

private:
    explicit OtherConfigAndTagsWatcher(QObject* parent = nullptr);
    ~OtherConfigAndTagsWatcher() override = default;

    static OtherConfigAndTagsWatcher* instance_;

    struct ConnectionHandlers
    {
        QMetaObject::Connection cacheObjectChanged;
        QMetaObject::Connection xenObjectsUpdated;
        QMetaObject::Connection stateChanged;
    };

    QHash<XenConnection*, ConnectionHandlers> handlers_;
    bool handlersRegistered_ = false;

    // C# parity: pending event flags are set by property changes and emitted on XenObjectsUpdated
    bool fireOtherConfigEvent_ = true;
    bool fireTagsEvent_ = true;
    bool fireGuiConfigEvent_ = true;

    void markEventsReadyToFire(bool fire);
};

#endif // OTHERCONFIGANDTAGSWATCHER_H
