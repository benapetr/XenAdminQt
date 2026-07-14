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

#include "xenobject.h"
#include "network/connection.h"
#include "pool.h"
#include "../xencache.h"

int XenObject::totalObjects = 0;

bool XenObject::ValueIsNULL(const QString &value)
{
    return value.isEmpty() || value == XENOBJECT_NULL;
}

XenObjectType XenObject::TypeFromString(const QString &name)
{
    const QString n = name.trimmed().toLower();

    if (n == "blob" || n == "blobs") return XenObjectType::Blob;
    if (n == "bond" || n == "bonds") return XenObjectType::Bond;
    if (n == "certificate" || n == "certificates") return XenObjectType::Certificate;
    if (n == "cluster" || n == "clusters") return XenObjectType::Cluster;
    if (n == "cluster_host" || n == "cluster_hosts") return XenObjectType::ClusterHost;
    if (n == "console" || n == "consoles") return XenObjectType::Console;
    if (n == "disconnected_host" || n == "disconnected_hosts") return XenObjectType::DisconnectedHost;
    if (n == "dockercontainer" || n == "dockercontainers" || n == "docker_container" || n == "docker_containers") return XenObjectType::DockerContainer;
    if (n == "event" || n == "events") return XenObjectType::Event;
    if (n == "feature" || n == "features") return XenObjectType::Feature;
    if (n == "folder" || n == "folders") return XenObjectType::Folder;
    if (n == "gpu_group" || n == "gpu_groups" || n == "gpugroup" || n == "gpugroups") return XenObjectType::GPUGroup;
    if (n == "host" || n == "hosts") return XenObjectType::Host;
    if (n == "host_cpu" || n == "host_cpus") return XenObjectType::HostCPU;
    if (n == "host_crashdump" || n == "host_crashdumps") return XenObjectType::HostCrashdump;
    if (n == "host_metrics") return XenObjectType::HostMetrics;
    if (n == "host_patch" || n == "host_patches") return XenObjectType::HostPatch;
    if (n == "message" || n == "messages") return XenObjectType::Message;
    if (n == "network" || n == "networks") return XenObjectType::Network;
    if (n == "network_sriov" || n == "network_sriovs") return XenObjectType::NetworkSriov;
    if (n == "pbd" || n == "pbds") return XenObjectType::PBD;
    if (n == "pci" || n == "pcis") return XenObjectType::PCI;
    if (n == "pif" || n == "pifs") return XenObjectType::PIF;
    if (n == "pif_metrics") return XenObjectType::PIFMetrics;
    if (n == "pgpu" || n == "pgpus") return XenObjectType::PGPU;
    if (n == "pool" || n == "pools") return XenObjectType::Pool;
    if (n == "pool_patch" || n == "pool_patches") return XenObjectType::PoolPatch;
    if (n == "pool_update" || n == "pool_updates") return XenObjectType::PoolUpdate;
    if (n == "role" || n == "roles") return XenObjectType::Role;
    if (n == "sm" || n == "sms") return XenObjectType::SM;
    if (n == "sr" || n == "srs") return XenObjectType::SR;
    if (n == "task" || n == "tasks") return XenObjectType::Task;
    if (n == "tunnel" || n == "tunnels") return XenObjectType::Tunnel;
    if (n == "usb_group" || n == "usb_groups" || n == "usbgroup" || n == "usbgroups") return XenObjectType::USBGroup;
    if (n == "user" || n == "users") return XenObjectType::User;
    if (n == "vbd" || n == "vbds") return XenObjectType::VBD;
    if (n == "vbd_metrics") return XenObjectType::VBDMetrics;
    if (n == "vdi" || n == "vdis") return XenObjectType::VDI;
    if (n == "vgpu" || n == "vgpus") return XenObjectType::VGPU;
    if (n == "vgpu_type" || n == "vgpu_types" || n == "vgputype" || n == "vgputypes") return XenObjectType::VGPUType;
    if (n == "vif" || n == "vifs") return XenObjectType::VIF;
    if (n == "vlan" || n == "vlans") return XenObjectType::VLAN;
    if (n == "vm" || n == "vms") return XenObjectType::VM;
    if (n == "vm_appliance" || n == "vm_appliances" || n == "vmappliance" || n == "vmappliances" || n == "vm appliance" || n == "vm appliances") return XenObjectType::VMAppliance;
    if (n == "vm_guest_metrics") return XenObjectType::VMGuestMetrics;
    if (n == "vm_metrics") return XenObjectType::VMMetrics;
    if (n == "vmpp") return XenObjectType::VMPP;
    if (n == "vmss") return XenObjectType::VMSS;
    if (n == "vtpm" || n == "vtpms") return XenObjectType::VTPM;
    if (n == "vusb" || n == "vusbs") return XenObjectType::VUSB;
    if (n == "pusb" || n == "pusbs") return XenObjectType::PUSB;

    return XenObjectType::Null;
}

QString XenObject::TypeToString(XenObjectType type)
{
    switch (type)
    {
    case XenObjectType::Null: return "null";
    case XenObjectType::Blob: return "blob";
    case XenObjectType::Bond: return "bond";
    case XenObjectType::Certificate: return "certificate";
    case XenObjectType::Cluster: return "cluster";
    case XenObjectType::ClusterHost: return "cluster_host";
    case XenObjectType::Console: return "console";
    case XenObjectType::DockerContainer: return "docker_container";
    case XenObjectType::Event: return "event";
    case XenObjectType::Feature: return "feature";
    case XenObjectType::Folder: return "folder";
    case XenObjectType::GPUGroup: return "GPU_group";
    case XenObjectType::Host: return "host";
    case XenObjectType::DisconnectedHost: return "host";
    case XenObjectType::HostCPU: return "host_cpu";
    case XenObjectType::HostCrashdump: return "host_crashdump";
    case XenObjectType::HostMetrics: return "host_metrics";
    case XenObjectType::HostPatch: return "host_patch";
    case XenObjectType::Message: return "message";
    case XenObjectType::Network: return "network";
    case XenObjectType::NetworkSriov: return "network_sriov";
    case XenObjectType::PBD: return "pbd";
    case XenObjectType::PCI: return "pci";
    case XenObjectType::PIF: return "pif";
    case XenObjectType::PIFMetrics: return "pif_metrics";
    case XenObjectType::PGPU: return "pgpu";
    case XenObjectType::Pool: return "pool";
    case XenObjectType::PoolPatch: return "pool_patch";
    case XenObjectType::PoolUpdate: return "pool_update";
    case XenObjectType::Role: return "role";
    case XenObjectType::SM: return "SM";
    case XenObjectType::SR: return "sr";
    case XenObjectType::Task: return "task";
    case XenObjectType::Tunnel: return "tunnel";
    case XenObjectType::USBGroup: return "USB_group";
    case XenObjectType::User: return "user";
    case XenObjectType::VBD: return "vbd";
    case XenObjectType::VBDMetrics: return "vbd_metrics";
    case XenObjectType::VDI: return "vdi";
    case XenObjectType::VGPU: return "vgpu";
    case XenObjectType::VGPUType: return "vgpu_type";
    case XenObjectType::VIF: return "vif";
    case XenObjectType::VLAN: return "vlan";
    case XenObjectType::VM: return "vm";
    case XenObjectType::VMAppliance: return "VM_appliance";
    case XenObjectType::VMGuestMetrics: return "vm_guest_metrics";
    case XenObjectType::VMMetrics: return "vm_metrics";
    case XenObjectType::VMPP: return "vmpp";
    case XenObjectType::VMSS: return "vmss";
    case XenObjectType::VTPM: return "vtpm";
    case XenObjectType::VUSB: return "vusb";
    case XenObjectType::PUSB: return "PUSB";
    }
    return "null";
}

XenObject::XenObject(XenConnection* connection, const QString& opaqueRef, QObject* parent) : QObject(parent), m_connection(connection), m_opaqueRef(opaqueRef)
{
    XenObject::totalObjects++;
    if (connection)
        this->m_cache = connection->GetCache();
    else
        this->m_cache = XenCache::GetDummy();
}

XenObject::~XenObject()
{
    XenObject::totalObjects--;
}

QString XenObject::GetUUID() const
{
    return this->stringProperty("uuid");
}

QString XenObject::GetName() const
{
    return this->stringProperty("name_label");
}

QString XenObject::GetNameWithLocation() const
{
    const QString name = this->GetName();
    const QString location = this->GetLocationString();

    if (location.isEmpty())
        return name;
    if (name.isEmpty())
        return location;

    return QString("%1 %2").arg(name, location);
}

QString XenObject::GetLocationString() const
{
    if (!this->m_connection)
        return QString();

    XenCache* cache = this->m_connection->GetCache();
    if (cache)
    {
        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (pool && !pool->GetName().isEmpty())
            return QString("in '%1'").arg(pool->GetName());
    }

    const QString hostname = this->m_connection->GetHostname();
    if (hostname.isEmpty())
        return QString();

    return QString("on '%1'").arg(hostname);
}

QString XenObject::GetDescription() const
{
    return this->stringProperty("name_description");
}

QStringList XenObject::GetTags() const
{
    return this->stringListProperty("tags");
}

bool XenObject::IsLocked() const
{
    return this->m_locked;;
}

void XenObject::Lock()
{
    this->m_locked = true;
}

void XenObject::Unlock()
{
    this->m_locked = false;
}

XenConnection *XenObject::GetConnection() const
{
    return this->m_connection;
}

bool XenObject::IsConnected() const
{
    if (!this->m_connection)
        return false;
    return this->m_connection->IsConnected();
}

QVariantMap XenObject::GetOtherConfig() const
{
    return this->property("other_config").toMap();
}

QString XenObject::GetFolderPath() const
{
    QVariantMap otherConfig = this->GetOtherConfig();
    return otherConfig.value("folder").toString();
}

bool XenObject::IsHidden() const
{
    QVariantMap otherConfig = this->GetOtherConfig();
    QVariant hiddenValue = otherConfig.value("HideFromXenCenter");
    if (!hiddenValue.isValid())
        hiddenValue = otherConfig.value("hide_from_xencenter");

    const QString hidden = hiddenValue.toString().trimmed().toLower();

    return hidden == "true" || hidden == "1";
}

QString XenObject::GetObjectTypeName() const
{
    return TypeToString(this->GetObjectType());
}

XenObjectType XenObject::GetObjectType() const
{
    return XenObjectType::Null;
}

QVariantMap XenObject::GetData() const
{
    if (this->m_opaqueRef.isEmpty())
        return this->m_localData;

    if (!this->m_cache)
        return QVariantMap();

    return this->m_cache->ResolveObjectData(this->GetObjectType(), this->m_opaqueRef);
}

void XenObject::SetLocalData(const QVariantMap& data)
{
    this->m_localData = data;
}

void XenObject::Refresh()
{
    emit DataChanged();
}

bool XenObject::IsValid() const
{
    return !this->GetData().isEmpty();
}

QVariant XenObject::property(const QString& key, const QVariant& defaultValue) const
{
    QVariantMap d = this->GetData();
    return d.value(key, defaultValue);
}

QString XenObject::stringProperty(const QString& key, const QString& defaultValue) const
{
    return property(key, defaultValue).toString();
}

bool XenObject::boolProperty(const QString& key, bool defaultValue) const
{
    return property(key, defaultValue).toBool();
}

int XenObject::intProperty(const QString& key, int defaultValue) const
{
    return property(key, defaultValue).toInt();
}

qint64 XenObject::longProperty(const QString& key, qint64 defaultValue) const
{
    return property(key, defaultValue).toLongLong();
}

QStringList XenObject::stringListProperty(const QString& key) const
{
    QVariant value = property(key);
    if (value.canConvert<QStringList>())
        return value.toStringList();

    // Handle QVariantList (from JSON arrays)
    if (value.canConvert<QVariantList>())
    {
        QStringList result;
        QVariantList list = value.toList();
        for (const QVariant& item : list)
            result << item.toString();
        return result;
    }

    return QStringList();
}
