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

#include "searchtabpage.h"
#include "../../xenlib/search.h"
#include "../../xenlib/query.h"
#include "../../xenlib/grouping.h"
#include "../../xenlib/queryscope.h"
#include "../../xenlib/queryfilter.h"
#include "../../xenlib/xenlib.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/metricupdater.h"
#include "../iconmanager.h"
#include "../widgets/progressbardelegate.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QDebug>

// C# Equivalent: XenAdmin.Controls.XenSearch.QueryPanel constructor
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 64-80
SearchTabPage::SearchTabPage(QWidget* parent)
    : BaseTabPage(parent), m_search(nullptr), m_tableWidget(new QTableWidget(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Setup table widget with columns matching C# QueryPanel
    // C# Reference: QueryPanel.SetupHeaderRow() lines 189-197
    this->m_tableWidget->setColumnCount(COL_COUNT);

    QStringList headers;
    headers << tr("Name")                      // COL_NAME
            << tr("CPU Usage")                 // COL_CPU
            << tr("Used Memory")               // COL_MEMORY
            << tr("Disks\n(avg / max KB/s)")   // COL_DISKS
            << tr("Network\n(avg / max KB/s)") // COL_NETWORK
            << tr("Address")                   // COL_ADDRESS
            << tr("Uptime");                   // COL_UPTIME

    this->m_tableWidget->setHorizontalHeaderLabels(headers);

    // Configure table appearance
    this->m_tableWidget->setAlternatingRowColors(true);
    this->m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    this->m_tableWidget->setSortingEnabled(true);
    this->m_tableWidget->verticalHeader()->setVisible(false);
    this->m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Column sizing (matching C# GetDefaultColumn widths)
    // C# Reference: QueryPanel.GetDefaultColumn() lines 198-250
    // Allow user to resize all columns with mouse (Interactive mode)
    this->m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    this->m_tableWidget->horizontalHeader()->setStretchLastSection(true); // Last column fills remaining space
    this->m_tableWidget->setColumnWidth(COL_NAME, 250);
    this->m_tableWidget->setColumnWidth(COL_CPU, 115);
    this->m_tableWidget->setColumnWidth(COL_MEMORY, 125);
    this->m_tableWidget->setColumnWidth(COL_DISKS, 120);
    this->m_tableWidget->setColumnWidth(COL_NETWORK, 120);
    this->m_tableWidget->setColumnWidth(COL_ADDRESS, 120);
    this->m_tableWidget->setColumnWidth(COL_UPTIME, 170);

    // Apply progress bar delegates to CPU and Memory columns
    // C# Equivalent: BarGraphColumn with Images.GetImageForPercentage()
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 50-51, 249-300
    this->m_tableWidget->setItemDelegateForColumn(COL_CPU, new ProgressBarDelegate(this));
    this->m_tableWidget->setItemDelegateForColumn(COL_MEMORY, new ProgressBarDelegate(this));

    layout->addWidget(this->m_tableWidget);

    // Connect signals
    connect(this->m_tableWidget, &QTableWidget::cellDoubleClicked,
            this, &SearchTabPage::onItemDoubleClicked);
    connect(this->m_tableWidget, &QTableWidget::itemSelectionChanged,
            this, &SearchTabPage::onItemSelectionChanged);
}

SearchTabPage::~SearchTabPage()
{
    delete this->m_search; // Search owns Query and Grouping
}

// C# Equivalent: QueryPanel.Search property setter
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 378-396
void SearchTabPage::setSearch(Search* search)
{
    if (this->m_search != search)
    {
        delete this->m_search;
        this->m_search = search;
        this->buildList();
    }
}

bool SearchTabPage::isApplicableForObjectType(const QString& type) const
{
    // SearchTabPage is shown for all object types as the last tab
    // Reference: MainWindow.cs BuildTabList() - newTabs.Add(TabPageSearch) is always added at the end
    Q_UNUSED(type);
    return true;
}

void SearchTabPage::setXenObject(const QString& type, const QString& ref, const QVariantMap& data)
{
    // C# Reference: MainWindow.cs lines 1716-1810
    // When Search tab is shown for a specific object (not in SearchMode),
    // we need to create a Search that shows all objects of the same type/group
    
    Q_UNUSED(data);
    
    // Store the current object
    this->m_currentObjectType = type;
    this->m_currentObjectRef = ref;
    
    if (!this->m_xenLib || !this->m_xenLib->getCache())
    {
        qDebug() << "SearchTabPage::setXenObject - No XenLib or cache available";
        return;
    }
    
    // Check if we need to create a new search
    // We need a new search if:
    // 1. No search exists yet, OR
    // 2. The object type has changed (switching from VM to Host, etc.)
    bool needNewSearch = false;
    
    if (!this->m_search)
    {
        needNewSearch = true;
    }
    else
    {
        // Check if this search was created by clicking a grouping tag
        // Those searches have meaningful names like "Windows Virtual Machines"
        // Our auto-generated searches have names like "All Virtual Machines"
        QString searchName = this->m_search->getName();
        
        // Check if the search scope matches the current object type
        // If we're viewing a VM but search is for hosts, we need a new search
        Query* query = this->m_search->getQuery();
        if (query)
        {
            QueryFilter* filter = query->getQueryFilter();
            // Check if the filter is a TypePropertyQuery for a different type
            if (filter)
            {
                // For now, we'll recreate search if switching between object types
                // A more sophisticated check would inspect the filter's target type
                // For simplicity, track the last type we created a search for
                static QString lastAutoSearchType;
                
                // If search name starts with "All ", it's our auto-generated search
                if (searchName.startsWith("All ") && lastAutoSearchType != type)
                {
                    needNewSearch = true;
                    lastAutoSearchType = type;
                }
            }
        }
    }
    
    if (needNewSearch)
    {
        // Create a simple default search showing all objects of this type
        // This provides useful functionality even without full grouping context
        // C# would create a search based on the root node grouping here (lines 1790-1805)
        this->createDefaultSearchForType(type);
    }
    
    if (this->m_search)
    {
        this->buildList();
    }
}

// C# Equivalent: QueryPanel.BuildList() + listUpdateManager_Update()
// C# Reference:
// - xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs line 530
// - xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 500-528
void SearchTabPage::buildList()
{
    if (!this->m_search || !this->m_xenLib || !this->m_xenLib->getCache())
    {
        this->m_tableWidget->setRowCount(0);
        return;
    }

    this->populateTable();
}

// C# Equivalent: QueryPanel.listUpdateManager_Update() + CreateRow()
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 500-570
void SearchTabPage::populateTable()
{
    // qDebug() << "SearchTabPage::populateTable() - Starting table population";

    // Disable sorting while populating (improves performance)
    this->m_tableWidget->setSortingEnabled(false);
    this->m_tableWidget->setRowCount(0);

    if (!this->m_search)
    {
        qDebug() << "SearchTabPage::populateTable() - No search object";
        this->m_tableWidget->setSortingEnabled(true);
        return;
    }

    Query* query = this->m_search->getQuery();

    if (!query || !this->m_xenLib || !this->m_xenLib->getCache())
    {
        qDebug() << "SearchTabPage::populateTable() - No query or cache";
        this->m_tableWidget->setSortingEnabled(true);
        return;
    }

    XenCache* cache = this->m_xenLib->getCache();
    int itemsAdded = 0;

    // qDebug() << "=== SearchTabPage::populateTable() START ===";
    // qDebug() << "Query scope:" << query->getQueryScope();

    // Iterate ALL cached objects (like C# connection.Cache.XenSearchableObjects)
    // C# Reference: GroupAlg.cs GetGrouped() lines 83-88
    // The Query.match() call will filter objects based on QueryScope
    QList<QPair<QString, QString>> allObjects = cache->GetAllObjectsData();

    // qDebug() << "Total objects from getAllObjects():" << allObjects.size();

    int hiddenCount = 0;
    int queryRejectedCount = 0;

    foreach (const auto& objPair, allObjects)
    {
        const QString& objectType = objPair.first;
        const QString& objectRef = objPair.second;

        QVariantMap objectData = cache->ResolveObjectData(objectType, objectRef);

        // Hide internal objects (control domain, tools SR, etc.)
        // C# Equivalent: if (Hide(o)) continue;
        // C# Reference: xenadmin/XenModel/XenSearch/GroupAlg.cs line 89
        if (shouldHideObject(objectType, objectData))
        {
            hiddenCount++;
            continue;
        }

        // Filter: does this object match the query?
        // C# Equivalent: Group.FilterAdd() calls query.Match(o)
        // C# Reference: xenadmin/XenModel/XenSearch/GroupAlg.cs line 161
        if (query->match(objectData, objectType, this->m_xenLib))
        {
            // Add object as a table row
            // C# Equivalent: QueryPanel.CreateRow() for IXenObject
            // C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 629-679
            this->addObjectRow(objectType, objectRef, objectData);
            itemsAdded++;
        } else
        {
            queryRejectedCount++;
        }
    }

    // qDebug() << "Summary: hidden=" << hiddenCount
    //          << ", rejected=" << queryRejectedCount
    //          << ", added=" << itemsAdded;
    // qDebug() << "=== SearchTabPage::populateTable() END ===";

    // Re-enable sorting
    this->m_tableWidget->setSortingEnabled(true);

    // If no results, show message
    // C# Equivalent: QueryPanel.AddNoResultsRow()
    // C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 573-578
    if (itemsAdded == 0)
    {
        this->m_tableWidget->setRowCount(1);
        QTableWidgetItem* noResultsItem = new QTableWidgetItem(tr("No results found"));
        noResultsItem->setForeground(QBrush(Qt::gray));
        noResultsItem->setFlags(noResultsItem->flags() & ~Qt::ItemIsSelectable);
        this->m_tableWidget->setItem(0, COL_NAME, noResultsItem);
        this->m_tableWidget->setSpan(0, COL_NAME, 1, COL_COUNT); // Merge all columns
    }
}

// C# Equivalent: QueryPanel.CreateRow() for IXenObject rows
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 629-679
void SearchTabPage::addObjectRow(const QString& objectType, const QString& objectRef,
                                 const QVariantMap& objectData)
{
    // Enrich object data with resolved references for icons
    // This matches C# pattern where Images.GetIconFor() resolves related objects
    QVariantMap enrichedData = objectData;

    if (objectType == "host")
    {
        // Resolve host_metrics to get liveness status for icon
        // C# pattern: Host_metrics metrics = host.Connection.Resolve(host.metrics);
        QString metricsRef = objectData.value("metrics").toString();
        if (!metricsRef.isEmpty() && !metricsRef.contains("NULL") && this->m_xenLib)
        {
            XenCache* cache = this->m_xenLib->getCache();
            if (cache)
            {
                QVariantMap metricsData = cache->ResolveObjectData("host_metrics", metricsRef);
                if (!metricsData.isEmpty())
                {
                    enrichedData["_metrics_live"] = metricsData.value("live", false);
                }
            }
        }
        // qDebug() << "SearchTabPage::addObjectRow - Host" << objectData.value("name_label").toString()
        //          << "_metrics_live=" << enrichedData.value("_metrics_live", false);
    }

    int row = this->m_tableWidget->rowCount();
    this->m_tableWidget->insertRow(row);

    // Column 0: Name
    // C# Equivalent: NameColumn.GetGridItem()
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 191-220
    QString name = this->getObjectName(objectType, objectData);
    QString description = this->getObjectDescription(objectType, objectData);
    QString nameWithDesc = name;
    if (!description.isEmpty())
        nameWithDesc += "\n" + description;

    QTableWidgetItem* nameItem = new QTableWidgetItem(nameWithDesc);
    QIcon icon = IconManager::instance().getIconForObject(objectType, enrichedData);
    if (!icon.isNull())
        nameItem->setIcon(icon);
    nameItem->setData(Qt::UserRole, objectType);    // Store type for sorting/filtering
    nameItem->setData(Qt::UserRole + 1, objectRef); // Store ref for double-click
    this->m_tableWidget->setItem(row, COL_NAME, nameItem);

    // Column 1: CPU Usage
    // C# Equivalent: BarGraphColumn(PropertyNames.cpuText, PropertyNames.cpuValue)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 48
    QString cpuText = this->getCPUUsageText(objectType, objectData);
    int cpuPercent = this->getCPUUsagePercent(objectType, objectData);
    QTableWidgetItem* cpuItem = new QTableWidgetItem(cpuText);
    cpuItem->setData(Qt::UserRole, cpuPercent); // Store for sorting
    cpuItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_CPU, cpuItem);

    // Column 2: Memory Usage
    // C# Equivalent: BarGraphColumn(PropertyNames.memoryText, PropertyNames.memoryValue, PropertyNames.memoryRank, true)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 49
    QString memoryText = getMemoryUsageText(objectType, objectData);
    int memoryPercent = getMemoryUsagePercent(objectType, objectData);
    QTableWidgetItem* memoryItem = new QTableWidgetItem(memoryText);
    memoryItem->setData(Qt::UserRole, memoryPercent); // Store for sorting
    memoryItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_MEMORY, memoryItem);

    // Column 3: Disks
    // C# Equivalent: PropertyColumn(PropertyNames.diskText, true)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs line 50
    QString disksText = this->getDiskUsageText(objectType, objectData);
    QTableWidgetItem* disksItem = new QTableWidgetItem(disksText);
    disksItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_DISKS, disksItem);

    // Column 4: Network
    // C# Equivalent: PropertyColumn(PropertyNames.networkText, true)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs line 51
    QString networkText = this->getNetworkUsageText(objectType, objectData);
    QTableWidgetItem* networkItem = new QTableWidgetItem(networkText);
    networkItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_NETWORK, networkItem);

    // Column 5: IP Address
    // C# Equivalent: PropertyColumn(PropertyNames.ip_address)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs line 53
    QString ipAddress = getIPAddress(objectType, objectData);
    QTableWidgetItem* ipItem = new QTableWidgetItem(ipAddress);
    ipItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_ADDRESS, ipItem);

    // Column 6: Uptime
    // C# Equivalent: PropertyColumn(PropertyNames.uptime)
    // C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs line 54
    QString uptime = this->getUptime(objectType, objectData);
    QTableWidgetItem* uptimeItem = new QTableWidgetItem(uptime);
    uptimeItem->setTextAlignment(Qt::AlignCenter);
    this->m_tableWidget->setItem(row, COL_UPTIME, uptimeItem);
}

QString SearchTabPage::getObjectName(const QString& objectType, const QVariantMap& objectData) const
{
    QString name = objectData.value("name_label", "").toString();
    if (name.isEmpty())
        name = tr("(Unnamed %1)").arg(objectType);
    return name;
}

QString SearchTabPage::getObjectDescription(const QString& objectType, const QVariantMap& objectData) const
{
    Q_UNUSED(objectType);
    return objectData.value("name_description", "").toString();
}

// C# Equivalent: PropertyAccessorHelper.vmCpuUsageString() / hostCpuUsageString()
// C# Reference: xenadmin/XenModel/XenSearch/PropertyAccessorHelper.cs lines 47-71
QString SearchTabPage::getCPUUsageText(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        // C# Logic: Get VM_metrics, sum VCPUs_utilisation for all VCPUs
        // Format: "X% of Y CPUs" (or "X% of 1 CPU" for single CPU)
        QString metricsRef = objectData.value("metrics", "").toString();
        if (metricsRef.isEmpty() || metricsRef == "OpaqueRef:NULL" || !this->m_xenLib || !this->m_xenLib->getCache())
            return "-";

        QVariantMap vmMetrics = this->m_xenLib->getCache()->ResolveObjectData("vm_metrics", metricsRef);
        if (vmMetrics.isEmpty())
            return "-";

        // Get number of VCPUs
        int vcpuCount = vmMetrics.value("VCPUs_number", 0).toInt();
        if (vcpuCount == 0)
            return "-";

        // Get UUID for RRD metrics lookup
        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        // Get CPU utilization from MetricUpdater
        // C# uses: for (int i = 0; i < vcpus; i++) sum += MetricUpdater.GetValue(vm, String.Format("cpu{0}", i))
        MetricUpdater* metrics = this->m_xenLib->getMetricUpdater();
        if (!metrics || !metrics->hasMetrics("vm", uuid))
        {
            // No metrics available yet, show placeholder
            if (vcpuCount == 1)
                return QString("-% of 1 CPU");
            else
                return QString("-% of %1 CPUs").arg(vcpuCount);
        }

        double sum = 0.0;
        for (int i = 0; i < vcpuCount; i++)
        {
            QString metricName = QString("cpu%1").arg(i);
            sum += metrics->getValue("vm", uuid, metricName);
        }

        // Calculate average percentage (RRD values are 0-1, convert to 0-100)
        double avgPercent = (sum / vcpuCount) * 100.0;

        // Format: "22% of 8 CPUs"
        if (vcpuCount == 1)
            return QString("%1% of 1 CPU").arg(qRound(avgPercent));
        else
            return QString("%1% of %2 CPUs").arg(qRound(avgPercent)).arg(vcpuCount);
    } else if (objectType == "host")
    {
        // C# Logic: Sum CPU utilization for all host_CPUs
        // Format: "X% of Y CPUs"
        int cpuCount = objectData.value("cpu_count", 0).toInt();
        if (cpuCount == 0)
            return "-";

        // Get UUID for RRD metrics lookup
        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        // Get CPU utilization from MetricUpdater
        MetricUpdater* metrics = this->m_xenLib->getMetricUpdater();
        if (!metrics || !metrics->hasMetrics("host", uuid))
        {
            // No metrics available yet
            if (cpuCount == 1)
                return QString("-% of 1 CPU");
            else
                return QString("-% of %1 CPUs").arg(cpuCount);
        }

        double sum = 0.0;
        for (int i = 0; i < cpuCount; i++)
        {
            QString metricName = QString("cpu%1").arg(i);
            sum += metrics->getValue("host", uuid, metricName);
        }

        // Calculate average percentage
        double avgPercent = (sum / cpuCount) * 100.0;

        if (cpuCount == 1)
            return QString("%1% of 1 CPU").arg(qRound(avgPercent));
        else
            return QString("%1% of %2 CPUs").arg(qRound(avgPercent)).arg(cpuCount);
    }

    return "";
}

int SearchTabPage::getCPUUsagePercent(const QString& objectType, const QVariantMap& objectData) const
{
    Q_UNUSED(objectType);
    Q_UNUSED(objectData);
    // TODO: Implement actual CPU usage percentage calculation
    // For now, return -1 (not available)
    return -1;
}

// C# Equivalent: PropertyAccessorHelper.vmMemoryUsageString() / hostMemoryUsageString()
// C# Reference: xenadmin/XenModel/XenSearch/PropertyAccessorHelper.cs lines 96-109 and 234-245
QString SearchTabPage::getMemoryUsageText(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        // C# Logic:
        // double free = MetricUpdater.GetValue(vm, "memory_internal_free");  // in KB
        // double total = MetricUpdater.GetValue(vm, "memory");  // in bytes
        // Format: "{used} of {total}" where used = total - (free * 1024)

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        MetricUpdater* metrics = this->m_xenLib ? this->m_xenLib->getMetricUpdater() : nullptr;
        if (!metrics || !metrics->hasMetrics("vm", uuid))
        {
            // No metrics available, show static allocation
            qulonglong memoryBytes = objectData.value("memory_static_max", 0).toULongLong();
            if (memoryBytes == 0)
                return "-";

            double memoryGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);
            return QString("%1 GB").arg(memoryGB, 0, 'f', 1);
        }

        // Get actual memory usage from RRD
        double memoryBytes = metrics->getValue("vm", uuid, "memory");          // total in bytes
        double freeKB = metrics->getValue("vm", uuid, "memory_internal_free"); // free in KB

        if (memoryBytes == 0.0)
        {
            // Fallback to static allocation
            memoryBytes = objectData.value("memory_static_max", 0).toULongLong();
            if (memoryBytes == 0)
                return "-";
        }

        double usedBytes = memoryBytes - (freeKB * 1024.0);
        double usedGB = usedBytes / (1024.0 * 1024.0 * 1024.0);
        double totalGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);

        return QString("%1 GB of %2 GB")
            .arg(usedGB, 0, 'f', 1)
            .arg(totalGB, 0, 'f', 1);
    } else if (objectType == "host")
    {
        // C# Logic:
        // double free = MetricUpdater.GetValue(host, "memory_free_kib");  // in KiB
        // double total = MetricUpdater.GetValue(host, "memory_total_kib");  // in KiB
        // Format: "{used} of {total}" where used = (total - free) * 1024 bytes

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        MetricUpdater* metrics = this->m_xenLib ? this->m_xenLib->getMetricUpdater() : nullptr;
        if (!metrics || !metrics->hasMetrics("host", uuid))
        {
            // No metrics available, show from host_metrics if available
            QString metricsRef = objectData.value("metrics", "").toString();
            if (!metricsRef.isEmpty() && metricsRef != "OpaqueRef:NULL" && this->m_xenLib && this->m_xenLib->getCache())
            {
                QVariantMap hostMetrics = this->m_xenLib->getCache()->ResolveObjectData("host_metrics", metricsRef);
                qulonglong memoryTotal = hostMetrics.value("memory_total", 0).toULongLong();
                if (memoryTotal > 0)
                {
                    double memoryGB = memoryTotal / (1024.0 * 1024.0 * 1024.0);
                    return QString("%1 GB total").arg(memoryGB, 0, 'f', 1);
                }
            }
            return "-";
        }

        // Get actual memory usage from RRD
        double totalKiB = metrics->getValue("host", uuid, "memory_total_kib");
        double freeKiB = metrics->getValue("host", uuid, "memory_free_kib");

        if (totalKiB == 0.0)
            return "-";

        double usedKiB = totalKiB - freeKiB;
        double usedGB = (usedKiB * 1024.0) / (1024.0 * 1024.0 * 1024.0);
        double totalGB = (totalKiB * 1024.0) / (1024.0 * 1024.0 * 1024.0);

        return QString("%1 GB of %2 GB")
            .arg(usedGB, 0, 'f', 1)
            .arg(totalGB, 0, 'f', 1);
    }

    return "";
}

int SearchTabPage::getMemoryUsagePercent(const QString& objectType, const QVariantMap& objectData) const
{
    if (!this->m_xenLib)
        return -1;

    MetricUpdater* metrics = this->m_xenLib->getMetricUpdater();
    if (!metrics)
        return -1;

    QString uuid = objectData.value("uuid", "").toString();
    if (uuid.isEmpty() || !metrics->hasMetrics(objectType, uuid))
        return -1;

    if (objectType == "vm")
    {
        double memoryBytes = metrics->getValue("vm", uuid, "memory");
        double freeKB = metrics->getValue("vm", uuid, "memory_internal_free");

        if (memoryBytes == 0.0)
            return -1;

        double usedBytes = memoryBytes - (freeKB * 1024.0);
        double percent = (usedBytes / memoryBytes) * 100.0;
        return qBound(0, qRound(percent), 100);
    } else if (objectType == "host")
    {
        double totalKiB = metrics->getValue("host", uuid, "memory_total_kib");
        double freeKiB = metrics->getValue("host", uuid, "memory_free_kib");

        if (totalKiB == 0.0)
            return -1;

        double usedKiB = totalKiB - freeKiB;
        double percent = (usedKiB / totalKiB) * 100.0;
        return qBound(0, qRound(percent), 100);
    }

    return -1;
}

// C# Equivalent: PropertyAccessorHelper.vmDiskUsageString()
// C# Reference: xenadmin/XenModel/XenSearch/Common.cs (PropertyAccessorHelper class)
QString SearchTabPage::getDiskUsageText(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        MetricUpdater* metrics = this->m_xenLib ? this->m_xenLib->getMetricUpdater() : nullptr;
        if (!metrics || !metrics->hasMetrics("vm", uuid))
            return "-";

        // Get VBDs (virtual block devices) for this VM
        QVariantList vbds = objectData.value("VBDs", QVariantList()).toList();
        if (vbds.isEmpty())
            return "-";

        // Sum read/write rates across all VBDs
        // C# uses: MetricUpdater.GetValue(vm, String.Format("vbd_{0}_read", vbdDevice))
        double totalRead = 0.0;
        double totalWrite = 0.0;
        int vbdCount = 0;

        foreach (QVariant vbdRef, vbds)
        {
            if (!this->m_xenLib->getCache())
                continue;

            QVariantMap vbdData = this->m_xenLib->getCache()->ResolveObjectData("vbd", vbdRef.toString());
            if (vbdData.isEmpty())
                continue;

            QString device = vbdData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            // RRD metric names: vbd_xvda_read, vbd_xvda_write
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

        // Convert bytes/sec to KB/sec
        double readKBps = totalRead / 1024.0;
        double writeKBps = totalWrite / 1024.0;

        // Format: "read / write KB/s"
        return QString("%1 / %2 KB/s")
            .arg(qRound(readKBps))
            .arg(qRound(writeKBps));
    }

    return "";
}

// C# Equivalent: PropertyAccessorHelper.vmNetworkUsageString()
// C# Reference: xenadmin/XenModel/XenSearch/Common.cs (PropertyAccessorHelper class)
QString SearchTabPage::getNetworkUsageText(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running")
            return "";

        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        MetricUpdater* metrics = this->m_xenLib ? this->m_xenLib->getMetricUpdater() : nullptr;
        if (!metrics || !metrics->hasMetrics("vm", uuid))
            return "-";

        // Get VIFs (virtual network interfaces) for this VM
        QVariantList vifs = objectData.value("VIFs", QVariantList()).toList();
        if (vifs.isEmpty())
            return "-";

        // Sum rx/tx rates across all VIFs
        // C# uses: MetricUpdater.GetValue(vm, String.Format("vif_{0}_rx", vifDevice))
        double totalRx = 0.0;
        double totalTx = 0.0;
        int vifCount = 0;

        foreach (QVariant vifRef, vifs)
        {
            if (!this->m_xenLib->getCache())
                continue;

            QVariantMap vifData = this->m_xenLib->getCache()->ResolveObjectData("vif", vifRef.toString());
            if (vifData.isEmpty())
                continue;

            QString device = vifData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            // RRD metric names: vif_0_rx, vif_0_tx, vif_1_rx, etc.
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

        // Convert bytes/sec to KB/sec
        double rxKBps = totalRx / 1024.0;
        double txKBps = totalTx / 1024.0;

        // Format: "rx / tx KB/s" (matching C# format like "1663/4466 KB/s")
        return QString("%1 / %2 KB/s")
            .arg(qRound(rxKBps))
            .arg(qRound(txKBps));
    } else if (objectType == "host")
    {
        // C# shows network stats in format "avg / max KB/s"
        // For hosts, sum all PIF (physical network interface) metrics
        QString uuid = objectData.value("uuid", "").toString();
        if (uuid.isEmpty())
            return "-";

        MetricUpdater* metrics = this->m_xenLib ? this->m_xenLib->getMetricUpdater() : nullptr;
        if (!metrics || !metrics->hasMetrics("host", uuid))
            return "-";

        // Get PIFs for this host
        QVariantList pifs = objectData.value("PIFs", QVariantList()).toList();
        if (pifs.isEmpty())
            return "-";

        double totalRx = 0.0;
        double totalTx = 0.0;
        int pifCount = 0;

        foreach (QVariant pifRef, pifs)
        {
            if (!this->m_xenLib->getCache())
                continue;

            QVariantMap pifData = this->m_xenLib->getCache()->ResolveObjectData("pif", pifRef.toString());
            if (pifData.isEmpty())
                continue;

            QString device = pifData.value("device", "").toString();
            if (device.isEmpty())
                continue;

            // RRD metric names: pif_eth0_rx, pif_eth0_tx, etc.
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

        // Convert bytes/sec to KB/sec
        double rxKBps = totalRx / 1024.0;
        double txKBps = totalTx / 1024.0;

        return QString("%1 / %2 KB/s")
            .arg(qRound(rxKBps))
            .arg(qRound(txKBps));
    }

    return "";
}

// C# Equivalent: VM.IPAddressForDisplay() / Host.address
// C# Reference:
// - xenadmin/XenModel/XenSearch/Common.cs lines 856-906 (IPAddressProperty)
// - xenadmin/XenModel/XenSearch/PropertyAccessorHelper.cs lines 175-187 (vmIPAddresses)
QString SearchTabPage::getIPAddress(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        // C# Logic: Get VM_guest_metrics, extract IP from networks map
        // guest_metrics.networks format: {"0/ip": "192.168.1.100", "0/ipv6/0": "fe80::...", ...}
        // We want the first IPv4 address from VIF device 0

        QString guestMetricsRef = objectData.value("guest_metrics", "").toString();
        if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL" || !this->m_xenLib || !this->m_xenLib->getCache())
            return "";

        QVariantMap guestMetrics = this->m_xenLib->getCache()->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
        if (guestMetrics.isEmpty())
            return "";

        // Extract IP addresses from networks map
        // C# uses: Helpers.FindIpAddresses(metrics.networks, vif.device)
        // We'll just get the first IPv4 address we find
        QVariantMap networks = guestMetrics.value("networks", QVariantMap()).toMap();
        QStringList ipAddresses;

        for (auto it = networks.constBegin(); it != networks.constEnd(); ++it)
        {
            QString key = it.key();
            QString value = it.value().toString();

            // Look for keys like "0/ip", "1/ip" (IPv4 addresses)
            if (key.endsWith("/ip") && !value.isEmpty())
            {
                ipAddresses.append(value);
            }
        }

        if (!ipAddresses.isEmpty())
            return ipAddresses.join(", ");

        return "";
    } else if (objectType == "host")
    {
        // C# Logic: Return host.address directly
        QString address = objectData.value("address", "").toString();
        return address;
    }

    return "";
}

// C# Equivalent: VM.RunningTime() / Host.Uptime()
// C# Reference:
// - xenadmin/XenModel/XenAPI-Extensions/VM.cs lines 1138-1159 (GetStartTime, RunningTime)
// - xenadmin/XenModel/XenSearch/Common.cs lines 420-428 (UptimeProperty)
QString SearchTabPage::getUptime(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        QString powerState = objectData.value("power_state", "").toString();
        if (powerState != "Running" && powerState != "Paused" && powerState != "Suspended")
            return "";

        // C# Logic: Get VM_metrics.start_time, calculate uptime = now - start_time
        // Format: "X days Y hours Z minutes" (using PrettyTimeSpan)
        QString metricsRef = objectData.value("metrics", "").toString();
        if (metricsRef.isEmpty() || metricsRef == "OpaqueRef:NULL" || !this->m_xenLib || !this->m_xenLib->getCache())
            return "";

        QVariantMap vmMetrics = this->m_xenLib->getCache()->ResolveObjectData("vm_metrics", metricsRef);
        if (vmMetrics.isEmpty())
            return "";

        // start_time is ISO 8601 format string: "20241125T10:30:00Z"
        QString startTimeStr = vmMetrics.value("start_time", "").toString();
        if (startTimeStr.isEmpty())
            return "";

        QDateTime startTime = QDateTime::fromString(startTimeStr, Qt::ISODate);
        if (!startTime.isValid())
            return "";

        // Calculate uptime
        QDateTime now = QDateTime::currentDateTimeUtc();
        qint64 uptimeSeconds = startTime.secsTo(now);
        if (uptimeSeconds < 0)
            return "";

        // Format as "X days Y hours Z minutes"
        // C# uses PrettyTimeSpan which formats similar to this
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
    } else if (objectType == "host")
    {
        // C# Logic: Similar to VM, calculate from Host_metrics or other_config
        // TODO: Implement host uptime calculation
        // For now, return placeholder
        return "-";
    }

    return "";
}

// C# Equivalent: GroupAlg.Hide()
// C# Reference: xenadmin/XenModel/XenSearch/GroupAlg.cs lines 105-148
bool SearchTabPage::shouldHideObject(const QString& objectType, const QVariantMap& objectData) const
{
    if (objectType == "vm")
    {
        // Hide control domain (Dom0) - C# checks is_control_domain
        bool isControlDomain = objectData.value("is_control_domain", false).toBool();
        if (isControlDomain)
            return true;
    } else if (objectType == "sr")
    {
        // Hide tools SRs
        QString srType = objectData.value("type", "").toString();
        if (srType == "tools" || objectData.value("is_tools_sr", false).toBool())
            return true;
    }

    return false;
}

// C# Equivalent: Double-click handler in QueryPanel.CreateRow()
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 653-660
void SearchTabPage::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    if (row < 0 || row >= this->m_tableWidget->rowCount())
        return;

    QTableWidgetItem* nameItem = this->m_tableWidget->item(row, COL_NAME);
    if (!nameItem)
        return;

    QString objectType = nameItem->data(Qt::UserRole).toString();
    QString objectRef = nameItem->data(Qt::UserRole + 1).toString();

    if (!objectType.isEmpty() && !objectRef.isEmpty())
    {
        emit objectSelected(objectType, objectRef);
    }
}

void SearchTabPage::onItemSelectionChanged()
{
    // Currently no action on selection change
    // In C# this updates context menus
}

void SearchTabPage::onXenLibObjectsChanged()
{
    // Rebuild list when objects change in cache
    this->buildList();
}

void SearchTabPage::createDefaultSearchForType(const QString& objectType)
{
    // Create a simple search that shows all objects of the given type
    // C# equivalent would use Search.SearchForNonVappGroup with a TypeGrouping
    // C# Reference: MainWindow.cs lines 1790-1805 (Infrastructure View default search)
    
    // Map object type to display name
    QString displayName;
    if (objectType == "vm")
        displayName = tr("All Virtual Machines");
    else if (objectType == "host")
        displayName = tr("All Servers");
    else if (objectType == "sr")
        displayName = tr("All Storage");
    else if (objectType == "network")
        displayName = tr("All Networks");
    else if (objectType == "pool")
        displayName = tr("All Pools");
    else
        displayName = tr("All %1 Objects").arg(objectType);
    
    // Create query scope - all objects except folders
    QueryScope* scope = new QueryScope(ObjectTypes::AllExcFolders);
    
    // Create filter for this object type
    // TypePropertyQuery matches objects with _type == objectType
    QueryFilter* filter = new TypePropertyQuery(objectType);
    
    // Create query combining scope and filter
    Query* query = new Query(scope, filter);
    
    // Create search with no grouping (flat list)
    // Columns: null (use default columns)
    // Sorting: null (use default sorting)
    // C# equivalent: new Search(query, null, displayName, "", false)
    Search* search = new Search(query, nullptr, displayName, "", false);
    
    // Replace existing search (if any)
    if (this->m_search)
    {
        delete this->m_search;
    }
    this->m_search = search;
}
