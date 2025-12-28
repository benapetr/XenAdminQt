/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * Copyright (c) Cloud Software Group, Inc.
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

#ifndef CUSTOMFIELDSMANAGER_H
#define CUSTOMFIELDSMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QVariant>
#include "customfielddefinition.h"

class XenConnection;

/**
 * @brief Singleton manager for custom field definitions
 *
 * Manages custom field definitions stored in pool gui_config.
 * Custom fields are user-defined metadata that can be attached to XenServer objects.
 * The master list is stored in pool.gui_config["XenCenter.CustomFields"] as XML.
 *
 * Values are stored in object.other_config["XenCenter.CustomFields.<name>"] = value
 *
 * C# equivalent: XenAdmin/CustomFields/CustomFieldsManager.cs + CustomFieldsCache.cs
 */
class CustomFieldsManager : public QObject
{
    Q_OBJECT

    public:
        static CustomFieldsManager* instance();

        // Field definition management
        QList<CustomFieldDefinition> getCustomFields() const;
        QList<CustomFieldDefinition> getCustomFields(XenConnection* connection) const;
        CustomFieldDefinition* getCustomFieldDefinition(const QString& name) const;

        // Constants
        static const QString CUSTOM_FIELD_BASE_KEY;
        static const QString CUSTOM_FIELD_DELIM;

        // Utility methods
        static QString getCustomFieldKey(const CustomFieldDefinition& definition);
        static bool hasCustomFields(const QVariantMap& otherConfig, XenConnection* connection);

    signals:
        /**
         * Emitted when custom field definitions change (add/remove/modify)
         * C# equivalent: CustomFieldsChanged event
         */
        void customFieldsChanged();

    public slots:
        void onGuiConfigChanged();

    private:
        explicit CustomFieldsManager(QObject* parent = nullptr);
        ~CustomFieldsManager();

        static CustomFieldsManager* instance_;

        void recalculateCustomFields();
        QList<CustomFieldDefinition> getCustomFieldsFromGuiConfig(XenConnection* connection) const;
        QList<CustomFieldDefinition> parseCustomFieldDefinitions(const QString& xml) const;

        // Cache per connection
        mutable QMutex mutex_;
        QMap<XenConnection*, QList<CustomFieldDefinition>> customFieldsPerConnection_;
        QList<CustomFieldDefinition> allCustomFields_;
};

#endif // CUSTOMFIELDSMANAGER_H
