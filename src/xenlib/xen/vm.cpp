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

QString VM::objectType() const
{
    return "vm";
}

QString VM::powerState() const
{
    return stringProperty("power_state");
}

bool VM::isTemplate() const
{
    return boolProperty("is_a_template", false);
}

bool VM::isDefaultTemplate() const
{
    return boolProperty("is_default_template", false);
}

bool VM::isSnapshot() const
{
    return boolProperty("is_a_snapshot", false);
}

QString VM::residentOnRef() const
{
    return stringProperty("resident_on");
}

QString VM::affinityRef() const
{
    return stringProperty("affinity");
}

QStringList VM::vbdRefs() const
{
    return stringListProperty("VBDs");
}

QStringList VM::vifRefs() const
{
    return stringListProperty("VIFs");
}

QStringList VM::consoleRefs() const
{
    return stringListProperty("consoles");
}

QString VM::snapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

QStringList VM::snapshotRefs() const
{
    return stringListProperty("snapshots");
}

QString VM::suspendVDIRef() const
{
    return stringProperty("suspend_VDI");
}

qint64 VM::memoryTarget() const
{
    return longProperty("memory_target", 0);
}

qint64 VM::memoryStaticMax() const
{
    return longProperty("memory_static_max", 0);
}

qint64 VM::memoryDynamicMax() const
{
    return longProperty("memory_dynamic_max", 0);
}

qint64 VM::memoryDynamicMin() const
{
    return longProperty("memory_dynamic_min", 0);
}

qint64 VM::memoryStaticMin() const
{
    return longProperty("memory_static_min", 0);
}

int VM::vcpusMax() const
{
    return intProperty("VCPUs_max", 0);
}

int VM::vcpusAtStartup() const
{
    return intProperty("VCPUs_at_startup", 0);
}

bool VM::isHvm() const
{
    return !stringProperty("HVM_boot_policy").isEmpty();
}

bool VM::isWindows() const
{
    QString guestMetricsRef = stringProperty("guest_metrics");
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        XenCache* cache = connection() ? connection()->getCache() : nullptr;
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

    if (this->isHvm())
    {
        QVariantMap platformMap = this->platform();
        QString viridian = platformMap.value("viridian").toString();
        if (viridian == "true" || viridian == "1")
            return true;
    }

    return false;
}

bool VM::supportsVcpuHotplug() const
{
    // C#: !IsWindows() && !Helpers.FeatureForbidden(Connection, Host.RestrictVcpuHotplug)
    // Feature restrictions are not implemented yet; follow the Windows check for now.
    return !this->isWindows();
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

int VM::maxVcpusAllowed() const
{
    XenCache* cache = connection() ? connection()->getCache() : nullptr;
    QVariantMap vmData = this->data();

    qint64 value = 0;
    if (tryGetMatchingTemplateRestriction(cache, vmData, "vcpus-max", "max", value))
        return static_cast<int>(value);

    QList<qint64> values = getRestrictionValuesAcrossTemplates(cache, "vcpus-max", "max");
    values.append(DEFAULT_NUM_VCPUS_ALLOWED);
    return static_cast<int>(*std::max_element(values.begin(), values.end()));
}

int VM::minVcpus() const
{
    XenCache* cache = connection() ? connection()->getCache() : nullptr;
    QVariantMap vmData = this->data();

    qint64 value = 0;
    if (tryGetMatchingTemplateRestriction(cache, vmData, "vcpus-min", "min", value))
        return static_cast<int>(value);

    QList<qint64> values = getRestrictionValuesAcrossTemplates(cache, "vcpus-min", "min");
    values.append(1);
    return static_cast<int>(*std::min_element(values.begin(), values.end()));
}

int VM::getVcpuWeight() const
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

long VM::getCoresPerSocket() const
{
    QVariantMap platformMap = this->platform();
    if (platformMap.contains("cores-per-socket"))
    {
        bool ok = false;
        long cores = platformMap.value("cores-per-socket").toString().toLongLong(&ok);
        return ok ? cores : DEFAULT_CORES_PER_SOCKET;
    }

    return DEFAULT_CORES_PER_SOCKET;
}

long VM::maxCoresPerSocket() const
{
    XenCache* cache = connection() ? connection()->getCache() : nullptr;
    if (!cache)
        return 0;

    QString home = this->homeRef();
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

QString VM::validVcpuConfiguration(long noOfVcpus, long coresPerSocket)
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

QString VM::getTopology(long sockets, long cores)
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

QVariantMap VM::otherConfig() const
{
    return property("other_config").toMap();
}

QVariantMap VM::platform() const
{
    return property("platform").toMap();
}

QStringList VM::tags() const
{
    return stringListProperty("tags");
}

QStringList VM::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap VM::currentOperations() const
{
    return property("current_operations").toMap();
}

QString VM::homeRef() const
{
    // Return affinity host if set, otherwise resident host
    QString affinity = affinityRef();
    if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
        return affinity;

    return residentOnRef();
}
