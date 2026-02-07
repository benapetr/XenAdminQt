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

#include "graphhelpers.h"
#include "palette.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vif.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/sr.h"
#include <algorithm>
#include <QRegularExpression>

namespace
{
    QString readOrWriteLabel(const QString& op)
    {
        return op.compare(QStringLiteral("rx"), Qt::CaseInsensitive) == 0
                   || op.compare(QStringLiteral("read"), Qt::CaseInsensitive) == 0
                   ? QObject::tr("Receive")
                   : QObject::tr("Send");
    }

    QSharedPointer<VIF> findVifByDevice(XenObject* xenObject, const QString& device)
    {
        if (!xenObject)
            return QSharedPointer<VIF>();

        if (xenObject->GetObjectType() == XenObjectType::VM)
        {
            auto* vm = dynamic_cast<VM*>(xenObject);
            if (!vm)
                return QSharedPointer<VIF>();

            const QList<QSharedPointer<VIF>> vifs = vm->GetVIFs();
            for (const QSharedPointer<VIF>& vif : vifs)
            {
                if (vif && vif->GetDevice() == device)
                    return vif;
            }
        }

        return QSharedPointer<VIF>();
    }

    QSharedPointer<PIF> findPifByDevice(XenObject* xenObject, const QString& device)
    {
        if (!xenObject)
            return QSharedPointer<PIF>();

        if (xenObject->GetObjectType() != XenObjectType::Host)
            return QSharedPointer<PIF>();

        auto* host = dynamic_cast<Host*>(xenObject);
        if (!host)
            return QSharedPointer<PIF>();

        const QList<QSharedPointer<PIF>> pifs = host->GetPIFs();
        for (const QSharedPointer<PIF>& pif : pifs)
        {
            if (pif && pif->GetDevice() == device)
                return pif;
        }

        return QSharedPointer<PIF>();
    }

    QSharedPointer<VBD> findVbdByDevice(XenObject* xenObject, const QString& device)
    {
        if (!xenObject)
            return QSharedPointer<VBD>();

        if (xenObject->GetObjectType() != XenObjectType::VM)
            return QSharedPointer<VBD>();

        auto* vm = dynamic_cast<VM*>(xenObject);
        if (!vm)
            return QSharedPointer<VBD>();

        const QList<QSharedPointer<VBD>> vbds = vm->GetVBDs();
        for (const QSharedPointer<VBD>& vbd : vbds)
        {
            if (!vbd)
                continue;

            if (vbd->GetDevice() == device || vbd->GetUserdevice() == device)
                return vbd;
        }

        return QSharedPointer<VBD>();
    }

    QSharedPointer<SR> findSrByShortUuid(XenObject* xenObject, const QString& shortUuid)
    {
        if (!xenObject || shortUuid.isEmpty())
            return QSharedPointer<SR>();

        XenCache* cache = xenObject->GetCache();
        if (!cache)
            return QSharedPointer<SR>();

        const QList<QSharedPointer<SR>> srs = cache->GetAll<SR>();
        for (const QSharedPointer<SR>& sr : srs)
        {
            if (sr && sr->GetUUID().startsWith(shortUuid, Qt::CaseInsensitive))
                return sr;
        }

        return QSharedPointer<SR>();
    }
}

namespace CustomDataGraph
{
    bool DesignedGraph::IsSame(const DesignedGraph& other) const
    {
        if (this->DisplayName != other.DisplayName)
            return false;

        if (this->DataSourceItems.size() != other.DataSourceItems.size())
            return false;

        for (int i = 0; i < this->DataSourceItems.size(); ++i)
        {
            if (!(this->DataSourceItems.at(i) == other.DataSourceItems.at(i)))
                return false;
        }

        return true;
    }

    bool DesignedGraph::operator==(const DesignedGraph& other) const
    {
        return this->IsSame(other);
    }

    QString DataSourceItemList::GetFriendlyDataSourceName(const QString& name, XenObject* xenObject)
    {
        if (name.isEmpty())
            return name;

        static const QRegularExpression cpuRx(QStringLiteral("^cpu(\\d+)$"));
        static const QRegularExpression vcpuRx(QStringLiteral("^vcpu(\\d+)$"));
        static const QRegularExpression vifRx(QStringLiteral("^vif_(.+)_(rx|tx)$"));
        static const QRegularExpression pifRx(QStringLiteral("^pif_(.+)_(rx|tx)$"));
        static const QRegularExpression vbdRwRx(QStringLiteral("^vbd_(.+)_(read|write)$"));
        static const QRegularExpression vbdIopsRx(QStringLiteral("^iops_(read|write|total)_(.+)$"));
        static const QRegularExpression vbdThroughputRx(QStringLiteral("^io_throughput_(read|write|total)_(.+)$"));
        static const QRegularExpression srRwRx(QStringLiteral("^(read|write|read_latency|write_latency)_([a-f0-9]{8})$"));
        static const QRegularExpression srIoRx(QStringLiteral("^(io_throughput|iops)_(read|write|total)_([a-f0-9]{8})$"));

        QRegularExpressionMatch match = cpuRx.match(name);
        if (match.hasMatch())
            return QObject::tr("CPU %1").arg(match.captured(1));

        match = vcpuRx.match(name);
        if (match.hasMatch())
            return QObject::tr("CPU %1").arg(match.captured(1));

        if (name == QStringLiteral("memory_free_kib") || name == QStringLiteral("memory_internal_free")
            || name == QStringLiteral("memory_used_kib") || name == QStringLiteral("memory_internal_used"))
            return QObject::tr("Used Memory");

        if (name == QStringLiteral("memory_total_kib") || name == QStringLiteral("memory"))
            return QObject::tr("Total Memory");

        match = vifRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<VIF> vif = findVifByDevice(xenObject, match.captured(1));
            const QSharedPointer<Network> network = vif ? vif->GetNetwork() : QSharedPointer<Network>();
            const QString prefix = network ? network->GetName() : QObject::tr("VIF %1").arg(match.captured(1));
            return QObject::tr("%1 %2").arg(prefix, readOrWriteLabel(match.captured(2)));
        }

        match = pifRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<PIF> pif = findPifByDevice(xenObject, match.captured(1));
            const QString prefix = pif ? pif->GetName() : match.captured(1);
            return QObject::tr("%1 %2").arg(prefix, readOrWriteLabel(match.captured(2)));
        }

        match = vbdRwRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<VBD> vbd = findVbdByDevice(xenObject, match.captured(1));
            const QString device = vbd ? vbd->GetUserdevice() : match.captured(1);
            const QString op = match.captured(2).compare(QStringLiteral("read"), Qt::CaseInsensitive) == 0
                                   ? QObject::tr("Read")
                                   : QObject::tr("Write");
            return QObject::tr("VBD %1 %2").arg(device, op);
        }

        match = vbdIopsRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<VBD> vbd = findVbdByDevice(xenObject, match.captured(2));
            const QString device = vbd ? vbd->GetUserdevice() : match.captured(2);
            const QString op = match.captured(1).toUpper();
            return QObject::tr("VBD %1 IOPS %2").arg(device, op);
        }

        match = vbdThroughputRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<VBD> vbd = findVbdByDevice(xenObject, match.captured(2));
            const QString device = vbd ? vbd->GetUserdevice() : match.captured(2);
            const QString op = match.captured(1).toUpper();
            return QObject::tr("VBD %1 Throughput %2").arg(device, op);
        }

        match = srRwRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<SR> sr = findSrByShortUuid(xenObject, match.captured(2));
            const QString srName = sr ? sr->GetName() : match.captured(2);
            return QObject::tr("%1 %2").arg(match.captured(1), srName);
        }

        match = srIoRx.match(name);
        if (match.hasMatch())
        {
            const QSharedPointer<SR> sr = findSrByShortUuid(xenObject, match.captured(3));
            const QString srName = sr ? sr->GetName() : match.captured(3);
            return QObject::tr("SR %1 %2 %3").arg(srName, match.captured(1), match.captured(2));
        }

        return name;
    }

    QList<DataSourceItem> DataSourceItemList::BuildList(XenObject* xenObject, const QList<QVariantMap>& dataSources)
    {
        QList<DataSourceItem> items;
        if (!xenObject)
            return items;

        static const QRegularExpression oldSrRwRegex(QStringLiteral("^io_throughput_(read|write)_([a-f0-9]{8})$"));
        static const QRegularExpression srRwRegex(QStringLiteral("^(read|write)_([a-f0-9]{8})$"));
        bool hasNewSrRw = false;

        for (const QVariantMap& source : dataSources)
        {
            const QString nameLabel = source.value(QStringLiteral("name_label")).toString();
            if (nameLabel == QStringLiteral("memory_total_kib") || nameLabel == QStringLiteral("memory"))
                continue;

            if (srRwRegex.match(nameLabel).hasMatch())
                hasNewSrRw = true;

            DataSourceDescriptor descriptor;
            descriptor.NameLabel = nameLabel;
            descriptor.Standard = source.value(QStringLiteral("standard")).toBool();
            descriptor.Enabled = source.value(QStringLiteral("enabled"), true).toBool();
            descriptor.Units = source.value(QStringLiteral("units")).toString();

            const QString id = Palette::GetUuid(nameLabel, xenObject);
            const QString friendlyName = DataSourceItemList::GetFriendlyDataSourceName(nameLabel, xenObject);
            DataSourceItem item(descriptor, friendlyName, Palette::GetColour(id), id);
            item.Hidden = descriptor.Units.isEmpty() || descriptor.Units == QStringLiteral("unknown");
            items.append(item);
        }

        if (hasNewSrRw)
        {
            items.erase(std::remove_if(items.begin(), items.end(), [](const DataSourceItem& item)
            {
                return oldSrRwRegex.match(item.DataSource.NameLabel).hasMatch();
            }), items.end());
        }

        return items;
    }
}
