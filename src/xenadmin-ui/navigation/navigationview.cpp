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

#include "ui_navigationview.h"
#include "../iconmanager.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/vmhelpers.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "../settingsmanager.h"
#include "../connectionprofile.h"
#include <algorithm>
#include <QDebug>

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

QSharedPointer<Host> buildDisconnectedHostObject(XenConnection* connection, XenCache* cache)
{
    if (!connection)
        return QSharedPointer<Host>();

    const QString hostname = connection->GetHostname();
    const QString ref = connection->GetPort() == 443
        ? hostname
        : QString("%1:%2").arg(hostname).arg(connection->GetPort());
    QString displayName = hostname;

    const QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
    for (const ConnectionProfile& profile : profiles)
    {
        if (profile.hostname() == hostname && profile.port() == connection->GetPort())
        {
            displayName = profile.displayName();
            break;
        }
    }

    QVariantMap record;
    record["ref"] = ref;
    record["opaqueRef"] = ref;
    record["name_label"] = displayName;
    record["name_description"] = QString();
    record["hostname"] = hostname;
    record["address"] = hostname;
    record["enabled"] = false;

    if (cache)
        cache->Update("host", ref, record);

    return QSharedPointer<Host>(new Host(connection, ref));
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
    : QWidget(parent), ui(new Ui::NavigationView), m_refreshTimer(new QTimer(this)), m_typeGrouping(new TypeGrouping()) // Create TypeGrouping for Objects view
{
    this->ui->setupUi(this);

    // Setup debounce timer for cache updates (200ms delay)
    this->m_refreshTimer->setSingleShot(true);
    this->m_refreshTimer->setInterval(200);
    connect(this->m_refreshTimer, &QTimer::timeout, this, &NavigationView::onRefreshTimerTimeout);

    // Connect tree widget signals to our signals
    // Emit before-selected signal before selection changes (C# TreeView.BeforeSelect)
    connect(this->ui->treeWidget, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* previous) {
                Q_UNUSED(current);
                Q_UNUSED(previous);
                emit this->treeNodeBeforeSelected();
            });

    // Connect selection changed signal with suppression check
    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged,
            this, [this]() {
                // Don't emit signal during tree rebuild (matches C# ignoring selection changes during BeginUpdate)
                if (!this->m_suppressSelectionSignals)
                {
                    emit this->treeViewSelectionChanged();
                }
            });

    connect(this->ui->treeWidget, &QTreeWidget::itemClicked, this, &NavigationView::treeNodeClicked);

    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, &NavigationView::treeNodeRightClicked);

    // Connect search box (matches C# searchTextBox_TextChanged line 215)
    connect(this->ui->searchLineEdit, &QLineEdit::textChanged, this, &NavigationView::onSearchTextChanged);

    this->m_connectionsManager = Xen::ConnectionsManager::instance();
    connect(this->m_connectionsManager, &Xen::ConnectionsManager::connectionAdded, this, &NavigationView::onConnectionAdded);
    connect(this->m_connectionsManager, &Xen::ConnectionsManager::connectionRemoved, this, &NavigationView::onConnectionRemoved);

    const QList<XenConnection*> connections = this->m_connectionsManager->getAllConnections();
    for (XenConnection* connection : connections)
        this->connectCacheSignals(connection);
}

NavigationView::~NavigationView()
{
    delete this->m_typeGrouping;
    delete this->ui;
}

QTreeWidget* NavigationView::treeWidget() const
{
    return this->ui->treeWidget;
}

void NavigationView::focusTreeView()
{
    this->ui->treeWidget->setFocus();
}

void NavigationView::requestRefreshTreeView()
{
    // Matches C# TreeView BeginUpdate/EndUpdate pattern with PersistExpandedNodes/RestoreExpandedNodes
    // Suppress selection signals while rebuilding to avoid clearing selection in MainWindow
    this->m_suppressSelectionSignals = true;

    emit this->treeViewRefreshSuspended(); // Signal that we're about to rebuild

    this->ui->treeWidget->setUpdatesEnabled(false); // Suspend painting

    // Persist current selection and expanded nodes BEFORE rebuild (matches C# PersistExpandedNodes)
    this->persistSelectionAndExpansion();

    // Rebuild tree based on navigation mode
    switch (this->m_navigationMode)
    {
        case NavigationPane::Infrastructure:
            this->buildInfrastructureTree();
            break;
        case NavigationPane::Objects:
            this->buildObjectsTree();
            break;
        case NavigationPane::Tags:
        case NavigationPane::Folders:
        case NavigationPane::CustomFields:
        case NavigationPane::vApps:
            this->buildOrganizationTree();
            break;
        default:
            this->buildInfrastructureTree();
            break;
    }

    // Restore selection and expanded nodes AFTER rebuild (matches C# RestoreExpandedNodes)
    bool selectionRestored = !this->m_savedSelectionType.isEmpty() && !this->m_savedSelectionRef.isEmpty();
    this->restoreSelectionAndExpansion();

    this->ui->treeWidget->setUpdatesEnabled(true); // Resume painting

    // Re-enable selection signals and emit a single change notification if we restored selection
    this->m_suppressSelectionSignals = false;
    if (selectionRestored && this->ui->treeWidget->currentItem())
    {
        emit this->treeViewSelectionChanged();
    }

    emit this->treeViewRefreshResumed(); // Signal rebuild is complete
    emit this->treeViewRefreshed();
}

void NavigationView::resetSearchBox()
{
    this->ui->searchLineEdit->clear();
}

void NavigationView::setInSearchMode(bool enabled)
{
    // Matches C# NavigationView.InSearchMode property (line 120)
    this->m_inSearchMode = enabled;
    // TODO: Update UI based on search mode (show/hide search-related indicators)
}

void NavigationView::setNavigationMode(NavigationPane::NavigationMode mode)
{
    // Matches C# NavigationView.NavigationMode property (line 114)
    if (this->m_navigationMode != mode)
    {
        this->m_navigationMode = mode;
        // Rebuild tree with new mode
        this->requestRefreshTreeView();
    }
}

QString NavigationView::searchText() const
{
    return this->ui->searchLineEdit->text();
}

void NavigationView::setSearchText(const QString& text)
{
    this->ui->searchLineEdit->setText(text);
}

void NavigationView::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);

    // Only refresh for object types that appear in the tree
    // This avoids unnecessary refreshes for metrics, tasks, etc.
    if (type == "vm" || type == "host" || type == "pool" ||
        type == "sr" || type == "network" || type == "vbd" ||
        type == "vdi" || type == "vif")
    {
        this->scheduleRefresh();
    }
}

void NavigationView::scheduleRefresh()
{
    // Debounce: restart timer on each call
    // This coalesces multiple rapid cache updates into a single tree refresh
    this->m_refreshTimer->start();
}

void NavigationView::onRefreshTimerTimeout()
{
    // Timer fired - perform actual refresh
    this->requestRefreshTreeView();
}

void NavigationView::onConnectionAdded(XenConnection* connection)
{
    this->connectCacheSignals(connection);
    this->scheduleRefresh();
}

void NavigationView::onConnectionRemoved(XenConnection* connection)
{
    this->disconnectCacheSignals(connection);
    this->scheduleRefresh();
}

void NavigationView::connectCacheSignals(XenConnection* connection)
{
    if (!connection)
        return;

    XenCache* cache = connection->GetCache();
    if (!cache)
        return;

    if (!this->m_cacheChangedHandlers.contains(connection))
    {
        this->m_cacheChangedHandlers.insert(connection, connect(cache, &XenCache::objectChanged, this, &NavigationView::onCacheObjectChanged));
    }

    if (!this->m_cacheRemovedHandlers.contains(connection))
    {
        this->m_cacheRemovedHandlers.insert(connection, connect(cache, &XenCache::objectRemoved, this, &NavigationView::onCacheObjectChanged));
    }
}

void NavigationView::disconnectCacheSignals(XenConnection* connection)
{
    if (!connection)
        return;

    if (this->m_cacheChangedHandlers.contains(connection))
        disconnect(this->m_cacheChangedHandlers.take(connection));
    if (this->m_cacheRemovedHandlers.contains(connection))
        disconnect(this->m_cacheRemovedHandlers.take(connection));
}

void NavigationView::onSearchTextChanged(const QString& text)
{
    // Matches C# NavigationView.searchTextBox_TextChanged (line 215)
    // Trigger tree refresh with search filter
    // TODO: When tree builder is implemented, this will:
    // 1. Create filtered search: CurrentSearch.AddFullTextFilter(text)
    // 2. Call treeBuilder.RefreshTreeView(newRoot, text, mode)
    Q_UNUSED(text);
    this->requestRefreshTreeView();
}

void NavigationView::buildInfrastructureTree()
{
    // Matches C# MainWindowTreeBuilder.CreateNewRootNode for Infrastructure mode
    // This creates hierarchical tree: Pool → Host → VM → SR
    // Uses cache directly like C# connection.Cache.XenSearchableObjects

    this->ui->treeWidget->clear();

    if (!this->m_connectionsManager)
    {
        // No XenLib - show placeholder
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
        placeholder->setText(0, "(No connection manager available)");
        return;
    }

    Xen::ConnectionsManager* connMgr = this->m_connectionsManager;
    if (!connMgr)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
        placeholder->setText(0, "(Connection manager not initialized)");
        return;
    }

    // Get all connections (both connected and disconnected)
    QList<XenConnection*> connections = connMgr->getAllConnections();

    qDebug() << "NavigationView::buildInfrastructureTree: connections count=" << connections.size();

    if (connections.isEmpty())
    {
        // No connections - show connection prompt (matches C# behavior)
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
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

        qDebug() << "NavigationView::buildInfrastructureTree: connection"
                 << connection->GetHostname()
                 << "IsConnected=" << connection->IsConnected()
                 << "IsConnectedNewFlow=" << connection->IsConnectedNewFlow();

        // Check if connected
        if (!connection->IsConnected())
        {
            // Show disconnected connection at ROOT LEVEL (matches C# behavior)
            // C# creates a fake Host object with opaque_ref = HostnameWithPort (GroupAlg.cs line 97)
            // This allows disconnected servers to appear in tree with context menu
            QTreeWidgetItem* connItem = new QTreeWidgetItem(this->ui->treeWidget);
            XenCache* cache = connection->GetCache();
            QSharedPointer<Host> disconnectedHost = buildDisconnectedHostObject(connection, cache);
            connItem->setText(0, disconnectedHost ? disconnectedHost->GetName() : connection->GetHostname());
            // Store fake Host object (matches C# disconnected host behavior)
            connItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(disconnectedHost));
            connItem->setData(0, Qt::UserRole + 1, QString("disconnected_host")); // Object type for context menu

            // Set disconnected host icon (C# uses HostDisconnected icon)
            QIcon disconnectedIcon = IconManager::instance().getDisconnectedIcon();
            connItem->setIcon(0, disconnectedIcon);
            continue;
        }

        XenCache* cache = connection->GetCache();
        if (!cache)
            continue;

        // For connected servers, get pool data from CACHE (not API)
        qDebug() << "NavigationView::buildInfrastructureTree: cache counts"
                 << "hosts=" << cache->Count("host")
                 << "pools=" << cache->Count("pool")
                 << "vms=" << cache->Count("vm")
                 << "srs=" << cache->Count("sr");
        // This matches C# connection.Cache.Resolve pattern
        QList<QVariantMap> pools = cache->GetAllData("pool");

        if (pools.isEmpty())
        {
            // Connection has no pool data yet
            QTreeWidgetItem* connItem = new QTreeWidgetItem(this->ui->treeWidget);
            connItem->setText(0, QString("%1 (connecting...) ").arg(connection->GetHostname()));
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
                poolName = connection->GetHostname(); // Fallback to hostname
            }

            QTreeWidgetItem* poolItem = new QTreeWidgetItem(this->ui->treeWidget);
            poolItem->setText(0, poolName);
            
            // Store QSharedPointer<Pool> instead of separate ref+type
            QSharedPointer<Pool> poolObj = cache->ResolveObject<Pool>("pool", poolRef);
            // Upcast to base type so canConvert<QSharedPointer<XenObject>>() works
            poolItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(poolObj));
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
                
                // Store QSharedPointer<Host>
                QSharedPointer<Host> hostObj = cache->ResolveObject<Host>("host", hostRef);
                // Upcast to base type so canConvert<QSharedPointer<XenObject>>() works
                hostItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(hostObj));
                hostItem->setExpanded(true);                             // Expand host by default

                QIcon hostIcon = hostObj
                    ? IconManager::instance().getIconForObject(hostObj.data())
                    : IconManager::instance().getIconForHost(hostData);
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
                    
                    // Store QSharedPointer<SR>
                    QSharedPointer<SR> srObj = cache->ResolveObject<SR>("sr", srRef);
                    // Upcast to base type so canConvert<QSharedPointer<XenObject>>() works
                    srItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(srObj));

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
                QString vmHomeRef = VMHelpers::getVMHome(connection, vmData);

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
                
                // Store QSharedPointer<VM>
                QSharedPointer<VM> vmObj = cache->ResolveObject<VM>("vm", vmRef);
                // Upcast to base type so canConvert<QSharedPointer<XenObject>>() works
                vmItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(vmObj));

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
    sortTreeTopLevel(this->ui->treeWidget);
}

void NavigationView::buildObjectsTree()
{
    // Matches C# OrganizationViewObjects.Populate
    // This creates flat tree grouped by TYPE (not "All X" categories)
    // Groups objects by PropertyNames.type: Pools, Hosts, VMs, SRs, Networks
    // Uses cache directly like C# connection.Cache.XenSearchableObjects

    this->ui->treeWidget->clear();

    if (!this->m_connectionsManager)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
        placeholder->setText(0, "(No connection manager available)");
        return;
    }

    Xen::ConnectionsManager* connMgr = this->m_connectionsManager;
    if (!connMgr)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
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
        if (conn && !conn->IsConnected())
        {
            disconnectedConnections.append(conn);
        }
    }

    // Check if we have ANY connections at all
    if (allConnections.isEmpty())
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
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

    // Aggregate objects across all connections (each connection has its own cache)

    for (XenConnection* connection : allConnections)
    {
        if (!connection)
            continue;

        XenCache* cache = connection->GetCache();
        if (!cache)
            continue;

        // Pools from cache
        QList<QVariantMap> pools = cache->GetAllData("pool");
        for (const QVariantMap& pool : pools)
        {
            QString poolRef = pool.value("ref").toString();
            QString poolName = pool.value("name_label").toString();

            if (poolName.isEmpty())
                continue;

            if (!poolsGroup)
            {
                poolsGroup = new QTreeWidgetItem(this->ui->treeWidget);
                poolsGroup->setText(0, "Pools");
                poolsGroup->setExpanded(true);

                GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("pool"));
                poolsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));
            }

            QTreeWidgetItem* poolItem = new QTreeWidgetItem(poolsGroup);
            poolItem->setText(0, poolName);

            QSharedPointer<Pool> poolObj = cache->ResolveObject<Pool>("pool", poolRef);
            poolItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(poolObj));

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

            if (!hostsGroup)
            {
                hostsGroup = new QTreeWidgetItem(this->ui->treeWidget);
                hostsGroup->setText(0, "Hosts");
                hostsGroup->setExpanded(true);

                GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("host"));
                hostsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));
            }

            QTreeWidgetItem* hostItem = new QTreeWidgetItem(hostsGroup);
            hostItem->setText(0, hostName);

            QSharedPointer<Host> hostObj = cache->ResolveObject<Host>("host", hostRef);
            hostItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(hostObj));

            QIcon hostIcon = hostObj
                ? IconManager::instance().getIconForObject(hostObj.data())
                : IconManager::instance().getIconForHost(hostData);
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

            if (isControlDomain || isSnapshot)
                continue;

            QString vmName = vmData.value("name_label").toString();
            if (vmName.isEmpty())
                continue;

            if (isTemplate)
            {
                if (!templatesGroup)
                {
                    templatesGroup = new QTreeWidgetItem(this->ui->treeWidget);
                    templatesGroup->setText(0, "Templates");
                    templatesGroup->setExpanded(false);

                    GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("template"));
                    templatesGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));
                }

                QTreeWidgetItem* templateItem = new QTreeWidgetItem(templatesGroup);
                templateItem->setText(0, vmName);

                QSharedPointer<VM> vmObj = cache->ResolveObject<VM>("vm", vmRef);
                templateItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(vmObj));

                QIcon templateIcon = IconManager::instance().getIconForVM(vmData);
                templateItem->setIcon(0, templateIcon);
            } else
            {
                if (!vmsGroup)
                {
                    vmsGroup = new QTreeWidgetItem(this->ui->treeWidget);
                    vmsGroup->setText(0, "VMs");
                    vmsGroup->setExpanded(true);

                    GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("vm"));
                    vmsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));
                }

                QTreeWidgetItem* vmItem = new QTreeWidgetItem(vmsGroup);
                vmItem->setText(0, vmName);

                QSharedPointer<VM> vmObj = cache->ResolveObject<VM>("vm", vmRef);
                vmItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(vmObj));

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

            if (!storageGroup)
            {
                storageGroup = new QTreeWidgetItem(this->ui->treeWidget);
                storageGroup->setText(0, "Storage");
                storageGroup->setExpanded(true);

                GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("sr"));
                storageGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));
            }

            QTreeWidgetItem* srItem = new QTreeWidgetItem(storageGroup);
            srItem->setText(0, srName);

            QSharedPointer<SR> srObj = cache->ResolveObject<SR>("sr", srRef);
            srItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(srObj));

            QIcon srIcon = IconManager::instance().getIconForSR(srData);
            srItem->setIcon(0, srIcon);
        }
    }

    // Disconnected Servers group (C# ObjectTypes.DisconnectedServer)
    // C# creates fake Host objects for disconnected connections (GroupAlg.cs line 93-100)
    // These appear in Objects view under "Disconnected Servers" group
    if (!disconnectedConnections.isEmpty())
    {
        // Create Disconnected Servers group
        disconnectedHostsGroup = new QTreeWidgetItem(this->ui->treeWidget);
        disconnectedHostsGroup->setText(0, "Disconnected servers"); // Matches C# Messages.DISCONNECTED_SERVERS
        disconnectedHostsGroup->setExpanded(true);

        // Attach GroupingTag (group value is "disconnected_host")
        GroupingTag* tag = new GroupingTag(this->m_typeGrouping, QVariant(), QVariant("disconnected_host"));
        disconnectedHostsGroup->setData(0, Qt::UserRole + 3, QVariant::fromValue(tag));

        // Set disconnected group icon
        QIcon disconnectedGroupIcon = IconManager::instance().getDisconnectedIcon();
        disconnectedHostsGroup->setIcon(0, disconnectedGroupIcon);

        // Add each disconnected connection as a fake host
        for (XenConnection* conn : disconnectedConnections)
        {
            QTreeWidgetItem* disconnectedItem = new QTreeWidgetItem(disconnectedHostsGroup);
            XenCache* cache = conn ? conn->GetCache() : nullptr;
            QSharedPointer<Host> disconnectedHost = buildDisconnectedHostObject(conn, cache);
            disconnectedItem->setText(0, disconnectedHost ? disconnectedHost->GetName() : conn->GetHostname());
            disconnectedItem->setData(0, Qt::UserRole, QVariant::fromValue<QSharedPointer<XenObject>>(disconnectedHost));
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

    this->ui->treeWidget->clear();

    QString viewName;
    switch (this->m_navigationMode)
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

    QTreeWidgetItem* root = new QTreeWidgetItem(this->ui->treeWidget);
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
        QVariant data = current->data(0, Qt::UserRole);
        QString type;
        QString ref;
        
        // Extract type and ref from QSharedPointer<XenObject>
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
        {
            type = obj->GetObjectType();
            ref = obj->OpaqueRef();
        }

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
            QString path = this->getItemPath(child);
            if (!path.isEmpty())
            {
                expandedPaths.append(path);
            }
        }

        // Recurse for children
        if (child->childCount() > 0)
        {
            this->collectExpandedItems(child, expandedPaths);
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
        
        // Extract XenObject from UserRole
        QVariant data = child->data(0, Qt::UserRole);
        
        // Check if it's a XenObject
        if (data.canConvert<QSharedPointer<XenObject>>())
        {
            QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
            if (obj && obj->GetObjectType() == type && obj->OpaqueRef() == ref)
            {
                return child;
            }
        }

        // Recurse for children
        QTreeWidgetItem* found = this->findItemByTypeAndRef(type, ref, child);
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
    QTreeWidgetItem* selectedItem = this->ui->treeWidget->currentItem();
    if (selectedItem)
    {
        QVariant data = selectedItem->data(0, Qt::UserRole);
        
        // Check if it's a XenObject
        if (data.canConvert<QSharedPointer<XenObject>>())
        {
            QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
            if (obj)
            {
                this->m_savedSelectionType = obj->GetObjectType();
                this->m_savedSelectionRef = obj->OpaqueRef();
            } else
            {
                this->m_savedSelectionType.clear();
                this->m_savedSelectionRef.clear();
            }
        } else
        {
            // Not a XenObject (e.g., disconnected server or GroupingTag)
            this->m_savedSelectionType.clear();
            this->m_savedSelectionRef.clear();
        }
    } else
    {
        this->m_savedSelectionType.clear();
        this->m_savedSelectionRef.clear();
    }

    // Save expanded nodes (matches C# PersistExpandedNodes)
    this->m_savedExpandedPaths.clear();

    // Check if root nodes are expanded
    int topLevelCount = this->ui->treeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelCount; ++i)
    {
        QTreeWidgetItem* rootItem = this->ui->treeWidget->topLevelItem(i);
        if (rootItem->isExpanded())
        {
            QString path = this->getItemPath(rootItem);
            if (!path.isEmpty())
            {
                this->m_savedExpandedPaths.append(path);
            }
        }

        // Collect expanded children
        this->collectExpandedItems(rootItem, this->m_savedExpandedPaths);
    }
}

void NavigationView::restoreSelectionAndExpansion()
{
    // Block selection signals during restore
    this->m_suppressSelectionSignals = true;

    // Restore expanded nodes (matches C# RestoreExpandedNodes)
    for (const QString& path : this->m_savedExpandedPaths)
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
                        QVariant data = item->data(0, Qt::UserRole);
                        
                        QString itemType;
                        QString itemRef;
                        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
                        if (obj)
                        {
                            itemType = obj->GetObjectType();
                            itemRef = obj->OpaqueRef();
                        }

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
                        QVariant data = child->data(0, Qt::UserRole);
                        
                        QString itemType;
                        QString itemRef;
                        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
                        if (obj)
                        {
                            itemType = obj->GetObjectType();
                            itemRef = obj->OpaqueRef();
                        }

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
                    int topCount = this->ui->treeWidget->topLevelItemCount();
                    for (int i = 0; i < topCount; ++i)
                    {
                        QTreeWidgetItem* item = this->ui->treeWidget->topLevelItem(i);
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
    if (!this->m_savedSelectionType.isEmpty() && !this->m_savedSelectionRef.isEmpty())
    {
        // Search all top-level items
        int topCount = this->ui->treeWidget->topLevelItemCount();
        QTreeWidgetItem* itemToSelect = nullptr;

        for (int i = 0; i < topCount; ++i)
        {
            QTreeWidgetItem* rootItem = this->ui->treeWidget->topLevelItem(i);
            itemToSelect = this->findItemByTypeAndRef(this->m_savedSelectionType, this->m_savedSelectionRef, rootItem);

            if (!itemToSelect)
            {
                // Check root item itself
                QVariant data = rootItem->data(0, Qt::UserRole);
                QString rootType;
                QString rootRef;
                QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
                if (obj)
                {
                    rootType = obj->GetObjectType();
                    rootRef = obj->OpaqueRef();
                }
                
                if (rootType == this->m_savedSelectionType && rootRef == this->m_savedSelectionRef)
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
            this->ui->treeWidget->setCurrentItem(itemToSelect);
        }
    }

    // Re-enable selection signals
    this->m_suppressSelectionSignals = false;
}
