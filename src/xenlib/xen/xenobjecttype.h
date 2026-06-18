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

#ifndef XENOBJECTTYPE_H
#define XENOBJECTTYPE_H

#include <QMetaType>
#include <QtGlobal>

enum class XenObjectType
{
    Null,
    Blob,
    Bond,
    Certificate,
    Cluster,
    ClusterHost,
    Console,
    DockerContainer,
    Event,
    Feature,
    Folder,
    GPUGroup,
    Host,
    DisconnectedHost, // This is a special case needed by commands system
    HostCPU,
    HostCrashdump,
    HostMetrics,
    HostPatch,
    Message,
    Network,
    NetworkSriov,
    PBD,
    PCI,
    PIF,
    PIFMetrics,
    PGPU,
    Pool,
    PoolPatch,
    PoolUpdate,
    Role,
    SM,
    SR,
    Task,
    Tunnel,
    USBGroup,
    User,
    VBD,
    VBDMetrics,
    VDI,
    VGPU,
    VGPUType,
    VIF,
    VLAN,
    VM,
    VMAppliance,
    VMGuestMetrics,
    VMMetrics,
    VMPP,
    VMSS,
    VTPM,
    VUSB,
    PUSB
};

// Hash function for XenObjectType to support QSet/QHash in both Qt5 and Qt6
inline uint qHash(XenObjectType key, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(key), seed);
}

Q_DECLARE_METATYPE(XenObjectType)

#include <QString>

/// Convert the lowercase XAPI class name (as used in XenServer event streams)
/// to the corresponding XenObjectType.  Returns XenObjectType::Null when the
/// name is unknown.
inline XenObjectType XenObjectTypeFromString(const QString& name)
{
    // Lower-case comparison to match the XAPI event "class" field.
    const QString n = name.toLower();
    if (n == "vm")              return XenObjectType::VM;
    if (n == "host")            return XenObjectType::Host;
    if (n == "pool")            return XenObjectType::Pool;
    if (n == "sr")              return XenObjectType::SR;
    if (n == "network")         return XenObjectType::Network;
    if (n == "vbd")             return XenObjectType::VBD;
    if (n == "vdi")             return XenObjectType::VDI;
    if (n == "vif")             return XenObjectType::VIF;
    if (n == "pif")             return XenObjectType::PIF;
    if (n == "pbd")             return XenObjectType::PBD;
    if (n == "pif_metrics")     return XenObjectType::PIFMetrics;
    if (n == "vbd_metrics")     return XenObjectType::VBDMetrics;
    if (n == "host_metrics")    return XenObjectType::HostMetrics;
    if (n == "host_cpu")        return XenObjectType::HostCPU;
    if (n == "host_crashdump")  return XenObjectType::HostCrashdump;
    if (n == "host_patch")      return XenObjectType::HostPatch;
    if (n == "task")            return XenObjectType::Task;
    if (n == "console")         return XenObjectType::Console;
    if (n == "vm_guest_metrics") return XenObjectType::VMGuestMetrics;
    if (n == "vm_metrics")      return XenObjectType::VMMetrics;
    if (n == "vm_appliance")    return XenObjectType::VMAppliance;
    if (n == "sm")              return XenObjectType::SM;
    if (n == "role")            return XenObjectType::Role;
    if (n == "user")            return XenObjectType::User;
    if (n == "bond")            return XenObjectType::Bond;
    if (n == "vlan")            return XenObjectType::VLAN;
    if (n == "tunnel")          return XenObjectType::Tunnel;
    if (n == "message")         return XenObjectType::Message;
    if (n == "certificate")     return XenObjectType::Certificate;
    if (n == "cluster")         return XenObjectType::Cluster;
    if (n == "cluster_host")    return XenObjectType::ClusterHost;
    if (n == "feature")         return XenObjectType::Feature;
    if (n == "pgpu")            return XenObjectType::PGPU;
    if (n == "pci")             return XenObjectType::PCI;
    if (n == "gpu_group")       return XenObjectType::GPUGroup;
    if (n == "vgpu")            return XenObjectType::VGPU;
    if (n == "vgpu_type")       return XenObjectType::VGPUType;
    if (n == "network_sriov")   return XenObjectType::NetworkSriov;
    if (n == "pusb")            return XenObjectType::PUSB;
    if (n == "usb_group")       return XenObjectType::USBGroup;
    if (n == "vusb")            return XenObjectType::VUSB;
    if (n == "vtpm")            return XenObjectType::VTPM;
    if (n == "blob")            return XenObjectType::Blob;
    if (n == "pool_patch")      return XenObjectType::PoolPatch;
    if (n == "pool_update")     return XenObjectType::PoolUpdate;
    if (n == "vmpp")            return XenObjectType::VMPP;
    if (n == "vmss")            return XenObjectType::VMSS;
    return XenObjectType::Null;
}

#endif // XENOBJECTTYPE_H
