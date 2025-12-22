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

#include "navigationview.h"
#include <QDebug>
#include "ui_navigationview.h"
#include <QDebug>
#include "../iconmanager.h"
#include <QDebug>
#include "../../xenlib/xenlib.h"
#include <QDebug>
#include "../../xenlib/collections/connectionsmanager.h"
#include <QDebug>
#include "../../xenlib/xen/connection.h"
#include <QDebug>
#include "../../xenlib/xencache.h"
#include <QDebug>
#include "../../xenlib/vmhelpers.h"
#include <QDebug>
#include "../../xenlib/groupingtag.h"
#include <QDebug>
#include "../../xenlib/grouping.h"
#include <QDebug>
#include <algorithm>

/**
 * @brief Natural string comparison (matches C# StringUtility.NaturalCompare)
 *
 * Compares strings in a way that handles embedded numbers naturally.
 * E.g., "VM2" < "VM10" (unlike alphabetical where "VM10" < "VM2")
 */
static int naturalCompare(const QString& s1, const QString& s2)
{
    if (s1.compare(s2, Qt::CaseInsensitive) == 0)
        return 0;

    if (s1.isEmpty())
        return -1;
    if (s2.isEmpty())
        return 1;

    int i = 0;
    int len1 = s1.length();
    int len2 = s2.length();
    int minLen = qMin(len1, len2);

    while (i < minLen)
    {
        QChar c1 = s1[i];
        QChar c2 = s2[i];

        bool c1IsDigit = c1.isDigit();
        bool c2IsDigit = c2.isDigit();

        if (!c1IsDigit && !c2IsDigit)
        {
            // Two non-digits: alphabetical comparison
            int cmp = s1.mid(i, 1).compare(s2.mid(i, 1), Qt::CaseInsensitive);
            if (cmp != 0)
                return cmp;
            i++;
        } else if (c1IsDigit && c2IsDigit)
        {
            // Both are digits: compare as numbers
            int j = i + 1;
            while (j < len1 && s1[j].isDigit())
                j++;
            int k = i + 1;
            while (k < len2 && s2[k].isDigit())
                k++;

            int numLen1 = j - i;
            int numLen2 = k - i;

            // Shorter number (in digits) is smaller
            if (numLen1 != numLen2)
                return numLen1 - numLen2;

            // Same length: compare digit by digit
            QString num1 = s1.mid(i, numLen1);
            QString num2 = s2.mid(i, numLen2);
            int cmp = num1.compare(num2);
            if (cmp != 0)
                return cmp;

            i = j;
        } else
        {
            // One is digit, one is not: digits come after letters
            return c1IsDigit ? 1 : -1;
        }
    }

    // Strings are equal up to minLen, shorter one is smaller
    return len1 - len2;
}

/**
 * @brief Sort children of a tree widget item using natural comparison
 */
static void sortTreeItemChildren(QTreeWidgetItem* parent)
{
    if (!parent || parent->childCount() == 0)
        return;

    // Collect children
    QList<QTreeWidgetItem*> children;
    while (parent->childCount() > 0)
    {
        children.append(parent->takeChild(0));
    }

    // Sort using natural compare
    std::sort(children.begin(), children.end(),
              [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                  return naturalCompare(a->text(0), b->text(0)) < 0;
              });

    // Re-add in sorted order
    for (QTreeWidgetItem* child : children)
    {
        parent->addChild(child);
    }
}

/**
 * @brief Sort top-level items in a tree widget using natural comparison
 */
static void sortTreeTopLevel(QTreeWidget* tree)
{
    if (!tree || tree->topLevelItemCount() == 0)
        return;

    // Collect top-level items
    QList<QTreeWidgetItem*> items;
    while (tree->topLevelItemCount() > 0)
    {
        items.append(tree->takeTopLevelItem(0));
    }

    // Sort using natural compare
    std::sort(items.begin(), items.end(),
              [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                  return naturalCompare(a->text(0), b->text(0)) < 0;
              });

    // Re-add in sorted order
    for (QTreeWidgetItem* item : items)
    {
        tree->addTopLevelItem(item);
    }
}

NavigationView::NavigationView(QWidget* parent)
    : QWidget(parent), ui(new Ui::NavigationView), m_inSearchMode(false), m_navigationMode(NavigationPane::Infrastructure), m_xenLib(nullptr), m_refreshTimer(new QTimer(this)), m_typeGrouping(new TypeGrouping()) // Create TypeGrouping for Objects view
      ,
      m_savedSelectionType(""), m_savedSelectionRef(""), m_suppressSelectionSignals(false)
{
    ui->setupUi(this);

    // Setup debounce timer for cache updates (200ms delay)
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(200);
    connect(m_refreshTimer, &QTimer::timeout, this, &NavigationView::onRefreshTimerTimeout);

    // Connect tree widget signals to our signals
    // Emit before-selected signal before selection changes (C# TreeView.BeforeSelect)
    connect(ui->treeWidget, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
                Q_UNUSED(current);
                Q_UNUSED(previous);
                emit treeNodeBeforeSelected();
            });

    // Connect selection changed signal with suppression check
    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged,
            this, [this]() {
                // Don't emit signal during tree rebuild (matches C# ignoring selection changes during BeginUpdate)
                if (!m_suppressSelectionSignals)
                {
                    emit treeViewSelectionChanged();
                }
            });

    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &NavigationView::treeNodeClicked);

    connect(ui->treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &NavigationView::treeNodeRightClicked);

    // Connect search box (matches C# searchTextBox_TextChanged line 215)
    connect(ui->searchLineEdit, &QLineEdit::textChanged,
            this, &NavigationView::onSearchTextChanged);
}

NavigationView::~NavigationView()
{
    delete m_typeGrouping;
    delete ui;
}

QTreeWidget* NavigationView::treeWidget() const
{
    return ui->treeWidget;
}

void NavigationView::focusTreeView()
{
    ui->treeWidget->setFocus();
}

void NavigationView::requestRefreshTreeView()
{
    // Matches C# TreeView BeginUpdate/EndUpdate pattern with PersistExpandedNodes/RestoreExpandedNodes
    // Suppress selection signals while rebuilding to avoid clearing selection in MainWindow
    m_suppressSelectionSignals = true;

    emit treeViewRefreshSuspended(); // Signal that we're about to rebuild

    ui->treeWidget->setUpdatesEnabled(false); // Suspend painting

    // Persist current selection and expanded nodes BEFORE rebuild (matches C# PersistExpandedNodes)
    persistSelectionAndExpansion();

    // Rebuild tree based on navigation mode
    switch (m_navigationMode)
    {
    case NavigationPane::Infrastructure:
        buildInfrastructureTree();
        break;
    case NavigationPane::Objects:
        buildObjectsTree();
        break;
    case NavigationPane::Tags:
    case NavigationPane::Folders:
    case NavigationPane::CustomFields:
    case NavigationPane::vApps:
        buildOrganizationTree();
        break;
    default:
        buildInfrastructureTree();
        break;
    }

    // Restore selection and expanded nodes AFTER rebuild (matches C# RestoreExpandedNodes)
    bool selectionRestored = !m_savedSelectionType.isEmpty() && !m_savedSelectionRef.isEmpty();
    restoreSelectionAndExpansion();

    ui->treeWidget->setUpdatesEnabled(true); // Resume painting

    // Re-enable selection signals and emit a single change notification if we restored selection
    m_suppressSelectionSignals = false;
    if (selectionRestored && ui->treeWidget->currentItem())
    {
        emit treeViewSelectionChanged();
    }

    emit treeViewRefreshResumed(); // Signal rebuild is complete
    emit treeViewRefreshed();
}

void NavigationView::resetSearchBox()
{
    ui->searchLineEdit->clear();
}

void NavigationView::setInSearchMode(bool enabled)
{
    // Matches C# NavigationView.InSearchMode property (line 120)
    m_inSearchMode = enabled;
    // TODO: Update UI based on search mode (show/hide search-related indicators)
}

void NavigationView::setNavigationMode(NavigationPane::NavigationMode mode)
{
    // Matches C# NavigationView.NavigationMode property (line 114)
    if (m_navigationMode != mode)
    {
        m_navigationMode = mode;
        // Rebuild tree with new mode
        requestRefreshTreeView();
    }
}

QString NavigationView::searchText() const
{
    return ui->searchLineEdit->text();
}

void NavigationView::setSearchText(const QString& text)
{
    ui->searchLineEdit->setText(text);
}

void NavigationView::setXenLib(XenLib* xenLib)
{
    // Disconnect from old cache if any
    if (m_xenLib && m_xenLib->getCache())
    {
        disconnect(m_xenLib->getCache(), nullptr, this, nullptr);
    }

    m_xenLib = xenLib;

    // Connect to cache signals for automatic tree refresh
    // This matches C# where cache changes trigger tree rebuilds
    if (m_xenLib && m_xenLib->getCache())
    {
        connect(m_xenLib->getCache(), &XenCache::objectChanged,
                this, &NavigationView::onCacheObjectChanged);
        connect(m_xenLib->getCache(), &XenCache::objectRemoved,
                this, &NavigationView::onCacheObjectChanged);
    }
}

void NavigationView::onCacheObjectChanged(const QString& type, const QString& ref)
{
    Q_UNUSED(ref);

    // Only refresh for object types that appear in the tree
    // This avoids unnecessary refreshes for metrics, tasks, etc.
    if (type == "vm" || type == "host" || type == "pool" ||
        type == "sr" || type == "network" || type == "vbd" ||
        type == "vdi" || type == "vif")
    {
        scheduleRefresh();
    }
}

void NavigationView::scheduleRefresh()
{
    // Debounce: restart timer on each call
    // This coalesces multiple rapid cache updates into a single tree refresh
    m_refreshTimer->start();
}

void NavigationView::onRefreshTimerTimeout()
{
    // Timer fired - perform actual refresh
    requestRefreshTreeView();
}

void NavigationView::onSearchTextChanged(const QString& text)
{
    // Matches C# NavigationView.searchTextBox_TextChanged (line 215)
    // Trigger tree refresh with search filter
    // TODO: When tree builder is implemented, this will:
    // 1. Create filtered search: CurrentSearch.AddFullTextFilter(text)
    // 2. Call treeBuilder.RefreshTreeView(newRoot, text, mode)
    Q_UNUSED(text);
    requestRefreshTreeView();
}

void NavigationView::buildInfrastructureTree()
{
    // Matches C# MainWindowTreeBuilder.CreateNewRootNode for Infrastructure mode
    // This creates hierarchical tree: Pool → Host → VM → SR
    // Uses cache directly like C# connection.Cache.XenSearchableObjects

    ui->treeWidget->clear();

    if (!m_xenLib)
    {
        // No XenLib - show placeholder
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "(No connection manager available)");
        return;
    }

    // Get ConnectionsManager and Cache
    ConnectionsManager* connMgr = m_xenLib->getConnectionsManager();
    XenCache* cache = m_xenLib->getCache();

    //qDebug() << "NavigationView::buildInfrastructureTree: connMgr=" << connMgr << "cache=" << cache;

    if (!connMgr || !cache)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "(Connection manager not initialized)");
        return;
    }

    // Get all connections (both connected and disconnected)
    QList<XenConnection*> connections = connMgr->getAllConnections();

    //qDebug() << "NavigationView::buildInfrastructureTree: connections count=" << connections.size();

    if (connections.isEmpty())
    {
        // No connections - show connection prompt (matches C# behavior)
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "Connect to a XenServer");
        placeholder->setData(0, Qt::UserRole, QVariant::fromValue<void*>(nullptr)); // No object data
        return;
    }

    // Build tree for each connection
    // In C#, infrastructure view shows: Pool → Hosts → VMs/SRs under each host
    // Disconnected servers appear at root level just like pools (C# MainWindowTreeBuilder pattern)
    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        // Check if connected
        if (!connection->isConnected())
        {
            // Show disconnected connection at ROOT LEVEL (matches C# behavior)
            // C# creates a fake Host object with opaque_ref = HostnameWithPort (GroupAlg.cs line 97)
            // This allows disconnected servers to appear in tree with context menu
            QTreeWidgetItem* connItem = new QTreeWidgetItem(ui->treeWidget);
            connItem->setText(0, connection->getHostname()); // Show hostname WITHOUT "(disconnected)"
            // Store connection object as data (matches C# where fake Host.Connection points to XenConnection)
            connItem->setData(0, Qt::UserRole, QVariant::fromValue(connection));
            connItem->setData(0, Qt::UserRole + 1, QString("disconnected_host")); // Object type for context menu

            // Set disconnected host icon (C# uses HostDisconnected icon)
            QIcon disconnectedIcon = IconManager::instance().getDisconnectedIcon();
            connItem->setIcon(0, disconnectedIcon);
            continue;
        }

        // For connected servers, get pool data from CACHE (not API)
        // This matches C# connection.Cache.Resolve pattern
        QList<QVariantMap> pools = cache->GetAllData("pool");

        if (pools.isEmpty())
        {
            // Connection has no pool data yet
            QTreeWidgetItem* connItem = new QTreeWidgetItem(ui->treeWidget);
            connItem->setText(0, QString("%1 (connecting...)").arg(connection->getHostname()));
            connItem->setData(0, Qt::UserRole, QVariant::fromValue(connection));
            continue;
        }

        // Build pool node (normally only one pool per connection)
        for (const QVariantMap& pool : pools)
        {
            QString poolRef = pool.value("ref").toString();
            QString poolName = pool.value("name_label").toString();

            if (poolName.isEmpty())
            {
                poolName = connection->getHostname(); // Fallback to hostname
            }

            QTreeWidgetItem* poolItem = new QTreeWidgetItem(ui->treeWidget);
            poolItem->setText(0, poolName);
            poolItem->setData(0, Qt::UserRole, poolRef);
            poolItem->setData(0, Qt::UserRole + 1, QString("pool")); // Object type
            poolItem->setExpanded(true);                             // Expand pool by default

            // Set pool icon
            QIcon poolIcon = IconManager::instance().getIconForPool(pool);
            poolItem->setIcon(0, poolIcon);
            // TODO: Set appropriate pool icon

            // Get hosts in this pool from CACHE
            QVariantList hostRefs = pool.value("master").toList();
            if (hostRefs.isEmpty())
            {
                // Try alternate field name
                hostRefs = pool.value("hosts").toList();
            }

            // Get all hosts from cache (pool.hosts is just list of refs)
            QList<QVariantMap> allHosts = cache->GetAllData("host");

            // Build map of hostRef -> QTreeWidgetItem* for VM placement
            QMap<QString, QTreeWidgetItem*> hostItems;

            for (const QVariantMap& hostData : allHosts)
            {
                QString hostRef = hostData.value("ref").toString();
                QString hostName = hostData.value("name_label").toString();
                if (hostName.isEmpty())
                    hostName = "(Unnamed Host)";

                QTreeWidgetItem* hostItem = new QTreeWidgetItem(poolItem);
                hostItem->setText(0, hostName);
                hostItem->setData(0, Qt::UserRole, hostRef);
                hostItem->setData(0, Qt::UserRole + 1, QString("host")); // Object type
                hostItem->setExpanded(true);                             // Expand host by default

                // Set host icon - resolve host_metrics to determine liveness
                // C# pattern: Host_metrics metrics = host.Connection.Resolve(host.metrics);
                //             if (metrics != null && metrics.live) { ... }
                QVariantMap enrichedHostData = hostData;
                QString metricsRef = hostData.value("metrics").toString();
                if (!metricsRef.isEmpty() && !metricsRef.contains("NULL"))
                {
                    QVariantMap metricsData = cache->ResolveObjectData("host_metrics", metricsRef);
                    if (!metricsData.isEmpty())
                    {
                        // Add the resolved 'live' status to enriched data for IconManager
                        enrichedHostData["_metrics_live"] = metricsData.value("live", false);
                    }
                }
                QIcon hostIcon = IconManager::instance().getIconForHost(enrichedHostData);
                hostItem->setIcon(0, hostIcon);

                // Store in map for VM placement later
                hostItems[hostRef] = hostItem;

                // Get storage repositories (PBDs) for this host from CACHE
                // In C# infrastructure, SRs are shown under hosts
                QVariantList pbdRefs = hostData.value("PBDs").toList();

                for (const QVariant& pbdRefVar : pbdRefs)
                {
                    QString pbdRef = pbdRefVar.toString();
                    QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);

                    if (pbdData.isEmpty())
                        continue;

                    QString srRef = pbdData.value("SR").toString();
                    QVariantMap srData = cache->ResolveObjectData("sr", srRef);

                    if (srData.isEmpty())
                        continue;

                    // Check if SR is shared (shows on multiple hosts)
                    // Only show SR once in tree (on first host if shared)
                    // TODO: Implement proper SR deduplication like C#

                    QString srName = srData.value("name_label").toString();
                    if (srName.isEmpty())
                        srName = "(Unnamed Storage)";

                    // Skip ISO and tools SRs in infrastructure view (C# behavior)
                    QString contentType = srData.value("content_type").toString();
                    if (contentType == "iso")
                        continue;

                    QTreeWidgetItem* srItem = new QTreeWidgetItem(hostItem);
                    srItem->setText(0, srName);
                    srItem->setData(0, Qt::UserRole, srRef);
                    srItem->setData(0, Qt::UserRole + 1, QString("sr")); // Object type

                    // Set SR icon
                    QIcon srIcon = IconManager::instance().getIconForSR(srData);
                    srItem->setIcon(0, srIcon);
                }
            }

            // CRITICAL: Place VMs using VMHelpers::getVMHome() (matches C# VM.Home() behavior)
            // This handles:
            // 1. Running/Paused VMs → placed under resident_on host
            // 2. VMs with local storage → placed under storage host
            // 3. VMs with affinity → placed under affinity host (if enabled)
            // 4. Offline VMs with no affinity → placed at POOL LEVEL (not under any host)
            //
            // This is the KEY FIX for Issue 1: offline VMs were invisible because
            // the old code only placed VMs under resident_on host.
            QList<QVariantMap> allVMs = cache->GetAllData("vm");

            for (const QVariantMap& vmData : allVMs)
            {
                QString vmRef = vmData.value("ref").toString();

                // Skip control domain, snapshots, and templates
                bool isControlDomain = vmData.value("is_control_domain").toBool();
                bool isSnapshot = vmData.value("is_a_snapshot").toBool();
                bool isTemplate = vmData.value("is_a_template").toBool();

                if (isSnapshot || isTemplate)
                    continue;

                QString vmName = vmData.value("name_label").toString();
                if (vmName.isEmpty())
                    vmName = "(Unnamed VM)";

                // Use VMHelpers to determine where this VM should appear
                QString vmHomeRef = VMHelpers::getVMHome(m_xenLib, vmData);

                QTreeWidgetItem* parentItem = nullptr;

                if (vmHomeRef.isEmpty())
                {
                    // VM has no home → place at pool level
                    // This is the KEY behavior: offline VMs with no affinity appear
                    // at the pool level (not under any host)
                    parentItem = poolItem;
                } else if (hostItems.contains(vmHomeRef))
                {
                    // VM belongs under a specific host
                    parentItem = hostItems[vmHomeRef];
                } else
                {
                    // Home ref doesn't match any host in this pool → skip
                    // (VM might be in another pool)
                    continue;
                }

                // Add VM to tree
                QString displayName = vmName;
                if (isControlDomain && parentItem != poolItem)
                {
                    // Add control domain indicator (but only if under a host, not pool)
                    QString hostName = parentItem->text(0);
                    displayName = QString("Control domain on %1").arg(hostName);
                }

                QTreeWidgetItem* vmItem = new QTreeWidgetItem(parentItem);
                vmItem->setText(0, displayName);
                vmItem->setData(0, Qt::UserRole, vmRef);
                vmItem->setData(0, Qt::UserRole + 1, QString("vm")); // Object type

                // Set VM icon based on power state
                QIcon vmIcon = IconManager::instance().getIconForVM(vmData);
                vmItem->setIcon(0, vmIcon);
            }

            // Sort VMs under each host naturally (alphabetically with number awareness)
            for (auto it = hostItems.begin(); it != hostItems.end(); ++it)
            {
                sortTreeItemChildren(it.value());
            }

            // Sort hosts under pool, and VMs directly under pool (offline VMs)
            sortTreeItemChildren(poolItem);
        }
    }

    // Sort top-level pool items
    sortTreeTopLevel(ui->treeWidget);
}

void NavigationView::buildObjectsTree()
{
    // Matches C# OrganizationViewObjects.Populate
    // This creates flat tree grouped by TYPE (not "All X" categories)
    // Groups objects by PropertyNames.type: Pools, Hosts, VMs, SRs, Networks
    // Uses cache directly like C# connection.Cache.XenSearchableObjects

    ui->treeWidget->clear();

    if (!m_xenLib)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "(No connection manager available)");
        return;
    }

    // Get ConnectionsManager and Cache
    ConnectionsManager* connMgr = m_xenLib->getConnectionsManager();
    XenCache* cache = m_xenLib->getCache();

    if (!connMgr || !cache)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "(Connection manager not initialized)");
        return;
    }

    // Get all connections (both connected and disconnected)
    // C# OrganizationViewObjects shows disconnected servers in a separate group
    QList<XenConnection*> allConnections = connMgr->getAllConnections();
    QList<XenConnection*> disconnectedConnections;

    // Separate disconnected connections
    for (XenConnection* conn : allConnections)
    {
        if (conn && !conn->isConnected())
        {
            disconnectedConnections.append(conn);
        }
    }

    // Check if we have ANY connections at all
    if (allConnections.isEmpty())
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(ui->treeWidget);
        placeholder->setText(0, "Connect to a XenServer");
        return;
    }

    // In C# Objects view, items are grouped BY TYPE
    // Create type group nodes
    QTreeWidgetItem* poolsGroup = nullptr;
    QTreeWidgetItem* hostsGroup = nullptr;
    QTreeWidgetItem* disconnectedHostsGroup = nullptr; // NEW: For disconnected servers
    QTreeWidgetItem* vmsGroup = nullptr;
    QTreeWidgetItem* templatesGroup = nullptr;
    QTreeWidgetItem* storageGroup = nullptr;
    // TODO: Add networks when XenLib supports them
    // QTreeWidgetItem* networksGroup = nullptr;

    // Get objects directly from CACHE (matches C# connection.Cache pattern)
    // No need to iterate connections - cache has all objects

    // Pools from cache
    QList<QVariantMap> pools = cache->GetAllData("pool");
    for (const QVariantMap& pool : pools)
    {
        QString poolRef = pool.value("ref").toString();
        QString poolName = pool.value("name_label").toString();

        if (poolName.isEmpty())
            continue;

        // Create Pools group if needed
        if (!poolsGroup)
        {
            poolsGroup = new QTreeWidgetItem(ui->treeWidget);
            poolsGroup->setText(0, "Pools");
            poolsGroup->setExpanded(true);

            // Attach GroupingTag (matches C# MainWindowTreeBuilder line 365)
            // GroupingTag(grouping, parent, group)
            // For type grouping, group value is the type string "pool"
            GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("pool"));
            poolsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

            // TODO: Set Pools icon
        }

        QTreeWidgetItem* poolItem = new QTreeWidgetItem(poolsGroup);
        poolItem->setText(0, poolName);
        poolItem->setData(0, Qt::UserRole, poolRef);
        poolItem->setData(0, Qt::UserRole + 1, QString("pool"));

        // Set pool icon
        QIcon poolIcon = IconManager::instance().getIconForPool(pool);
        poolItem->setIcon(0, poolIcon);
    }

    // Hosts from cache
    QList<QVariantMap> hosts = cache->GetAllData("host");
    for (const QVariantMap& hostData : hosts)
    {
        QString hostRef = hostData.value("ref").toString();
        QString hostName = hostData.value("name_label").toString();

        if (hostName.isEmpty())
            continue;

        // Create Hosts group if needed
        if (!hostsGroup)
        {
            hostsGroup = new QTreeWidgetItem(ui->treeWidget);
            hostsGroup->setText(0, "Hosts");
            hostsGroup->setExpanded(true);

            // Attach GroupingTag
            GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("host"));
            hostsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

            // TODO: Set Hosts icon
        }

        QTreeWidgetItem* hostItem = new QTreeWidgetItem(hostsGroup);
        hostItem->setText(0, hostName);
        hostItem->setData(0, Qt::UserRole, hostRef);
        hostItem->setData(0, Qt::UserRole + 1, QString("host"));

        // Set host icon - resolve host_metrics to determine liveness
        // C# pattern: Host_metrics metrics = host.Connection.Resolve(host.metrics);
        QVariantMap enrichedHostData = hostData;
        QString metricsRef = hostData.value("metrics").toString();
        if (!metricsRef.isEmpty() && !metricsRef.contains("NULL"))
        {
            QVariantMap metricsData = cache->ResolveObjectData("host_metrics", metricsRef);
            if (!metricsData.isEmpty())
            {
                enrichedHostData["_metrics_live"] = metricsData.value("live", false);
            }
        }
        QIcon hostIcon = IconManager::instance().getIconForHost(enrichedHostData);
        hostItem->setIcon(0, hostIcon);
    }

    // VMs and Templates from cache
    QList<QVariantMap> vms = cache->GetAllData("vm");
    for (const QVariantMap& vmData : vms)
    {
        QString vmRef = vmData.value("ref").toString();
        bool isTemplate = vmData.value("is_a_template").toBool();
        bool isSnapshot = vmData.value("is_a_snapshot").toBool();
        bool isControlDomain = vmData.value("is_control_domain").toBool();

        // Skip control domain and snapshots
        if (isControlDomain || isSnapshot)
            continue;

        QString vmName = vmData.value("name_label").toString();
        if (vmName.isEmpty())
            continue;

        if (isTemplate)
        {
            // Create Templates group if needed
            if (!templatesGroup)
            {
                templatesGroup = new QTreeWidgetItem(ui->treeWidget);
                templatesGroup->setText(0, "Templates");
                templatesGroup->setExpanded(false); // Templates collapsed by default

                // Attach GroupingTag (group value is "template")
                GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("template"));
                templatesGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

                // TODO: Set Templates icon
            }

            QTreeWidgetItem* templateItem = new QTreeWidgetItem(templatesGroup);
            templateItem->setText(0, vmName);
            templateItem->setData(0, Qt::UserRole, vmRef);
            templateItem->setData(0, Qt::UserRole + 1, QString("template"));

            // Set template icon
            QIcon templateIcon = IconManager::instance().getIconForVM(vmData);
            templateItem->setIcon(0, templateIcon);
        } else
        {
            // Create VMs group if needed
            if (!vmsGroup)
            {
                vmsGroup = new QTreeWidgetItem(ui->treeWidget);
                vmsGroup->setText(0, "VMs");
                vmsGroup->setExpanded(true);

                // Attach GroupingTag (group value is "vm")
                GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("vm"));
                vmsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

                // TODO: Set VMs icon
            }

            QTreeWidgetItem* vmItem = new QTreeWidgetItem(vmsGroup);
            vmItem->setText(0, vmName);
            vmItem->setData(0, Qt::UserRole, vmRef);
            vmItem->setData(0, Qt::UserRole + 1, QString("vm"));

            // Set VM icon based on power state
            QIcon vmIcon = IconManager::instance().getIconForVM(vmData);
            vmItem->setIcon(0, vmIcon);
        }
    }

    // Storage Repositories from cache
    QList<QVariantMap> srs = cache->GetAllData("sr");
    for (const QVariantMap& srData : srs)
    {
        QString srRef = srData.value("ref").toString();
        QString srName = srData.value("name_label").toString();

        if (srName.isEmpty())
            continue;

        // Create Storage group if needed
        if (!storageGroup)
        {
            storageGroup = new QTreeWidgetItem(ui->treeWidget);
            storageGroup->setText(0, "Storage");
            storageGroup->setExpanded(true);

            // Attach GroupingTag (group value is "sr")
            GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("sr"));
            storageGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

            // TODO: Set Storage icon
        }

        QTreeWidgetItem* srItem = new QTreeWidgetItem(storageGroup);
        srItem->setText(0, srName);
        srItem->setData(0, Qt::UserRole, srRef);
        srItem->setData(0, Qt::UserRole + 1, QString("sr"));

        // Set SR icon
        QIcon srIcon = IconManager::instance().getIconForSR(srData);
        srItem->setIcon(0, srIcon);
    }

    // Disconnected Servers group (C# ObjectTypes.DisconnectedServer)
    // C# creates fake Host objects for disconnected connections (GroupAlg.cs line 93-100)
    // These appear in Objects view under "Disconnected Servers" group
    if (!disconnectedConnections.isEmpty())
    {
        // Create Disconnected Servers group
        disconnectedHostsGroup = new QTreeWidgetItem(ui->treeWidget);
        disconnectedHostsGroup->setText(0, "Disconnected servers"); // Matches C# Messages.DISCONNECTED_SERVERS
        disconnectedHostsGroup->setExpanded(true);

        // Attach GroupingTag (group value is "disconnected_host")
        GroupingTag* tag = new GroupingTag(m_typeGrouping, QVariant(), QVariant("disconnected_host"));
        disconnectedHostsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

        // Set disconnected group icon
        QIcon disconnectedGroupIcon = IconManager::instance().getDisconnectedIcon();
        disconnectedHostsGroup->setIcon(0, disconnectedGroupIcon);

        // Add each disconnected connection as a fake host
        for (XenConnection* conn : disconnectedConnections)
        {
            QTreeWidgetItem* disconnectedItem = new QTreeWidgetItem(disconnectedHostsGroup);
            disconnectedItem->setText(0, conn->getHostname());
            disconnectedItem->setData(0, Qt::UserRole, QVariant::fromValue(conn));
            disconnectedItem->setData(0, Qt::UserRole + 1, QString("disconnected_host"));

            // Set disconnected host icon
            QIcon disconnectedIcon = IconManager::instance().getDisconnectedIcon();
            disconnectedItem->setIcon(0, disconnectedIcon);
        }

        // Sort disconnected servers naturally
        sortTreeItemChildren(disconnectedHostsGroup);
    }

    // Sort items within each group naturally
    if (poolsGroup)
        sortTreeItemChildren(poolsGroup);
    if (hostsGroup)
        sortTreeItemChildren(hostsGroup);
    if (vmsGroup)
        sortTreeItemChildren(vmsGroup);
    if (templatesGroup)
        sortTreeItemChildren(templatesGroup);
    if (storageGroup)
        sortTreeItemChildren(storageGroup);

    // TODO: Add networks when XenLib supports them
}

void NavigationView::buildOrganizationTree()
{
    // Matches C# OrganizationViewTags/Folders/Fields/Vapps.Populate
    // This creates tree organized by tags, folders, custom fields, or vApps

    ui->treeWidget->clear();

    QString viewName;
    switch (m_navigationMode)
    {
    case NavigationPane::Tags:
        viewName = "Tags View";
        break;
    case NavigationPane::Folders:
        viewName = "Folders View";
        break;
    case NavigationPane::CustomFields:
        viewName = "Custom Fields View";
        break;
    case NavigationPane::vApps:
        viewName = "vApps View";
        break;
    default:
        viewName = "Organization View";
        break;
    }

    QTreeWidgetItem* root = new QTreeWidgetItem(ui->treeWidget);
    root->setText(0, viewName);
    root->setExpanded(true);

    QTreeWidgetItem* placeholder = new QTreeWidgetItem(root);
    placeholder->setText(0, "(Organization views require connected server)");

    // TODO: Implement full organization view logic from C# OrganizationView* classes
}

// ========== Tree State Preservation Methods (matches C# MainWindowTreeBuilder) ==========

QString NavigationView::getItemPath(QTreeWidgetItem* item) const
{
    if (!item)
        return QString();

    QStringList pathParts;
    QTreeWidgetItem* current = item;

    // Build path from item to root (excluding root)
    while (current)
    {
        QString type = current->data(0, Qt::UserRole + 1).toString();
        QString ref = current->data(0, Qt::UserRole).toString();

        // Use type:ref or just text if no type/ref available
        if (!type.isEmpty() && !ref.isEmpty())
        {
            pathParts.prepend(type + ":" + ref);
        } else
        {
            pathParts.prepend(current->text(0));
        }

        current = current->parent();
    }

    return pathParts.join("/");
}

void NavigationView::collectExpandedItems(QTreeWidgetItem* parent, QStringList& expandedPaths) const
{
    if (!parent)
        return;

    int count = parent->childCount();
    for (int i = 0; i < count; ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        if (child->isExpanded())
        {
            QString path = getItemPath(child);
            if (!path.isEmpty())
            {
                expandedPaths.append(path);
            }
        }

        // Recurse for children
        if (child->childCount() > 0)
        {
            collectExpandedItems(child, expandedPaths);
        }
    }
}

QTreeWidgetItem* NavigationView::findItemByTypeAndRef(const QString& type, const QString& ref, QTreeWidgetItem* parent) const
{
    if (!parent)
        return nullptr;

    int count = parent->childCount();
    for (int i = 0; i < count; ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        QString itemType = child->data(0, Qt::UserRole + 1).toString();
        QString itemRef = child->data(0, Qt::UserRole).toString();

        if (itemType == type && itemRef == ref)
        {
            return child;
        }

        // Recurse for children
        QTreeWidgetItem* found = findItemByTypeAndRef(type, ref, child);
        if (found)
        {
            return found;
        }
    }

    return nullptr;
}

void NavigationView::persistSelectionAndExpansion()
{
    // Save current selection (matches C# PersistSelectedNode)
    QTreeWidgetItem* selectedItem = ui->treeWidget->currentItem();
    if (selectedItem)
    {
        m_savedSelectionType = selectedItem->data(0, Qt::UserRole + 1).toString();
        m_savedSelectionRef = selectedItem->data(0, Qt::UserRole).toString();
    } else
    {
        m_savedSelectionType.clear();
        m_savedSelectionRef.clear();
    }

    // Save expanded nodes (matches C# PersistExpandedNodes)
    m_savedExpandedPaths.clear();

    // Check if root nodes are expanded
    int topLevelCount = ui->treeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelCount; ++i)
    {
        QTreeWidgetItem* rootItem = ui->treeWidget->topLevelItem(i);
        if (rootItem->isExpanded())
        {
            QString path = getItemPath(rootItem);
            if (!path.isEmpty())
            {
                m_savedExpandedPaths.append(path);
            }
        }

        // Collect expanded children
        collectExpandedItems(rootItem, m_savedExpandedPaths);
    }
}

void NavigationView::restoreSelectionAndExpansion()
{
    // Block selection signals during restore
    m_suppressSelectionSignals = true;

    // Restore expanded nodes (matches C# RestoreExpandedNodes)
    for (const QString& path : m_savedExpandedPaths)
    {
        // Try to find item by path
        QStringList pathParts = path.split("/", Qt::SkipEmptyParts);
        QTreeWidgetItem* current = nullptr;

        // Navigate through path
        for (const QString& part : pathParts)
        {
            // Parse type:ref from part
            QStringList typeRef = part.split(":");
            if (typeRef.size() >= 2)
            {
                QString type = typeRef[0];
                QString ref = typeRef.mid(1).join(":"); // Handle refs with colons

                if (!current)
                {
                    // Search top-level items
                    int topCount = ui->treeWidget->topLevelItemCount();
                    for (int i = 0; i < topCount; ++i)
                    {
                        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
                        QString itemType = item->data(0, Qt::UserRole + 1).toString();
                        QString itemRef = item->data(0, Qt::UserRole).toString();

                        if (itemType == type && itemRef == ref)
                        {
                            current = item;
                            break;
                        } else if (item->text(0) == part)
                        {
                            current = item;
                            break;
                        }
                    }
                } else
                {
                    // Search children of current
                    int childCount = current->childCount();
                    QTreeWidgetItem* found = nullptr;
                    for (int i = 0; i < childCount; ++i)
                    {
                        QTreeWidgetItem* child = current->child(i);
                        QString itemType = child->data(0, Qt::UserRole + 1).toString();
                        QString itemRef = child->data(0, Qt::UserRole).toString();

                        if (itemType == type && itemRef == ref)
                        {
                            found = child;
                            break;
                        } else if (child->text(0) == part)
                        {
                            found = child;
                            break;
                        }
                    }
                    current = found;
                }
            } else
            {
                // Plain text part (for group nodes like "Virtual Machines")
                if (!current)
                {
                    // Search top-level items
                    int topCount = ui->treeWidget->topLevelItemCount();
                    for (int i = 0; i < topCount; ++i)
                    {
                        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
                        if (item->text(0) == part)
                        {
                            current = item;
                            break;
                        }
                    }
                } else
                {
                    // Search children
                    int childCount = current->childCount();
                    QTreeWidgetItem* found = nullptr;
                    for (int i = 0; i < childCount; ++i)
                    {
                        QTreeWidgetItem* child = current->child(i);
                        if (child->text(0) == part)
                        {
                            found = child;
                            break;
                        }
                    }
                    current = found;
                }
            }

            if (!current)
            {
                break; // Path not found, stop searching
            }
        }

        // Expand the item if found
        if (current)
        {
            current->setExpanded(true);
        }
    }

    // Restore selection (matches C# RestoreSelectedNode)
    if (!m_savedSelectionType.isEmpty() && !m_savedSelectionRef.isEmpty())
    {
        // Search all top-level items
        int topCount = ui->treeWidget->topLevelItemCount();
        QTreeWidgetItem* itemToSelect = nullptr;

        for (int i = 0; i < topCount; ++i)
        {
            QTreeWidgetItem* rootItem = ui->treeWidget->topLevelItem(i);
            itemToSelect = findItemByTypeAndRef(m_savedSelectionType, m_savedSelectionRef, rootItem);

            if (!itemToSelect)
            {
                // Check root item itself
                QString rootType = rootItem->data(0, Qt::UserRole + 1).toString();
                QString rootRef = rootItem->data(0, Qt::UserRole).toString();
                if (rootType == m_savedSelectionType && rootRef == m_savedSelectionRef)
                {
                    itemToSelect = rootItem;
                }
            }

            if (itemToSelect)
            {
                break;
            }
        }

        if (itemToSelect)
        {
            ui->treeWidget->setCurrentItem(itemToSelect);
        }
    }

    // Re-enable selection signals
    m_suppressSelectionSignals = false;
}
