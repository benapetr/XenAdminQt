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

#ifndef XENOBJECTTRAITS_H
#define XENOBJECTTRAITS_H

#include "xenobjecttype.h"

class Blob;
class Bond;
class Certificate;
class Cluster;
class ClusterHost;
class Console;
class DockerContainer;
class Event;
class Feature;
class Folder;
class GPUGroup;
class Host;
class HostCPU;
class HostCrashdump;
class HostMetrics;
class HostPatch;
class Message;
class Network;
class NetworkSriov;
class PBD;
class PCI;
class PIF;
class PIFMetrics;
class PGPU;
class Pool;
class PoolPatch;
class PoolUpdate;
class Role;
class SM;
class SR;
class Task;
class Tunnel;
class USBGroup;
class User;
class VBD;
class VBDMetrics;
class VDI;
class VGPU;
class VIF;
class VLAN;
class VM;
class VMAppliance;
class VMMetrics;
class VMGuestMetrics;
class VMPP;
class VMSS;
class VTPM;
class VUSB;
class PUSB;

template <typename T>
struct XenObjectTraits
{
    static constexpr XenObjectType kType = XenObjectType::Null;
};

#define XEN_OBJECT_TRAIT(TypeName, EnumValue) \
    template <>                               \
    struct XenObjectTraits<TypeName>          \
    {                                         \
        static constexpr XenObjectType kType = XenObjectType::EnumValue; \
    }

XEN_OBJECT_TRAIT(Blob, Blob);
XEN_OBJECT_TRAIT(Bond, Bond);
XEN_OBJECT_TRAIT(Certificate, Certificate);
XEN_OBJECT_TRAIT(Cluster, Cluster);
XEN_OBJECT_TRAIT(ClusterHost, ClusterHost);
XEN_OBJECT_TRAIT(Console, Console);
XEN_OBJECT_TRAIT(DockerContainer, DockerContainer);
XEN_OBJECT_TRAIT(Event, Event);
XEN_OBJECT_TRAIT(Feature, Feature);
XEN_OBJECT_TRAIT(Folder, Folder);
XEN_OBJECT_TRAIT(GPUGroup, GPUGroup);
XEN_OBJECT_TRAIT(Host, Host);
XEN_OBJECT_TRAIT(HostCPU, HostCPU);
XEN_OBJECT_TRAIT(HostCrashdump, HostCrashdump);
XEN_OBJECT_TRAIT(HostMetrics, HostMetrics);
XEN_OBJECT_TRAIT(HostPatch, HostPatch);
XEN_OBJECT_TRAIT(Message, Message);
XEN_OBJECT_TRAIT(Network, Network);
XEN_OBJECT_TRAIT(NetworkSriov, NetworkSriov);
XEN_OBJECT_TRAIT(PBD, PBD);
XEN_OBJECT_TRAIT(PCI, PCI);
XEN_OBJECT_TRAIT(PIF, PIF);
XEN_OBJECT_TRAIT(PIFMetrics, PIFMetrics);
XEN_OBJECT_TRAIT(PGPU, PGPU);
XEN_OBJECT_TRAIT(Pool, Pool);
XEN_OBJECT_TRAIT(PoolPatch, PoolPatch);
XEN_OBJECT_TRAIT(PoolUpdate, PoolUpdate);
XEN_OBJECT_TRAIT(Role, Role);
XEN_OBJECT_TRAIT(SM, SM);
XEN_OBJECT_TRAIT(SR, SR);
XEN_OBJECT_TRAIT(Task, Task);
XEN_OBJECT_TRAIT(Tunnel, Tunnel);
XEN_OBJECT_TRAIT(USBGroup, USBGroup);
XEN_OBJECT_TRAIT(User, User);
XEN_OBJECT_TRAIT(VBD, VBD);
XEN_OBJECT_TRAIT(VBDMetrics, VBDMetrics);
XEN_OBJECT_TRAIT(VDI, VDI);
XEN_OBJECT_TRAIT(VGPU, VGPU);
XEN_OBJECT_TRAIT(VIF, VIF);
XEN_OBJECT_TRAIT(VLAN, VLAN);
XEN_OBJECT_TRAIT(VM, VM);
XEN_OBJECT_TRAIT(VMAppliance, VMAppliance);
XEN_OBJECT_TRAIT(VMMetrics, VMMetrics);
XEN_OBJECT_TRAIT(VMGuestMetrics, VMGuestMetrics);
XEN_OBJECT_TRAIT(VMPP, VMPP);
XEN_OBJECT_TRAIT(VMSS, VMSS);
XEN_OBJECT_TRAIT(VTPM, VTPM);
XEN_OBJECT_TRAIT(VUSB, VUSB);
XEN_OBJECT_TRAIT(PUSB, PUSB);

#undef XEN_OBJECT_TRAIT

#endif // XENOBJECTTRAITS_H
