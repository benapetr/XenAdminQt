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

#include "propertyaccessorhelper.h"
#include "../xen/vm.h"
#include "../xen/host.h"
#include "../xen/vdi.h"
#include "../xen/pool.h"
#include "../xen/sr.h"
#include "../xencache.h"
#include "../xen/network/connection.h"

namespace XenSearch
{

// VM CPU usage
QString PropertyAccessorHelper::vmCpuUsageString(VM* vm)
{
    // TODO: Implement with MetricUpdater once metrics system is ported
    // For now return placeholder
    Q_UNUSED(vm);
    return "-";
}

int PropertyAccessorHelper::vmCpuUsageRank(VM* vm)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(vm);
    return 0;
}

// VM Memory usage
QString PropertyAccessorHelper::vmMemoryUsageString(VM* vm)
{
    // TODO: Implement with MetricUpdater
    // Should return format: "1.2 GB of 2.0 GB"
    Q_UNUSED(vm);
    return "-";
}

int PropertyAccessorHelper::vmMemoryUsageRank(VM* vm)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(vm);
    return 0;
}

double PropertyAccessorHelper::vmMemoryUsageValue(VM* vm)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(vm);
    return 0.0;
}

// VM Network usage
QString PropertyAccessorHelper::vmNetworkUsageString(VM* vm)
{
    // TODO: Implement with MetricUpdater
    // Should return format: "Avg 12.3 KB/s, Max 45.6 KB/s"
    Q_UNUSED(vm);
    return "-";
}

// VM Disk usage
QString PropertyAccessorHelper::vmDiskUsageString(VM* vm)
{
    // TODO: Implement with MetricUpdater
    // Should return format: "Avg 12.3 KB/s, Max 45.6 KB/s"
    Q_UNUSED(vm);
    return "-";
}

// Host CPU usage
QString PropertyAccessorHelper::hostCpuUsageString(Host* host)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(host);
    return "-";
}

int PropertyAccessorHelper::hostCpuUsageRank(Host* host)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(host);
    return 0;
}

// Host Memory usage
QString PropertyAccessorHelper::hostMemoryUsageString(Host* host)
{
    // TODO: Implement with MetricUpdater
    // Should return format: "12.3 GB of 64.0 GB"
    Q_UNUSED(host);
    return "-";
}

int PropertyAccessorHelper::hostMemoryUsageRank(Host* host)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(host);
    return 0;
}

double PropertyAccessorHelper::hostMemoryUsageValue(Host* host)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(host);
    return 0.0;
}

// Host Network usage
QString PropertyAccessorHelper::hostNetworkUsageString(Host* host)
{
    // TODO: Implement with MetricUpdater
    Q_UNUSED(host);
    return "-";
}

// VDI Memory usage
QString PropertyAccessorHelper::vdiMemoryUsageString(VDI* vdi)
{
    if (!vdi)
        return "-";
    
    qint64 virtualSize = vdi->VirtualSize();
    if (virtualSize == 0)
        return "-";
    
    // Format size as human-readable
    if (virtualSize < 1024)
        return QString::number(virtualSize) + " B";
    else if (virtualSize < 1024 * 1024)
        return QString::number(virtualSize / 1024.0, 'f', 1) + " KB";
    else if (virtualSize < 1024 * 1024 * 1024)
        return QString::number(virtualSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    else
        return QString::number(virtualSize / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

// HA Status
QString PropertyAccessorHelper::GetPoolHAStatus(Pool* pool)
{
    if (!pool)
        return QString();
    
    bool haEnabled = pool->HAEnabled();
    if (!haEnabled)
        return "Disabled";
    
    qint64 planExistsFor = pool->HAPlanExistsFor();
    if (planExistsFor == 1)
        return QString("Tolerates %1 host failure").arg(planExistsFor);
    else
        return QString("Tolerates %1 host failures").arg(planExistsFor);
}

QString PropertyAccessorHelper::GetSRHAStatus(SR* sr)
{
    if (!sr || !sr->GetConnection())
        return QString();
    
    XenCache* cache = sr->GetConnection()->GetCache();
    if (!cache)
        return QString();
    
    // Get pool
    QStringList poolRefs = cache->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return QString();
    
    QVariantMap poolData = cache->ResolveObjectData("pool", poolRefs.first());
    QStringList haStatefiles = poolData.value("ha_statefiles").toStringList();
    
    if (haStatefiles.isEmpty())
        return QString();
    
    // Check if this SR contains HA statefile VDI
    QStringList vdiRefs = sr->VDIRefs();
    for (const QString& vdiRef : vdiRefs)
    {
        if (haStatefiles.contains(vdiRef))
            return "HA Heartbeat SR";
    }
    
    return QString();
}

QString PropertyAccessorHelper::GetVMHAStatus(VM* vm)
{
    if (!vm || !vm->IsRealVM())
        return "-";
    
    QString priority = vm->HARestartPriority();
    
    // Map priorities to display strings
    if (priority == "restart")
        return "Restart";
    else if (priority == "best-effort" || priority == "")
        return "Best-effort";
    else if (priority.isEmpty())
        return "Do not restart";
    else
        return priority;  // Return as-is for unknown priorities
}

} // namespace XenSearch
