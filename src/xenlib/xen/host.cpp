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
#include "../xenlib.h"
#include "../xencache.h"

Host::Host(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString Host::objectType() const
{
    return "host";
}

QString Host::hostname() const
{
    return stringProperty("hostname");
}

QString Host::address() const
{
    return stringProperty("address");
}

bool Host::enabled() const
{
    return boolProperty("enabled", true);
}

QStringList Host::residentVMRefs() const
{
    return stringListProperty("resident_VMs");
}

QVariantMap Host::softwareVersion() const
{
    return property("software_version").toMap();
}

QStringList Host::capabilities() const
{
    return stringListProperty("capabilities");
}

QVariantMap Host::cpuInfo() const
{
    return property("cpu_info").toMap();
}

int Host::cpuSockets() const
{
    QVariantMap cpuInfoMap = this->cpuInfo();
    if (!cpuInfoMap.contains("socket_count"))
        return 0;

    bool ok = false;
    int sockets = cpuInfoMap.value("socket_count").toString().toInt(&ok);
    return ok ? sockets : 0;
}

int Host::cpuCount() const
{
    QVariantMap cpuInfoMap = this->cpuInfo();
    if (!cpuInfoMap.contains("cpu_count"))
        return 0;

    bool ok = false;
    int cpuCount = cpuInfoMap.value("cpu_count").toString().toInt(&ok);
    return ok ? cpuCount : 0;
}

int Host::coresPerSocket() const
{
    int sockets = this->cpuSockets();
    int cpuCount = this->cpuCount();
    if (sockets > 0 && cpuCount > 0)
        return cpuCount / sockets;

    return 0;
}

int Host::hostCpuCount() const
{
    return stringListProperty("host_CPUs").size();
}

QVariantMap Host::otherConfig() const
{
    return property("other_config").toMap();
}

QStringList Host::tags() const
{
    return stringListProperty("tags");
}

QString Host::suspendImageSRRef() const
{
    return stringProperty("suspend_image_sr");
}

QString Host::crashDumpSRRef() const
{
    return stringProperty("crash_dump_sr");
}

QStringList Host::pbdRefs() const
{
    return stringListProperty("PBDs");
}

QStringList Host::pifRefs() const
{
    return stringListProperty("PIFs");
}

bool Host::isMaster() const
{
    // Check if this host is referenced as master in its pool
    // Query the pool and check if pool.master == this->opaqueRef()
    XenConnection* conn = this->connection();
    if (!conn || !conn->getCache())
        return false;

    // Get the pool for this connection (there's always exactly one pool per connection)
    QStringList poolRefs = conn->getCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return false;

    // Get the first (and only) pool
    QString poolRef = poolRefs.first();
    QVariantMap poolData = conn->getCache()->ResolveObjectData("pool", poolRef);
    
    // Compare pool's master reference with this host's opaque reference
    QString masterRef = poolData.value("master", "").toString();
    return masterRef == this->opaqueRef();
}

QString Host::poolRef() const
{
    // In XenAPI, there's always exactly one pool per connection
    // Query the cache for the pool that contains this host
    XenConnection* conn = this->connection();
    if (!conn || !conn->getCache())
        return QString();

    // Get all pool refs (there's always exactly one pool per connection)
    QStringList poolRefs = conn->getCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return QString();

    // Return the first (and only) pool reference
    return poolRefs.first();
}

qint64 Host::memoryOverhead() const
{
    return intProperty("memory_overhead", 0);
}

qint64 Host::apiVersionMajor() const
{
    return intProperty("API_version_major", 0);
}

qint64 Host::apiVersionMinor() const
{
    return intProperty("API_version_minor", 0);
}

QString Host::apiVersionVendor() const
{
    return stringProperty("API_version_vendor");
}

QVariantMap Host::apiVersionVendorImplementation() const
{
    return property("API_version_vendor_implementation").toMap();
}

QVariantMap Host::cpuConfiguration() const
{
    return property("cpu_configuration").toMap();
}

QString Host::schedPolicy() const
{
    return stringProperty("sched_policy");
}

QStringList Host::hostCPURefs() const
{
    return stringListProperty("host_CPUs");
}

QStringList Host::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap Host::currentOperations() const
{
    return property("current_operations").toMap();
}

QStringList Host::supportedBootloaders() const
{
    return stringListProperty("supported_bootloaders");
}

QVariantMap Host::logging() const
{
    return property("logging").toMap();
}

QString Host::metricsRef() const
{
    return stringProperty("metrics");
}

QStringList Host::haStatefiles() const
{
    return stringListProperty("ha_statefiles");
}

QStringList Host::haNetworkPeers() const
{
    return stringListProperty("ha_network_peers");
}

QVariantMap Host::biosStrings() const
{
    return property("bios_strings").toMap();
}

QVariantMap Host::chipsetInfo() const
{
    return property("chipset_info").toMap();
}

QString Host::externalAuthType() const
{
    return stringProperty("external_auth_type");
}

QString Host::externalAuthServiceName() const
{
    return stringProperty("external_auth_service_name");
}

QVariantMap Host::externalAuthConfiguration() const
{
    return property("external_auth_configuration").toMap();
}

QString Host::powerOnMode() const
{
    return stringProperty("power_on_mode");
}

QVariantMap Host::powerOnConfig() const
{
    return property("power_on_config").toMap();
}

QString Host::localCacheSRRef() const
{
    return stringProperty("local_cache_sr");
}

QStringList Host::pciRefs() const
{
    return stringListProperty("PCIs");
}

QStringList Host::pgpuRefs() const
{
    return stringListProperty("PGPUs");
}

QStringList Host::pusbRefs() const
{
    return stringListProperty("PUSBs");
}

QStringList Host::patchRefs() const
{
    return stringListProperty("patches");
}

QStringList Host::updateRefs() const
{
    return stringListProperty("updates");
}

QStringList Host::updatesRequiringRebootRefs() const
{
    return stringListProperty("updates_requiring_reboot");
}

QStringList Host::featureRefs() const
{
    return stringListProperty("features");
}

QStringList Host::pendingGuidances() const
{
    return stringListProperty("pending_guidances");
}

bool Host::sslLegacy() const
{
    return boolProperty("ssl_legacy", true);
}

bool Host::tlsVerificationEnabled() const
{
    return boolProperty("tls_verification_enabled", false);
}

bool Host::httpsOnly() const
{
    return boolProperty("https_only", false);
}

QVariantMap Host::guestVCPUsParams() const
{
    return property("guest_VCPUs_params").toMap();
}

QString Host::display() const
{
    return stringProperty("display");
}

QList<qint64> Host::virtualHardwarePlatformVersions() const
{
    QVariantList list = property("virtual_hardware_platform_versions").toList();
    QList<qint64> result;
    for (const QVariant& v : list)
    {
        result.append(v.toLongLong());
    }
    return result;
}

QString Host::controlDomainRef() const
{
    return stringProperty("control_domain");
}

QString Host::iscsiIqn() const
{
    return stringProperty("iscsi_iqn");
}

bool Host::multipathing() const
{
    return boolProperty("multipathing", false);
}

QString Host::uefiCertificates() const
{
    return stringProperty("uefi_certificates");
}

QStringList Host::certificateRefs() const
{
    return stringListProperty("certificates");
}

QStringList Host::editions() const
{
    return stringListProperty("editions");
}

QStringList Host::crashdumpRefs() const
{
    return stringListProperty("crashdumps");
}

QDateTime Host::lastSoftwareUpdate() const
{
    QString dateStr = stringProperty("last_software_update");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

QString Host::latestSyncedUpdatesApplied() const
{
    return stringProperty("latest_synced_updates_applied");
}

QVariantMap Host::licenseParams() const
{
    return property("license_params").toMap();
}

QString Host::edition() const
{
    return stringProperty("edition");
}

QVariantMap Host::licenseServer() const
{
    return property("license_server").toMap();
}
