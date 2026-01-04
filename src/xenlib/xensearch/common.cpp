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
QMap<int, QString> PropertyAccessors::vmPowerStateImages_;
QMap<ObjectTypes, QString> PropertyAccessors::objectTypesImages_;
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
    
    // Initialize object type icons (using icon names that will be resolved later)
    objectTypesImages_[ObjectTypes::DefaultTemplate] = "template";
    objectTypesImages_[ObjectTypes::UserTemplate] = "template-user";
    objectTypesImages_[ObjectTypes::Pool] = "pool";
    objectTypesImages_[ObjectTypes::Server] = "host";
    objectTypesImages_[ObjectTypes::DisconnectedServer] = "host-disconnected";
    objectTypesImages_[ObjectTypes::LocalSR] = "storage";
    objectTypesImages_[ObjectTypes::RemoteSR] = "storage";
    objectTypesImages_[ObjectTypes::LocalSR | ObjectTypes::RemoteSR] = "storage";
    objectTypesImages_[ObjectTypes::VM] = "vm";
    objectTypesImages_[ObjectTypes::Network] = "network";
    objectTypesImages_[ObjectTypes::Snapshot] = "snapshot";
    objectTypesImages_[ObjectTypes::VDI] = "vdi";
    objectTypesImages_[ObjectTypes::Folder] = "folder";
    objectTypesImages_[ObjectTypes::Appliance] = "vm-appliance";
    
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
        // TODO: Get pool from connection
        return QVariant();
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
            // TODO: Get actual memory from metrics
            return QVariant();
        }
        return QVariant();
    };
    
    properties_[PropertyNames::uptime] = UptimeProperty;
    properties_[PropertyNames::ip_address] = IPAddressProperty;
    
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
            
        // TODO: Add other enum property i18n maps
        default:
            break;
    }
    
    return result;
}

std::function<QIcon(const QVariant&)> PropertyAccessors::GetImagesFor(PropertyNames property)
{
    Initialize();
    
    switch (property)
    {
        case PropertyNames::type:
            return [](const QVariant& value) -> QIcon {
                // TODO: Implement icon lookup
                Q_UNUSED(value);
                return QIcon();
            };
            
        default:
            return nullptr;
    }
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

QIcon PropertyAccessors::GetObjectTypeIcon(ObjectTypes type)
{
    Initialize();
    QString iconName = objectTypesImages_.value(type);
    // TODO: Load actual icon from IconManager
    return QIcon();
}

// Property accessor implementations (stubs for now)

QVariant PropertyAccessors::DescriptionProperty(XenObject* o)
{
    return o ? o->GetDescription() : QVariant();
}

QVariant PropertyAccessors::UptimeProperty(XenObject* o)
{
    // TODO: Implement uptime calculation
    // VM needs: GetStartTime() - DateTime.UtcNow - Connection.ServerTimeOffset
    // Host needs: BootTime() converted from Unix time
    Q_UNUSED(o);
    return QVariant();
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
        QStringList vbdRefs = vdi->VBDRefs();
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
            // TODO: Check if default template
            return QVariant::fromValue(ObjectTypes::UserTemplate);
        }
        
        if (vm->IsControlDomain())
            return QVariant();
        
        return QVariant::fromValue(ObjectTypes::VM);
    }
    
    if (qobject_cast<VMAppliance*>(o))
        return QVariant::fromValue(ObjectTypes::Appliance);
    
    if (qobject_cast<Host*>(o))
    {
        // TODO: Check if connected
        return QVariant::fromValue(ObjectTypes::Server);
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
            QStringList vifRefs = vm->VIFRefs();
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
        QStringList residentVMs = host->ResidentVMRefs();
        vmRefs = residentVMs;
    }
    else if (SR* sr = qobject_cast<SR*>(o))
    {
        QStringList vdiRefs = sr->VDIRefs();
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
        QStringList vbdRefs = vdi->VBDRefs();
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
        QString homeHost = vm->HomeRef();
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
            // TODO: Get SR.Home() - for now just skip
        }
    }
    
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
        QStringList vbdRefs = vm->VBDRefs();
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
            QStringList vbdRefs = vm->VBDRefs();
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
        QString guestMetricsRef = vm->GuestMetricsRef();
        if (guestMetricsRef.isEmpty())
            return QVariant();
        
        QVariantMap metricsData = cache->ResolveObjectData("VM_guest_metrics", guestMetricsRef);
        QVariantMap networks = metricsData.value("networks").toMap();
        
        QStringList vifRefs = vm->VIFRefs();
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
        QStringList pifRefs = host->PIFRefs();
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
        // TODO: Get SR target IP address
        // This requires implementing SR::Target() method
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
