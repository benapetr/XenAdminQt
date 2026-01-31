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

#endif // XENOBJECTTYPE_H
