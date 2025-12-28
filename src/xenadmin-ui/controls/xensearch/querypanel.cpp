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
#include "xenlib.h"
#include "xencache.h"
#include "xen/xenobject.h"
#include "xen/vm.h"
#include "xen/host.h"
#include "xen/pool.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>

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
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    
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
    connect(this->updateThrottleTimer_, &QTimer::timeout,
            this, &QueryPanel::buildListInternal);
    
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
    if (this->search_)
    {
        delete this->search_;
        this->search_ = nullptr;
    }
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
    if (this->search_)
    {
        delete this->search_;
    }
    
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
    // This delegates to the Search object which applies QueryScope, QueryFilter, and Grouping
    TreeWidgetGroupAcceptor adapter(this, this);
    QList<IAcceptGroups*> adapters;
    adapters.append(&adapter);

    bool addedAny = this->search_->PopulateAdapters(this->m_conn, adapters);

    if (!addedAny)
    {
        this->addNoResultsRow();
    }
    
    this->restoreRowStates();
}

QTreeWidgetItem* QueryPanel::CreateRow(Grouping* grouping, const QString& objectType,
                                        const QVariantMap& objectData, XenConnection* conn)
{
    Q_UNUSED(grouping);
    
    if (objectType.isEmpty() || objectData.isEmpty())
        return nullptr;

    QString ref = objectData.value("opaque_ref").toString();
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
            // TODO: Add icon
        }
        else if (columnName == "cpu")
        {
            value = this->formatCpuUsage(xenObject);
        }
        else if (columnName == "memory")
        {
            value = this->formatMemoryUsage(xenObject);
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

QString QueryPanel::formatCpuUsage(XenObject* xenObject) const
{
    // TODO: Implement metrics reading
    Q_UNUSED(xenObject);
    return QString("--");
}

QString QueryPanel::formatMemoryUsage(XenObject* xenObject) const
{
    // TODO: Implement metrics reading
    Q_UNUSED(xenObject);
    return QString("--");
}

QString QueryPanel::formatDiskIO(XenObject* xenObject) const
{
    // TODO: Implement metrics reading
    Q_UNUSED(xenObject);
    return QString("--");
}

QString QueryPanel::formatNetworkIO(XenObject* xenObject) const
{
    // TODO: Implement metrics reading
    Q_UNUSED(xenObject);
    return QString("--");
}

QString QueryPanel::formatIpAddress(XenObject* xenObject) const
{
    // For VMs, get guest_metrics IP addresses
    VM* vm = dynamic_cast<VM*>(xenObject);
    if (vm)
    {
        // TODO: Implement VM IP address extraction from guest_metrics
        return QString("--");
    }
    
    // For hosts, get management interface address
    Host* host = dynamic_cast<Host*>(xenObject);
    if (host)
    {
        // TODO: Implement host IP from management interface
        return QString("--");
    }
    
    return QString();
}

QString QueryPanel::formatUptime(XenObject* xenObject) const
{
    // TODO: Calculate uptime from start_time or metrics
    Q_UNUSED(xenObject);
    return QString("--");
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
    // Show column chooser menu
    this->ShowChooseColumnsMenu(event->pos());
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
