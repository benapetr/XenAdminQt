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

#ifndef QUERYPANEL_H
#define QUERYPANEL_H

#include <QTreeWidget>
#include <QMap>
#include <QString>
#include <QList>
#include <QMenu>
#include <QTimer>

class Search;
class Grouping;
class XenConnection;
class XenCache;
class XenObject;

/**
 * @brief QueryPanel - Grid view that displays search results
 *
 * C# Equivalent: XenAdmin.Controls.XenSearch.QueryPanel : GridView
 * C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs
 *
 * QueryPanel is the core result display widget for the search feature.
 * It extends QTreeWidget to provide a hierarchical grid with:
 * - Configurable columns (name, cpu, memory, disks, network, ip, ha, uptime, custom fields)
 * - Sorting by any column
 * - Grouping support (displays groups as parent nodes)
 * - Column chooser menu
 * - Metrics updating for live stats
 *
 * Key Differences from C#:
 * - C# uses custom GridView control, we use QTreeWidget
 * - C# has GridRow/GridItem classes, we use QTreeWidgetItem
 * - C# has complex cell rendering, we use Qt's standard delegates
 *
 * Architecture:
 * - Takes Search object via SetSearch()
 * - Iterates all XenConnections from ConnectionsManager
 * - For each connection, queries cache and applies QueryFilter
 * - Displays results grouped by Grouping (if specified)
 * - Updates metrics periodically via timer
 */
class QueryPanel : public QTreeWidget
{
    Q_OBJECT

    public:
        explicit QueryPanel(QWidget* parent = nullptr);
        ~QueryPanel() override;

        /**
         * @brief Set the search to display
         * @param search The Search object (takes ownership)
         */
        void SetSearch(Search* search);

        /**
         * @brief Get the current search
         */
        Search* GetSearch() const { return this->search_; }

        /**
         * @brief Get the current column sorting configuration
         * @return List of (column, ascending) pairs
         */
        QList<QPair<QString, bool>> GetSorting() const;

        /**
         * @brief Set the column sorting configuration
         */
        void SetSorting(const QList<QPair<QString, bool>>& sorting);

        /**
         * @brief Check if any sort column is a metric (cpu, memory, network, disks, uptime)
         */
        bool IsSortingByMetrics() const;

        /**
         * @brief Show the column chooser menu at specified point
         */
        void ShowChooseColumnsMenu(const QPoint& point);

        /**
         * @brief Get column chooser menu items (for embedding in other menus)
         */
        QList<QAction*> GetChooseColumnsMenu();

        /**
         * @brief Rebuild the result list from current search
         */
        void BuildList();

        /**
         * @brief Start metrics updating
         */
        static void PanelShown();

        /**
         * @brief Stop metrics updating
         */
        static void PanelHidden();

        /**
         * @brief Create a row for a specific object (called by TreeWidgetGroupAcceptor)
         * @param grouping The grouping algorithm (may be nullptr)
         * @param objectType The object type ("vm", "host", "pool", etc.)
         * @param objectData Full object data map
         * @param conn XenLib instance for resolving references
         * @return QTreeWidgetItem populated with object data
         */
        QTreeWidgetItem* CreateRow(Grouping* grouping, const QString& objectType,
                                   const QVariantMap& objectData, XenConnection *conn);

        /**
         * @brief Set the Xen Connection instance for cache access
         */
        void SetConnection(XenConnection* conn) { this->m_conn = conn; }

    signals:
        /**
         * @brief Emitted when search criteria changes (e.g., sort order)
         */
        void SearchChanged();

    protected:
        void contextMenuEvent(QContextMenuEvent* event) override;

    private slots:
        void onHeaderContextMenu(const QPoint& point);
        void onMetricsUpdateTimerTimeout();
        void onSortIndicatorChanged(int logicalIndex, Qt::SortOrder order);

    private:
        // Column management
        void setupColumns();
        void showColumn(const QString& column);
        void hideColumn(const QString& column);
        void toggleColumn(const QString& column);
        void removeColumn(const QString& column);
        bool isDefaultColumn(const QString& column) const;
        bool isMovableColumn(const QString& column) const;
        QString getI18nColumnName(const QString& column) const;
        int getDefaultColumnWidth(const QString& column) const;

        // Build result rows
        void buildListInternal();
        void addNoResultsRow();
        QTreeWidgetItem* createRow(Grouping* grouping, XenObject* xenObject, int indent);
        QTreeWidgetItem* createGroupRow(Grouping* grouping, const QVariant& group, int indent);
        void populateRow(QTreeWidgetItem* item, XenObject* xenObject);
        void populateGroupRow(QTreeWidgetItem* item, Grouping* grouping, const QVariant& group);

        // Cell content helpers
        QString formatCpuUsage(XenObject* xenObject, int* percentOut) const;
        QString formatMemoryUsage(XenObject* xenObject, int* percentOut) const;
        QString formatDiskIO(XenObject* xenObject) const;
        QString formatNetworkIO(XenObject* xenObject) const;
        QString formatIpAddress(XenObject* xenObject) const;
        QString formatUptime(XenObject* xenObject) const;
        QString formatHA(XenObject* xenObject) const;

        // State persistence
        void saveRowStates();
        void restoreRowStates();

    private:
        Search* search_;
        XenConnection* m_conn = nullptr;
        
        // Column configuration
        // Map of column name -> visible
        QMap<QString, bool> columns_;
        
        // Default columns (matching C# DEFAULT_COLUMNS)
        static const QStringList DEFAULT_COLUMNS;
        
        // Row state for expand/collapse
        QMap<QString, bool> expandedState_;
        
        // Metrics update timer
        static QTimer* metricsUpdateTimer_;
        static QList<XenObject*> metricsObjects_;
        
        // Update throttling
        bool updatePending_;
        QTimer* updateThrottleTimer_;
};

#endif // QUERYPANEL_H
