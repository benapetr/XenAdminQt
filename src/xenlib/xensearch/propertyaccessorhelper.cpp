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
#include "../metricupdater.h"

namespace XenSearch
{

// VM CPU usage
QString PropertyAccessorHelper::vmCpuUsageString(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    // Sum CPU usage across all vCPUs
    QString vmUuid = vm->GetUUID();
    int numVCPUs = vm->VCPUsAtStartup();
    if (numVCPUs == 0)
        return "-";
    
    double sum = 0.0;
    for (int i = 0; i < numVCPUs; i++)
    {
        QString metricName = QString("cpu%1").arg(i);
        sum += metricUpdater->getValue("vm", vmUuid, metricName);
    }
    
    if (numVCPUs == 1)
        return QString("%1% of 1 CPU").arg(QString::number(sum * 100, 'f', 0));
    
    return QString("%1% of %2 CPUs").arg(QString::number((sum * 100) / numVCPUs, 'f', 0)).arg(numVCPUs);
}

int PropertyAccessorHelper::vmCpuUsageRank(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return 0;
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0;
    
    QString vmUuid = vm->GetUUID();
    int numVCPUs = vm->VCPUsAtStartup();
    if (numVCPUs == 0)
        return 0;
    
    double sum = 0.0;
    for (int i = 0; i < numVCPUs; i++)
    {
        QString metricName = QString("cpu%1").arg(i);
        sum += metricUpdater->getValue("vm", vmUuid, metricName);
    }
    
    return static_cast<int>(sum * 100);
}

// VM Memory usage
QString PropertyAccessorHelper::vmMemoryUsageString(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    QString vmUuid = vm->GetUUID();
    double memoryUsed = metricUpdater->getValue("vm", vmUuid, "memory");
    double memoryFree = metricUpdater->getValue("vm", vmUuid, "memory_internal_free");
    
    if (memoryUsed == 0.0 && memoryFree == 0.0)
        return "-";
    
    double totalMemory = memoryUsed + memoryFree;
    
    // Format as human-readable
    auto formatBytes = [](double bytes) -> QString {
        if (bytes < 1024)
            return QString("%1 B").arg(QString::number(bytes, 'f', 0));
        if (bytes < 1024 * 1024)
            return QString("%1 KB").arg(QString::number(bytes / 1024, 'f', 1));
        if (bytes < 1024 * 1024 * 1024)
            return QString("%1 MB").arg(QString::number(bytes / (1024 * 1024), 'f', 1));
        return QString("%1 GB").arg(QString::number(bytes / (1024 * 1024 * 1024), 'f', 1));
    };
    
    return QString("%1 of %2").arg(formatBytes(memoryUsed)).arg(formatBytes(totalMemory));
}

int PropertyAccessorHelper::vmMemoryUsageRank(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return 0;
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0;
    
    QString vmUuid = vm->GetUUID();
    double memoryUsed = metricUpdater->getValue("vm", vmUuid, "memory");
    double memoryFree = metricUpdater->getValue("vm", vmUuid, "memory_internal_free");
    
    if (memoryUsed == 0.0 && memoryFree == 0.0)
        return 0;
    
    double totalMemory = memoryUsed + memoryFree;
    return static_cast<int>((memoryUsed / totalMemory) * 100);
}

double PropertyAccessorHelper::vmMemoryUsageValue(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return 0.0;
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0.0;
    
    QString vmUuid = vm->GetUUID();
    return metricUpdater->getValue("vm", vmUuid, "memory");
}

// VM Network usage
QString PropertyAccessorHelper::vmNetworkUsageString(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    XenCache* cache = vm->GetConnection()->GetCache();
    if (!cache)
        return "-";
    
    QString vmUuid = vm->GetUUID();
    QStringList vifRefs = vm->GetVIFRefs();
    
    double sum = 0.0;
    double max = 0.0;
    int vifCount = 0;
    
    for (const QString& vifRef : vifRefs)
    {
        QVariantMap vifData = cache->ResolveObjectData(XenObjectType::VIF, vifRef);
        QString device = vifData.value("device").toString();
        if (device.isEmpty())
            continue;
        
        double rx = metricUpdater->getValue("vm", vmUuid, QString("vif_%1_rx").arg(device));
        double tx = metricUpdater->getValue("vm", vmUuid, QString("vif_%1_tx").arg(device));
        double total = rx + tx;
        
        sum += total;
        if (total > max)
            max = total;
        vifCount++;
    }
    
    if (vifCount == 0 || sum == 0.0)
        return "-";
    
    double avg = sum / vifCount;
    
    auto formatBytesPerSec = [](double bytesPerSec) -> QString {
        if (bytesPerSec < 1024)
            return QString("%1 B/s").arg(QString::number(bytesPerSec, 'f', 1));
        if (bytesPerSec < 1024 * 1024)
            return QString("%1 KB/s").arg(QString::number(bytesPerSec / 1024, 'f', 1));
        return QString("%1 MB/s").arg(QString::number(bytesPerSec / (1024 * 1024), 'f', 1));
    };
    
    return QString("Avg %1, Max %2").arg(formatBytesPerSec(avg)).arg(formatBytesPerSec(max));
}

// VM Disk usage
QString PropertyAccessorHelper::vmDiskUsageString(VM* vm)
{
    if (!vm || !vm->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = vm->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    XenCache* cache = vm->GetConnection()->GetCache();
    if (!cache)
        return "-";
    
    QString vmUuid = vm->GetUUID();
    QStringList vbdRefs = vm->GetVBDRefs();
    
    double sum = 0.0;
    double max = 0.0;
    int vbdCount = 0;
    
    for (const QString& vbdRef : vbdRefs)
    {
        QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);
        QString device = vbdData.value("device").toString();
        if (device.isEmpty())
            continue;
        
        double read = metricUpdater->getValue("vm", vmUuid, QString("vbd_%1_read").arg(device));
        double write = metricUpdater->getValue("vm", vmUuid, QString("vbd_%1_write").arg(device));
        double total = read + write;
        
        sum += total;
        if (total > max)
            max = total;
        vbdCount++;
    }
    
    if (vbdCount == 0 || sum == 0.0)
        return "-";
    
    double avg = sum / vbdCount;
    
    auto formatBytesPerSec = [](double bytesPerSec) -> QString {
        if (bytesPerSec < 1024)
            return QString("%1 B/s").arg(QString::number(bytesPerSec, 'f', 1));
        if (bytesPerSec < 1024 * 1024)
            return QString("%1 KB/s").arg(QString::number(bytesPerSec / 1024, 'f', 1));
        return QString("%1 MB/s").arg(QString::number(bytesPerSec / (1024 * 1024), 'f', 1));
    };
    
    return QString("Avg %1, Max %2").arg(formatBytesPerSec(avg)).arg(formatBytesPerSec(max));
}

// Host CPU usage
QString PropertyAccessorHelper::hostCpuUsageString(Host* host)
{
    if (!host || !host->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    QString hostUuid = host->GetUUID();
    int numCPUs = host->GetCPUCount();
    if (numCPUs == 0)
        return "-";
    
    double sum = 0.0;
    for (int i = 0; i < numCPUs; i++)
    {
        QString metricName = QString("cpu%1").arg(i);
        sum += metricUpdater->getValue("host", hostUuid, metricName);
    }
    
    if (numCPUs == 1)
        return QString("%1%").arg(QString::number(sum * 100, 'f', 0));
    
    return QString("%1% (avg)").arg(QString::number((sum * 100) / numCPUs, 'f', 0));
}

int PropertyAccessorHelper::hostCpuUsageRank(Host* host)
{
    if (!host || !host->GetConnection())
        return 0;
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0;
    
    QString hostUuid = host->GetUUID();
    int numCPUs = host->GetCPUCount();
    if (numCPUs == 0)
        return 0;
    
    double sum = 0.0;
    for (int i = 0; i < numCPUs; i++)
    {
        QString metricName = QString("cpu%1").arg(i);
        sum += metricUpdater->getValue("host", hostUuid, metricName);
    }
    
    return static_cast<int>(sum * 100);
}

// Host Memory usage
QString PropertyAccessorHelper::hostMemoryUsageString(Host* host)
{
    if (!host || !host->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    QString hostUuid = host->GetUUID();
    double memoryTotal = metricUpdater->getValue("host", hostUuid, "memory_total_kib") * 1024;
    double memoryFree = metricUpdater->getValue("host", hostUuid, "memory_free_kib") * 1024;
    
    if (memoryTotal == 0.0)
        return "-";
    
    double memoryUsed = memoryTotal - memoryFree;
    
    auto formatBytes = [](double bytes) -> QString {
        if (bytes < 1024)
            return QString("%1 B").arg(QString::number(bytes, 'f', 0));
        if (bytes < 1024 * 1024)
            return QString("%1 KB").arg(QString::number(bytes / 1024, 'f', 1));
        if (bytes < 1024 * 1024 * 1024)
            return QString("%1 MB").arg(QString::number(bytes / (1024 * 1024), 'f', 1));
        return QString("%1 GB").arg(QString::number(bytes / (1024 * 1024 * 1024), 'f', 1));
    };
    
    return QString("%1 of %2").arg(formatBytes(memoryUsed)).arg(formatBytes(memoryTotal));
}

int PropertyAccessorHelper::hostMemoryUsageRank(Host* host)
{
    if (!host || !host->GetConnection())
        return 0;
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0;
    
    QString hostUuid = host->GetUUID();
    double memoryTotal = metricUpdater->getValue("host", hostUuid, "memory_total_kib") * 1024;
    double memoryFree = metricUpdater->getValue("host", hostUuid, "memory_free_kib") * 1024;
    
    if (memoryTotal == 0.0)
        return 0;
    
    double memoryUsed = memoryTotal - memoryFree;
    return static_cast<int>((memoryUsed / memoryTotal) * 100);
}

double PropertyAccessorHelper::hostMemoryUsageValue(Host* host)
{
    if (!host || !host->GetConnection())
        return 0.0;
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return 0.0;
    
    QString hostUuid = host->GetUUID();
    double memoryTotal = metricUpdater->getValue("host", hostUuid, "memory_total_kib") * 1024;
    double memoryFree = metricUpdater->getValue("host", hostUuid, "memory_free_kib") * 1024;
    
    return memoryTotal - memoryFree;
}

// Host Network usage
QString PropertyAccessorHelper::hostNetworkUsageString(Host* host)
{
    if (!host || !host->GetConnection())
        return "-";
    
    MetricUpdater* metricUpdater = host->GetConnection()->GetMetricUpdater();
    if (!metricUpdater)
        return "-";
    
    XenCache* cache = host->GetConnection()->GetCache();
    if (!cache)
        return "-";
    
    QString hostUuid = host->GetUUID();
    QStringList pifRefs = host->GetPIFRefs();
    
    double sum = 0.0;
    double max = 0.0;
    int pifCount = 0;
    
    for (const QString& pifRef : pifRefs)
    {
        QVariantMap pifData = cache->ResolveObjectData(XenObjectType::PIF, pifRef);
        bool physical = pifData.value("physical").toBool();
        if (!physical)
            continue;
        
        QString device = pifData.value("device").toString();
        if (device.isEmpty())
            continue;
        
        double rx = metricUpdater->getValue("host", hostUuid, QString("pif_%1_rx").arg(device));
        double tx = metricUpdater->getValue("host", hostUuid, QString("pif_%1_tx").arg(device));
        double total = rx + tx;
        
        sum += total;
        if (total > max)
            max = total;
        pifCount++;
    }
    
    if (pifCount == 0 || sum == 0.0)
        return "-";
    
    double avg = sum / pifCount;
    
    auto formatBytesPerSec = [](double bytesPerSec) -> QString {
        if (bytesPerSec < 1024)
            return QString("%1 B/s").arg(QString::number(bytesPerSec, 'f', 1));
        if (bytesPerSec < 1024 * 1024)
            return QString("%1 KB/s").arg(QString::number(bytesPerSec / 1024, 'f', 1));
        return QString("%1 MB/s").arg(QString::number(bytesPerSec / (1024 * 1024), 'f', 1));
    };
    
    return QString("Avg %1, Max %2").arg(formatBytesPerSec(avg)).arg(formatBytesPerSec(max));
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
    QStringList poolRefs = cache->GetAllRefs(XenObjectType::Pool);
    if (poolRefs.isEmpty())
        return QString();
    
    QVariantMap poolData = cache->ResolveObjectData(XenObjectType::Pool, poolRefs.first());
    QStringList haStatefiles = poolData.value("ha_statefiles").toStringList();
    
    if (haStatefiles.isEmpty())
        return QString();
    
    // Check if this SR contains HA statefile VDI
    QStringList vdiRefs = sr->GetVDIRefs();
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
