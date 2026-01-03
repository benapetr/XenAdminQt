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

#include "querypanel.h"
#include "treewidgetgroupacceptor.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/sort.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/query.h"
#include "xenlib/xensearch/queryfilter.h"
#include "xenlib/xensearch/queryscope.h"
#include "xen/network/connectionsmanager.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include "xen/xenobject.h"
#include "iconmanager.h"
#include "../../widgets/progressbardelegate.h"
#include "xenlib/metricupdater.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDateTime>
#include <QClipboard>
#include <QGuiApplication>
#include <cmath>

// Static members
const QStringList QueryPanel::DEFAULT_COLUMNS = QStringList()
    << "name" << "cpu" << "memory" << "disks" << "network" << "ip" << "ha" << "uptime";

QTimer* QueryPanel::metricsUpdateTimer_ = nullptr;
QList<XenObject*> QueryPanel::metricsObjects_;

QueryPanel::QueryPanel(QWidget* parent)
    : QTreeWidget(parent)
    , search_(nullptr)
    , updatePending_(false)
{
    // Configure tree widget
    this->setColumnCount(DEFAULT_COLUMNS.size());
    this->setRootIsDecorated(true);
    this->setAlternatingRowColors(true);
    this->setSortingEnabled(true);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->setContextMenuPolicy(Qt::DefaultContextMenu);
    this->setItemDelegateForColumn(DEFAULT_COLUMNS.indexOf("cpu"), new ProgressBarDelegate(this));
    this->setItemDelegateForColumn(DEFAULT_COLUMNS.indexOf("memory"), new ProgressBarDelegate(this));
    
    // Setup header
    this->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->header(), &QHeaderView::customContextMenuRequested,
            this, &QueryPanel::onHeaderContextMenu);
    connect(this->header(), &QHeaderView::sortIndicatorChanged,
            this, &QueryPanel::onSortIndicatorChanged);
    
    // Setup columns
    this->setupColumns();
    
    // Initialize column visibility map
    for (const QString& col : DEFAULT_COLUMNS)
    {
        this->columns_[col] = true;
    }
    
    // Setup update throttle timer (100ms delay to batch updates)
    this->updateThrottleTimer_ = new QTimer(this);
    this->updateThrottleTimer_->setSingleShot(true);
    this->updateThrottleTimer_->setInterval(100);
    connect(this->updateThrottleTimer_, &QTimer::timeout, this, &QueryPanel::buildListInternal);
    
    // Setup static metrics timer if not already created
    if (!metricsUpdateTimer_)
    {
        metricsUpdateTimer_ = new QTimer();
        metricsUpdateTimer_->setInterval(2000); // 2 second updates
    }
    connect(metricsUpdateTimer_, &QTimer::timeout,
            this, &QueryPanel::onMetricsUpdateTimerTimeout);
}

QueryPanel::~QueryPanel()
{
    this->search_ = nullptr;
}

void QueryPanel::setupColumns()
{
    QStringList headers;
    for (const QString& col : DEFAULT_COLUMNS)
    {
        headers << this->getI18nColumnName(col);
    }
    this->setHeaderLabels(headers);
    
    // Set default column widths
    for (int i = 0; i < DEFAULT_COLUMNS.size(); ++i)
    {
        int width = this->getDefaultColumnWidth(DEFAULT_COLUMNS[i]);
        this->header()->resizeSection(i, width);
    }
    
    // Name column is immovable
    this->header()->setSectionsMovable(true);
    this->header()->setFirstSectionMovable(false);
}

void QueryPanel::SetSearch(Search* search)
{
    this->search_ = search;
    
    if (this->search_)
    {
        // Apply column configuration from search
        QList<QPair<QString, int>> columns = this->search_->GetColumns();
        if (!columns.isEmpty())
        {
            // Hide all columns first
            for (int i = 0; i < this->columnCount(); ++i)
            {
                this->setColumnHidden(i, true);
            }
            
            // Show and resize columns from search config
            for (const auto& columnConfig : columns)
            {
                QString columnName = columnConfig.first;
                int width = columnConfig.second;
                
                int columnIndex = DEFAULT_COLUMNS.indexOf(columnName);
                if (columnIndex >= 0)
                {
                    this->setColumnHidden(columnIndex, false);
                    if (width > 0)
                    {
                        this->setColumnWidth(columnIndex, width);
                    }
                }
            }
        }
        
        // Apply sorting from search
        QList<Sort> sorting = this->search_->GetSorting();
        if (!sorting.isEmpty())
        {
            // Apply first sort (QTreeWidget supports single column sort in UI)
            // C# supports multi-column sorting but that requires custom comparator
            const Sort& firstSort = sorting.first();
            int columnIndex = DEFAULT_COLUMNS.indexOf(firstSort.GetColumn());
            if (columnIndex >= 0)
            {
                Qt::SortOrder order = firstSort.IsAscending() ? Qt::AscendingOrder : Qt::DescendingOrder;
                this->sortByColumn(columnIndex, order);
            }
        }
        
        this->BuildList();
    }
}

void QueryPanel::BuildList()
{
    if (!this->search_)
        return;
    
    // Throttle updates - don't rebuild immediately if already pending
    if (this->updatePending_)
        return;
    
    this->updatePending_ = true;
    this->updateThrottleTimer_->start();
}

void QueryPanel::buildListInternal()
{
    this->updatePending_ = false;
    
    if (!this->search_)
        return;
    
    this->saveRowStates();
    this->clear();
    metricsObjects_.clear();
    
    // Get XenLib instance from any connected connection
    // C# uses search.PopulateAdapters() which internally queries all connections
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QList<XenConnection*> connections = connMgr->getConnectedConnections();
    
    if (connections.isEmpty())
    {
        this->addNoResultsRow();
        this->restoreRowStates();
        return;
    }

    // Use the stored xenLib_ member instead of getting from connection
    /*if (!this->xenLib_)
    {
        this->addNoResultsRow();
        this->restoreRowStates();
        return;
    }*/

    // Use Search.PopulateAdapters() to filter, group, and populate the tree
    // This delegates to the Search object which applies QueryScope, QueryFilter, and Grouping.
    // C# runs global searches across all connected connections (no single connection required).
    TreeWidgetGroupAcceptor adapter(this, this);
    QList<IAcceptGroups*> adapters;
    adapters.append(&adapter);

    bool addedAny = false;
    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;
        addedAny |= this->search_->PopulateAdapters(connection, adapters);
    }

    if (!addedAny)
    {
        this->addNoResultsRow();
    }
    
    this->restoreRowStates();
}

QTreeWidgetItem* QueryPanel::CreateRow(Grouping* grouping, const QString& objectType, const QVariantMap& objectData, XenConnection* conn)
{
    Q_UNUSED(grouping);
    
    if (objectType.isEmpty() || objectData.isEmpty())
        return nullptr;

    QString ref = objectData.value("opaque_ref").toString();
    if (ref.isEmpty())
        ref = objectData.value("_ref").toString();
    if (ref.isEmpty())
        ref = objectData.value("ref").toString();
    if (ref.isEmpty())
        ref = objectData.value("opaqueRef").toString();
    if (ref.isEmpty())
        return nullptr;

    // Get XenObject wrapper from cache
    QSharedPointer<XenObject> xenObj = conn->GetCache()->ResolveObject(objectType, ref);
    if (!xenObj || !xenObj->IsValid())
        return nullptr;

    // Create tree widget item
    QTreeWidgetItem* item = new QTreeWidgetItem();
    
    // Store XenObject pointer in UserRole for later access
    item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(xenObj.data()));
    item->setData(0, Qt::UserRole + 1, objectType); // Store object type
    
    // Populate all columns
    this->populateRow(item, xenObj.data());
    
    // Track for metrics updates
    metricsObjects_.append(xenObj.data());
    
    return item;
}

void QueryPanel::addNoResultsRow()
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, tr("No results"));
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    this->addTopLevelItem(item);
}

QTreeWidgetItem* QueryPanel::createRow(Grouping* grouping, XenObject* xenObject, int indent)
{
    Q_UNUSED(grouping);
    Q_UNUSED(indent);
    
    if (!xenObject)
        return nullptr;
    
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(xenObject));
    
    this->populateRow(item, xenObject);
    
    return item;
}

void QueryPanel::populateRow(QTreeWidgetItem* item, XenObject* xenObject)
{
    if (!item || !xenObject)
        return;
    
    int col = 0;
    for (const QString& columnName : DEFAULT_COLUMNS)
    {
        if (!this->columns_.value(columnName, false))
        {
            col++;
            continue;
        }
        
        QString value;
        
        if (columnName == "name")
        {
            value = xenObject->GetName();
            QIcon icon = IconManager::instance().getIconForObject(xenObject);
            if (!icon.isNull())
                item->setIcon(0, icon);
        }
        else if (columnName == "cpu")
        {
            int percent = -1;
            value = this->formatCpuUsage(xenObject, &percent);
            item->setData(col, Qt::UserRole, percent);
        }
        else if (columnName == "memory")
        {
            int percent = -1;
            value = this->formatMemoryUsage(xenObject, &percent);
            item->setData(col, Qt::UserRole, percent);
        }
        else if (columnName == "disks")
        {
            value = this->formatDiskIO(xenObject);
        }
        else if (columnName == "network")
        {
            value = this->formatNetworkIO(xenObject);
        }
        else if (columnName == "ip")
        {
            value = this->formatIpAddress(xenObject);
        }
        else if (columnName == "ha")
        {
            value = this->formatHA(xenObject);
        }
        else if (columnName == "uptime")
        {
            value = this->formatUptime(xenObject);
        }
        
        item->setText(col, value);
        col++;
    }
}

QString QueryPanel::formatCpuUsage(XenObject* xenObject, int* percentOut) const
{
    if (percentOut)
        *percentOut = -1;

    if (!xenObject)
        return QString("--");

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString("--");

    MetricUpdater* metrics = connection->GetMetricUpdater();
    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString metricsRef = objectData.value("metrics", "").toString();
        if (metricsRef.isEmpty() || metricsRef == "OpaqueRef:NULL" || !cache)
            return "-";

        QVariantMap vmMetrics = cache->ResolveObjectData("vm_metrics", metricsRef);
        if (vmMetrics.isEmpty())
            return "-";

        int vcpuCount = vmMetrics.value("VCPUs_number", 0).toInt();
        if (vcpuCount == 0)
            return "-";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("vm", uuid))
        {
            if (vcpuCount == 1)
                return QString("-% of 1 CPU");
            return QString("-% of %1 CPUs").arg(vcpuCount);
        }

        double sum = 0.0;
        for (int i = 0; i < vcpuCount; i++)
        {
            QString metricName = QString("cpu%1").arg(i);
            double value = metrics->getValue("vm", uuid, metricName);
            if (!std::isfinite(value))
                return "-";
            sum += value;
        }

        if (!std::isfinite(sum))
            return "-";

        double avgPercent = (sum / vcpuCount) * 100.0;
        int rounded = qRound(avgPercent);
        if (percentOut)
            *percentOut = qBound(0, rounded, 100);

        if (vcpuCount == 1)
            return QString("%1% of 1 CPU").arg(rounded);
        return QString("%1% of %2 CPUs").arg(rounded).arg(vcpuCount);
    }

    if (objectType == "host")
    {
        int cpuCount = objectData.value("cpu_count", 0).toInt();
        if (cpuCount == 0)
            return "-";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("host", uuid))
        {
            if (cpuCount == 1)
                return QString("-% of 1 CPU");
            return QString("-% of %1 CPUs").arg(cpuCount);
        }

        double sum = 0.0;
        for (int i = 0; i < cpuCount; i++)
        {
            QString metricName = QString("cpu%1").arg(i);
            double value = metrics->getValue("host", uuid, metricName);
            if (!std::isfinite(value))
                return "-";
            sum += value;
        }

        if (!std::isfinite(sum))
            return "-";

        double avgPercent = (sum / cpuCount) * 100.0;
        int rounded = qRound(avgPercent);
        if (percentOut)
            *percentOut = qBound(0, rounded, 100);

        if (cpuCount == 1)
            return QString("%1% of 1 CPU").arg(rounded);
        return QString("%1% of %2 CPUs").arg(rounded).arg(cpuCount);
    }

    return "";
}

QString QueryPanel::formatMemoryUsage(XenObject* xenObject, int* percentOut) const
{
    if (percentOut)
        *percentOut = -1;

    if (!xenObject)
        return QString("--");

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString("--");

    MetricUpdater* metrics = connection->GetMetricUpdater();
    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("vm", uuid))
        {
            qulonglong memoryBytes = objectData.value("memory_static_max", 0).toULongLong();
            if (memoryBytes == 0)
                return "-";

            double memoryGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);
            return QString("%1 GB").arg(memoryGB, 0, 'f', 1);
        }

        double memoryBytes = metrics->getValue("vm", uuid, "memory");
        double freeKB = metrics->getValue("vm", uuid, "memory_internal_free");

        if (!std::isfinite(memoryBytes) || !std::isfinite(freeKB))
            return "-";

        if (memoryBytes == 0.0)
        {
            memoryBytes = objectData.value("memory_static_max", 0).toULongLong();
            if (memoryBytes == 0)
                return "-";
        }

        if (memoryBytes < (freeKB * 1024.0))
            return "-";

        double usedBytes = memoryBytes - (freeKB * 1024.0);
        double usedGB = usedBytes / (1024.0 * 1024.0 * 1024.0);
        double totalGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);

        if (percentOut && memoryBytes > 0.0)
        {
            double percent = (usedBytes / memoryBytes) * 100.0;
            *percentOut = qBound(0, qRound(percent), 100);
        }

        return QString("%1 GB of %2 GB")
            .arg(usedGB, 0, 'f', 1)
            .arg(totalGB, 0, 'f', 1);
    }

    if (objectType == "host")
    {
        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("host", uuid))
        {
            QString metricsRef = objectData.value("metrics", "").toString();
            if (!metricsRef.isEmpty() && metricsRef != "OpaqueRef:NULL" && cache)
            {
                QVariantMap hostMetrics = cache->ResolveObjectData("host_metrics", metricsRef);
                qulonglong memoryTotal = hostMetrics.value("memory_total", 0).toULongLong();
                qulonglong memoryFree = hostMetrics.value("memory_free", 0).toULongLong();
                if (memoryTotal > 0 && memoryFree > 0 && memoryFree <= memoryTotal)
                {
                    qulonglong usedBytes = memoryTotal - memoryFree;
                    double usedGB = usedBytes / (1024.0 * 1024.0 * 1024.0);
                    double totalGB = memoryTotal / (1024.0 * 1024.0 * 1024.0);
                    if (percentOut)
                    {
                        double percent = (static_cast<double>(usedBytes) / memoryTotal) * 100.0;
                        *percentOut = qBound(0, qRound(percent), 100);
                    }
                    return QString("%1 GB of %2 GB")
                        .arg(usedGB, 0, 'f', 1)
                        .arg(totalGB, 0, 'f', 1);
                }
                if (memoryTotal > 0)
                {
                    double memoryGB = memoryTotal / (1024.0 * 1024.0 * 1024.0);
                    return QString("%1 GB total").arg(memoryGB, 0, 'f', 1);
                }
            }
            return "-";
        }

        double totalKiB = metrics->getValue("host", uuid, "memory_total_kib");
        double freeKiB = metrics->getValue("host", uuid, "memory_free_kib");

        if (!std::isfinite(totalKiB) || !std::isfinite(freeKiB))
            return "-";

        if (totalKiB == 0.0)
            return "-";

        double usedKiB = totalKiB - freeKiB;
        double usedGB = (usedKiB * 1024.0) / (1024.0 * 1024.0 * 1024.0);
        double totalGB = (totalKiB * 1024.0) / (1024.0 * 1024.0 * 1024.0);

        if (percentOut)
        {
            double percent = (usedKiB / totalKiB) * 100.0;
            *percentOut = qBound(0, qRound(percent), 100);
        }

        return QString("%1 GB of %2 GB")
            .arg(usedGB, 0, 'f', 1)
            .arg(totalGB, 0, 'f', 1);
    }

    return "";
}

QString QueryPanel::formatDiskIO(XenObject* xenObject) const
{
    if (!xenObject)
        return QString("--");

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString("--");

    MetricUpdater* metrics = connection->GetMetricUpdater();
    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("vm", uuid))
            return "-";

        QVariantList vbds = objectData.value("VBDs", QVariantList()).toList();
        if (vbds.isEmpty())
            return "-";

        double totalRead = 0.0;
        double totalWrite = 0.0;
        int vbdCount = 0;

        for (const QVariant& vbdRef : vbds)
        {
            if (!cache)
                continue;

            QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef.toString());
            if (vbdData.isEmpty())
                continue;

            QString device = vbdData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            QString readMetric = QString("vbd_%1_read").arg(device);
            QString writeMetric = QString("vbd_%1_write").arg(device);

            double read = metrics->getValue("vm", uuid, readMetric);
            double write = metrics->getValue("vm", uuid, writeMetric);

            totalRead += read;
            totalWrite += write;
            vbdCount++;
        }

        if (vbdCount == 0)
            return "-";

        double readKBps = totalRead / 1024.0;
        double writeKBps = totalWrite / 1024.0;

        return QString("%1 / %2 KB/s")
            .arg(qRound(readKBps))
            .arg(qRound(writeKBps));
    }

    return "";
}

QString QueryPanel::formatNetworkIO(XenObject* xenObject) const
{
    if (!xenObject)
        return QString("--");

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString("--");

    MetricUpdater* metrics = connection->GetMetricUpdater();
    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("vm", uuid))
            return "-";

        QVariantList vifs = objectData.value("VIFs", QVariantList()).toList();
        if (vifs.isEmpty())
            return "-";

        double totalRx = 0.0;
        double totalTx = 0.0;
        int vifCount = 0;

        for (const QVariant& vifRef : vifs)
        {
            if (!cache)
                continue;

            QVariantMap vifData = cache->ResolveObjectData("vif", vifRef.toString());
            if (vifData.isEmpty())
                continue;

            QString device = vifData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            QString rxMetric = QString("vif_%1_rx").arg(device);
            QString txMetric = QString("vif_%1_tx").arg(device);

            double rx = metrics->getValue("vm", uuid, rxMetric);
            double tx = metrics->getValue("vm", uuid, txMetric);

            totalRx += rx;
            totalTx += tx;
            vifCount++;
        }

        if (vifCount == 0)
            return "-";

        double rxKBps = totalRx / 1024.0;
        double txKBps = totalTx / 1024.0;

        return QString("%1 / %2 KB/s")
            .arg(qRound(rxKBps))
            .arg(qRound(txKBps));
    }

    if (objectType == "host")
    {
        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        if (!metrics || !metrics->hasMetrics("host", uuid))
            return "-";

        QVariantList pifs = objectData.value("PIFs", QVariantList()).toList();
        if (pifs.isEmpty())
            return "-";

        double totalRx = 0.0;
        double totalTx = 0.0;
        int pifCount = 0;

        for (const QVariant& pifRef : pifs)
        {
            if (!cache)
                continue;

            QVariantMap pifData = cache->ResolveObjectData("pif", pifRef.toString());
            if (pifData.isEmpty())
                continue;

            QString device = pifData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            QString rxMetric = QString("pif_%1_rx").arg(device);
            QString txMetric = QString("pif_%1_tx").arg(device);

            double rx = metrics->getValue("host", uuid, rxMetric);
            double tx = metrics->getValue("host", uuid, txMetric);

            totalRx += rx;
            totalTx += tx;
            pifCount++;
        }

        if (pifCount == 0)
            return "-";

        double rxKBps = totalRx / 1024.0;
        double txKBps = totalTx / 1024.0;

        return QString("%1 / %2 KB/s")
            .arg(qRound(rxKBps))
            .arg(qRound(txKBps));
    }

    return "";
}

QString QueryPanel::formatIpAddress(XenObject* xenObject) const
{
    if (!xenObject)
        return QString();

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString();

    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString guestMetricsRef = objectData.value("guest_metrics", "").toString();
        if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL" || !cache)
            return "";

        QVariantMap guestMetrics = cache->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
        if (guestMetrics.isEmpty())
            return "";

        QVariantMap networks = guestMetrics.value("networks", QVariantMap()).toMap();
        QStringList ipAddresses;

        for (auto it = networks.constBegin(); it != networks.constEnd(); ++it)
        {
            QString key = it.key();
            QString value = it.value().toString();
            if (key.endsWith("/ip") && !value.isEmpty())
            {
                ipAddresses.append(value);
            }
        }

        if (!ipAddresses.isEmpty())
            return ipAddresses.join(", ");

        return "";
    }

    if (objectType == "host")
    {
        QString address = objectData.value("address", "").toString();
        return address;
    }

    return QString();
}

QString QueryPanel::formatUptime(XenObject* xenObject) const
{
    if (!xenObject)
        return QString();

    const QString objectType = xenObject->GetObjectType();
    const QVariantMap objectData = xenObject->GetData();
    XenConnection* connection = xenObject->GetConnection();
    if (!connection)
        return QString();

    XenCache* cache = connection->GetCache();

    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running" && powerState != "Paused" && powerState != "Suspended")
            return "";

        QString metricsRef = objectData.value("metrics", "").toString();
        if (metricsRef.isEmpty() || metricsRef == "OpaqueRef:NULL" || !cache)
            return "";

        QVariantMap vmMetrics = cache->ResolveObjectData("vm_metrics", metricsRef);
        if (vmMetrics.isEmpty())
            return "";

        QString startTimeStr = vmMetrics.value("start_time", "").toString();
        if (startTimeStr.isEmpty())
            return "";

        QDateTime startTime = QDateTime::fromString(startTimeStr, Qt::ISODate);
        if (!startTime.isValid())
            return "";

        QDateTime now = QDateTime::currentDateTimeUtc();
        qint64 uptimeSeconds = startTime.secsTo(now);
        if (uptimeSeconds < 0)
            return "";

        int days = uptimeSeconds / 86400;
        int hours = (uptimeSeconds % 86400) / 3600;
        int minutes = (uptimeSeconds % 3600) / 60;

        QStringList parts;
        if (days > 0)
            parts.append(tr("%1 days").arg(days));
        if (hours > 0 || days > 0)
            parts.append(tr("%1 hours").arg(hours));
        if (minutes > 0 || parts.isEmpty())
            parts.append(tr("%1 minutes").arg(minutes));

        return parts.join(" ");
    }

    if (objectType == "host")
    {
        return "-";
    }

    return "";
}

QString QueryPanel::formatHA(XenObject* xenObject) const
{
    // TODO: Get HA restart priority
    Q_UNUSED(xenObject);
    return QString("--");
}

void QueryPanel::showColumn(const QString& column)
{
    this->columns_[column] = true;
    
    // Find column index
    int index = DEFAULT_COLUMNS.indexOf(column);
    if (index >= 0)
    {
        this->header()->showSection(index);
    }
}

void QueryPanel::hideColumn(const QString& column)
{
    this->columns_[column] = false;
    
    int index = DEFAULT_COLUMNS.indexOf(column);
    if (index >= 0)
    {
        this->header()->hideSection(index);
    }
}

void QueryPanel::toggleColumn(const QString& column)
{
    if (this->columns_.value(column, false))
        this->hideColumn(column);
    else
        this->showColumn(column);
}

void QueryPanel::removeColumn(const QString& column)
{
    if (this->isDefaultColumn(column))
        return;
    
    this->hideColumn(column);
    this->columns_.remove(column);
}

bool QueryPanel::isDefaultColumn(const QString& column) const
{
    return DEFAULT_COLUMNS.contains(column);
}

bool QueryPanel::isMovableColumn(const QString& column) const
{
    // Name column is immovable
    return column != "name";
}

QString QueryPanel::getI18nColumnName(const QString& column) const
{
    if (column == "name")
        return tr("Name");
    else if (column == "cpu")
        return tr("CPU Usage");
    else if (column == "memory")
        return tr("Memory Usage");
    else if (column == "disks")
        return tr("Disks");
    else if (column == "network")
        return tr("Network");
    else if (column == "ha")
        return tr("HA");
    else if (column == "ip")
        return tr("IP Address");
    else if (column == "uptime")
        return tr("Uptime");
    else
        return column;
}

int QueryPanel::getDefaultColumnWidth(const QString& column) const
{
    if (column == "name")
        return 250;
    else if (column == "cpu")
        return 115;
    else if (column == "memory")
        return 125;
    else if (column == "disks")
        return 100;
    else if (column == "network")
        return 100;
    else if (column == "ha")
        return 120;
    else if (column == "ip")
        return 120;
    else if (column == "uptime")
        return 170;
    else
        return 100;
}

void QueryPanel::ShowChooseColumnsMenu(const QPoint& point)
{
    QMenu menu(this);
    
    for (const QString& column : this->columns_.keys())
    {
        if (!this->isMovableColumn(column))
            continue;
        
        QAction* action = new QAction(this->getI18nColumnName(column), &menu);
        action->setCheckable(true);
        action->setChecked(this->columns_.value(column, false));
        action->setData(column);
        
        connect(action, &QAction::triggered, [this, column]() {
            this->toggleColumn(column);
            this->BuildList();
        });
        
        menu.addAction(action);
    }
    
    menu.exec(this->mapToGlobal(point));
}

QList<QAction*> QueryPanel::GetChooseColumnsMenu()
{
    QList<QAction*> actions;
    
    for (const QString& column : this->columns_.keys())
    {
        if (!this->isMovableColumn(column))
            continue;
        
        QAction* action = new QAction(this->getI18nColumnName(column), this);
        action->setCheckable(true);
        action->setChecked(this->columns_.value(column, false));
        action->setData(column);
        
        connect(action, &QAction::triggered, [this, column]() {
            this->toggleColumn(column);
            this->BuildList();
        });
        
        actions.append(action);
    }
    
    return actions;
}

void QueryPanel::saveRowStates()
{
    this->expandedState_.clear();
    
    QTreeWidgetItemIterator it(this);
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        if (item->childCount() > 0)
        {
            // Use opaque_ref as key if available
            XenObject* xenObj = static_cast<XenObject*>(item->data(0, Qt::UserRole).value<void*>());
            if (xenObj)
            {
                this->expandedState_[xenObj->OpaqueRef()] = item->isExpanded();
            }
        }
        ++it;
    }
}

void QueryPanel::restoreRowStates()
{
    QTreeWidgetItemIterator it(this);
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        if (item->childCount() > 0)
        {
            XenObject* xenObj = static_cast<XenObject*>(item->data(0, Qt::UserRole).value<void*>());
            if (xenObj && this->expandedState_.contains(xenObj->OpaqueRef()))
            {
                item->setExpanded(this->expandedState_[xenObj->OpaqueRef()]);
            }
        }
        ++it;
    }
}

QList<QPair<QString, bool>> QueryPanel::GetSorting() const
{
    QList<QPair<QString, bool>> sorting;
    
    int sortColumn = this->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = this->header()->sortIndicatorOrder();
    
    if (sortColumn >= 0 && sortColumn < DEFAULT_COLUMNS.size())
    {
        QString columnName = DEFAULT_COLUMNS[sortColumn];
        bool ascending = (sortOrder == Qt::AscendingOrder);
        sorting.append(qMakePair(columnName, ascending));
    }
    
    return sorting;
}

void QueryPanel::SetSorting(const QList<QPair<QString, bool>>& sorting)
{
    if (sorting.isEmpty())
        return;
    
    // Qt only supports single-column sorting, use first sort
    QPair<QString, bool> firstSort = sorting.first();
    int columnIndex = DEFAULT_COLUMNS.indexOf(firstSort.first);
    
    if (columnIndex >= 0)
    {
        Qt::SortOrder order = firstSort.second ? Qt::AscendingOrder : Qt::DescendingOrder;
        this->sortByColumn(columnIndex, order);
    }
}

bool QueryPanel::IsSortingByMetrics() const
{
    QList<QPair<QString, bool>> sorting = this->GetSorting();
    
    for (const QPair<QString, bool>& sort : sorting)
    {
        QString column = sort.first;
        if (column == "cpu" || column == "memory" || column == "disks" ||
            column == "network" || column == "uptime")
        {
            return true;
        }
    }
    
    return false;
}

void QueryPanel::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = this->itemAt(event->pos());
    if (!item)
    {
        this->ShowChooseColumnsMenu(event->pos());
        return;
    }

    QMenu menu(this);
    QAction* copyCellAction = menu.addAction(tr("Copy Cell"));
    QAction* copyRowAction = menu.addAction(tr("Copy Row"));
    menu.addSeparator();
    QAction* columnsAction = menu.addAction(tr("Columns..."));

    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;

    if (chosen == copyCellAction)
    {
        int column = this->columnAt(event->pos().x());
        if (column < 0)
            column = 0;
        this->copyCell(item, column);
        return;
    }

    if (chosen == copyRowAction)
    {
        this->copyRow(item);
        return;
    }

    if (chosen == columnsAction)
    {
        this->ShowChooseColumnsMenu(event->pos());
        return;
    }
}

void QueryPanel::onHeaderContextMenu(const QPoint& point)
{
    this->ShowChooseColumnsMenu(point);
}

void QueryPanel::onMetricsUpdateTimerTimeout()
{
    // Update metrics for visible rows
    if (this->IsSortingByMetrics())
    {
        // Re-sort if sorting by metrics
        this->BuildList();
    }
    else
    {
        // Just update display
        this->viewport()->update();
    }
}

void QueryPanel::onSortIndicatorChanged(int logicalIndex, Qt::SortOrder order)
{
    Q_UNUSED(logicalIndex);
    Q_UNUSED(order);
    
    // Notify that search changed (sort order changed)
    emit this->SearchChanged();
}

void QueryPanel::copyCell(QTreeWidgetItem* item, int column) const
{
    if (!item || column < 0 || column >= this->columnCount())
        return;

    const QString text = item->text(column);
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(text);
}

void QueryPanel::copyRow(QTreeWidgetItem* item) const
{
    if (!item)
        return;

    QStringList cells;
    for (int col = 0; col < this->columnCount(); ++col)
    {
        if (this->isColumnHidden(col))
            continue;
        cells.append(item->text(col));
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(cells.join("\t"));
}

void QueryPanel::PanelShown()
{
    if (metricsUpdateTimer_)
        metricsUpdateTimer_->start();
}

void QueryPanel::PanelHidden()
{
    if (metricsUpdateTimer_)
        metricsUpdateTimer_->stop();
}
