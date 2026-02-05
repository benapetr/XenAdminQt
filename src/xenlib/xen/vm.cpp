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

#include "vm.h"
#include "network/connection.h"
#include "network/comparableaddress.h"
#include "../xencache.h"
#include "vbd.h"
#include "vif.h"
#include "vdi.h"
#include "sr.h"
#include "../xencache.h"
#include "host.h"
#include "pool.h"
#include "console.h"
#include "vusb.h"
#include "vtpm.h"
#include "vmmetrics.h"
#include "blob.h"
#include "pci.h"
#include <QDomDocument>
#include <algorithm>

VM::VM(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString VM::GetPowerState() const
{
    return this->stringProperty("power_state");
}

QString VM::NameWithLocation() const
{
    if (this->GetConnection())
    {
        if (IsRealVM())
            return XenObject::NameWithLocation();

        if (IsSnapshot())
        {
            QSharedPointer<VM> snapshotOf = this->SnapshotOf();
            if (snapshotOf)
            {
                return QString("%1 (snapshot of '%2' %3)").arg(GetName(), snapshotOf->GetName(), LocationString());
            }
        }
    }

    return XenObject::NameWithLocation();
}

QString VM::LocationString() const
{
    QSharedPointer<Host> server = GetHome();
    if (server)
        return QString("on '%1'").arg(server->GetName());

    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QString();

    XenCache* cache = connection->GetCache();
    if (cache)
    {
        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (pool && !pool->GetName().isEmpty())
            return QString("in '%1'").arg(pool->GetName());
    }

    return QString();
}

bool VM::IsTemplate() const
{
    return this->boolProperty("is_a_template", false);
}

bool VM::IsLocked() const
{
    return this->boolProperty("locked", false);
}

bool VM::DefaultTemplate() const
{
    const QVariantMap otherConfig = this->GetOtherConfig();
    const QVariant value = otherConfig.value("default_template");
    return value.isValid() && value.toBool();
}

bool VM::InternalTemplate() const
{
    return this->GetOtherConfig().contains("xensource_internal");
}

bool VM::Show(bool showHiddenVMs) const
{
    if (this->InternalTemplate())
        return false;

    const QString name = GetName();
    if (name.startsWith("__gui__"))
        return false;

    if (showHiddenVMs)
        return true;

    return !IsHidden();
}

bool VM::IsSnapshot() const
{
    return this->boolProperty("is_a_snapshot", false);
}

QString VM::GetResidentOnRef() const
{
    return this->stringProperty("resident_on");
}

QSharedPointer<Host> VM::GetResidentOnHost()
{
    XenConnection* connection = this->GetConnection();
    if (!connection || this->GetResidentOnRef().isEmpty())
        return QSharedPointer<Host>();

    XenCache* cache = connection->GetCache();

    QString residentOn = this->GetResidentOnRef();
    if (residentOn != XENOBJECT_NULL)
    {
        QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, residentOn);
        if (host)
            return host;
    }

    // Fallback to pool coordinator if VM is not currently resident
    const QVariantMap poolData = cache->ResolveObjectData(XenObjectType::Pool, QString());
    const QString masterRef = poolData.value("master").toString();
    if (!masterRef.isEmpty() && masterRef != XENOBJECT_NULL)
        return cache->ResolveObject<Host>(XenObjectType::Host, masterRef);

    return QSharedPointer<Host>();
}

QSharedPointer<Pool> VM::GetPool()
{
    if (!this->GetConnection())
        return QSharedPointer<Pool>();

    return this->GetConnection()->GetCache()->GetPool();
}

QString VM::GetAffinityRef() const
{
    return this->stringProperty("affinity");
}

QStringList VM::GetVBDRefs() const
{
    return this->stringListProperty("VBDs");
}

QSharedPointer<VBD> VM::FindVMCDROM() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VBD>();

    XenCache* cache = connection->GetCache();

    QList<QSharedPointer<VBD>> cdroms;
    const QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(XenObjectType::VBD, vbdRef);
        if (vbd && vbd->IsValid() && vbd->IsCD())
            cdroms.append(vbd);
    }

    if (cdroms.isEmpty())
        return QSharedPointer<VBD>();

    std::sort(cdroms.begin(), cdroms.end(),
              [](const QSharedPointer<VBD>& a, const QSharedPointer<VBD>& b)
              {
                  return a->GetUserdevice() < b->GetUserdevice();
              });

    return cdroms.first();
}

QList<QSharedPointer<VBD>> VM::GetVBDs() const
{
    QList<QSharedPointer<VBD>> vbds;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return vbds;

    XenCache* cache = connection->GetCache();

    const QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(XenObjectType::VBD, vbdRef);
        if (vbd && vbd->IsValid())
            vbds.append(vbd);
    }

    return vbds;
}

QStringList VM::GetVIFRefs() const
{
    return this->stringListProperty("VIFs");
}

QList<QSharedPointer<VIF>> VM::GetVIFs() const
{
    QList<QSharedPointer<VIF>> vifs;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return vifs;

    XenCache* cache = connection->GetCache();

    const QStringList vifRefs = this->GetVIFRefs();
    for (const QString& vifRef : vifRefs)
    {
        QSharedPointer<VIF> vif = cache->ResolveObject<VIF>(XenObjectType::VIF, vifRef);
        if (vif && vif->IsValid())
            vifs.append(vif);
    }

    return vifs;
}

QStringList VM::GetConsoleRefs() const
{
    return this->stringListProperty("consoles");
}

QString VM::GetSuspendVDIRef() const
{
    return this->stringProperty("suspend_VDI");
}

QSharedPointer<Host> VM::GetAffinityHost() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    
    QString ref = this->GetAffinityRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>(XenObjectType::Host, ref);
}

QList<QSharedPointer<Console>> VM::GetConsoles() const
{
    QList<QSharedPointer<Console>> result;
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    
    QStringList refs = this->GetConsoleRefs();
    for (const QString& ref : refs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<Console> obj = cache->ResolveObject<Console>(ref);
            if (obj)
                result.append(obj);
        }
    }
    return result;
}

QSharedPointer<VDI> VM::GetSuspendVDI() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VDI>();
    
    XenCache* cache = connection->GetCache();
    
    QString ref = this->GetSuspendVDIRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<VDI>();
    
    return cache->ResolveObject<VDI>(ref);
}

QSharedPointer<Host> VM::GetHome() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    
    QString ref = this->GetHomeRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>(XenObjectType::Host, ref);
}

QList<QSharedPointer<VUSB>> VM::GetVUSBs() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<VUSB>>();
    
    XenCache* cache = connection->GetCache();
    
    QStringList refs = this->VUSBRefs();
    QList<QSharedPointer<VUSB>> result;
    for (const QString& ref : refs)
    {
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<VUSB> obj = cache->ResolveObject<VUSB>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}

QList<QSharedPointer<VTPM>> VM::GetVTPMs() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<VTPM>>();
    
    XenCache* cache = connection->GetCache();
    
    QStringList refs = this->VTPMRefs();
    QList<QSharedPointer<VTPM>> result;
    for (const QString& ref : refs)
    {
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<VTPM> obj = cache->ResolveObject<VTPM>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}

QList<QSharedPointer<Blob>> VM::GetBlobs() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<Blob>>();
    
    XenCache* cache = connection->GetCache();
    
    QVariantMap blobsMap = this->Blobs();
    QList<QSharedPointer<Blob>> result;
    for (auto it = blobsMap.begin(); it != blobsMap.end(); ++it)
    {
        QString ref = it.value().toString();
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<Blob> obj = cache->ResolveObject<Blob>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}

QSharedPointer<VM> VM::GetParent() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<VM>();
    
    XenCache* cache = connection->GetCache();
    
    QString ref = this->ParentRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<VM>();
    
    return cache->ResolveObject<VM>(XenObjectType::VM, ref);
}

QList<QSharedPointer<PCI>> VM::GetAttachedPCIDevices() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QList<QSharedPointer<PCI>>();
    
    XenCache* cache = connection->GetCache();
    
    QStringList refs = this->AttachedPCIRefs();
    QList<QSharedPointer<PCI>> result;
    for (const QString& ref : refs)
    {
        if (ref.isEmpty() || ref == XENOBJECT_NULL)
            continue;
        QSharedPointer<PCI> obj = cache->ResolveObject<PCI>(ref);
        if (obj)
            result.append(obj);
    }
    return result;
}

QSharedPointer<SR> VM::GetSuspendSR()
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<SR>();
    
    XenCache* cache = connection->GetCache();
    
    QString ref = this->SuspendSRRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<SR>();
    
    return cache->ResolveObject<SR>(ref);
}

QString VM::SnapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

QSharedPointer<VM> VM::SnapshotOf() const
{
    QString snapshot_of_ref = this->SnapshotOfRef();
    if (snapshot_of_ref.isEmpty())
        return QSharedPointer<VM>();
    return this->GetCache()->ResolveObject<VM>(XenObjectType::VM, snapshot_of_ref);
}

QStringList VM::GetSnapshotRefs() const
{
    return stringListProperty("snapshots");
}

qint64 VM::MemoryTarget() const
{
    return longProperty("memory_target", 0);
}

qint64 VM::GetMemoryStaticMax() const
{
    return longProperty("memory_static_max", 0);
}

qint64 VM::GetMemoryDynamicMax() const
{
    return longProperty("memory_dynamic_max", 0);
}

qint64 VM::GetMemoryDynamicMin() const
{
    return longProperty("memory_dynamic_min", 0);
}

qint64 VM::GetMemoryStaticMin() const
{
    return longProperty("memory_static_min", 0);
}

bool VM::SupportsBallooning() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;

    // C# equivalent: VM.SupportsBallooning()
    // For templates: ballooning supported if dynamic_min != static_max
    if (this->IsTemplate())
        return this->GetMemoryDynamicMin() != this->GetMemoryStaticMax();

    // For VMs: check if guest_metrics.other["feature-balloon"] exists
    QString guestMetricsRef = this->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == XENOBJECT_NULL)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QVariantMap guestMetricsData = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
    if (guestMetricsData.isEmpty())
        return false;

    QVariantMap otherConfig = guestMetricsData.value("other", QVariantMap()).toMap();
    return otherConfig.contains("feature-balloon");
}

bool VM::UsesBallooning() const
{
    // C# equivalent: VM.UsesBallooning()
    return this->GetMemoryDynamicMax() != this->GetMemoryStaticMax() && this->SupportsBallooning();
}

int VM::VCPUsMax() const
{
    return intProperty("VCPUs_max", 0);
}

int VM::VCPUsAtStartup() const
{
    return intProperty("VCPUs_at_startup", 0);
}

bool VM::IsHVM() const
{
    return !stringProperty("HVM_boot_policy").isEmpty();
}

bool VM::IsWindows() const
{
    QString guestMetricsRef = stringProperty("guest_metrics");
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != XENOBJECT_NULL)
    {
        XenCache* cache = GetConnection() ? GetConnection()->GetCache() : nullptr;
        if (cache)
        {
            QVariantMap metricsData = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
            if (!metricsData.isEmpty())
            {
                QVariantMap osVersion = metricsData.value("os_version").toMap();
                QString distro = osVersion.value("distro").toString().toLower();
                if (!distro.isEmpty() &&
                    (distro.contains("ubuntu") || distro.contains("debian") ||
                     distro.contains("centos") || distro.contains("redhat") ||
                     distro.contains("suse") || distro.contains("fedora") ||
                     distro.contains("linux")))
                {
                    return false;
                }

                QString uname = osVersion.value("uname").toString().toLower();
                if (!uname.isEmpty() && uname.contains("netscaler"))
                {
                    return false;
                }

                QString osName = osVersion.value("name").toString();
                if (osName.contains("Microsoft", Qt::CaseInsensitive))
                {
                    return true;
                }
            }
        }
    }

    if (this->IsHVM())
    {
        QVariantMap platformMap = this->Platform();
        QString viridian = platformMap.value("viridian").toString();
        if (viridian == "true" || viridian == "1")
            return true;
    }

    return false;
}

bool VM::SupportsVCPUHotplug() const
{
    // C#: !IsWindows() && !Helpers.FeatureForbidden(Connection, Host.RestrictVcpuHotplug)
    // Feature restrictions are not implemented yet; follow the Windows check for now.
    return !this->IsWindows();
}

namespace
{
    static const int DEFAULT_NUM_VCPUS_ALLOWED = 16;
    static const int DEFAULT_NUM_VBDS_ALLOWED = 255;

    bool tryParseRestrictionValue(const QVariantMap& vmData,
                                  const QString& field,
                                  const QString& attribute,
                                  qint64& outValue)
    {
        QString recommendations = vmData.value("recommendations").toString();
        if (recommendations.isEmpty())
            return false;

        QDomDocument doc;
        QString parseError;
        int errorLine = 0;
        int errorColumn = 0;
        if (!doc.setContent(recommendations, &parseError, &errorLine, &errorColumn))
            return false;

        QDomNodeList restrictions = doc.elementsByTagName("restriction");
        for (int i = 0; i < restrictions.count(); ++i)
        {
            QDomElement element = restrictions.at(i).toElement();
            if (element.isNull())
                continue;

            if (element.attribute("field") != field)
                continue;

            QString valueText = element.attribute(attribute);
            if (valueText.isEmpty())
                continue;

            bool ok = false;
            qint64 value = valueText.toLongLong(&ok);
            if (!ok)
                return false;

            outValue = value;
            return true;
        }

        return false;
    }

    bool tryGetMatchingTemplateRestriction(XenCache* cache,
                                           const QVariantMap& vmData,
                                           const QString& field,
                                           const QString& attribute,
                                           qint64& outValue)
    {
        if (!cache)
            return false;

        if (vmData.value("is_a_template").toBool())
        {
            return tryParseRestrictionValue(vmData, field, attribute, outValue);
        }

        QString referenceLabel = vmData.value("reference_label").toString();
        if (referenceLabel.isEmpty())
            return false;

        QList<QVariantMap> vms = cache->GetAllData(XenObjectType::VM);
        for (const QVariantMap& candidate : vms)
        {
            if (!candidate.value("is_a_template").toBool())
                continue;

            if (candidate.value("reference_label").toString() != referenceLabel)
                continue;

            if (tryParseRestrictionValue(candidate, field, attribute, outValue))
                return true;
        }

        return false;
    }

    QList<qint64> getRestrictionValuesAcrossTemplates(XenCache* cache,
                                                      const QString& field,
                                                      const QString& attribute)
    {
        QList<qint64> values;
        if (!cache)
            return values;

        QList<QVariantMap> vms = cache->GetAllData(XenObjectType::VM);
        for (const QVariantMap& candidate : vms)
        {
            if (!candidate.value("is_a_template").toBool())
                continue;

            qint64 value = 0;
            if (tryParseRestrictionValue(candidate, field, attribute, value))
                values.append(value);
        }

        return values;
    }
}

int VM::MaxVCPUsAllowed() const
{
    XenCache* cache = GetConnection() ? GetConnection()->GetCache() : nullptr;
    QVariantMap vmData = this->GetData();

    qint64 value = 0;
    if (tryGetMatchingTemplateRestriction(cache, vmData, "vcpus-max", "max", value))
        return static_cast<int>(value);

    QList<qint64> values = getRestrictionValuesAcrossTemplates(cache, "vcpus-max", "max");
    values.append(DEFAULT_NUM_VCPUS_ALLOWED);
    return static_cast<int>(*std::max_element(values.begin(), values.end()));
}

int VM::MaxVBDsAllowed() const
{
    XenCache* cache = this->GetConnection() ? this->GetConnection()->GetCache() : nullptr;
    QVariantMap vmData = this->GetData();

    qint64 value = 0;
    if (tryGetMatchingTemplateRestriction(cache, vmData, "number-of-vbds", "max", value))
        return static_cast<int>(value);

    QList<qint64> values = getRestrictionValuesAcrossTemplates(cache, "number-of-vbds", "max");
    values.append(DEFAULT_NUM_VBDS_ALLOWED);
    return static_cast<int>(*std::max_element(values.begin(), values.end()));
}

int VM::MinVCPUs() const
{
    XenCache* cache = GetConnection() ? GetConnection()->GetCache() : nullptr;
    QVariantMap vmData = this->GetData();

    qint64 value = 0;
    if (tryGetMatchingTemplateRestriction(cache, vmData, "vcpus-min", "min", value))
        return static_cast<int>(value);

    QList<qint64> values = getRestrictionValuesAcrossTemplates(cache, "vcpus-min", "min");
    values.append(1);
    return static_cast<int>(*std::min_element(values.begin(), values.end()));
}

int VM::GetVCPUWeight() const
{
    QVariantMap vcpusParams = property("VCPUs_params").toMap();
    if (vcpusParams.contains("weight"))
    {
        bool ok = false;
        int weight = vcpusParams.value("weight").toString().toInt(&ok);
        if (ok)
            return weight > 0 ? weight : 1;

        return 65536;
    }

    return 256;
}

long VM::GetCoresPerSocket() const
{
    QVariantMap platformMap = this->Platform();
    if (platformMap.contains("cores-per-socket"))
    {
        bool ok = false;
        long cores = platformMap.value("cores-per-socket").toString().toLongLong(&ok);
        return ok ? cores : DEFAULT_CORES_PER_SOCKET;
    }

    return DEFAULT_CORES_PER_SOCKET;
}

long VM::MaxCoresPerSocket() const
{
    XenCache* cache = GetConnection() ? GetConnection()->GetCache() : nullptr;
    if (!cache)
        return 0;

    QString home = this->GetHomeRef();
    if (!home.isEmpty() && home != XENOBJECT_NULL)
    {
        QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, home);
        if (host)
            return host->GetCoresPerSocket();
    }

    long maxCores = 0;
    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;

        long cores = host->GetCoresPerSocket();
        if (cores > maxCores)
            maxCores = cores;
    }

    return maxCores;
}

QString VM::ValidVCPUConfiguration(long noOfVcpus, long coresPerSocket)
{
    if (coresPerSocket > 0)
    {
        if (noOfVcpus % coresPerSocket != 0)
            return QObject::tr("The number of vCPUs must be a multiple of the number of cores per socket");
        if (noOfVcpus / coresPerSocket > MAX_SOCKETS)
            return QObject::tr("The number of sockets must be at most 16");
    }

    return QString();
}

QString VM::GetTopology(long sockets, long cores)
{
    if (sockets == 0)
    {
        if (cores == 1)
            return QObject::tr("1 core per socket (Invalid configuration)");
        return QObject::tr("%1 cores per socket (Invalid configuration)").arg(cores);
    }

    if (sockets == 1 && cores == 1)
        return QObject::tr("1 socket with 1 core per socket");
    if (sockets == 1)
        return QObject::tr("1 socket with %1 cores per socket").arg(cores);
    if (cores == 1)
        return QObject::tr("%1 sockets with 1 core per socket").arg(sockets);
    return QObject::tr("%1 sockets with %2 cores per socket").arg(sockets).arg(cores);
}

QDomElement VM::ProvisionXml() const
{
    const QVariantMap otherConfig = this->GetOtherConfig();
    const QString xml = otherConfig.value("disks").toString();
    if (xml.isEmpty())
        return QDomElement();

    QDomDocument doc;
    if (!doc.setContent(xml))
        return QDomElement();

    return doc.documentElement();
}

QVariantMap VM::Platform() const
{
    return property("platform").toMap();
}

QStringList VM::GetAllowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap VM::CurrentOperations() const
{
    return property("current_operations").toMap();
}

bool VM::CanMigrateToHost(const QString& hostRef, QString* error) const
{
    XenConnection* connection = this->GetConnection();
    if (!connection || !connection->IsConnected())
    {
        if (error)
            *error = "Not connected to server";
        return false;
    }

    if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
    {
        if (error)
            *error = "Invalid host reference";
        return false;
    }

    if (!this->IsValid())
    {
        if (error)
            *error = "VM not found in cache";
        return false;
    }

    QStringList allowedOps = this->GetAllowedOperations();
    if (!allowedOps.contains("pool_migrate"))
    {
        if (error)
            *error = "VM does not allow migration";
        return false;
    }

    QString residentOn = this->GetResidentOnRef();
    if (!residentOn.isEmpty() && residentOn == hostRef)
    {
        if (error)
            *error = "VM is already on the selected host";
        return false;
    }

    return true;
}

bool VM::CanBeMoved() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    bool hasOwner = false;
    QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(vbdRef);
        if (!vbd || !vbd->IsValid())
            continue;

        QVariantMap otherConfig = vbd->GetOtherConfig();
        if (otherConfig.contains("owner"))
            hasOwner = true;

        QString vdiRef = vbd->GetVDIRef();
        if (vdiRef.isEmpty())
            continue;

        QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
        if (!vdi || !vdi->IsValid())
            continue;

        QString srRef = vdi->SRRef();
        if (srRef.isEmpty())
            continue;

        QSharedPointer<SR> sr = cache->ResolveObject<SR>(srRef);
        if (sr && sr->IsValid() && sr->HBALunPerVDI())
            return false;
    }

    if (this->IsTemplate())
        return false;
    if (this->IsLocked())
        return false;

    if (!this->GetAllowedOperations().contains("export"))
        return false;

    if (this->GetPowerState() == "Suspended")
        return false;

    return hasOwner;
}

bool VM::AnyDiskFastClonable() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QList<QVariantMap> smRecords = cache->GetAllData(XenObjectType::SM);
    if (smRecords.isEmpty())
        return false;

    QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(vbdRef);
        if (!vbd || !vbd->IsValid())
            continue;

        QString vbdType = vbd->GetType();
        if (vbdType.compare("Disk", Qt::CaseInsensitive) != 0)
            continue;

        QString vdiRef = vbd->GetVDIRef();
        if (vdiRef.isEmpty())
            continue;

        QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
        if (!vdi || !vdi->IsValid())
            continue;

        QString srRef = vdi->SRRef();
        if (srRef.isEmpty())
            continue;

        QSharedPointer<SR> sr = cache->ResolveObject<SR>(srRef);
        if (!sr || !sr->IsValid())
            continue;

        QString srType = sr->GetType();
        if (srType.isEmpty())
            continue;

        for (const QVariantMap& smData : smRecords)
        {
            QString smType = smData.value("type").toString();
            if (smType != srType)
                continue;

            QVariantList caps = smData.value("capabilities").toList();
            for (const QVariant& cap : caps)
            {
                if (cap.toString() == "VDI_CLONE")
                    return true;
            }
        }
    }

    return false;
}

bool VM::HasAtLeastOneDisk() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(vbdRef);
        if (!vbd || !vbd->IsValid())
            continue;

        if (vbd->GetType().compare("Disk", Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

QString VM::GetHomeRef() const
{
    // Return affinity host if set, otherwise resident host
    QString affinity = this->GetAffinityRef();
    if (!affinity.isEmpty() && affinity != XENOBJECT_NULL)
        return affinity;

    return this->GetResidentOnRef();
}

qint64 VM::UserVersion() const
{
    return this->intProperty("user_version", 0);
}

QString VM::ScheduledToBeResidentOnRef() const
{
    return this->stringProperty("scheduled_to_be_resident_on");
}

qint64 VM::MemoryOverhead() const
{
    return this->intProperty("memory_overhead", 0);
}

QVariantMap VM::VCPUsParams() const
{
    return this->property("VCPUs_params").toMap();
}

QString VM::ActionsAfterSoftreboot() const
{
    return this->stringProperty("actions_after_softreboot");
}

QString VM::ActionsAfterShutdown() const
{
    return this->stringProperty("actions_after_shutdown");
}

QString VM::ActionsAfterReboot() const
{
    return this->stringProperty("actions_after_reboot");
}

QString VM::ActionsAfterCrash() const
{
    return this->stringProperty("actions_after_crash");
}

QStringList VM::VUSBRefs() const
{
    return this->stringListProperty("VUSBs");
}

QStringList VM::CrashDumpRefs() const
{
    return this->stringListProperty("crash_dumps");
}

QStringList VM::VTPMRefs() const
{
    return this->stringListProperty("VTPMs");
}

QString VM::PVBootloader() const
{
    return this->stringProperty("PV_bootloader");
}

QString VM::PVKernel() const
{
    return this->stringProperty("PV_kernel");
}

QString VM::PVRamdisk() const
{
    return this->stringProperty("PV_ramdisk");
}

QString VM::PVArgs() const
{
    return this->stringProperty("PV_args");
}

QString VM::PVBootloaderArgs() const
{
    return this->stringProperty("PV_bootloader_args");
}

QString VM::PVLegacyArgs() const
{
    return this->stringProperty("PV_legacy_args");
}

QString VM::HVMBootPolicy() const
{
    return this->property("HVM_boot_policy").toString();
}

QVariantMap VM::HVMBootParams() const
{
    return this->property("HVM_boot_params").toMap();
}

double VM::HVMShadowMultiplier() const
{
    return this->property("HVM_shadow_multiplier").toDouble();
}

QString VM::PCIBus() const
{
    return this->stringProperty("PCI_bus");
}

qint64 VM::Domid() const
{
    return this->property("domid").toLongLong();
}

QString VM::Domarch() const
{
    return this->property("domarch").toString();
}

QVariantMap VM::LastBootCPUFlags() const
{
    return this->property("last_boot_CPU_flags").toMap();
}

bool VM::IsControlDomain() const
{
    return this->boolProperty("is_control_domain", false);
}

QString VM::MetricsRef() const
{
    return this->stringProperty("metrics");
}

QSharedPointer<VMMetrics> VM::GetMetrics()
{
    XenCache* cache = this->GetCache();
    QString ref = this->MetricsRef();
    if (ref.isEmpty() || ref == XENOBJECT_NULL)
        return QSharedPointer<VMMetrics>();

    return cache->ResolveObject<VMMetrics>(ref);
}

QString VM::GetGuestMetricsRef() const
{
    return this->stringProperty("guest_metrics");
}

QString VM::LastBootedRecord() const
{
    return this->stringProperty("last_booted_record");
}

QString VM::Recommendations() const
{
    return this->stringProperty("recommendations");
}

QVariantMap VM::XenstoreData() const
{
    return this->property("xenstore_data").toMap();
}

bool VM::HAAlwaysRun() const
{
    return this->boolProperty("ha_always_run", false);
}

QString VM::HARestartPriority() const
{
    return this->property("ha_restart_priority").toString();
}

QDateTime VM::SnapshotTime() const
{
    QString dateStr = this->stringProperty("snapshot_time");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

QString VM::TransportableSnapshotId() const
{
    return this->stringProperty("transportable_snapshot_id");
}

QVariantMap VM::Blobs() const
{
    return this->property("blobs").toMap();
}

QVariantMap VM::BlockedOperations() const
{
    return this->property("blocked_operations").toMap();
}

QVariantMap VM::SnapshotInfo() const
{
    return this->property("snapshot_info").toMap();
}

QString VM::SnapshotMetadata() const
{
    return this->stringProperty("snapshot_metadata");
}

QString VM::ParentRef() const
{
    return this->stringProperty("parent");
}

QStringList VM::ChildrenRefs() const
{
    return this->stringListProperty("children");
}

QVariantMap VM::BIOSStrings() const
{
    return this->property("bios_strings").toMap();
}

QString VM::ProtectionPolicyRef() const
{
    return this->stringProperty("protection_policy");
}

bool VM::IsSnapshotFromVMPP() const
{
    return this->boolProperty("is_snapshot_from_vmpp", false);
}

QString VM::SnapshotScheduleRef() const
{
    return this->stringProperty("snapshot_schedule");
}

bool VM::IsVMSSSnapshot() const
{
    return this->boolProperty("is_vmss_snapshot", false);
}

QString VM::ApplianceRef() const
{
    return this->stringProperty("appliance");
}

qint64 VM::StartDelay() const
{
    return this->intProperty("start_delay", 0);
}

qint64 VM::ShutdownDelay() const
{
    return this->intProperty("shutdown_delay", 0);
}

qint64 VM::Order() const
{
    return this->intProperty("order", 0);
}

QStringList VM::VGPURefs() const
{
    return this->stringListProperty("VGPUs");
}

QStringList VM::AttachedPCIRefs() const
{
    return this->stringListProperty("attached_PCIs");
}

QString VM::SuspendSRRef() const
{
    return this->stringProperty("suspend_SR");
}

qint64 VM::Version() const
{
    return this->intProperty("version", 0);
}

QString VM::GenerationId() const
{
    return this->stringProperty("generation_id");
}

qint64 VM::HardwarePlatformVersion() const
{
    return this->intProperty("hardware_platform_version", 0);
}

bool VM::HasVendorDevice() const
{
    return this->boolProperty("has_vendor_device", false);
}

bool VM::HasVendorDeviceState() const
{
    // C# reference: XenModel/XenAPI-Extensions/VM.cs WindowsUpdateCapable()
    // return has_vendor_device && IsWindows();
    return this->HasVendorDevice() && this->IsWindows();
}

bool VM::ReadCachingEnabled() const
{
    // C# reference: XenModel/XenAPI-Extensions/VM.cs ReadCachingEnabled()
    // Returns true if any attached VDI has read caching enabled
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return false;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;
    
    // Get resident host
    QString residentHostRef = this->GetResidentOnRef();
    if (residentHostRef.isEmpty() || residentHostRef == "OpaqueRef:NULL")
        return false;
    
    // Check all VBDs
    QStringList vbdRefs = this->GetVBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QSharedPointer<VBD> vbd = cache->ResolveObject<VBD>(vbdRef);
        if (!vbd || !vbd->IsValid())
            continue;
        
        // Check if VBD is currently attached
        QVariantMap vbdData = vbd->GetData();
        bool currentlyAttached = vbdData.value("currently_attached", false).toBool();
        if (!currentlyAttached)
            continue;
        
        QString vdiRef = vbd->GetVDIRef();
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
            continue;
        
        QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
        if (!vdi || !vdi->IsValid())
            continue;
        
        // Check if VDI has read caching enabled for this host
        // In C#, VDI.ReadCachingEnabled(host) checks SR's allowed_operations
        QString srRef = vdi->SRRef();
        if (srRef.isEmpty())
            continue;
        
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(srRef);
        if (!sr || !sr->IsValid())
            continue;
        
        // Check if SR allows read caching operations
        QVariantMap srData = sr->GetData();
        QStringList allowedOps = srData.value("allowed_operations").toStringList();
        if (allowedOps.contains("vdi_read_caching"))
            return true;
    }
    
    return false;
}

bool VM::RequiresReboot() const
{
    return this->boolProperty("requires_reboot", false);
}

QString VM::ReferenceLabel() const
{
    return this->stringProperty("reference_label");
}

QString VM::DomainType() const
{
    return this->stringProperty("domain_type");
}

QVariantMap VM::NVRAM() const
{
    return this->property("NVRAM").toMap();
}

QStringList VM::PendingGuidances() const
{
    return this->stringListProperty("pending_guidances");
}

// Property getters for search/query functionality
// C# equivalent: VM extensions in XenAPI-Extensions/VM.cs and Common.cs PropertyAccessors

bool VM::IsRealVM() const
{
    // C# equivalent: VM.IsRealVm()
    // Returns true if VM is not a template, not a snapshot, and not a control domain
    return !this->IsTemplate() && !this->IsSnapshot() && !this->IsControlDomain();
}

QString VM::GetOSName() const
{
    // C# equivalent: VM.GetOSName()
    // Gets OS name from guest_metrics.os_version["name"]
    
    if (!this->IsRealVM())
        return QString();
    
    QString guestMetricsRef = this->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return QString();
    
    // Get guest_metrics from cache
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return QString();
    
    XenCache* cache = conn->GetCache();
    if (!cache)
        return QString();
    
    QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
    if (guestMetrics.isEmpty())
        return QString();
    
    // Get os_version dictionary and extract "name" key
    QVariantMap osVersion = guestMetrics.value("os_version").toMap();
    return osVersion.value("name", QString()).toString();
}

int VM::GetVirtualizationStatus() const
{
    // C# equivalent: VM.GetVirtualizationStatus(out _)
    // Returns VirtualizationStatus flags:
    // 0 = NotInstalled
    // 1 = Unknown
    // 2 = PvDriversOutOfDate
    // 4 = IoDriversInstalled
    // 8 = ManagementInstalled
    
    if (!this->IsRealVM())
        return 0; // NotInstalled
    
    QString guestMetricsRef = this->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return 0; // NotInstalled
    
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return 0;
    
    XenCache* cache = conn->GetCache();
    if (!cache)
        return 0;
    
    QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
    if (guestMetrics.isEmpty())
        return 0; // NotInstalled
    
    // Get PV drivers version from guest_metrics
    QVariantMap pvDriversVersion = guestMetrics.value("PV_drivers_version").toMap();
    if (pvDriversVersion.isEmpty())
        return 0; // NotInstalled
    
    // Check for management agent (xe-daemon)
    bool hasManagement = pvDriversVersion.contains("major") && pvDriversVersion.contains("minor");
    
    // Check if drivers are up to date
    // C# logic: Compare pvDriversVersion with host's recommended version
    // For now, simplified: if we have PV drivers, assume IoDriversInstalled
    int status = 0;
    
    if (hasManagement)
    {
        status |= 4; // IoDriversInstalled
        status |= 8; // ManagementInstalled
    } else if (!pvDriversVersion.isEmpty())
    {
        status |= 4; // IoDriversInstalled
    }
    
    // Check if drivers are out of date
    // C# checks PV_drivers_up_to_date field
    bool upToDate = guestMetrics.value("PV_drivers_up_to_date", true).toBool();
    if (!upToDate && status != 0)
    {
        status |= 2; // PvDriversOutOfDate
    }
    
    return status;
}

QList<ComparableAddress> VM::GetIPAddresses() const
{
    // C# equivalent: PropertyAccessors IP address property
    // Gets IPs from guest_metrics.networks dictionary
    
    QList<ComparableAddress> addresses;
    
    if (!this->IsRealVM())
        return addresses;
    
    QString guestMetricsRef = this->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return addresses;
    
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return addresses;
    
    XenCache* cache = conn->GetCache();
    if (!cache)
        return addresses;
    
    QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
    if (guestMetrics.isEmpty())
        return addresses;
    
    // Get networks dictionary: key = "0/ip", value = "192.168.1.100"
    QVariantMap networks = guestMetrics.value("networks").toMap();
    
    for (const QString& key : networks.keys())
    {
        // Keys are like "0/ip", "1/ip", "0/ipv6/0", etc.
        if (key.contains("/ip"))
        {
            QString ipStr = networks.value(key).toString();
            if (ipStr.isEmpty())
                continue;
            
            // Try to parse as IP address (not partial IP, allow name fallback)
            ComparableAddress addr;
            if (ComparableAddress::TryParse(ipStr, false, true, addr))
            {
                addresses.append(addr);
            }
        }
    }
    
    return addresses;
}

qint64 VM::GetStartTime() const
{
    // C# equivalent: VM.GetStartTime()
    // Gets start_time from guest_metrics or metrics
    
    QString guestMetricsRef = this->GetGuestMetricsRef();
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        XenConnection* conn = this->GetConnection();
        if (conn)
        {
            XenCache* cache = conn->GetCache();
            if (cache)
            {
                QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
                if (!guestMetrics.isEmpty())
                {
                    // Check if start_time exists
                    if (guestMetrics.contains("start_time"))
                    {
                        // Parse ISO 8601 datetime or epoch seconds
                        QString startTimeStr = guestMetrics.value("start_time").toString();
                        if (!startTimeStr.isEmpty())
                        {
                            // Try to parse as epoch seconds first
                            bool ok;
                            qint64 epochTime = startTimeStr.toLongLong(&ok);
                            if (ok)
                                return epochTime;
                            
                            // Otherwise try QDateTime parsing
                            QDateTime dt = QDateTime::fromString(startTimeStr, Qt::ISODate);
                            if (dt.isValid())
                                return dt.toSecsSinceEpoch();
                        }
                    }
                }
            }
        }
    }
    
    return 0; // Not available
}
