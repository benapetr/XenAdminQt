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

#include "common.h"
#include "propertyaccessorhelper.h"
#include "../xen/xenobject.h"
#include "../xen/vm.h"
#include "../xen/host.h"
#include "../xen/pool.h"
#include "../xen/sr.h"
#include "../xen/vdi.h"
#include "../xen/network.h"
#include "../xen/folder.h"
#include "../xen/vmappliance.h"
#include "../xen/dockercontainer.h"
#include "../xen/network/connection.h"
#include "../xencache.h"


namespace XenSearch
{

// Static member initialization
bool PropertyAccessors::initialized_ = false;
QMap<PropertyNames, QString> PropertyAccessors::propertyTypes_;
QMap<PropertyNames, std::function<QVariant(XenObject*)>> PropertyAccessors::properties_;
QMap<QString, int> PropertyAccessors::vmPowerStateI18n_;
QMap<QString, int> PropertyAccessors::virtualisationStatusI18n_;
QMap<QString, ObjectTypes> PropertyAccessors::objectTypesI18n_;
QMap<QString, int> PropertyAccessors::haRestartPriorityI18n_;
QMap<QString, int> PropertyAccessors::srTypeI18n_;
QMap<PropertyNames, QString> PropertyAccessors::propertyNamesI18n_;
QMap<PropertyNames, QString> PropertyAccessors::propertyNamesI18nFalse_;
QMap<ColumnNames, PropertyNames> PropertyAccessors::columnSortBy_;

void PropertyAccessors::Initialize()
{
    if (initialized_)
        return;
    
    initialized_ = true;
    
    // Initialize property types
    propertyTypes_[PropertyNames::pool] = "Pool";
    propertyTypes_[PropertyNames::host] = "Host";
    propertyTypes_[PropertyNames::os_name] = "QString";
    propertyTypes_[PropertyNames::power_state] = "int";  // vm_power_state enum
    propertyTypes_[PropertyNames::virtualisation_status] = "int";  // VirtualizationStatus flags
    propertyTypes_[PropertyNames::type] = "ObjectTypes";
    propertyTypes_[PropertyNames::networks] = "Network";
    propertyTypes_[PropertyNames::storage] = "SR";
    propertyTypes_[PropertyNames::ha_restart_priority] = "int";  // HaRestartPriority enum
    propertyTypes_[PropertyNames::read_caching_enabled] = "bool";
    propertyTypes_[PropertyNames::appliance] = "VMAppliance";
    propertyTypes_[PropertyNames::tags] = "QString";
    propertyTypes_[PropertyNames::has_custom_fields] = "bool";
    propertyTypes_[PropertyNames::ip_address] = "QString";  // ComparableAddress
    propertyTypes_[PropertyNames::vm] = "VM";
    propertyTypes_[PropertyNames::sr_type] = "int";  // SR::SRTypes enum
    propertyTypes_[PropertyNames::folder] = "Folder";
    propertyTypes_[PropertyNames::folders] = "Folder";
    propertyTypes_[PropertyNames::in_any_appliance] = "bool";
    propertyTypes_[PropertyNames::disks] = "VDI";
    
    // Initialize i18n display names for properties
    propertyNamesI18n_[PropertyNames::description] = QObject::tr("Description");
    propertyNamesI18n_[PropertyNames::host] = QObject::tr("Server");
    propertyNamesI18n_[PropertyNames::label] = QObject::tr("Name");
    propertyNamesI18n_[PropertyNames::uuid] = QObject::tr("UUID");
    propertyNamesI18n_[PropertyNames::networks] = QObject::tr("Network");
    propertyNamesI18n_[PropertyNames::os_name] = QObject::tr("Operating System");
    propertyNamesI18n_[PropertyNames::pool] = QObject::tr("Pool");
    propertyNamesI18n_[PropertyNames::power_state] = QObject::tr("Power State");
    propertyNamesI18n_[PropertyNames::start_time] = QObject::tr("Start Time");
    propertyNamesI18n_[PropertyNames::storage] = QObject::tr("SR");
    propertyNamesI18n_[PropertyNames::disks] = QObject::tr("Virtual Disk");
    propertyNamesI18n_[PropertyNames::type] = QObject::tr("Type");
    propertyNamesI18n_[PropertyNames::virtualisation_status] = QObject::tr("Tools Status");
    propertyNamesI18n_[PropertyNames::ha_restart_priority] = QObject::tr("HA Restart Priority");
    propertyNamesI18n_[PropertyNames::appliance] = QObject::tr("VM Appliance");
    propertyNamesI18n_[PropertyNames::tags] = QObject::tr("Tags");
    propertyNamesI18n_[PropertyNames::shared] = QObject::tr("Shared");
    propertyNamesI18n_[PropertyNames::ha_enabled] = QObject::tr("HA");
    propertyNamesI18n_[PropertyNames::isNotFullyUpgraded] = QObject::tr("Pool Versions");
    propertyNamesI18n_[PropertyNames::ip_address] = QObject::tr("Address");
    propertyNamesI18n_[PropertyNames::vm] = QObject::tr("VM");
    propertyNamesI18n_[PropertyNames::dockervm] = QObject::tr("Docker VM");
    propertyNamesI18n_[PropertyNames::read_caching_enabled] = QObject::tr("Read Caching Enabled");
    propertyNamesI18n_[PropertyNames::memory] = QObject::tr("Memory");
    propertyNamesI18n_[PropertyNames::sr_type] = QObject::tr("Storage Type");
    propertyNamesI18n_[PropertyNames::folder] = QObject::tr("Parent Folder");
    propertyNamesI18n_[PropertyNames::folders] = QObject::tr("Ancestor Folders");
    propertyNamesI18n_[PropertyNames::has_custom_fields] = QObject::tr("Has Custom Fields");
    propertyNamesI18n_[PropertyNames::in_any_appliance] = QObject::tr("In Any Appliance");
    propertyNamesI18n_[PropertyNames::vendor_device_state] = QObject::tr("Windows Update Capable");
    
    // False value display names
    propertyNamesI18nFalse_[PropertyNames::read_caching_enabled] = QObject::tr("Read Caching Disabled");
    propertyNamesI18nFalse_[PropertyNames::vendor_device_state] = QObject::tr("Not Windows Update Capable");
    
    // Initialize object type i18n
    objectTypesI18n_[QObject::tr("VMs")] = ObjectTypes::VM;
    objectTypesI18n_[QObject::tr("XenServer Templates")] = ObjectTypes::DefaultTemplate;
    objectTypesI18n_[QObject::tr("Custom Templates")] = ObjectTypes::UserTemplate;
    objectTypesI18n_[QObject::tr("Pools")] = ObjectTypes::Pool;
    objectTypesI18n_[QObject::tr("Servers")] = ObjectTypes::Server;
    objectTypesI18n_[QObject::tr("Disconnected Servers")] = ObjectTypes::DisconnectedServer;
    objectTypesI18n_[QObject::tr("Local SRs")] = ObjectTypes::LocalSR;
    objectTypesI18n_[QObject::tr("Remote SRs")] = ObjectTypes::RemoteSR;
    objectTypesI18n_[QObject::tr("Networks")] = ObjectTypes::Network;
    objectTypesI18n_[QObject::tr("Snapshots")] = ObjectTypes::Snapshot;
    objectTypesI18n_[QObject::tr("Virtual Disks")] = ObjectTypes::VDI;
    objectTypesI18n_[QObject::tr("Folders")] = ObjectTypes::Folder;
    objectTypesI18n_[QObject::tr("VM Appliance")] = ObjectTypes::Appliance;
    
    // Initialize column sort mappings
    columnSortBy_[ColumnNames::name] = PropertyNames::label;
    columnSortBy_[ColumnNames::cpu] = PropertyNames::cpuValue;
    columnSortBy_[ColumnNames::memory] = PropertyNames::memoryValue;
    columnSortBy_[ColumnNames::disks] = PropertyNames::diskText;
    columnSortBy_[ColumnNames::network] = PropertyNames::networkText;
    columnSortBy_[ColumnNames::ha] = PropertyNames::haText;
    columnSortBy_[ColumnNames::ip] = PropertyNames::ip_address;
    columnSortBy_[ColumnNames::uptime] = PropertyNames::uptime;
    
    // Initialize property accessor functions
    // Core properties
    properties_[PropertyNames::label] = [](XenObject* o) -> QVariant {
        return o ? o->GetName() : QVariant();
    };
    
    properties_[PropertyNames::uuid] = UUIDProperty;
    properties_[PropertyNames::description] = DescriptionProperty;
    properties_[PropertyNames::type] = TypeProperty;
    
    // Relationship properties
    properties_[PropertyNames::pool] = [](XenObject* o) -> QVariant {
        if (!o || !o->GetConnection())
            return QVariant();
        
        XenCache* cache = o->GetConnection()->GetCache();
        if (!cache)
            return QVariant();
        
        // Get pool from cache - there's exactly one pool per connection
        QStringList poolRefs = cache->GetAllRefs("pool");
        if (poolRefs.isEmpty())
            return QVariant();
        
        // Return the first (and only) pool ref
        return poolRefs.first();
    };
    
    properties_[PropertyNames::host] = HostProperty;
    properties_[PropertyNames::vm] = VMProperty;
    properties_[PropertyNames::networks] = NetworksProperty;
    properties_[PropertyNames::storage] = StorageProperty;
    properties_[PropertyNames::disks] = DisksProperty;
    
    // VM-specific properties
    properties_[PropertyNames::os_name] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
            return vm->GetOSName();
        return QVariant();
    };
    
    properties_[PropertyNames::power_state] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
            return vm->GetPowerState();
        return QVariant();
    };
    
    properties_[PropertyNames::memory] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
        {
            // C# reference: XenModel/XenSearch/Common.cs line 329-339
            // Get memory_actual from VM metrics
            XenConnection* conn = vm->GetConnection();
            if (conn)
            {
                XenCache* cache = conn->GetCache();
                if (cache)
                {
                    QString metricsRef = vm->MetricsRef();
                    if (!metricsRef.isEmpty() && metricsRef != "OpaqueRef:NULL")
                    {
                        QVariantMap metrics = cache->ResolveObjectData("vm_metrics", metricsRef);
                        if (!metrics.isEmpty())
                        {
                            qint64 memoryActual = metrics.value("memory_actual").toLongLong();
                            if (memoryActual > 0)
                                return memoryActual;
                        }
                    }
                }
            }
        }
        return QVariant();
    };
    
    properties_[PropertyNames::uptime] = UptimeProperty;
    properties_[PropertyNames::ip_address] = IPAddressProperty;
    
    // VM-specific properties (continued)
    properties_[PropertyNames::tags] = [](XenObject* o) -> QVariant {
        return o ? o->GetTags().join(", ") : QVariant();
    };
    
    properties_[PropertyNames::start_time] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
        {
            qint64 startTime = vm->GetStartTime();
            if (startTime > 0)
                return QDateTime::fromSecsSinceEpoch(startTime);
        }
        return QVariant();
    };
    
    properties_[PropertyNames::ha_restart_priority] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
            return vm->HARestartPriority();
        return QVariant();
    };
    
    properties_[PropertyNames::read_caching_enabled] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
            return vm->ReadCachingEnabled();
        return QVariant();
    };
    
    properties_[PropertyNames::vendor_device_state] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
            return vm->HasVendorDeviceState();
        return QVariant();
    };
    
    properties_[PropertyNames::appliance] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
        {
            QString applianceRef = vm->ApplianceRef();
            if (!applianceRef.isEmpty())
                return applianceRef;
        }
        return QVariant();
    };
    
    properties_[PropertyNames::in_any_appliance] = [](XenObject* o) -> QVariant {
        VM* vm = qobject_cast<VM*>(o);
        if (vm && vm->IsRealVM())
        {
            QString applianceRef = vm->ApplianceRef();
            return !applianceRef.isEmpty();
        }
        return QVariant();
    };
    
    // Pool/HA properties
    properties_[PropertyNames::ha_enabled] = [](XenObject* o) -> QVariant {
        Pool* pool = qobject_cast<Pool*>(o);
        if (pool)
            return pool->HAEnabled();
        return QVariant();
    };
    
    properties_[PropertyNames::isNotFullyUpgraded] = [](XenObject* o) -> QVariant {
        Pool* pool = qobject_cast<Pool*>(o);
        if (pool && pool->GetConnection())
        {
            XenCache* cache = pool->GetConnection()->GetCache();
            if (!cache)
                return QVariant();
            
            // Check if all hosts in pool have same product version
            QStringList hostRefs = cache->GetAllRefs("host");
            if (hostRefs.isEmpty())
                return false;
            
            QString firstVersion;
            for (const QString& hostRef : hostRefs)
            {
                QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
                QVariantMap softwareVersion = hostData.value("software_version").toMap();
                QString version = softwareVersion.value("product_version").toString();
                
                if (firstVersion.isEmpty())
                    firstVersion = version;
                else if (version != firstVersion)
                    return true; // Mixed versions
            }
            return false; // All same version
        }
        return QVariant();
    };
    
    // Storage properties
    properties_[PropertyNames::sr_type] = [](XenObject* o) -> QVariant {
        SR* sr = qobject_cast<SR*>(o);
        if (sr)
            return sr->GetType();
        return QVariant();
    };
    
    properties_[PropertyNames::size] = [](XenObject* o) -> QVariant {
        VDI* vdi = qobject_cast<VDI*>(o);
        if (vdi)
            return static_cast<qint64>(vdi->VirtualSize());
        return QVariant();
    };
    
    // Display/UI properties
    properties_[PropertyNames::cpuText] = CPUTextProperty;
    properties_[PropertyNames::cpuValue] = CPUValueProperty;
    properties_[PropertyNames::memoryText] = MemoryTextProperty;
    properties_[PropertyNames::memoryValue] = MemoryValueProperty;
    properties_[PropertyNames::memoryRank] = MemoryRankProperty;
    properties_[PropertyNames::networkText] = NetworkTextProperty;
    properties_[PropertyNames::diskText] = DiskTextProperty;
    properties_[PropertyNames::haText] = HATextProperty;
    
    // Misc properties
    properties_[PropertyNames::shared] = SharedProperty;
    properties_[PropertyNames::connection_hostname] = ConnectionHostnameProperty;
    
    // Docker container parent VM
    // C# reference: XenModel/XenSearch/Common.cs line 315
    // properties[PropertyNames.dockervm] = o => o is DockerContainer dc ? new ComparableList<VM> {dc.Parent} : new ComparableList<VM>();
    properties_[PropertyNames::dockervm] = [](XenObject* o) -> QVariant {
        DockerContainer* dc = qobject_cast<DockerContainer*>(o);
        QVariantList result;
        if (dc && dc->parent())
        {
            result.append(QVariant::fromValue(dc->parent()));
        }
        return result;
    };
}

std::function<QVariant(XenObject*)> PropertyAccessors::Get(PropertyNames property)
{
    Initialize();
    return properties_.value(property);
}

QString PropertyAccessors::GetType(PropertyNames property)
{
    Initialize();
    return propertyTypes_.value(property);
}

QMap<QString, QVariant> PropertyAccessors::GetI18nFor(PropertyNames property)
{
    Initialize();
    QMap<QString, QVariant> result;
    
    switch (property)
    {
        case PropertyNames::type:
            for (auto it = objectTypesI18n_.begin(); it != objectTypesI18n_.end(); ++it)
                result[it.key()] = QVariant::fromValue(it.value());
            break;
            
        case PropertyNames::virtualisation_status:
            // Populate virtualisationStatusI18n_ on first use
            if (virtualisationStatusI18n_.isEmpty())
            {
                virtualisationStatusI18n_[QObject::tr("Not optimized")] = 0; // VM.VirtualizationStatus.NotInstalled
                virtualisationStatusI18n_[QObject::tr("Out of date")] = 1; // VM.VirtualizationStatus.PvDriversOutOfDate
                virtualisationStatusI18n_[QObject::tr("Unknown")] = 2; // VM.VirtualizationStatus.Unknown
                virtualisationStatusI18n_[QObject::tr("I/O optimized")] = 4; // VM.VirtualizationStatus.IoDriversInstalled
                virtualisationStatusI18n_[QObject::tr("Management Agent installed")] = 8; // VM.VirtualizationStatus.ManagementInstalled
                virtualisationStatusI18n_[QObject::tr("Optimized")] = 12; // IoDriversInstalled | ManagementInstalled
            }
            for (auto it = virtualisationStatusI18n_.begin(); it != virtualisationStatusI18n_.end(); ++it)
                result[it.key()] = it.value();
            break;
            
        case PropertyNames::power_state:
            // Populate vmPowerStateI18n_ on first use
            if (vmPowerStateI18n_.isEmpty())
            {
                vmPowerStateI18n_[QObject::tr("Halted")] = 0; // vm_power_state::Halted
                vmPowerStateI18n_[QObject::tr("Paused")] = 1; // vm_power_state::Paused
                vmPowerStateI18n_[QObject::tr("Running")] = 2; // vm_power_state::Running
                vmPowerStateI18n_[QObject::tr("Suspended")] = 3; // vm_power_state::Suspended
            }
            for (auto it = vmPowerStateI18n_.begin(); it != vmPowerStateI18n_.end(); ++it)
                result[it.key()] = it.value();
            break;
            
        case PropertyNames::ha_restart_priority:
            // Populate haRestartPriorityI18n_ on first use
            if (haRestartPriorityI18n_.isEmpty())
            {
                haRestartPriorityI18n_[QObject::tr("Restart if possible")] = 0; // BestEffort
                haRestartPriorityI18n_[QObject::tr("Always restart")] = 1; // Restart
                haRestartPriorityI18n_[QObject::tr("Do not restart")] = 2; // DoNotRestart
                haRestartPriorityI18n_[QObject::tr("Restart (order 1)")] = 3; // Priority 1
                haRestartPriorityI18n_[QObject::tr("Restart (order 2)")] = 4; // Priority 2
                haRestartPriorityI18n_[QObject::tr("Restart (order 3)")] = 5; // Priority 3
            }
            for (auto it = haRestartPriorityI18n_.begin(); it != haRestartPriorityI18n_.end(); ++it)
                result[it.key()] = it.value();
            break;
            
        case PropertyNames::sr_type:
            // Populate srTypeI18n_ on first use
            if (srTypeI18n_.isEmpty())
            {
                srTypeI18n_[QObject::tr("NFS VHD")] = 0; // nfs
                srTypeI18n_[QObject::tr("iSCSI")] = 1; // lvmoiscsi
                srTypeI18n_[QObject::tr("FC")] = 2; // lvmohba
                srTypeI18n_[QObject::tr("Local")] = 3; // lvm
                srTypeI18n_[QObject::tr("ISO")] = 4; // iso
                srTypeI18n_[QObject::tr("CIFS")] = 5; // cifs
                srTypeI18n_[QObject::tr("NetApp")] = 6; // netapp
                srTypeI18n_[QObject::tr("EqualLogic")] = 7; // equal
                srTypeI18n_[QObject::tr("Software iSCSI")] = 8; // lvmofcoe
                srTypeI18n_[QObject::tr("Hardware HBA")] = 9; // udev
            }
            for (auto it = srTypeI18n_.begin(); it != srTypeI18n_.end(); ++it)
                result[it.key()] = it.value();
            break;
            
        default:
            break;
    }
    
    return result;
}

PropertyNames PropertyAccessors::GetSortPropertyName(ColumnNames column)
{
    Initialize();
    return columnSortBy_.value(column);
}

QString PropertyAccessors::GetPropertyDisplayName(PropertyNames property)
{
    Initialize();
    return propertyNamesI18n_.value(property);
}

QString PropertyAccessors::GetPropertyDisplayNameFalse(PropertyNames property)
{
    Initialize();
    return propertyNamesI18nFalse_.value(property);
}

QString PropertyAccessors::GetObjectTypeDisplayName(ObjectTypes type)
{
    Initialize();
    for (auto it = objectTypesI18n_.begin(); it != objectTypesI18n_.end(); ++it)
    {
        if (it.value() == type)
            return it.key();
    }
    return QString();
}

// Property accessor implementations (stubs for now)

QVariant PropertyAccessors::DescriptionProperty(XenObject* o)
{
    return o ? o->GetDescription() : QVariant();
}

QVariant PropertyAccessors::UptimeProperty(XenObject* o)
{
    if (!o || !o->GetConnection())
        return QVariant();
    
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM())
    {
        QString powerState = vm->GetPowerState();
        if (powerState != "Running" && powerState != "Paused" && powerState != "Suspended")
            return QVariant();
        
        qint64 startTime = vm->GetStartTime();
        if (startTime == 0)
            return QVariant();
        
        // Calculate uptime: current time - start time - server time offset
        QDateTime now = QDateTime::currentDateTimeUtc();
        qint64 serverOffset = o->GetConnection()->GetServerTimeOffsetSeconds();
        qint64 uptimeSeconds = now.toSecsSinceEpoch() - startTime - serverOffset;
        
        if (uptimeSeconds < 0)
            return QVariant();
        
        // Format as human-readable duration
        return FormatDuration(uptimeSeconds);
    }
    
    Host* host = qobject_cast<Host*>(o);
    if (host)
    {
        double bootTime = host->BootTime();
        if (bootTime == 0.0)
            return QVariant();
        
        // Calculate uptime: current time - boot time - server time offset
        QDateTime now = QDateTime::currentDateTimeUtc();
        qint64 serverOffset = o->GetConnection()->GetServerTimeOffsetSeconds();
        qint64 uptimeSeconds = now.toSecsSinceEpoch() - static_cast<qint64>(bootTime) - serverOffset;
        
        if (uptimeSeconds < 0)
            return QVariant();
        
        return FormatDuration(uptimeSeconds);
    }
    
    return QVariant();
}

QString PropertyAccessors::FormatDuration(qint64 seconds)
{
    if (seconds < 60)
        return QString("%1 second%2").arg(seconds).arg(seconds == 1 ? "" : "s");
    
    qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QString("%1 minute%2").arg(minutes).arg(minutes == 1 ? "" : "s");
    
    qint64 hours = minutes / 60;
    if (hours < 24)
    {
        qint64 remainingMinutes = minutes % 60;
        if (remainingMinutes == 0)
            return QString("%1 hour%2").arg(hours).arg(hours == 1 ? "" : "s");
        return QString("%1 hour%2, %3 minute%4")
            .arg(hours).arg(hours == 1 ? "" : "s")
            .arg(remainingMinutes).arg(remainingMinutes == 1 ? "" : "s");
    }
    
    qint64 days = hours / 24;
    qint64 remainingHours = hours % 24;
    if (remainingHours == 0)
        return QString("%1 day%2").arg(days).arg(days == 1 ? "" : "s");
    return QString("%1 day%2, %3 hour%4")
        .arg(days).arg(days == 1 ? "" : "s")
        .arg(remainingHours).arg(remainingHours == 1 ? "" : "s");
}

QVariant PropertyAccessors::CPUTextProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmCpuUsageString(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostCpuUsageString(host);
    
    return QVariant();
}

QVariant PropertyAccessors::CPUValueProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmCpuUsageRank(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostCpuUsageRank(host);
    
    return QVariant();
}

QVariant PropertyAccessors::MemoryTextProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmMemoryUsageString(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostMemoryUsageString(host);
    
    VDI* vdi = qobject_cast<VDI*>(o);
    if (vdi)
        return PropertyAccessorHelper::vdiMemoryUsageString(vdi);
    
    return QVariant();
}

QVariant PropertyAccessors::MemoryValueProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmMemoryUsageValue(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostMemoryUsageValue(host);
    
    VDI* vdi = qobject_cast<VDI*>(o);
    if (vdi)
        return static_cast<double>(vdi->VirtualSize());
    
    return QVariant();
}

QVariant PropertyAccessors::MemoryRankProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmMemoryUsageRank(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostMemoryUsageRank(host);
    
    VDI* vdi = qobject_cast<VDI*>(o);
    if (vdi && vdi->VirtualSize() > 0)
    {
        qint64 physicalUtil = vdi->PhysicalUtilisation();
        qint64 virtualSize = vdi->VirtualSize();
        return static_cast<int>((physicalUtil * 100) / virtualSize);
    }
    
    return QVariant();
}

QVariant PropertyAccessors::NetworkTextProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmNetworkUsageString(vm);
    
    Host* host = qobject_cast<Host*>(o);
    if (host && host->GetConnection() && host->GetConnection()->IsConnected())
        return PropertyAccessorHelper::hostNetworkUsageString(host);
    
    return QVariant();
}

QVariant PropertyAccessors::DiskTextProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetPowerState() == "Running")
        return PropertyAccessorHelper::vmDiskUsageString(vm);
    
    return QVariant();
}

QVariant PropertyAccessors::HATextProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm)
        return PropertyAccessorHelper::GetVMHAStatus(vm);
    
    Pool* pool = qobject_cast<Pool*>(o);
    if (pool)
        return PropertyAccessorHelper::GetPoolHAStatus(pool);
    
    SR* sr = qobject_cast<SR*>(o);
    if (sr)
        return PropertyAccessorHelper::GetSRHAStatus(sr);
    
    return QVariant();
}

QVariant PropertyAccessors::UUIDProperty(XenObject* o)
{
    if (!o)
        return QVariant();
    
    // Folders don't have UUIDs, return opaque_ref instead
    if (qobject_cast<Folder*>(o))
        return o->OpaqueRef();
    
    return o->GetUUID();
}

QVariant PropertyAccessors::ConnectionHostnameProperty(XenObject* o)
{
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenConnection* conn = o->GetConnection();
    if (conn && conn->IsConnected())
        return conn->GetHostname();
    
    return QVariant();
}

QVariant PropertyAccessors::SharedProperty(XenObject* o)
{
    SR* sr = qobject_cast<SR*>(o);
    if (sr)
        return sr->IsShared();
    
    VDI* vdi = qobject_cast<VDI*>(o);
    if (vdi && vdi->GetConnection())
    {
        XenCache* cache = vdi->GetConnection()->GetCache();
        if (!cache)
            return QVariant();
        
        // VDI is shared if it's attached to 2 or more VMs
        QStringList vbdRefs = vdi->GetVBDRefs();
        int vmCount = 0;
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = cache->ResolveObjectData("VBD", vbdRef);
            QString vmRef = vbdData.value("VM").toString();
            if (!vmRef.isEmpty())
            {
                if (++vmCount >= 2)
                    return true;
            }
        }
        return false;
    }
    
    return QVariant();
}

QVariant PropertyAccessors::TypeProperty(XenObject* o)
{
    VM* vm = qobject_cast<VM*>(o);
    if (vm)
    {
        if (vm->IsSnapshot())
            return QVariant::fromValue(ObjectTypes::Snapshot);
        
        if (vm->IsTemplate())
        {
            // Distinguish default templates from user templates
            if (vm->IsDefaultTemplate())
                return QVariant::fromValue(ObjectTypes::DefaultTemplate);
            return QVariant::fromValue(ObjectTypes::UserTemplate);
        }
        
        if (vm->IsControlDomain())
            return QVariant();
        
        return QVariant::fromValue(ObjectTypes::VM);
    }
    
    if (qobject_cast<VMAppliance*>(o))
        return QVariant::fromValue(ObjectTypes::Appliance);
    
    Host* host = qobject_cast<Host*>(o);
    if (host)
    {
        // Check if host is connected
        if (host->GetConnection() && host->GetConnection()->IsConnected())
            return QVariant::fromValue(ObjectTypes::Server);
        return QVariant::fromValue(ObjectTypes::DisconnectedServer);
    }
    
    if (qobject_cast<Pool*>(o))
        return QVariant::fromValue(ObjectTypes::Pool);
    
    SR* sr = qobject_cast<SR*>(o);
    if (sr)
        return sr->IsLocal() ? QVariant::fromValue(ObjectTypes::LocalSR) : QVariant::fromValue(ObjectTypes::RemoteSR);
    
    if (qobject_cast<Network*>(o))
        return QVariant::fromValue(ObjectTypes::Network);
    
    if (qobject_cast<VDI*>(o))
        return QVariant::fromValue(ObjectTypes::VDI);
    
    if (qobject_cast<Folder*>(o))
        return QVariant::fromValue(ObjectTypes::Folder);
    
    return QVariant();
}

QVariant PropertyAccessors::NetworksProperty(XenObject* o)
{
    QStringList networkRefs;
    
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM() && vm->GetConnection())
    {
        XenCache* cache = vm->GetConnection()->GetCache();
        if (cache)
        {
            QStringList vifRefs = vm->GetVIFRefs();
            for (const QString& vifRef : vifRefs)
            {
                QVariantMap vifData = cache->ResolveObjectData("VIF", vifRef);
                QString networkRef = vifData.value("network").toString();
                if (!networkRef.isEmpty() && !networkRefs.contains(networkRef))
                    networkRefs.append(networkRef);
            }
        }
    }
    else if (qobject_cast<Network*>(o))
    {
        networkRefs.append(o->OpaqueRef());
    }
    
    return networkRefs;
}

QVariant PropertyAccessors::VMProperty(XenObject* o)
{
    QStringList vmRefs;
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenCache* cache = o->GetConnection()->GetCache();
    if (!cache)
        return QVariant();
    
    Pool* pool = qobject_cast<Pool*>(o);
    if (pool)
    {
        vmRefs = cache->GetAllRefs("VM");
    }
    else if (Host* host = qobject_cast<Host*>(o))
    {
        QStringList residentVMs = host->GetResidentVMRefs();
        vmRefs = residentVMs;
    }
    else if (SR* sr = qobject_cast<SR*>(o))
    {
        QStringList vdiRefs = sr->GetVDIRefs();
        for (const QString& vdiRef : vdiRefs)
        {
            QVariantMap vdiData = cache->ResolveObjectData("VDI", vdiRef);
            QVariantList vbdRefs = vdiData.value("VBDs").toList();
            for (const QVariant& vbdRefVar : vbdRefs)
            {
                QString vbdRef = vbdRefVar.toString();
                QVariantMap vbdData = cache->ResolveObjectData("VBD", vbdRef);
                QString vmRef = vbdData.value("VM").toString();
                if (!vmRef.isEmpty() && !vmRefs.contains(vmRef))
                    vmRefs.append(vmRef);
            }
        }
    }
    else if (Network* network = qobject_cast<Network*>(o))
    {
        QStringList vifRefs = network->GetVIFRefs();
        for (const QString& vifRef : vifRefs)
        {
            QVariantMap vifData = cache->ResolveObjectData("VIF", vifRef);
            QString vmRef = vifData.value("VM").toString();
            if (!vmRef.isEmpty() && !vmRefs.contains(vmRef))
                vmRefs.append(vmRef);
        }
    }
    else if (VDI* vdi = qobject_cast<VDI*>(o))
    {
        QStringList vbdRefs = vdi->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = cache->ResolveObjectData("VBD", vbdRef);
            QString vmRef = vbdData.value("VM").toString();
            if (!vmRef.isEmpty() && !vmRefs.contains(vmRef))
                vmRefs.append(vmRef);
        }
    }
    else if (VM* vm = qobject_cast<VM*>(o))
    {
        if (vm->IsSnapshot())
        {
            QString snapshotOf = vm->SnapshotOfRef();
            if (!snapshotOf.isEmpty())
                vmRefs.append(snapshotOf);
        }
        else
        {
            vmRefs.append(vm->OpaqueRef());
        }
    }
    
    // Filter out non-real VMs
    QStringList realVMs;
    for (const QString& vmRef : vmRefs)
    {
        QVariantMap vmData = cache->ResolveObjectData("VM", vmRef);
        bool isTemplate = vmData.value("is_a_template").toBool();
        bool isSnapshot = vmData.value("is_a_snapshot").toBool();
        bool isControlDomain = vmData.value("is_control_domain").toBool();
        
        if (!isTemplate && !isSnapshot && !isControlDomain)
            realVMs.append(vmRef);
    }
    
    return realVMs;
}

QVariant PropertyAccessors::HostProperty(XenObject* o)
{
    QStringList hostRefs;
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenCache* cache = o->GetConnection()->GetCache();
    if (!cache)
        return QVariant();
    
    // Get pool to check if we're in a pool
    QStringList poolRefs = cache->GetAllRefs("pool");
    bool inPool = !poolRefs.isEmpty();
    
    if (!inPool)
    {
        // Not in a pool - group everything under same host
        hostRefs = cache->GetAllRefs("host");
    }
    else if (VM* vm = qobject_cast<VM*>(o))
    {
        QString homeHost = vm->GetHomeRef();
        if (!homeHost.isEmpty())
            hostRefs.append(homeHost);
    }
    else if (SR* sr = qobject_cast<SR*>(o))
    {
        QString homeHost = sr->HomeRef();
        if (!homeHost.isEmpty())
            hostRefs.append(homeHost);
    }
    else if (Network* network = qobject_cast<Network*>(o))
    {
        QStringList pifRefs = network->GetPIFRefs();
        if (pifRefs.isEmpty())
            hostRefs = cache->GetAllRefs("host");
    }
    else if (Host* host = qobject_cast<Host*>(o))
    {
        hostRefs.append(host->OpaqueRef());
    }
    else if (VDI* vdi = qobject_cast<VDI*>(o))
    {
        QString srRef = vdi->SRRef();
        if (!srRef.isEmpty())
        {
            QVariantMap srData = cache->ResolveObjectData("SR", srRef);
            // Get SR.Home() - the host reference for storage repository
            QString homeRef = srData.value("home").toString();
            if (!homeRef.isEmpty() && homeRef != "OpaqueRef:NULL")
                hostRefs.append(homeRef);
        }
    }
    // Note: DockerContainer case not implemented yet as DockerContainer class not ported
    
    return hostRefs;
}

QVariant PropertyAccessors::StorageProperty(XenObject* o)
{
    QStringList srRefs;
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenCache* cache = o->GetConnection()->GetCache();
    if (!cache)
        return QVariant();
    
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM())
    {
        QStringList vbdRefs = vm->GetVBDRefs();
        for (const QString& vbdRef : vbdRefs)
        {
            QVariantMap vbdData = cache->ResolveObjectData("VBD", vbdRef);
            QString vdiRef = vbdData.value("VDI").toString();
            if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
            {
                QVariantMap vdiData = cache->ResolveObjectData("VDI", vdiRef);
                QString srRef = vdiData.value("SR").toString();
                if (!srRef.isEmpty() && !srRefs.contains(srRef))
                    srRefs.append(srRef);
            }
        }
    }
    else if (SR* sr = qobject_cast<SR*>(o))
    {
        srRefs.append(sr->OpaqueRef());
    }
    else if (VDI* vdi = qobject_cast<VDI*>(o))
    {
        QString srRef = vdi->SRRef();
        if (!srRef.isEmpty())
            srRefs.append(srRef);
    }
    
    return srRefs;
}

QVariant PropertyAccessors::DisksProperty(XenObject* o)
{
    QStringList vdiRefs;
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenCache* cache = o->GetConnection()->GetCache();
    if (!cache)
        return QVariant();
    
    if (VDI* vdi = qobject_cast<VDI*>(o))
    {
        vdiRefs.append(vdi->OpaqueRef());
    }
    else if (VM* vm = qobject_cast<VM*>(o))
    {
        if (vm->IsRealVM())
        {
            QStringList vbdRefs = vm->GetVBDRefs();
            for (const QString& vbdRef : vbdRefs)
            {
                QVariantMap vbdData = cache->ResolveObjectData("VBD", vbdRef);
                QString vdiRef = vbdData.value("VDI").toString();
                if (!vdiRef.isEmpty() && !vdiRefs.contains(vdiRef))
                    vdiRefs.append(vdiRef);
            }
        }
    }
    
    return vdiRefs;
}

QVariant PropertyAccessors::IPAddressProperty(XenObject* o)
{
    QStringList addresses;
    if (!o || !o->GetConnection())
        return QVariant();
    
    XenCache* cache = o->GetConnection()->GetCache();
    if (!cache)
        return QVariant();
    
    VM* vm = qobject_cast<VM*>(o);
    if (vm && vm->IsRealVM())
    {
        QString guestMetricsRef = vm->GetGuestMetricsRef();
        if (guestMetricsRef.isEmpty())
            return QVariant();
        
        QVariantMap metricsData = cache->ResolveObjectData("VM_guest_metrics", guestMetricsRef);
        QVariantMap networks = metricsData.value("networks").toMap();
        
        QStringList vifRefs = vm->GetVIFRefs();
        for (const QString& vifRef : vifRefs)
        {
            QVariantMap vifData = cache->ResolveObjectData("VIF", vifRef);
            QString device = vifData.value("device").toString();
            
            // Look for IP addresses in guest metrics for this device
            for (auto it = networks.begin(); it != networks.end(); ++it)
            {
                if (it.key().contains(device))
                {
                    QString ipAddr = it.value().toString();
                    if (!ipAddr.isEmpty() && !addresses.contains(ipAddr))
                        addresses.append(ipAddr);
                }
            }
        }
    }
    else if (Host* host = qobject_cast<Host*>(o))
    {
        QStringList pifRefs = host->GetPIFRefs();
        for (const QString& pifRef : pifRefs)
        {
            QVariantMap pifData = cache->ResolveObjectData("PIF", pifRef);
            QString ipAddr = pifData.value("IP").toString();
            if (!ipAddr.isEmpty() && !addresses.contains(ipAddr))
                addresses.append(ipAddr);
        }
    }
    else if (SR* sr = qobject_cast<SR*>(o))
    {
        // TODO move to SR class


        // C# reference: XenModel/XenAPI-Extensions/SR.cs Target() method
        // Get target IP from PBD device_config (iSCSI target, NFS server, etc.)
        XenConnection* conn = sr->GetConnection();
        if (conn)
        {
            XenCache* cache = conn->GetCache();
            if (cache)
            {
                QVariantMap srData = sr->GetData();
                QStringList pbdRefs = srData.value("PBDs").toStringList();
                QString srType = sr->GetType();
                
                for (const QString& pbdRef : pbdRefs)
                {
                    QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
                    if (pbdData.isEmpty())
                        continue;
                    
                    QVariantMap deviceConfig = pbdData.value("device_config").toMap();
                    QString target;
                    
                    if (srType == "lvmoiscsi" && deviceConfig.contains("target"))
                    {
                        // iSCSI target
                        target = deviceConfig.value("target").toString();
                    }
                    else if (srType == "iso" && deviceConfig.contains("location"))
                    {
                        // CIFS or NFS ISO - extract hostname from location
                        QString location = deviceConfig.value("location").toString();
                        // Location has form //ip_address/path
                        if (location.startsWith("//"))
                        {
                            int slashPos = location.indexOf('/', 2);
                            if (slashPos > 0)
                                target = location.mid(2, slashPos - 2);
                            else
                                target = location.mid(2);
                        }
                    }
                    else if (srType == "nfs" && deviceConfig.contains("server"))
                    {
                        // NFS server
                        target = deviceConfig.value("server").toString();
                    }
                    
                    if (!target.isEmpty())
                    {
                        // Just add the target string directly (it's already an IP/hostname)
                        if (!addresses.contains(target))
                            addresses.append(target);
                        break;  // Found target, no need to check other PBDs
                    }
                }
            }
        }
    }
    
    return addresses.isEmpty() ? QVariant() : addresses;
}

// PropertyWrapper implementation

PropertyWrapper::PropertyWrapper(PropertyNames property, XenObject* object)
    : property_(PropertyAccessors::Get(property)),
      object_(object)
{
}

QString PropertyWrapper::ToString() const
{
    if (!property_ || !object_)
        return "-";
    
    QVariant value = property_(object_);
    
    if (!value.isValid() || value.isNull())
        return "-";
    
    return value.toString();
}

} // namespace XenSearch
