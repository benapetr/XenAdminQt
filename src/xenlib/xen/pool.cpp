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
#include "../xenlib.h"
#include "../xencache.h"

Pool::Pool(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString Pool::objectType() const
{
    return "pool";
}

QString Pool::masterRef() const
{
    return stringProperty("master");
}

QString Pool::defaultSRRef() const
{
    return stringProperty("default_SR");
}

bool Pool::haEnabled() const
{
    return boolProperty("ha_enabled", false);
}

QVariantMap Pool::haConfiguration() const
{
    return property("ha_configuration").toMap();
}

QVariantMap Pool::otherConfig() const
{
    return property("other_config").toMap();
}

QStringList Pool::hostRefs() const
{
    // In XenAPI, there's exactly one pool per connection, so all hosts belong to this pool
    // Query the cache for all host references
    XenConnection* conn = this->connection();
    if (!conn || !conn->getCache())
        return QStringList();

    // Get all host references from the cache
    return conn->getCache()->GetAllRefs("host");
}

bool Pool::isPoolOfOne() const
{
    return hostRefs().count() == 1;
}

QStringList Pool::tags() const
{
    return stringListProperty("tags");
}

bool Pool::wlbEnabled() const
{
    return boolProperty("wlb_enabled", false);
}

bool Pool::livePatchingDisabled() const
{
    return boolProperty("live_patching_disabled", false);
}

QString Pool::suspendImageSRRef() const
{
    return stringProperty("suspend_image_SR");
}

QString Pool::crashDumpSRRef() const
{
    return stringProperty("crash_dump_SR");
}

QStringList Pool::haStatefiles() const
{
    return stringListProperty("ha_statefiles");
}

qint64 Pool::haHostFailuresToTolerate() const
{
    return intProperty("ha_host_failures_to_tolerate", 0);
}

qint64 Pool::haPlanExistsFor() const
{
    return intProperty("ha_plan_exists_for", 0);
}

bool Pool::haAllowOvercommit() const
{
    return boolProperty("ha_allow_overcommit", false);
}

bool Pool::haOvercommitted() const
{
    return boolProperty("ha_overcommitted", false);
}

QString Pool::haClusterStack() const
{
    return stringProperty("ha_cluster_stack");
}

bool Pool::redoLogEnabled() const
{
    return boolProperty("redo_log_enabled", false);
}

QString Pool::redoLogVDIRef() const
{
    return stringProperty("redo_log_vdi");
}

QVariantMap Pool::guiConfig() const
{
    return property("gui_config").toMap();
}

QVariantMap Pool::healthCheckConfig() const
{
    return property("health_check_config").toMap();
}

QVariantMap Pool::guestAgentConfig() const
{
    return property("guest_agent_config").toMap();
}

QVariantMap Pool::cpuInfo() const
{
    return property("cpu_info").toMap();
}

QVariantMap Pool::blobs() const
{
    return property("blobs").toMap();
}

QStringList Pool::metadataVDIRefs() const
{
    return stringListProperty("metadata_VDIs");
}

QString Pool::wlbUrl() const
{
    return stringProperty("wlb_url");
}

QString Pool::wlbUsername() const
{
    return stringProperty("wlb_username");
}

bool Pool::wlbVerifyCert() const
{
    return boolProperty("wlb_verify_cert", false);
}

QString Pool::vswitchController() const
{
    return stringProperty("vswitch_controller");
}

QVariantMap Pool::restrictions() const
{
    return property("restrictions").toMap();
}

bool Pool::policyNoVendorDevice() const
{
    return boolProperty("policy_no_vendor_device", false);
}

QStringList Pool::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap Pool::currentOperations() const
{
    return property("current_operations").toMap();
}

bool Pool::igmpSnoopingEnabled() const
{
    return boolProperty("igmp_snooping_enabled", false);
}

QString Pool::uefiCertificates() const
{
    return stringProperty("uefi_certificates");
}

bool Pool::tlsVerificationEnabled() const
{
    return boolProperty("tls_verification_enabled", false);
}

bool Pool::clientCertificateAuthEnabled() const
{
    return boolProperty("client_certificate_auth_enabled", false);
}

QString Pool::clientCertificateAuthName() const
{
    return stringProperty("client_certificate_auth_name");
}

QStringList Pool::repositoryRefs() const
{
    return stringListProperty("repositories");
}

QString Pool::repositoryProxyUrl() const
{
    return stringProperty("repository_proxy_url");
}

QString Pool::repositoryProxyUsername() const
{
    return stringProperty("repository_proxy_username");
}

QString Pool::repositoryProxyPasswordRef() const
{
    return stringProperty("repository_proxy_password");
}

bool Pool::migrationCompression() const
{
    return boolProperty("migration_compression", false);
}

bool Pool::coordinatorBias() const
{
    return boolProperty("coordinator_bias", true);
}

QString Pool::telemetryUuidRef() const
{
    return stringProperty("telemetry_uuid");
}

QString Pool::telemetryFrequency() const
{
    return stringProperty("telemetry_frequency");
}

QDateTime Pool::telemetryNextCollection() const
{
    QString dateStr = stringProperty("telemetry_next_collection");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

QDateTime Pool::lastUpdateSync() const
{
    QString dateStr = stringProperty("last_update_sync");
    if (dateStr.isEmpty())
        return QDateTime();
    return QDateTime::fromString(dateStr, Qt::ISODate);
}

QString Pool::updateSyncFrequency() const
{
    return stringProperty("update_sync_frequency");
}

qint64 Pool::updateSyncDay() const
{
    return intProperty("update_sync_day", 0);
}

bool Pool::updateSyncEnabled() const
{
    return boolProperty("update_sync_enabled", false);
}

bool Pool::isPsrPending() const
{
    return boolProperty("is_psr_pending", false);
}
