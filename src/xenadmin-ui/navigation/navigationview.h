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

#ifndef NAVIGATIONVIEW_H
#define NAVIGATIONVIEW_H

#include <QWidget>
#include <QTreeWidget>
#include <QTimer>
#include <QSharedPointer>
#include <QHash>
#include <QMetaObject>
#include "mainwindowtreebuilder.h"
#include "navigationpane.h" // For NavigationMode enum
#include "xensearch/search.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NavigationView;
}
QT_END_NAMESPACE

class XenConnection;
class XenCache;
class QueryScope;
namespace Xen
{
    class ConnectionsManager;
}
class Host;
class XenCache;
class XenConnection;

/**
 * @brief Navigation tree view widget
 *
 * Matches C# NavigationView control - hosts the main tree view with
 * Infrastructure/Objects/Organization modes, search support, selection
 * management, and drag/drop.
 *
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NavigationView.cs
 */
class NavigationView : public QWidget
{
    Q_OBJECT

    public:
        explicit NavigationView(QWidget* parent = nullptr);
        ~NavigationView();

        // Access to tree widget
        QTreeWidget* TreeWidget() const;

        // Public methods matching C# NavigationView interface
        void FocusTreeView();
        void RequestRefreshTreeView();
        void ResetSearchBox();

        // Navigation mode (matches C# NavigationView.NavigationMode property line 114)
        void SetNavigationMode(NavigationPane::NavigationMode mode);
        NavigationPane::NavigationMode GetNavigationMode() const
        {
            return this->m_navigationMode;
        }

        // Search mode control (matches C# NavigationView.InSearchMode property line 120)
        void SetInSearchMode(bool enabled);
        bool GetInSearchMode() const
        {
            return this->m_inSearchMode;
        }

        // Search text access (matches C# searchTextBox.Text)
        QString GetSearchText() const;
        void SetSearchText(const QString& text);

        struct ViewFilters
        {
            bool showDefaultTemplates = false;
            bool showUserTemplates = true;
            bool showLocalStorage = true;
            bool showHiddenObjects = false;
        };

        void SetViewFilters(const ViewFilters& filters);

    signals:
        // Events matching C# NavigationView
        void treeViewSelectionChanged();
        void treeNodeBeforeSelected();
        void treeNodeClicked();
        void treeNodeRightClicked();
        void treeViewRefreshed();
        void dragDropCommandActivated(const QString& commandKey);
        void connectToServerRequested();

        /**
         * Emitted when a folder is selected in the tree
         * @param folderPath Full path of the selected folder (e.g., "/MyFolder/SubFolder")
         * Used by SearchTabPage to filter search results by folder
         * C# equivalent: Custom implementation for folder filtering
         */
        void folderSelected(const QString& folderPath);

    private slots:
        void onSearchTextChanged(const QString& text);
        void onCacheObjectChanged(XenConnection *connection, const QString& type, const QString& ref);
        void onRefreshTimerTimeout();
        void onConnectionAdded(XenConnection* connection);
        void onConnectionRemoved(XenConnection* connection);
        void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);

    private:
        void buildInfrastructureTree();
        void buildObjectsTree();
        void buildOrganizationTree();
        void scheduleRefresh(); // Debounced refresh
        void connectCacheSignals(XenConnection* connection);
        void disconnectCacheSignals(XenConnection* connection);
        XenConnection* primaryConnection() const;
        QueryScope* buildTreeSearchScope() const;

        // Selection and expansion preservation (matches C# MainWindowTreeBuilder)
        void persistSelectionAndExpansion();
        void restoreSelectionAndExpansion();
        QTreeWidgetItem* findItemByTypeAndRef(const QString& type, const QString& ref, QTreeWidgetItem* parent = nullptr) const;
        void collectExpandedItems(QTreeWidgetItem* item, QStringList& expandedPaths) const;
        QString getItemPath(QTreeWidgetItem* item) const;

        Ui::NavigationView* ui;
        bool m_inSearchMode = false;
        NavigationPane::NavigationMode m_navigationMode = NavigationPane::Infrastructure;
        ViewFilters m_viewFilters;
        QTimer* m_refreshTimer; // Debounce timer for cache updates
        QHash<XenConnection*, QMetaObject::Connection> m_cacheChangedHandlers;
        QHash<XenConnection*, QMetaObject::Connection> m_cacheRemovedHandlers;

        // Grouping instances for Objects view (matches C# OrganizationViewObjects)
        class TypeGrouping* m_typeGrouping;
        MainWindowTreeBuilder* m_treeBuilder = nullptr;
        Search* m_objectsSearch = nullptr;

        // State preservation for tree refresh (matches C# PersistExpandedNodes/RestoreExpandedNodes)
        QString m_savedSelectionType;
        QString m_savedSelectionRef;
        QStringList m_savedExpandedPaths;
        bool m_suppressSelectionSignals = false; // Block itemSelectionChanged during rebuild
};

#endif // NAVIGATIONVIEW_H
