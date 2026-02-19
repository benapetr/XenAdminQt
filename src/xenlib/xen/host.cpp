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

#include "host.h"
#include "network/connection.h"
#include "network/comparableaddress.h"
#include "hostmetrics.h"
#include "poolupdate.h"
#include "feature.h"
#include "vmmetrics.h"
#include "../xencache.h"
#include "vm.h"
#include "pbd.h"
#include "pif.h"
#include "pgpu.h"
#include "../utils/misc.h"
#include <QDateTime>

Host::Host(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString Host::GetHostname() const
{
    return stringProperty("hostname");
}

QString Host::GetAddress() const
{
    return stringProperty("address");
}

bool Host::IsEnabled() const
{
    return boolProperty("enabled", true);
}

bool Host::IsLive() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return false;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return false;

    const QString metricsRef = GetMetricsRef();
    if (metricsRef.isEmpty())
        return false;

    QSharedPointer<HostMetrics> metrics = cache->ResolveObject<HostMetrics>(metricsRef);
    return metrics && metrics->IsLive();
}

static bool boolKeyPreferTrue(const QVariantMap& map, const QString& key)
{
    const QString value = map.value(key).toString();
    return value.isEmpty() || value.compare("false", Qt::CaseInsensitive) != 0;
}

static bool boolKey(const QVariantMap& map, const QString& key)
{
    const QString value = map.value(key).toString();
    return value.compare("true", Qt::CaseInsensitive) == 0;
}

bool Host::RestrictVtpm() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_vtpm");
}

bool Host::RestrictIntraPoolMigrate() const
{
    return boolKey(LicenseParams(), "restrict_xen_motion");
}

bool Host::RestrictGpu() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_gpu");
}

bool Host::RestrictVgpu() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_vgpu");
}

bool Host::RestrictIntegratedGpuPassthrough() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_integrated_gpu_passthrough");
}

bool Host::RestrictVSwitchController() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_vswitch_controller");
}

bool Host::RestrictSriovNetwork() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_network_sriov");
}

bool Host::RestrictManagementOnVLAN() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_management_on_vlan");
}

bool Host::RestrictPooling() const
{
    return boolKeyPreferTrue(LicenseParams(), "restrict_pooling");
}

bool Host::RestrictPoolSize() const
{
    return boolKey(LicenseParams(), "restrict_pool_size");
}

bool Host::RestrictPoolSecretRotation() const
{
    return boolKeyPreferTrue(this->LicenseParams(), "restrict_pool_secret_rotation");
}

bool Host::SriovNetworkDisabled() const
{
    if (!this->RestrictSriovNetwork())
        return false;

    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    const QStringList featureRefs = this->FeatureRefs();
    for (const QString& ref : featureRefs)
    {
        QSharedPointer<Feature> feature = cache->ResolveObject<Feature>(ref);
        if (!feature || !feature->IsValid())
            continue;
        if (feature->GetName().compare("network_sriov", Qt::CaseInsensitive) == 0)
            return !feature->IsEnabled();
    }

    return false;
}

bool Host::vSwitchNetworkBackend() const
{
    const QVariantMap version = this->SoftwareVersion();
    return version.value("network_backend").toString() == "openvswitch";
}

QSharedPointer<PGPU> Host::SystemDisplayDevice() const
{
    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return QSharedPointer<PGPU>();

    const QStringList refs = this->PGPURefs();
    for (const QString& ref : refs)
    {
        QSharedPointer<PGPU> pgpu = cache->ResolveObject<PGPU>(ref);
        if (pgpu && pgpu->IsValid() && pgpu->IsSystemDisplayDevice())
            return pgpu;
    }

    return QSharedPointer<PGPU>();
}

bool Host::CanEnableDisableIntegratedGpu() const
{
    XenConnection* connection = this->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    // Same effective condition as C#: Helpers.GpuCapability(connection) &&
    // !Helpers.FeatureForbidden(connection, Host.RestrictIntegratedGpuPassthrough)
    const QList<QSharedPointer<PGPU>> allPgpus = cache->GetAll<PGPU>(XenObjectType::PGPU);
    const bool hasAnyGpu = !allPgpus.isEmpty();
    return hasAnyGpu && !this->RestrictIntegratedGpuPassthrough();
}

QStringList Host::GetResidentVMRefs() const
{
    return stringListProperty("resident_VMs");
}

QVariantMap Host::SoftwareVersion() const
{
    return property("software_version").toMap();
}

QStringList Host::GetCapabilities() const
{
    return stringListProperty("capabilities");
}

QVariantMap Host::GetCPUInfo() const
{
    return property("cpu_info").toMap();
}

int Host::GetCPUSockets() const
{
    QVariantMap cpuInfoMap = this->GetCPUInfo();
    if (!cpuInfoMap.contains("socket_count"))
        return 0;

    bool ok = false;
    int sockets = cpuInfoMap.value("socket_count").toString().toInt(&ok);
    return ok ? sockets : 0;
}

int Host::GetCPUCount() const
{
    QVariantMap cpuInfoMap = this->GetCPUInfo();
    if (!cpuInfoMap.contains("cpu_count"))
        return 0;

    bool ok = false;
    int cpuCount = cpuInfoMap.value("cpu_count").toString().toInt(&ok);
    return ok ? cpuCount : 0;
}

int Host::GetCoresPerSocket() const
{
    int sockets = this->GetCPUSockets();
    int cpuCount = this->GetCPUCount();
    if (sockets > 0 && cpuCount > 0)
        return cpuCount / sockets;

    return 0;
}

int Host::GetHostCpuCount() const
{
    return stringListProperty("host_CPUs").size();
}

double Host::BootTime() const
{
    // C# equivalent: Host.BootTime()
    // Gets boot_time from other_config dictionary
    
    QVariantMap otherConfigMap = this->GetOtherConfig();
    if (otherConfigMap.isEmpty())
        return 0.0;
    
    if (!otherConfigMap.contains("boot_time"))
        return 0.0;
    
    bool ok = false;
    double bootTime = otherConfigMap.value("boot_time").toDouble(&ok);
    
    return ok ? bootTime : 0.0;
}

qint64 Host::GetUptime() const
{
    const double bootTime = this->BootTime();
    if (bootTime <= 0.0)
        return -1;

    XenConnection* connection = this->GetConnection();
    if (!connection)
        return -1;

    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    const qint64 serverOffset = connection->GetServerTimeOffsetSeconds();
    const qint64 uptimeSeconds = now - static_cast<qint64>(bootTime) - serverOffset;

    if (uptimeSeconds < 0)
        return -1;

    return uptimeSeconds;
}

QString Host::GetSuspendImageSRRef() const
{
    return stringProperty("suspend_image_sr");
}

QString Host::GetCrashDumpSRRef() const
{
    return stringProperty("crash_dump_sr");
}

QStringList Host::GetPBDRefs() const
{
    return stringListProperty("PBDs");
}

QStringList Host::GetPIFRefs() const
{
    return stringListProperty("PIFs");
}

bool Host::IsMaster() const
{
    // Check if this host is referenced as master in its pool
    // Query the pool and check if pool.master == this->opaqueRef()
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return false;

    QVariantMap poolData = conn->GetCache()->ResolveObjectData(XenObjectType::Pool, this->GetPoolRef());
    
    // Compare pool's master reference with this host's opaque reference
    QString masterRef = poolData.value("master", "").toString();
    return masterRef == this->OpaqueRef();
}

QString Host::GetPoolRef() const
{
    // In XenAPI, there's always exactly one pool per GetConnection
    // Query the cache for the pool that contains this host
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return QString();

    return conn->GetCache()->GetPoolRef();
}

QSharedPointer<Pool> Host::GetPool()
{
    if (!this->GetConnection())
        return QSharedPointer<Pool>();

    return this->GetConnection()->GetCache()->GetPool();
}

QSharedPointer<Pool> Host::GetPoolOfOne()
{
    if (!this->GetConnection())
        return QSharedPointer<Pool>();

    return this->GetConnection()->GetCache()->GetPoolOfOne();
}

qint64 Host::MemoryOverhead() const
{
    return intProperty("memory_overhead", 0);
}

qint64 Host::APIVersionMajor() const
{
    return intProperty("API_version_major", 0);
}

qint64 Host::APIVersionMinor() const
{
    return this->intProperty("API_version_minor", 0);
}

QString Host::APIVersionVendor() const
{
    return this->stringProperty("API_version_vendor");
}

QVariantMap Host::APIVersionVendorImplementation() const
{
    return this->property("API_version_vendor_implementation").toMap();
}

QVariantMap Host::CPUConfiguration() const
{
    return this->property("cpu_configuration").toMap();
}

QString Host::SchedPolicy() const
{
    return this->stringProperty("sched_policy");
}

QStringList Host::GetHostCPURefs() const
{
    return this->stringListProperty("host_CPUs");
}

QStringList Host::AllowedOperations() const
{
    return this->stringListProperty("allowed_operations");
}

QVariantMap Host::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

QStringList Host::SupportedBootloaders() const
{
    return this->stringListProperty("supported_bootloaders");
}

QVariantMap Host::Logging() const
{
    return this->property("logging").toMap();
}

QString Host::GetMetricsRef() const
{
    return this->stringProperty("metrics");
}

QSharedPointer<HostMetrics> Host::GetMetrics() const
{
    XenConnection* connection = GetConnection();
    if (!connection)
        return QSharedPointer<HostMetrics>();

    XenCache* cache = connection->GetCache();
    if (!cache)
        return QSharedPointer<HostMetrics>();

    const QString metricsRef = GetMetricsRef();
    if (metricsRef.isEmpty())
        return QSharedPointer<HostMetrics>();

    return cache->ResolveObject<HostMetrics>(metricsRef);
}

QStringList Host::HAStatefiles() const
{
    return this->stringListProperty("ha_statefiles");
}

QStringList Host::HANetworkPeers() const
{
    return this->stringListProperty("ha_network_peers");
}

QVariantMap Host::BIOSStrings() const
{
    return this->property("bios_strings").toMap();
}

QVariantMap Host::ChipsetInfo() const
{
    return this->property("chipset_info").toMap();
}

QString Host::ExternalAuthType() const
{
    return this->stringProperty("external_auth_type");
}

QString Host::ExternalAuthServiceName() const
{
    return this->stringProperty("external_auth_service_name");
}

QVariantMap Host::ExternalAuthConfiguration() const
{
    return this->property("external_auth_configuration").toMap();
}

bool Host::IsFreeLicense() const
{
    const QString edition = this->Edition().toLower();
    return edition == "free" || edition == "express" || edition == "trial";
}

bool Host::LinuxPackPresent() const
{
    return this->SoftwareVersion().contains("xs:linux");
}

QString Host::PowerOnMode() const
{
    return this->stringProperty("power_on_mode");
}

QVariantMap Host::PowerOnConfig() const
{
    return this->property("power_on_config").toMap();
}

QString Host::LocalCacheSRRef() const
{
    return this->stringProperty("local_cache_sr");
}

QStringList Host::PCIRefs() const
{
    return this->stringListProperty("PCIs");
}

QStringList Host::PGPURefs() const
{
    return this->stringListProperty("PGPUs");
}

QStringList Host::PUSBRefs() const
{
    return this->stringListProperty("PUSBs");
}

QStringList Host::PatchRefs() const
{
    return this->stringListProperty("patches");
}

QStringList Host::UpdateRefs() const
{
    return this->stringListProperty("updates");
}

QStringList Host::UpdatesRequiringRebootRefs() const
{
    return this->stringListProperty("updates_requiring_reboot");
}

QStringList Host::FeatureRefs() const
{
    return this->stringListProperty("features");
}

QStringList Host::PendingGuidances() const
{
    return this->stringListProperty("pending_guidances");
}

bool Host::SSLLegacy() const
{
    return this->boolProperty("ssl_legacy", true);
}

bool Host::TLSVerificationEnabled() const
{
    return this->boolProperty("tls_verification_enabled", false);
}

bool Host::HTTPSOnly() const
{
    return this->boolProperty("https_only", false);
}

QVariantMap Host::GuestVCPUsParams() const
{
    return this->property("guest_VCPUs_params").toMap();
}

QString Host::Display() const
{
    return this->stringProperty("display");
}

QList<qint64> Host::VirtualHardwarePlatformVersions() const
{
    QVariantList list = this->property("virtual_hardware_platform_versions").toList();
    QList<qint64> result;
    for (const QVariant& v : list)
    {
        result.append(v.toLongLong());
    }
    return result;
}

QString Host::ControlDomainRef() const
{
    return this->stringProperty("control_domain");
}

QString Host::IscsiIQN() const
{
    return this->stringProperty("iscsi_iqn");
}

bool Host::Multipathing() const
{
    return this->boolProperty("multipathing", false);
}

QString Host::UEFICertificates() const
{
    return this->stringProperty("uefi_certificates");
}

QStringList Host::CertificateRefs() const
{
    return this->stringListProperty("certificates");
}

QStringList Host::Editions() const
{
    return this->stringListProperty("editions");
}

QStringList Host::CrashdumpRefs() const
{
    return this->stringListProperty("crashdumps");
}

QDateTime Host::LastSoftwareUpdate() const
{
    QString dateStr = this->stringProperty("last_software_update");
    return Misc::ParseXenDateTime(dateStr);
}

QString Host::LatestSyncedUpdatesApplied() const
{
    return this->stringProperty("latest_synced_updates_applied");
}

QVariantMap Host::LicenseParams() const
{
    return this->property("license_params").toMap();
}

QString Host::Edition() const
{
    return this->stringProperty("edition");
}

QVariantMap Host::LicenseServer() const
{
    return this->property("license_server").toMap();
}

QString Host::BuildNumberRaw() const
{
    return this->SoftwareVersion().value("build_number").toString();
}

QString Host::PlatformVersion() const
{
    return this->SoftwareVersion().value("platform_version").toString();
}

QString Host::ProductBrand() const
{
    return this->SoftwareVersion().value("product_brand").toString();
}

QString Host::GetDatabaseSchema() const
{
    const QVariantMap softwareVersion = this->SoftwareVersion();
    const QString dbSchema = softwareVersion.value("db_schema").toString();
    if (!dbSchema.isEmpty())
        return dbSchema;

    return softwareVersion.value("database_schema").toString();
}

QList<QSharedPointer<PoolUpdate>> Host::AppliedUpdates() const
{
    QList<QSharedPointer<PoolUpdate>> updates;

    XenConnection* connection = this->GetConnection();
    if (!connection || !connection->GetCache())
        return updates;

    const QStringList updateRefs = this->UpdateRefs();
    for (const QString& ref : updateRefs)
    {
        QSharedPointer<PoolUpdate> update = connection->GetCache()->ResolveObject<PoolUpdate>(XenObjectType::PoolUpdate, ref);
        if (update && update->IsValid())
            updates.append(update);
    }

    return updates;
}

QList<Host::SuppPack> Host::SuppPacks() const
{
    QList<SuppPack> packs;
    const QVariantMap softwareVersion = this->SoftwareVersion();
    if (softwareVersion.isEmpty())
        return packs;

    for (auto it = softwareVersion.constBegin(); it != softwareVersion.constEnd(); ++it)
    {
        const QString key = it.key();
        const QString value = it.value().toString();

        SuppPack pack;

        const QStringList keyParts = key.split(':');
        if (keyParts.size() != 2)
            continue;

        pack.Originator = keyParts.at(0);
        pack.Name = keyParts.at(1);

        const int versionIndex = value.lastIndexOf(", version ");
        if (versionIndex <= 0)
            continue;

        pack.Description = value.left(versionIndex);
        QString remainder = value.mid(versionIndex + 10);
        const QStringList parts = remainder.split(", ");
        if (parts.isEmpty() || parts.size() > 3)
            continue;

        pack.Version = parts.at(0);

        if (parts.size() >= 2)
        {
            if (!parts.at(1).startsWith("build "))
                continue;
            pack.Build = parts.at(1).mid(6);
        }

        if (parts.size() >= 3)
        {
            if (parts.at(2) != "homogeneous")
                continue;
            pack.Homogeneous = true;
        }

        pack.IsValid = true;
        packs.append(pack);
    }

    return packs;
}

// Property getters for search/query functionality

QList<QSharedPointer<VM>> Host::GetResidentVMs() const
{
    QList<QSharedPointer<VM>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList vmRefs = this->GetResidentVMRefs();
    for (const QString& ref : vmRefs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
            if (vm)
                result.append(vm);
        }
    }
    
    return result;
}

bool Host::HasRunningVMs() const
{
    const QList<QSharedPointer<VM>> vms = GetResidentVMs();
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (vm && vm->IsRunning())
            return true;
    }

    return false;
}

QList<QSharedPointer<PBD>> Host::GetPBDs() const
{
    QList<QSharedPointer<PBD>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList pbdRefs = this->GetPBDRefs();
    for (const QString& ref : pbdRefs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<PBD> pbd = cache->ResolveObject<PBD>(ref);
            if (pbd)
                result.append(pbd);
        }
    }
    
    return result;
}

QList<QSharedPointer<PIF>> Host::GetPIFs() const
{
    QList<QSharedPointer<PIF>> result;
    
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return result;
    
    XenCache* cache = connection->GetCache();
    if (!cache)
        return result;
    
    QStringList pifRefs = this->GetPIFRefs();
    for (const QString& ref : pifRefs)
    {
        if (!ref.isEmpty() && ref != XENOBJECT_NULL)
        {
            QSharedPointer<PIF> pif = cache->ResolveObject<PIF>(ref);
            if (pif)
                result.append(pif);
        }
    }
    
    return result;
}

QList<ComparableAddress> Host::GetIPAddresses() const
{
    // C# equivalent: PropertyAccessors IP address property for Host
    // Gets IPs from PIFs (physical network interfaces)
    
    QList<ComparableAddress> addresses;
    
    QStringList pifRefs = this->GetPIFRefs();
    if (pifRefs.isEmpty())
        return addresses;
    
    XenConnection* conn = this->GetConnection();
    if (!conn)
        return addresses;
    
    XenCache* cache = conn->GetCache();
    if (!cache)
        return addresses;
    
    // Iterate through all PIFs and collect IP addresses
    for (const QString& pifRef : pifRefs)
    {
        QVariantMap pifData = cache->ResolveObjectData(XenObjectType::PIF, pifRef);
        if (pifData.isEmpty())
            continue;
        
        // Get IP address from PIF
        QString ipStr = pifData.value("IP", QString()).toString();
        if (ipStr.isEmpty() || ipStr == "0.0.0.0")
            continue;
        
        // Try to parse as IP address
        ComparableAddress addr;
        if (ComparableAddress::TryParse(ipStr, false, true, addr))
        {
            addresses.append(addr);
        }
    }
    
    return addresses;
}
qint64 Host::MemoryFreeCalc() const
{
    QSharedPointer<HostMetrics> metrics = this->GetMetrics();
    if (metrics.isNull())
        return 0;

    qint64 used = this->MemoryOverhead();
    
    QList<QSharedPointer<VM>> residentVMs = this->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull())
            continue;
            
        used += vm->MemoryOverhead();
        
        QSharedPointer<VMMetrics> vmMetrics = vm->GetMetrics();
        if (!vmMetrics.isNull())
        {
            used += vmMetrics->GetMemoryActual();
        }
    }

    // This hack is needed because of bug CA-32509. xapi uses a deliberately generous
    // estimate of VM.memory_overhead: but the low-level squeezer code doesn't (and can't)
    // know about the same calculation, and so uses some of this memory_overhead for the
    // VM's memory_actual. This causes up to 1MB of double-counting per VM.
    qint64 totalMemory = metrics->GetMemoryTotal();
    return (totalMemory > used) ? (totalMemory - used) : 0;
}

qint64 Host::TotDynMin() const
{
    qint64 total = 0;
    
    QList<QSharedPointer<VM>> residentVMs = this->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull() || vm->IsControlDomain())
            continue;
            
        total += vm->SupportsBallooning() ? vm->GetMemoryDynamicMin() : vm->GetMemoryStaticMax();
    }
    
    return total;
}

qint64 Host::TotDynMax() const
{
    qint64 total = 0;
    
    QList<QSharedPointer<VM>> residentVMs = this->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull() || vm->IsControlDomain())
            continue;
            
        total += vm->SupportsBallooning() ? vm->GetMemoryDynamicMax() : vm->GetMemoryStaticMax();
    }
    
    return total;
}

qint64 Host::MemoryAvailableCalc() const
{
    QSharedPointer<HostMetrics> metrics = this->GetMetrics();
    if (metrics.isNull())
        return 0;

    qint64 avail = metrics->GetMemoryTotal() - this->TotDynMin() - this->XenMemoryCalc();
    
    // Don't return negative values (shouldn't happen, but play it safe per CA-32509)
    if (avail < 0)
        avail = 0;
        
    return avail;
}

qint64 Host::XenMemoryCalc() const
{
    qint64 xenMem = this->MemoryOverhead();
    
    QList<QSharedPointer<VM>> residentVMs = this->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull())
            continue;
            
        xenMem += vm->MemoryOverhead();
        
        if (vm->IsControlDomain())
        {
            QSharedPointer<VMMetrics> vmMetrics = vm->GetMetrics();
            if (!vmMetrics.isNull())
            {
                xenMem += vmMetrics->GetMemoryActual();
            }
        }
    }
    
    return xenMem;
}

qint64 Host::Dom0Memory() const
{
    QSharedPointer<VM> dom0 = this->ControlDomainZero();
    if (dom0.isNull())
        return 0;

    QSharedPointer<VMMetrics> metrics = dom0->GetMetrics();
    return metrics.isNull() ? dom0->GetMemoryDynamicMin() : metrics->GetMemoryActual();
}

QSharedPointer<VM> Host::ControlDomainZero() const
{
    QList<QSharedPointer<VM>> residentVMs = this->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (!vm.isNull() && vm->IsControlDomain())
            return vm;
    }
    
    return QSharedPointer<VM>();
}
