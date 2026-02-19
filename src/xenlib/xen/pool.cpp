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

#include "pool.h"
#include "network/connection.h"
#include "../xencache.h"
#include "host.h"
#include "sr.h"
#include "vm.h"
#include "vdi.h"
#include "pif.h"
#include "pgpu.h"
#include "../utils/misc.h"

Pool::Pool(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QString Pool::GetName() const
{
    QString name = XenObject::GetName();
    if (!name.isEmpty())
        return name;

    QSharedPointer<Host> master = this->GetCache()->ResolveObject<Host>(this->GetMasterHostRef());
    return master ? master->GetName() : QString();
}

QString Pool::LocationString() const
{
    return QString();
}

QString Pool::GetMasterHostRef() const
{
    return stringProperty("master");
}

QString Pool::GetDefaultSRRef() const
{
    return stringProperty("default_SR");
}

bool Pool::HAEnabled() const
{
    return boolProperty("ha_enabled", false);
}

QVariantMap Pool::GetHAConfiguration() const
{
    return property("ha_configuration").toMap();
}

QStringList Pool::GetHostRefs() const
{
    // Get all host references from the cache
    return this->GetCache()->GetAllRefs(XenObjectType::Host);
}

bool Pool::IsPoolOfOne() const
{
    return GetHostRefs().count() == 1;
}

bool Pool::IsVisible() const
{
    if (!this->GetConnection() || !this->GetCache())
        return false;

    const int hostCount = this->GetCache()->GetAllRefs(XenObjectType::Host).size();
    // Match C# Pool.IsVisible: name_label must be set (no master-name fallback)
    return !this->stringProperty("name_label").isEmpty() || hostCount > 1;
}

bool Pool::IsWLBEnabled() const
{
    return boolProperty("wlb_enabled", false);
}

bool Pool::IsLivePatchingDisabled() const
{
    return boolProperty("live_patching_disabled", false);
}

QString Pool::GetSuspendImageSRRef() const
{
    return stringProperty("suspend_image_SR");
}

QString Pool::GetCrashDumpSRRef() const
{
    return stringProperty("crash_dump_SR");
}

QStringList Pool::HAStatefiles() const
{
    return stringListProperty("ha_statefiles");
}

qint64 Pool::HAHostFailuresToTolerate() const
{
    return intProperty("ha_host_failures_to_tolerate", 0);
}

qint64 Pool::HAPlanExistsFor() const
{
    return intProperty("ha_plan_exists_for", 0);
}

bool Pool::HAAllowOvercommit() const
{
    return boolProperty("ha_allow_overcommit", false);
}

bool Pool::HAOvercommitted() const
{
    return boolProperty("ha_overcommitted", false);
}

QString Pool::HAClusterStack() const
{
    return stringProperty("ha_cluster_stack");
}

bool Pool::RedoLogEnabled() const
{
    return boolProperty("redo_log_enabled", false);
}

QString Pool::GetRedoLogVDIRef() const
{
    return stringProperty("redo_log_vdi");
}

QVariantMap Pool::GUIConfig() const
{
    return property("gui_config").toMap();
}

QVariantMap Pool::HealthCheckConfig() const
{
    return property("health_check_config").toMap();
}

QVariantMap Pool::GuestAgentConfig() const
{
    return property("guest_agent_config").toMap();
}

QVariantMap Pool::CPUInfo() const
{
    return property("cpu_info").toMap();
}

QVariantMap Pool::Blobs() const
{
    return property("blobs").toMap();
}

QStringList Pool::GetMetadataVDIRefs() const
{
    return stringListProperty("metadata_VDIs");
}

QString Pool::WLBUrl() const
{
    return stringProperty("wlb_url");
}

QString Pool::WLBUsername() const
{
    return stringProperty("wlb_username");
}

bool Pool::WLBVerifyCert() const
{
    return boolProperty("wlb_verify_cert", false);
}

QString Pool::VswitchController() const
{
    return stringProperty("vswitch_controller");
}

bool Pool::vSwitchController() const
{
    if (this->VswitchController().isEmpty())
        return false;

    const QList<QSharedPointer<Host>> hosts = this->GetCache()->GetAll<Host>(XenObjectType::Host);
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;
        if (host->RestrictVSwitchController())
            return false;
        if (!host->vSwitchNetworkBackend())
            return false;
    }

    return true;
}

bool Pool::HasSriovNic() const
{
    const QList<QSharedPointer<PIF>> pifs = this->GetCache()->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->SriovCapable())
            return true;
    }
    return false;
}

bool Pool::HasGpu() const
{
    XenCache* cache = this->GetCache();
    if (!cache)
        return false;

    return !cache->GetAllRefs(XenObjectType::PGPU).isEmpty();
}

bool Pool::HasVGpu() const
{
    XenCache* cache = this->GetCache();
    if (!cache || !this->HasGpu())
        return false;

    const QList<QSharedPointer<PGPU>> pgpus = cache->GetAll<PGPU>(XenObjectType::PGPU);
    for (const QSharedPointer<PGPU>& pgpu : pgpus)
    {
        if (pgpu && pgpu->IsValid() && pgpu->HasVGpu())
            return true;
    }

    return false;
}

QVariantMap Pool::Restrictions() const
{
    return property("restrictions").toMap();
}

bool Pool::PolicyNoVendorDevice() const
{
    return boolProperty("policy_no_vendor_device", false);
}

QStringList Pool::AllowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap Pool::CurrentOperations() const
{
    return property("current_operations").toMap();
}

bool Pool::IGMPSnoopingEnabled() const
{
    return boolProperty("igmp_snooping_enabled", false);
}

QString Pool::UEFICertificates() const
{
    return stringProperty("uefi_certificates");
}

bool Pool::TLSVerificationEnabled() const
{
    return boolProperty("tls_verification_enabled", false);
}

bool Pool::ClientCertificateAuthEnabled() const
{
    return boolProperty("client_certificate_auth_enabled", false);
}

QString Pool::ClientCertificateAuthName() const
{
    return stringProperty("client_certificate_auth_name");
}

QStringList Pool::RepositoryRefs() const
{
    return stringListProperty("repositories");
}

QString Pool::RepositoryProxyUrl() const
{
    return stringProperty("repository_proxy_url");
}

QString Pool::RepositoryProxyUsername() const
{
    return stringProperty("repository_proxy_username");
}

QString Pool::RepositoryProxyPasswordRef() const
{
    return stringProperty("repository_proxy_password");
}

bool Pool::MigrationCompression() const
{
    return boolProperty("migration_compression", false);
}

bool Pool::CoordinatorBias() const
{
    return boolProperty("coordinator_bias", true);
}

QString Pool::TelemetryUuidRef() const
{
    return stringProperty("telemetry_uuid");
}

QString Pool::TelemetryFrequency() const
{
    return stringProperty("telemetry_frequency");
}

QDateTime Pool::TelemetryNextCollection() const
{
    QString dateStr = stringProperty("telemetry_next_collection");
    return Misc::ParseXenDateTime(dateStr);
}

QDateTime Pool::LastUpdateSync() const
{
    QString dateStr = stringProperty("last_update_sync");
    return Misc::ParseXenDateTime(dateStr);
}

QString Pool::UpdateSyncFrequency() const
{
    return stringProperty("update_sync_frequency");
}

qint64 Pool::UpdateSyncDay() const
{
    return intProperty("update_sync_day", 0);
}

bool Pool::UpdateSyncEnabled() const
{
    return boolProperty("update_sync_enabled", false);
}

bool Pool::IsPsrPending() const
{
    return boolProperty("is_psr_pending", false);
}

// Property getters for search/query functionality

QStringList Pool::GetAllVMRefs() const
{
    // C# equivalent: PropertyAccessors VM property for Pool
    // Returns all VMs from connection.Cache.VMs
    return this->GetCache()->GetAllRefs(XenObjectType::VM);
}

QList<QSharedPointer<VDI>> Pool::GetMetadataVDIs() const
{
    QList<QSharedPointer<VDI>> vdis;
    
    XenCache* cache = this->GetCache();

    const QStringList vdiRefs = this->GetMetadataVDIRefs();
    for (const QString& vdiRef : vdiRefs)
    {
        QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
        if (vdi && vdi->IsValid())
            vdis.append(vdi);
    }

    return vdis;
}

QSharedPointer<SR> Pool::GetDefaultSR() const
{
    QString srRef = this->GetDefaultSRRef();
    if (srRef.isEmpty() || srRef == XENOBJECT_NULL)
        return QSharedPointer<SR>();

    return this->GetCache()->ResolveObject<SR>(srRef);
}

QSharedPointer<SR> Pool::GetSuspendImageSR() const
{
    QString srRef = this->GetSuspendImageSRRef();
    if (srRef.isEmpty() || srRef == XENOBJECT_NULL)
        return QSharedPointer<SR>();

    return this->GetCache()->ResolveObject<SR>(srRef);
}

QSharedPointer<SR> Pool::GetCrashDumpSR() const
{
    QString srRef = this->GetCrashDumpSRRef();
    if (srRef.isEmpty() || srRef == XENOBJECT_NULL)
        return QSharedPointer<SR>();

    return this->GetCache()->ResolveObject<SR>(srRef);
}

QSharedPointer<VDI> Pool::GetRedoLogVDI() const
{
    QString vdiRef = this->GetRedoLogVDIRef();
    if (vdiRef.isEmpty() || vdiRef == XENOBJECT_NULL)
        return QSharedPointer<VDI>();

    return this->GetCache()->ResolveObject<VDI>(vdiRef);
}

QList<QSharedPointer<Host>> Pool::GetHosts() const
{
    QList<QSharedPointer<Host>> hosts;

    XenCache* cache = this->GetCache();

    const QStringList hostRefs = this->GetHostRefs();
    for (const QString& hostRef : hostRefs)
    {
        QSharedPointer<Host> host = cache->ResolveObject<Host>(hostRef);
        if (host && host->IsValid())
            hosts.append(host);
    }

    return hosts;
}

QSharedPointer<Host> Pool::GetMasterHost() const
{
    QString masterRef = this->GetMasterHostRef();
    if (masterRef.isEmpty() || masterRef == XENOBJECT_NULL)
        return QSharedPointer<Host>();

    return this->GetCache()->ResolveObject<Host>(XenObjectType::Host, masterRef);
}

QList<QSharedPointer<VM>> Pool::GetAllVMs() const
{
    QList<QSharedPointer<VM>> vms;

    XenCache* cache = this->GetCache();

    const QStringList vmRefs = this->GetAllVMRefs();
    for (const QString& vmRef : vmRefs)
    {
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
        if (vm && vm->IsValid())
            vms.append(vm);
    }

    return vms;
}

bool Pool::IsNotFullyUpgraded() const
{
    // C# equivalent: PropertyNames.isNotFullyUpgraded
    // Returns true if hosts in pool have different software versions
    
    QStringList hostRefs = this->GetHostRefs();
    if (hostRefs.size() <= 1)
        return false; // Single host or no hosts, can't be mismatched
    
    XenCache* cache = this->GetCache();
    
    // Compare software_version of all hosts
    QString firstVersion;
    for (const QString& hostRef : hostRefs)
    {
        QVariantMap hostData = cache->ResolveObjectData(XenObjectType::Host, hostRef);
        if (hostData.isEmpty())
            continue;
        
        QVariantMap softwareVersion = hostData.value("software_version").toMap();
        QString versionStr = softwareVersion.value("product_version", QString()).toString();
        
        if (firstVersion.isEmpty())
        {
            firstVersion = versionStr;
        } else if (firstVersion != versionStr)
        {
            return true; // Version mismatch found
        }
    }
    
    return false; // All versions match
}
