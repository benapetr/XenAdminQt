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

#include "propertyaccessorhelper.h"

PropertyAccessorHelper* PropertyAccessorHelper::instance_ = nullptr;

PropertyAccessorHelper::PropertyAccessorHelper()
{
    this->initializePropertyNames();
    this->initializePowerStates();
    this->initializeVirtualizationStatuses();
    this->initializeObjectTypes();
    this->initializeHARestartPriorities();
    this->initializeSRTypes();
}

PropertyAccessorHelper::~PropertyAccessorHelper()
{
}

PropertyAccessorHelper* PropertyAccessorHelper::instance()
{
    if (!instance_)
    {
        instance_ = new PropertyAccessorHelper();
    }
    return instance_;
}

void PropertyAccessorHelper::initializePropertyNames()
{
    // Property display names (i18n)
    this->propertyNamesI18n_[PropertyNames::label] = "Name";
    this->propertyNamesI18n_[PropertyNames::description] = "Description";
    this->propertyNamesI18n_[PropertyNames::uuid] = "UUID";
    this->propertyNamesI18n_[PropertyNames::tags] = "Tags";
    this->propertyNamesI18n_[PropertyNames::type] = "Type";
    this->propertyNamesI18n_[PropertyNames::power_state] = "Power State";
    this->propertyNamesI18n_[PropertyNames::virtualisation_status] = "Tools Status";
    this->propertyNamesI18n_[PropertyNames::os_name] = "Operating System";
    this->propertyNamesI18n_[PropertyNames::ha_restart_priority] = "HA Restart Priority";
    this->propertyNamesI18n_[PropertyNames::start_time] = "Start Time";
    this->propertyNamesI18n_[PropertyNames::memory] = "Memory";
    this->propertyNamesI18n_[PropertyNames::size] = "Size";
    this->propertyNamesI18n_[PropertyNames::shared] = "Shared";
    this->propertyNamesI18n_[PropertyNames::ha_enabled] = "HA";
    this->propertyNamesI18n_[PropertyNames::sr_type] = "Storage Type";
    this->propertyNamesI18n_[PropertyNames::read_caching_enabled] = "Read caching enabled";
    this->propertyNamesI18n_[PropertyNames::ip_address] = "IP Address";
    this->propertyNamesI18n_[PropertyNames::vendor_device_state] = "Windows Update capable";
    this->propertyNamesI18n_[PropertyNames::in_any_appliance] = "In any vApp";
    this->propertyNamesI18n_[PropertyNames::isNotFullyUpgraded] = "Pool not fully upgraded";
    this->propertyNamesI18n_[PropertyNames::pool] = "Pool";
    this->propertyNamesI18n_[PropertyNames::host] = "Server";
    this->propertyNamesI18n_[PropertyNames::vm] = "VM";
    this->propertyNamesI18n_[PropertyNames::networks] = "Network";
    this->propertyNamesI18n_[PropertyNames::storage] = "SR";
    this->propertyNamesI18n_[PropertyNames::disks] = "Virtual Disk";
    this->propertyNamesI18n_[PropertyNames::appliance] = "vApp";
    this->propertyNamesI18n_[PropertyNames::folder] = "Parent Folder";
    this->propertyNamesI18n_[PropertyNames::folders] = "Ancestor Folders";
    this->propertyNamesI18n_[PropertyNames::has_custom_fields] = "Has custom fields";
    
    // False values for boolean properties
    this->propertyNamesI18nFalse_[PropertyNames::read_caching_enabled] = "Read caching disabled";
    this->propertyNamesI18nFalse_[PropertyNames::vendor_device_state] = "Windows Update not capable";
}

void PropertyAccessorHelper::initializePowerStates()
{
    // Power state values
    this->powerStateI18n_["Halted"] = "Halted";
    this->powerStateI18n_["Running"] = "Running";
    this->powerStateI18n_["Suspended"] = "Suspended";
    this->powerStateI18n_["Paused"] = "Paused";
    this->powerStateI18n_["unknown"] = "Unknown";
}

void PropertyAccessorHelper::initializeVirtualizationStatuses()
{
    // Virtualization status values
    this->virtualizationStatusI18n_["not_optimized"] = "Not optimized";
    this->virtualizationStatusI18n_["out_of_date"] = "Out of date";
    this->virtualizationStatusI18n_["unknown"] = "Unknown";
    this->virtualizationStatusI18n_["io_optimized"] = "I/O optimized";
    this->virtualizationStatusI18n_["management_installed"] = "Management agent installed";
    this->virtualizationStatusI18n_["optimized"] = "Optimized";
}

void PropertyAccessorHelper::initializeObjectTypes()
{
    // Object type values
    this->objectTypeI18n_["vm"] = "VMs";
    this->objectTypeI18n_["host"] = "Servers";
    this->objectTypeI18n_["pool"] = "Pools";
    this->objectTypeI18n_["sr"] = "Storage";
    this->objectTypeI18n_["network"] = "Networks";
    this->objectTypeI18n_["vdi"] = "Virtual Disks";
    this->objectTypeI18n_["template"] = "Templates";
    this->objectTypeI18n_["snapshot"] = "Snapshots";
    this->objectTypeI18n_["folder"] = "Folders";
}

void PropertyAccessorHelper::initializeHARestartPriorities()
{
    // HA restart priority values
    this->haRestartPriorityI18n_["Restart"] = "Restart";
    this->haRestartPriorityI18n_["BestEffort"] = "Best-effort";
    this->haRestartPriorityI18n_["DoNotRestart"] = "Do not restart";
}

void PropertyAccessorHelper::initializeSRTypes()
{
    // SR type values
    this->srTypeI18n_["lvm"] = "LVM";
    this->srTypeI18n_["ext"] = "EXT";
    this->srTypeI18n_["nfs"] = "NFS";
    this->srTypeI18n_["lvmoiscsi"] = "iSCSI";
    this->srTypeI18n_["lvmohba"] = "HBA";
    this->srTypeI18n_["iso"] = "ISO";
    this->srTypeI18n_["udev"] = "DVD Drive";
}

QString PropertyAccessorHelper::getPropertyDisplayName(PropertyNames property) const
{
    return this->propertyNamesI18n_.value(property, "Unknown");
}

QString PropertyAccessorHelper::getPropertyDisplayNameFalse(PropertyNames property) const
{
    if (this->propertyNamesI18nFalse_.contains(property))
        return this->propertyNamesI18nFalse_.value(property);
    
    return "Not " + this->getPropertyDisplayName(property);
}

QString PropertyAccessorHelper::powerStateToString(const QString& powerState) const
{
    return this->powerStateI18n_.value(powerState, powerState);
}

QString PropertyAccessorHelper::powerStateFromString(const QString& displayName) const
{
    return this->powerStateI18n_.key(displayName, displayName);
}

QStringList PropertyAccessorHelper::getAllPowerStates() const
{
    return this->powerStateI18n_.values();
}

QString PropertyAccessorHelper::virtualizationStatusToString(const QString& status) const
{
    return this->virtualizationStatusI18n_.value(status, status);
}

QString PropertyAccessorHelper::virtualizationStatusFromString(const QString& displayName) const
{
    return this->virtualizationStatusI18n_.key(displayName, displayName);
}

QStringList PropertyAccessorHelper::getAllVirtualizationStatuses() const
{
    return this->virtualizationStatusI18n_.values();
}

QString PropertyAccessorHelper::objectTypeToString(const QString& type) const
{
    return this->objectTypeI18n_.value(type, type);
}

QStringList PropertyAccessorHelper::getAllObjectTypes() const
{
    return this->objectTypeI18n_.values();
}

QString PropertyAccessorHelper::haRestartPriorityToString(const QString& priority) const
{
    return this->haRestartPriorityI18n_.value(priority, priority);
}

QStringList PropertyAccessorHelper::getAllHARestartPriorities() const
{
    return this->haRestartPriorityI18n_.values();
}

QString PropertyAccessorHelper::srTypeToString(const QString& type) const
{
    return this->srTypeI18n_.value(type, type);
}

QStringList PropertyAccessorHelper::getAllSRTypes() const
{
    return this->srTypeI18n_.values();
}
