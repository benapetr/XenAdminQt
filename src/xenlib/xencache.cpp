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

#include "xencache.h"
#include "xen/network.h"
#include "xen/network/connection.h"
#include "xen/bond.h"
#include "xen/certificate.h"
#include "xen/console.h"
#include "xen/gpugroup.h"
#include "xen/host.h"
#include "xen/hostcpu.h"
#include "xen/hostmetrics.h"
#include "xen/vmguestmetrics.h"
#include "xen/vmmetrics.h"
#include "xen/message.h"
#include "xen/network_sriov.h"
#include "xen/folder.h"
#include "xen/pbd.h"
#include "xen/pci.h"
#include "xen/pgpu.h"
#include "xen/pif.h"
#include "xen/pifmetrics.h"
#include "xen/pool.h"
#include "xen/role.h"
#include "xen/sr.h"
#include "xen/task.h"
#include "xen/tunnel.h"
#include "xen/vbd.h"
#include "xen/vbdmetrics.h"
#include "xen/vdi.h"
#include "xen/vif.h"
#include "xen/vlan.h"
#include "xen/vm.h"
#include <QDebug>
#include <QMutexLocker>

XenCache *XenCache::dummyCache = nullptr;

XenCache *XenCache::GetDummy()
{
    if (!XenCache::dummyCache)
        XenCache::dummyCache = new XenCache(nullptr);
    return XenCache::dummyCache;
}

XenCache::XenCache(XenConnection* connection) : QObject(connection)
{
    this->m_connection = connection;
}

XenCache::~XenCache()
{
    this->Clear();
}

XenObjectType XenCache::TypeFromString(const QString& type)
{
    // Normalize type names to lowercase for consistency
    // C# uses capitalized names, we'll use lowercase internally
    // Types are sorted alphabetically for easier maintenance
    const QString normalized = type.toLower();

    // Handle variations (alphabetically sorted)
    if (normalized == "blob" || normalized == "blobs")
        return XenObjectType::Blob;
    if (normalized == "bond" || normalized == "bonds")
        return XenObjectType::Bond;
    if (normalized == "certificate" || normalized == "certificates")
        return XenObjectType::Certificate;
    if (normalized == "cluster" || normalized == "clusters")
        return XenObjectType::Cluster;
    if (normalized == "cluster_host" || normalized == "cluster_hosts")
        return XenObjectType::ClusterHost;
    if (normalized == "console" || normalized == "consoles")
        return XenObjectType::Console;
    if (normalized == "dockercontainer" || normalized == "dockercontainers"
        || normalized == "docker_container" || normalized == "docker_containers")
        return XenObjectType::DockerContainer;
    if (normalized == "event" || normalized == "events")
        return XenObjectType::Event;
    if (normalized == "feature" || normalized == "features")
        return XenObjectType::Feature;
    if (normalized == "folder" || normalized == "folders")
        return XenObjectType::Folder;
    if (normalized == "gpu_group" || normalized == "gpu_groups" || normalized == "gpugroup" || normalized == "gpugroups")
        return XenObjectType::GPUGroup;
    if (normalized == "host" || normalized == "hosts")
        return XenObjectType::Host;
    if (normalized == "host_cpu" || normalized == "host_cpus")
        return XenObjectType::HostCPU;
    if (normalized == "host_crashdump" || normalized == "host_crashdumps")
        return XenObjectType::HostCrashdump;
    if (normalized == "host_metrics")
        return XenObjectType::HostMetrics;
    if (normalized == "host_patch" || normalized == "host_patches")
        return XenObjectType::HostPatch;
    if (normalized == "message" || normalized == "messages")
        return XenObjectType::Message;
    if (normalized == "network" || normalized == "networks")
        return XenObjectType::Network;
    if (normalized == "network_sriov" || normalized == "network_sriovs")
        return XenObjectType::NetworkSriov;
    if (normalized == "pbd" || normalized == "pbds")
        return XenObjectType::PBD;
    if (normalized == "pci" || normalized == "pcis")
        return XenObjectType::PCI;
    if (normalized == "pif" || normalized == "pifs")
        return XenObjectType::PIF;
    if (normalized == "pif_metrics")
        return XenObjectType::PIFMetrics;
    if (normalized == "pgpu" || normalized == "pgpus")
        return XenObjectType::PGPU;
    if (normalized == "pool" || normalized == "pools")
        return XenObjectType::Pool;
    if (normalized == "pool_patch" || normalized == "pool_patches")
        return XenObjectType::PoolPatch;
    if (normalized == "pool_update" || normalized == "pool_updates")
        return XenObjectType::PoolUpdate;
    if (normalized == "role" || normalized == "roles")
        return XenObjectType::Role;
    if (normalized == "sm" || normalized == "sms")
        return XenObjectType::SM;
    if (normalized == "sr" || normalized == "srs")
        return XenObjectType::SR;
    if (normalized == "task" || normalized == "tasks")
        return XenObjectType::Task;
    if (normalized == "tunnel" || normalized == "tunnels")
        return XenObjectType::Tunnel;
    if (normalized == "usb_group" || normalized == "usb_groups" || normalized == "usbgroup" || normalized == "usbgroups")
        return XenObjectType::USBGroup;
    if (normalized == "user" || normalized == "users")
        return XenObjectType::User;
    if (normalized == "vbd" || normalized == "vbds")
        return XenObjectType::VBD;
    if (normalized == "vbd_metrics")
        return XenObjectType::VBDMetrics;
    if (normalized == "vdi" || normalized == "vdis")
        return XenObjectType::VDI;
    if (normalized == "vgpu" || normalized == "vgpus")
        return XenObjectType::VGPU;
    if (normalized == "vif" || normalized == "vifs")
        return XenObjectType::VIF;
    if (normalized == "vlan" || normalized == "vlans")
        return XenObjectType::VLAN;
    if (normalized == "vm" || normalized == "vms")
        return XenObjectType::VM;
    if (normalized == "vm_appliance" || normalized == "vm_appliances")
        return XenObjectType::VMAppliance;
    if (normalized == "vm_guest_metrics")
        return XenObjectType::VMGuestMetrics;
    if (normalized == "vm_metrics")
        return XenObjectType::VMMetrics;
    if (normalized == "vmpp")
        return XenObjectType::VMPP;
    if (normalized == "vmss")
        return XenObjectType::VMSS;
    if (normalized == "vtpm" || normalized == "vtpms")
        return XenObjectType::VTPM;
    if (normalized == "vusb" || normalized == "vusbs")
        return XenObjectType::VUSB;
    if (normalized == "pusb" || normalized == "pusbs")
        return XenObjectType::PUSB;

    return XenObjectType::Null;
}

QString XenCache::TypeToCacheString(XenObjectType type)
{
    return XenObject::TypeToString(type).toLower();
}

XenObjectType XenCache::normalizeType(const QString& type) const
{
    return XenCache::TypeFromString(type);
}

QString XenCache::typeToCacheString(XenObjectType type) const
{
    return XenCache::TypeToCacheString(type);
}

QVariantMap XenCache::ResolveObjectData(const QString& type, const QString& ref) const
{
    if (ref.isEmpty())
    {
        return QVariantMap();
    }

    QMutexLocker locker(&this->m_mutex);

    XenObjectType normalizedType = normalizeType(type);
    if (normalizedType == XenObjectType::Null)
        return QVariantMap();

    if (!this->m_cache.contains(normalizedType))
    {
        return QVariantMap();
    }

    const QMap<QString, QVariantMap>& typeCache = this->m_cache[normalizedType];
    if (!typeCache.contains(ref))
    {
        return QVariantMap();
    }

    return typeCache[ref];
}

QVariantMap XenCache::ResolveObjectData(XenObjectType type, const QString& ref) const
{
    if (ref.isEmpty() || type == XenObjectType::Null)
        return QVariantMap();

    QMutexLocker locker(&this->m_mutex);

    if (!this->m_cache.contains(type))
        return QVariantMap();

    const QMap<QString, QVariantMap>& typeCache = this->m_cache[type];
    if (!typeCache.contains(ref))
        return QVariantMap();

    return typeCache[ref];
}

QSharedPointer<XenObject> XenCache::ResolveObject(const QString& type, const QString& ref)
{
    if (ref.isEmpty())
        return QSharedPointer<XenObject>();

    XenObjectType normalizedType = this->normalizeType(type);
    if (normalizedType == XenObjectType::Null)
        return QSharedPointer<XenObject>();

    QMutexLocker locker(&this->m_mutex);

    if (this->m_objects.contains(normalizedType))
    {
        auto it = this->m_objects[normalizedType].find(ref);
        if (it != this->m_objects[normalizedType].end())
        {
            return it.value();
        }
    }

    if (!this->m_cache.contains(normalizedType) || !this->m_cache[normalizedType].contains(ref))
        return QSharedPointer<XenObject>();

    QSharedPointer<XenObject> created = this->createObjectForType(normalizedType, ref);
    if (!created)
        return QSharedPointer<XenObject>();

    if (!this->m_objects.contains(normalizedType))
    {
        this->m_objects[normalizedType] = QMap<QString, QSharedPointer<XenObject>>();
    }
    this->m_objects[normalizedType].insert(ref, created);
    return created;
}

QSharedPointer<XenObject> XenCache::ResolveObject(XenObjectType type, const QString& ref)
{
    if (ref.isEmpty() || type == XenObjectType::Null)
        return QSharedPointer<XenObject>();

    QMutexLocker locker(&this->m_mutex);

    if (this->m_objects.contains(type))
    {
        auto it = this->m_objects[type].find(ref);
        if (it != this->m_objects[type].end())
            return it.value();
    }

    if (!this->m_cache.contains(type) || !this->m_cache[type].contains(ref))
        return QSharedPointer<XenObject>();

    QSharedPointer<XenObject> created = this->createObjectForType(type, ref);
    if (!created)
        return QSharedPointer<XenObject>();

    if (!this->m_objects.contains(type))
        this->m_objects[type] = QMap<QString, QSharedPointer<XenObject>>();

    this->m_objects[type].insert(ref, created);
    return created;
}

QString XenCache::CanonicalType(const QString& type) const
{
    return this->typeToCacheString(this->normalizeType(type));
}

// Note: don't work with m_objects in this because m_objects is dynamic cache of stuff
// that was looked up explicitly, while m_cache contains everything
bool XenCache::Contains(XenObjectType type, const QString &ref) const
{
    if (ref.isEmpty())
        return false;

    QMutexLocker locker(&this->m_mutex);

    if (type == XenObjectType::Null)
        return false;

    if (!this->m_cache.contains(type))
        return false;

    return this->m_cache[type].contains(ref);
}

QList<QVariantMap> XenCache::GetAllData(const QString& type) const
{
    return this->GetAllData(this->normalizeType(type));
}

QList<QVariantMap> XenCache::GetAllData(XenObjectType type) const
{
    QMutexLocker locker(&this->m_mutex);

    if (type == XenObjectType::Null)
        return QList<QVariantMap>();

    if (!this->m_cache.contains(type))
        return QList<QVariantMap>();

    return this->m_cache[type].values();
}

QList<QSharedPointer<XenObject>> XenCache::GetAll(const QString& type)
{
    return this->GetAll(this->normalizeType(type));
}

QList<QSharedPointer<XenObject> > XenCache::GetAll(XenObjectType type)
{
    QStringList refs = this->GetAllRefs(type);
    QList<QSharedPointer<XenObject>> objects;
    objects.reserve(refs.size());

    for (const QString& ref : refs)
    {
        QSharedPointer<XenObject> obj = this->ResolveObject(type, ref);
        if (obj)
            objects.append(obj);
    }

    return objects;
}

QStringList XenCache::GetAllRefs(XenObjectType type) const
{
    QMutexLocker locker(&this->m_mutex);

    if (type == XenObjectType::Null)
        return QStringList();

    if (!this->m_cache.contains(type))
        return QStringList();

    return this->m_cache[type].keys();
}

// C# Equivalent: connection.Cache.XenSearchableObjects
// C# Reference: Cache.cs lines 565-600 - only returns searchable objects
// Used by: SearchTabPage::populateTree() -> GroupAlg.cs GetGrouped()
//
// CRITICAL: Only returns object types that should appear in searches/trees.
// Does NOT return internal objects like: console, host_cpu, host_metrics,
// message, pbd, pif, vbd, vif, bond, vgpu, etc.
QList<QPair<XenObjectType, QString>> XenCache::GetXenSearchableObjects() const
{
    QMutexLocker locker(&this->m_mutex);

    QList<QPair<XenObjectType, QString>> allObjects;

    // C# XenSearchableObjects returns (in order):
    // 1. VMs (includes templates and snapshots)
    // 2. VM_appliances
    // 3. Hosts
    // 4. SRs
    // 5. Networks
    // 6. VDIs
    // 7. Folders
    // 8. DockerContainers
    // 9. Pools (only if visible)

    // Define searchable types matching C# Cache.XenSearchableObjects
    QList<XenObjectType> searchableTypes;
    searchableTypes << XenObjectType::VM
                    << XenObjectType::VMAppliance
                    << XenObjectType::Host
                    << XenObjectType::SR
                    << XenObjectType::Network
                    << XenObjectType::VDI
                    << XenObjectType::Folder
                    << XenObjectType::DockerContainer
                    << XenObjectType::Pool;

    // Iterate only searchable types
    for (XenObjectType type : searchableTypes)
    {
        if (!this->m_cache.contains(type))
            continue;

        const QMap<QString, QVariantMap>& objectsOfType = this->m_cache[type];

        // Iterate all objects of this type
        for (auto refIt = objectsOfType.constBegin(); refIt != objectsOfType.constEnd(); ++refIt)
        {
            const QString& ref = refIt.key();
            allObjects.append(qMakePair(type, ref));
        }
    }

    return allObjects;
}

void XenCache::Update(XenObjectType type, const QString &ref, const QVariantMap &data)
{
    if (ref.isEmpty())
    {
        qWarning() << "XenCache::update - Empty ref provided for type:" << XenObject::TypeToString(type);
        return;
    }

    if (type == XenObjectType::Null)
        return;

    bool refresh = false;
    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(type))
            this->m_cache[type] = QMap<QString, QVariantMap>();

               // Ensure ref is in the data
        QVariantMap dataWithRef = data;
        if (!dataWithRef.contains("ref"))
            dataWithRef["ref"] = ref;

        this->m_cache[type][ref] = dataWithRef;
        refresh = this->m_objects.contains(type) && this->m_objects[type].contains(ref);
    }

    if (refresh)
        this->refreshObject(type, ref);

    emit objectChanged(this->m_connection, this->typeToCacheString(type), ref);
}

void XenCache::UpdateBulk(XenObjectType type, const QVariantMap &allRecords)
{
    if (type == XenObjectType::Null)
        return;
    int updateCount = 0;
    QStringList refreshedRefs;

    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(type))
            this->m_cache[type] = QMap<QString, QVariantMap>();

        // Iterate through all records and add to cache
        for (auto it = allRecords.constBegin(); it != allRecords.constEnd(); ++it)
        {
            QString ref = it.key();
            QVariantMap data = it.value().toMap();

            // Ensure ref is in the data
            if (!data.contains("ref"))
                data["ref"] = ref;

            this->m_cache[type][ref] = data;
            if (this->m_objects.contains(type) && this->m_objects[type].contains(ref))
                refreshedRefs.append(ref);
            updateCount++;
        }
    }

    for (const QString& ref : refreshedRefs)
        refreshObject(type, ref);

    qDebug() << "XenCache: Bulk update completed for" << XenObject::TypeToString(type)
             << "- added/updated" << updateCount << "objects";

    emit bulkUpdateComplete(this->typeToCacheString(type), updateCount);
}

void XenCache::Remove(XenObjectType type, const QString &ref)
{
    if (ref.isEmpty())
        return;

    if (type == XenObjectType::Null)
        return;

    {
        QMutexLocker locker(&this->m_mutex);

        if (!this->m_cache.contains(type))
            return;

        this->m_cache[type].remove(ref);
    }

    this->evictObject(type, ref);
    emit objectRemoved(this->m_connection, this->typeToCacheString(type), ref);
}

void XenCache::ClearType(XenObjectType type)
{
    if (type == XenObjectType::Null)
        return;

    int count = 0;
    QStringList refs;
    {
        QMutexLocker locker(&this->m_mutex);

        if (this->m_cache.contains(type))
        {
            count = this->m_cache[type].count();
            refs = this->m_cache[type].keys();
            this->m_cache.remove(type);
        }
    }

    for (const QString& ref : refs)
        this->evictObject(type, ref);

    if (count > 0)
    {
        qDebug() << "XenCache: Cleared" << count << this->typeToCacheString(type) << "objects from cache";
        emit cacheCleared(); // Could add a typeCleared signal if needed
    }
}

void XenCache::Clear()
{
    QList<QPair<XenObjectType, QString>> refs;
    {
        QMutexLocker locker(&this->m_mutex);
        for (auto typeIt = this->m_cache.constBegin(); typeIt != this->m_cache.constEnd(); ++typeIt)
        {
            const XenObjectType type = typeIt.key();
            const QMap<QString, QVariantMap>& records = typeIt.value();
            for (auto refIt = records.constBegin(); refIt != records.constEnd(); ++refIt)
                refs.append(qMakePair(type, refIt.key()));
        }
        this->m_cache.clear();
    }

    for (const auto& entry : refs)
        this->evictObject(entry.first, entry.second);

    qDebug() << "XenCache: Cache cleared";
    emit cacheCleared();
}

int XenCache::Count(XenObjectType type) const
{
    QMutexLocker locker(&this->m_mutex);
    if (type == XenObjectType::Null)
        return 0;

    if (!this->m_cache.contains(type))
        return 0;

    return this->m_cache[type].count();
}

bool XenCache::IsEmpty() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_cache.isEmpty();
}

QStringList XenCache::GetKnownTypes() const
{
    // Return all types that createObjectForType() can instantiate
    // Keep alphabetically sorted to match createObjectForType() and normalizeType()
    return QStringList{
        this->typeToCacheString(XenObjectType::Bond),
        this->typeToCacheString(XenObjectType::Certificate),
        this->typeToCacheString(XenObjectType::Console),
        this->typeToCacheString(XenObjectType::Folder),
        this->typeToCacheString(XenObjectType::GPUGroup),
        this->typeToCacheString(XenObjectType::Host),
        this->typeToCacheString(XenObjectType::HostCPU),
        this->typeToCacheString(XenObjectType::HostMetrics),
        this->typeToCacheString(XenObjectType::Message),
        this->typeToCacheString(XenObjectType::Network),
        this->typeToCacheString(XenObjectType::NetworkSriov),
        this->typeToCacheString(XenObjectType::PBD),
        this->typeToCacheString(XenObjectType::PCI),
        this->typeToCacheString(XenObjectType::PGPU),
        this->typeToCacheString(XenObjectType::PIF),
        this->typeToCacheString(XenObjectType::PIFMetrics),
        this->typeToCacheString(XenObjectType::Pool),
        this->typeToCacheString(XenObjectType::Role),
        this->typeToCacheString(XenObjectType::SR),
        this->typeToCacheString(XenObjectType::Task),
        this->typeToCacheString(XenObjectType::Tunnel),
        this->typeToCacheString(XenObjectType::VBD),
        this->typeToCacheString(XenObjectType::VBDMetrics),
        this->typeToCacheString(XenObjectType::VDI),
        this->typeToCacheString(XenObjectType::VIF),
        this->typeToCacheString(XenObjectType::VLAN),
        this->typeToCacheString(XenObjectType::VM),
        this->typeToCacheString(XenObjectType::VMGuestMetrics),
        this->typeToCacheString(XenObjectType::VMMetrics)
    };
}

QSharedPointer<XenObject> XenCache::createObjectForType(XenObjectType type, const QString& ref)
{
    if (!this->m_connection)
        return QSharedPointer<XenObject>();

    // Alphabetically sorted for easier maintenance
    if (type == XenObjectType::Bond)
        return QSharedPointer<XenObject>(new Bond(this->m_connection, ref));
    if (type == XenObjectType::Certificate)
        return QSharedPointer<XenObject>(new Certificate(this->m_connection, ref));
    if (type == XenObjectType::Console)
        return QSharedPointer<XenObject>(new Console(this->m_connection, ref));
    if (type == XenObjectType::Folder)
        return QSharedPointer<XenObject>(new Folder(this->m_connection, ref));
    if (type == XenObjectType::GPUGroup)
        return QSharedPointer<XenObject>(new GPUGroup(this->m_connection, ref));
    if (type == XenObjectType::Host)
        return QSharedPointer<XenObject>(new Host(this->m_connection, ref));
    if (type == XenObjectType::HostCPU)
        return QSharedPointer<XenObject>(new HostCPU(this->m_connection, ref));
    if (type == XenObjectType::HostMetrics)
        return QSharedPointer<XenObject>(new HostMetrics(this->m_connection, ref));
    if (type == XenObjectType::Message)
        return QSharedPointer<XenObject>(new Message(this->m_connection, ref));
    if (type == XenObjectType::Network)
        return QSharedPointer<XenObject>(new Network(this->m_connection, ref));
    if (type == XenObjectType::NetworkSriov)
        return QSharedPointer<XenObject>(new NetworkSriov(this->m_connection, ref));
    if (type == XenObjectType::PBD)
        return QSharedPointer<XenObject>(new PBD(this->m_connection, ref));
    if (type == XenObjectType::PCI)
        return QSharedPointer<XenObject>(new PCI(this->m_connection, ref));
    if (type == XenObjectType::PGPU)
        return QSharedPointer<XenObject>(new PGPU(this->m_connection, ref));
    if (type == XenObjectType::PIF)
        return QSharedPointer<XenObject>(new PIF(this->m_connection, ref));
    if (type == XenObjectType::PIFMetrics)
        return QSharedPointer<XenObject>(new PIFMetrics(this->m_connection, ref));
    if (type == XenObjectType::Pool)
        return QSharedPointer<XenObject>(new Pool(this->m_connection, ref));
    if (type == XenObjectType::Role)
        return QSharedPointer<XenObject>(new Role(this->m_connection, ref));
    if (type == XenObjectType::SR)
        return QSharedPointer<XenObject>(new SR(this->m_connection, ref));
    if (type == XenObjectType::Task)
        return QSharedPointer<XenObject>(new Task(this->m_connection, ref));
    if (type == XenObjectType::Tunnel)
        return QSharedPointer<XenObject>(new Tunnel(this->m_connection, ref));
    if (type == XenObjectType::VBD)
        return QSharedPointer<XenObject>(new VBD(this->m_connection, ref));
    if (type == XenObjectType::VBDMetrics)
        return QSharedPointer<XenObject>(new VBDMetrics(this->m_connection, ref));
    if (type == XenObjectType::VDI)
        return QSharedPointer<XenObject>(new VDI(this->m_connection, ref));
    if (type == XenObjectType::VIF)
        return QSharedPointer<XenObject>(new VIF(this->m_connection, ref));
    if (type == XenObjectType::VLAN)
        return QSharedPointer<XenObject>(new VLAN(this->m_connection, ref));
    if (type == XenObjectType::VM)
        return QSharedPointer<XenObject>(new VM(this->m_connection, ref));
    if (type == XenObjectType::VMGuestMetrics)
        return QSharedPointer<XenObject>(new VMGuestMetrics(this->m_connection, ref));
    if (type == XenObjectType::VMMetrics)
        return QSharedPointer<XenObject>(new VMMetrics(this->m_connection, ref));

    return QSharedPointer<XenObject>();
}

void XenCache::refreshObject(XenObjectType type, const QString& ref)
{
    // TODO - consider migrating to QHash for better performance of lookups
    QSharedPointer<XenObject> obj;
    {
        QMutexLocker locker(&this->m_mutex);
        if (!this->m_objects.contains(type))
            return;
        auto it = this->m_objects[type].find(ref);
        if (it == this->m_objects[type].end())
            return;
        obj = it.value();
        if (obj)
            obj->SetEvicted(false);
    }

    if (obj)
        obj->Refresh();
}

void XenCache::evictObject(XenObjectType type, const QString& ref)
{
    QSharedPointer<XenObject> obj;
    {
        QMutexLocker locker(&this->m_mutex);
        if (!this->m_objects.contains(type))
            return;
        auto it = this->m_objects[type].find(ref);
        if (it == this->m_objects[type].end())
            return;
        obj = it.value();
        this->m_objects[type].remove(ref);
    }

    if (obj)
        obj->SetEvicted(true);
}

// NOTE: if you ever get the idea to cache this, keep in mind that pool ref changes when host leaves / joins a pool
// so we might need to hook to such events and invalidate any cached reference.
// Easy, but stupid solution is what we do now (and what C# version does) - always look it up from all cached data
QString XenCache::GetPoolRef() const
{
    QMutexLocker locker(&this->m_mutex);
    
    // Get pool type (normalized)
    XenObjectType poolType = this->normalizeType("pool");
    
    // Get all pool refs (there should be exactly one)
    if (this->m_cache.contains(poolType) && !this->m_cache[poolType].isEmpty())
        return this->m_cache[poolType].firstKey();
    
    return QString();
}

QSharedPointer<Pool> XenCache::GetPool()
{
    QString pool_ref = this->GetPoolRef();
    if (pool_ref.isEmpty())
        return QSharedPointer<Pool>();

    QSharedPointer<Pool> pool = this->ResolveObject<Pool>(pool_ref);
    if (pool && pool->IsVisible())
        return pool;

    return QSharedPointer<Pool>();
}

QSharedPointer<Pool> XenCache::GetPoolOfOne()
{
    QString pool_ref = this->GetPoolRef();
    if (pool_ref.isEmpty())
        return QSharedPointer<Pool>();

    return this->ResolveObject<Pool>(pool_ref);
}
