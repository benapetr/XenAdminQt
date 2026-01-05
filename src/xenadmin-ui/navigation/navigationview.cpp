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
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/query.h"
#include "xenadmin-ui/xensearch/treesearch.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "../settingsmanager.h"
#include "../connectionprofile.h"
#include <algorithm>
#include <QDebug>

using namespace XenSearch;

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

static bool isHiddenObject(const QVariantMap& record)
{
    const QString name = record.value("name_label").toString();
    if (name.startsWith("__gui__"))
        return true;

    const QVariantMap otherConfig = record.value("other_config").toMap();
    const QString hiddenFlag = otherConfig.value("hide_from_xencenter").toString().toLower();
    return hiddenFlag == "true";
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

static int poolChildSortRank(QTreeWidgetItem* item)
{
    if (!item)
        return 99;

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<QSharedPointer<XenObject>>())
        return 99;

    QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
    if (!obj)
        return 99;

    const QString type = obj->GetObjectType().toLower();
    if (type == "host")
        return 0;
    if (type == "sr")
        return 1;
    if (type == "vm")
        return 2;
    return 50;
}

static void sortPoolChildren(QTreeWidgetItem* parent)
{
    if (!parent || parent->childCount() == 0)
        return;

    QList<QTreeWidgetItem*> children;
    while (parent->childCount() > 0)
    {
        children.append(parent->takeChild(0));
    }

    std::stable_sort(children.begin(), children.end(),
              [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                  const int rankA = poolChildSortRank(a);
                  const int rankB = poolChildSortRank(b);
                  if (rankA != rankB)
                      return rankA < rankB;
                  return naturalCompare(a->text(0), b->text(0)) < 0;
              });

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
    this->m_treeBuilder = new MainWindowTreeBuilder(this->ui->treeWidget, this);

    SettingsManager& settings = SettingsManager::instance();
    this->m_viewFilters.showDefaultTemplates = settings.getDefaultTemplatesVisible();
    this->m_viewFilters.showUserTemplates = settings.getUserTemplatesVisible();
    this->m_viewFilters.showLocalStorage = settings.getLocalSRsVisible();
    this->m_viewFilters.showHiddenObjects = settings.getShowHiddenObjects();

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
    delete this->m_objectsSearch;
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

void NavigationView::setViewFilters(const ViewFilters& filters)
{
    this->m_viewFilters = filters;
    this->requestRefreshTreeView();
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

XenConnection* NavigationView::primaryConnection() const
{
    if (!this->m_connectionsManager)
        return nullptr;

    const QList<XenConnection*> connections = this->m_connectionsManager->getAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;
        if (connection->IsConnected() && connection->GetCache())
            return connection;
    }

    for (XenConnection* connection : connections)
    {
        if (connection && connection->GetCache())
            return connection;
    }

    return connections.isEmpty() ? nullptr : connections.first();
}

QueryScope* NavigationView::buildTreeSearchScope() const
{
    ObjectTypes types = Search::DefaultObjectTypes();
    types |= ObjectTypes::Pool;

    SettingsManager& settings = SettingsManager::instance();

    if (settings.getDefaultTemplatesVisible())
        types |= ObjectTypes::DefaultTemplate;

    if (settings.getUserTemplatesVisible())
        types |= ObjectTypes::UserTemplate;

    if (settings.getLocalSRsVisible())
        types |= ObjectTypes::LocalSR;

    return new QueryScope(types);
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
    QList<XenConnection*> connections;
    if (this->m_connectionsManager)
        connections = this->m_connectionsManager->getAllConnections();

    if (!this->m_connectionsManager || connections.isEmpty())
    {
        QTreeWidgetItem* root = new QTreeWidgetItem(this->ui->treeWidget);
        root->setText(0, "XenAdmin");
        root->setExpanded(true);

        QTreeWidgetItem* placeholder = new QTreeWidgetItem(root);
        placeholder->setText(0, connections.isEmpty()
            ? "Connect to a XenServer"
            : "(No connection manager available)");
        return;
    }

    Search* baseSearch = TreeSearch::DefaultTreeSearch();
    Search* effectiveSearch = baseSearch->AddFullTextFilter(this->searchText());

    QTreeWidgetItem* root = this->m_treeBuilder->createNewRootNode(
        effectiveSearch,
        MainWindowTreeBuilder::NavigationMode::Infrastructure,
        nullptr);

    this->m_treeBuilder->refreshTreeView(
        root,
        this->searchText(),
        MainWindowTreeBuilder::NavigationMode::Infrastructure);

    if (effectiveSearch != baseSearch)
        delete effectiveSearch;

}

void NavigationView::buildObjectsTree()
{
    if (!this->m_connectionsManager)
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
        placeholder->setText(0, "(No connection manager available)");
        return;
    }

    const QList<XenConnection*> connections = this->m_connectionsManager->getAllConnections();
    if (connections.isEmpty())
    {
        QTreeWidgetItem* placeholder = new QTreeWidgetItem(this->ui->treeWidget);
        placeholder->setText(0, "Connect to a XenServer");
        return;
    }

    if (!this->m_objectsSearch)
    {
        QueryScope* scope = this->buildTreeSearchScope();
        Query* query = new Query(scope, nullptr);
        this->m_objectsSearch = new Search(query, new TypeGrouping(nullptr), "Objects", QString(), false);
    }

    Search* baseSearch = this->m_objectsSearch;
    Search* effectiveSearch = baseSearch->AddFullTextFilter(this->searchText());

    QTreeWidgetItem* root = this->m_treeBuilder->createNewRootNode(
        effectiveSearch,
        MainWindowTreeBuilder::NavigationMode::Objects,
        nullptr);

    this->m_treeBuilder->refreshTreeView(
        root,
        this->searchText(),
        MainWindowTreeBuilder::NavigationMode::Objects);

    if (effectiveSearch != baseSearch)
        delete effectiveSearch;
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
