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
#include "../xenlib.h"
#include "../xencache.h"
#include "host.h"
#include <QDomDocument>
#include <algorithm>
#include <cmath>

VM::VM(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VM::GetObjectType() const
{
    return "vm";
}

QString VM::GetPowerState() const
{
    return stringProperty("power_state");
}

bool VM::IsTemplate() const
{
    return boolProperty("is_a_template", false);
}

bool VM::IsDefaultTemplate() const
{
    return boolProperty("is_default_template", false);
}

bool VM::IsSnapshot() const
{
    return boolProperty("is_a_snapshot", false);
}

QString VM::ResidentOnRef() const
{
    return stringProperty("resident_on");
}

QString VM::AffinityRef() const
{
    return stringProperty("affinity");
}

QStringList VM::VBDRefs() const
{
    return stringListProperty("VBDs");
}

QStringList VM::VIFRefs() const
{
    return stringListProperty("VIFs");
}

QStringList VM::ConsoleRefs() const
{
    return stringListProperty("consoles");
}

QString VM::SnapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

QStringList VM::SnapshotRefs() const
{
    return stringListProperty("snapshots");
}

QString VM::SuspendVDIRef() const
{
    return stringProperty("suspend_VDI");
}

qint64 VM::MemoryTarget() const
{
    return longProperty("memory_target", 0);
}

qint64 VM::MemoryStaticMax() const
{
    return longProperty("memory_static_max", 0);
}

qint64 VM::MemoryDynamicMax() const
{
    return longProperty("memory_dynamic_max", 0);
}

qint64 VM::MemoryDynamicMin() const
{
    return longProperty("memory_dynamic_min", 0);
}

qint64 VM::MemoryStaticMin() const
{
    return longProperty("memory_static_min", 0);
}

int VM::VCPUsMax() const
{
    return intProperty("VCPUs_max", 0);
}

int VM::VCPUsAtStartup() const
{
    return intProperty("VCPUs_at_startup", 0);
}

bool VM::IsHvm() const
{
    return !stringProperty("HVM_boot_policy").isEmpty();
}

bool VM::IsWindows() const
{
    QString guestMetricsRef = stringProperty("guest_metrics");
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        XenCache* cache = GetConnection() ? GetConnection()->GetCache() : nullptr;
        if (cache)
        {
            QVariantMap metricsData = cache->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
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

    if (this->IsHvm())
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

        QList<QVariantMap> vms = cache->GetAllData("vm");
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

        QList<QVariantMap> vms = cache->GetAllData("vm");
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

    QString home = this->HomeRef();
    if (!home.isEmpty() && home != "OpaqueRef:NULL")
    {
        QSharedPointer<Host> host = cache->ResolveObject<Host>("host", home);
        if (host)
            return host->coresPerSocket();
    }

    long maxCores = 0;
    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>("host");
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;

        long cores = host->coresPerSocket();
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

QVariantMap VM::OtherConfig() const
{
    return property("other_config").toMap();
}

QVariantMap VM::Platform() const
{
    return property("platform").toMap();
}

QStringList VM::Tags() const
{
    return stringListProperty("tags");
}

QStringList VM::AllowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap VM::CurrentOperations() const
{
    return property("current_operations").toMap();
}

QString VM::HomeRef() const
{
    // Return affinity host if set, otherwise resident host
    QString affinity = this->AffinityRef();
    if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
        return affinity;

    return this->ResidentOnRef();
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

QString VM::GuestMetricsRef() const
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
