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

#ifndef PROPERTYACCESSORHELPER_H
#define PROPERTYACCESSORHELPER_H

#include <QString>
#include <QStringList>
#include <QMap>
#include "queries.h"

/**
 * PropertyAccessorHelper - Provides property value lookups and i18n mappings
 * 
 * C# Equivalent: PropertyAccessors in Common.cs + PropertyAccessorHelper.cs
 * 
 * This class provides:
 * - Property display name i18n strings
 * - Enum value i18n maps (power_state → "Running", etc.)
 * - Property value extraction from XenObjects
 */
class PropertyAccessorHelper
{
public:
    static PropertyAccessorHelper* instance();
    
    // Get display name for property
    QString getPropertyDisplayName(PropertyNames property) const;
    QString getPropertyDisplayNameFalse(PropertyNames property) const;
    
    // Power state i18n
    QString powerStateToString(const QString& powerState) const;
    QString powerStateFromString(const QString& displayName) const;
    QStringList getAllPowerStates() const;
    
    // Virtualization status i18n
    QString virtualizationStatusToString(const QString& status) const;
    QString virtualizationStatusFromString(const QString& displayName) const;
    QStringList getAllVirtualizationStatuses() const;
    
    // Object type i18n
    QString objectTypeToString(const QString& type) const;
    QStringList getAllObjectTypes() const;
    
    // HA restart priority i18n
    QString haRestartPriorityToString(const QString& priority) const;
    QStringList getAllHARestartPriorities() const;
    
    // SR type i18n
    QString srTypeToString(const QString& type) const;
    QStringList getAllSRTypes() const;

private:
    PropertyAccessorHelper();
    ~PropertyAccessorHelper();
    
    void initializePropertyNames();
    void initializePowerStates();
    void initializeVirtualizationStatuses();
    void initializeObjectTypes();
    void initializeHARestartPriorities();
    void initializeSRTypes();
    
    static PropertyAccessorHelper* instance_;
    
    QMap<PropertyNames, QString> propertyNamesI18n_;
    QMap<PropertyNames, QString> propertyNamesI18nFalse_;
    
    QMap<QString, QString> powerStateI18n_;  // "Halted" → "Halted"
    QMap<QString, QString> virtualizationStatusI18n_;  // "not_optimized" → "Not optimized"
    QMap<QString, QString> objectTypeI18n_;  // "vm" → "VMs"
    QMap<QString, QString> haRestartPriorityI18n_;  // "restart" → "Restart"
    QMap<QString, QString> srTypeI18n_;  // "lvm" → "LVM"
};

#endif // PROPERTYACCESSORHELPER_H
