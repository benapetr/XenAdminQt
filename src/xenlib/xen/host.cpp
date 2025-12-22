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
#include "connection.h"
#include "../xenlib.h"

Host::Host(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
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
    // TODO: Query pool and check if pool.master == this->opaqueRef()
    // For now, check other_config for pool_master flag
    QVariantMap config = otherConfig();
    return config.value("pool_master", false).toBool();
}

QString Host::poolRef() const
{
    // In XenAPI, hosts don't directly store pool reference
    // We need to query the cache for the pool that contains this host
    // TODO: Implement proper pool lookup
    return QString();
}
