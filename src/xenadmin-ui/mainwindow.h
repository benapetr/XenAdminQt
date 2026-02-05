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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>

#include "tabpages/basetabpage.h"
#include "navigation/navigationpane.h"
#include "xen/asyncoperation.h"
#include "selectionmanager.h"

class QMenu;

namespace Ui
{
    class MainWindow;
}
class QTreeWidgetItem;
class QTreeWidget;
class QProgressDialog;
class QVBoxLayout;
class QDockWidget;
class QAction;
class QProgressBar;
class QLabel;
class QToolButton;

class DebugWindow;
class AsyncOperation;

class PlaceholderWidget;
class SettingsManager;
class TitleBar;
class ConnectionProfile;
class ConsolePanel;
class CvmConsolePanel;

class NavigationHistory;
class Command;
class NavigationPane;
class XenCache;
class XenObject;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(QWidget* parent = nullptr);
        ~MainWindow();

        static MainWindow *instance();
        SelectionManager* GetSelectionManager() const { return m_selectionManager; }

        // Console panel access for snapshot screenshots (C#: Program.MainWindow.ConsolePanel)
        ConsolePanel* GetConsolePanel() const
        {
            return this->m_consolePanel;
        }

        QTreeWidget* GetServerTreeWidget() const;
        void ShowStatusMessage(const QString& message, int timeout = 0);
        void RefreshServerTree();

        // Navigation support for history (matches C# MainWindow methods called by History)
        void SelectObjectInTree(const QString& objectRef, const QString& objectType);
        void SetCurrentTab(const QString& tabName);

        // Update navigation button states (called by NavigationHistory)
        void UpdateHistoryButtons(bool canGoBack, bool canGoForward);
        void SaveServerList();

        //! True if there is any open connection
        bool IsConnected();

    private slots:
        void connectToServer();
        void showAbout();
        void showDebugWindow();
        void showXenCacheExplorer();
        void showOptions();
        void showImportWizard();
        void showExportWizard();
        void showNewNetworkWizard();
        void showNewStorageRepositoryWizard();

        // Console operations (Reference: C# MainWindow.cs line 2051)
        // TODO should be wired or removed
        void sendCADToConsole();

        void onConnectionStateChanged(XenConnection *conn, bool connected);
        void onCachePopulated();
        void onConnectionAdded(XenConnection* connection);
        void onConnectionTaskAdded(const QString& taskRef, const QVariantMap& taskData);
        void onConnectionTaskModified(const QString& taskRef, const QVariantMap& taskData);
        void onConnectionTaskDeleted(const QString& taskRef);
        void onTreeItemSelected();
        void showTreeContextMenu(const QPoint& position);
        void onTabChanged(int index);

        // Search functionality
        void onSearchTextChanged(const QString& text);
        void focusSearch();

        // NavigationPane events (matches C# MainWindow.navigationPane_* handlers)
        void onNavigationModeChanged(int mode);
        void onNotificationsSubModeChanged(int subMode);
        void onNavigationPaneTreeViewSelectionChanged();
        void onNavigationPaneTreeNodeRightClicked();

        // View menu filters (matches C# View menu toggles)
        void onViewTemplatesToggled(bool checked);
        void onViewCustomTemplatesToggled(bool checked);
        void onViewLocalStorageToggled(bool checked);
        void onViewShowHiddenObjectsToggled(bool checked);

        // Cache update handler for refreshing selected object
        void onCacheObjectChanged(XenConnection *connection, const QString& objectType, const QString& objectRef);

        // XenAPI Message handlers for alert system (matches C# MainWindow.cs line 993 - MessageCollectionChanged)
        void onMessageReceived(const QString& messageRef, const QVariantMap& messageData);
        void onMessageRemoved(const QString& messageRef);

        // SearchTabPage handlers (matches C# SearchPage double-click navigation)
        void onSearchTabPageObjectSelected(const QString& objectType, const QString& objectRef);

        // Operation progress tracking (matches C# History_CollectionChanged pattern)
        void onNewOperation(AsyncOperation* operation);
        void onOperationProgressChanged(int percent);
        void onOperationCompleted();
        void onOperationFailed(const QString& error);
        void onOperationCancelled();
        void finalizeOperation(AsyncOperation* operation, AsyncOperation::OperationState state, const QString& errorMessage = QString());

        // Toolbar navigation (matches C# MainWindow)
        void onBackButton();
        void onForwardButton();

        // Toolbar VM operation buttons (matches C# VM command execution)
        void onStartVMButton();
        void onShutDownButton();
        void onRebootButton();
        void onResumeButton();
        void onSuspendButton();
        void onPauseButton();
        void onUnpauseButton();
        void onForceShutdownButton();
        void onForceRebootButton();

        // Toolbar Host operation buttons
        void onPowerOnHostButton();

        // Toolbar Container operation buttons
        void onStartContainerButton();
        void onStopContainerButton();
        void onRestartContainerButton();
        void onPauseContainerButton();
        void onResumeContainerButton();

        // Menu action slots (Server menu)
        void onReconnectHost();
        void onDisconnectHost();
        void onConnectAllHosts();
        void onDisconnectAllHosts();
        void onRestartToolstack();
        void onReconnectAs();
        void onMaintenanceMode();
        void onServerProperties();

        // Menu action slots (Pool menu)
        void onNewPool();
        void onDeletePool();
        void onHAConfigure();
        void onHADisable();
        void onDisconnectPool();
        void onPoolProperties();
        void onRotatePoolSecret();
        void onJoinPool();
        void onEjectFromPool();
        void onAddServerToPool();

        // Menu action slots (VM menu)
        void onNewVM();
        void onStartShutdownVM();
        void onCopyVM();
        void onMoveVM();
        void onDeleteVM();
        void onDisableChangedBlockTracking();
        void onInstallTools();
        void onUninstallVM();
        void onVMProperties();
        void onTakeSnapshot();
        void onConvertToTemplate();
        void onExportVM();
        void refreshVmMenu();

        // Menu action slots (Template menu)
        void onNewVMFromTemplate();
        void onInstantVM();
        void onExportTemplate();
        void onDuplicateTemplate();
        void onDeleteTemplate();
        void onTemplateProperties();

        // Menu action slots (Storage menu)
        void onAddVirtualDisk();
        void onAttachVirtualDisk();
        void onDetachSR();
        void onReattachSR();
        void onForgetSR();
        void onDestroySR();
        void onRepairSR();
        void onSetDefaultSR();
        void onNewSR();
        void onStorageProperties();
        void onTrimSR();

        // Menu action slots (Network menu)
        void onNewNetwork();
        void on_actionCheck_for_leaks_triggered();

    private:
        static MainWindow *g_instance;

        void updateActions();
        void updateViewMenu(NavigationPane::NavigationMode mode);
        void applyViewSettingsToMenu();
        void onViewSettingsChanged();
        bool mixedVmTemplateSelection() const;
        void initializeToolbar();                                        // Initialize toolbar buttons matching C# MainWindow.Designer.cs
        void updateToolbarsAndMenus();                                   // Update both toolbar buttons and menu items from Commands (matches C# UpdateToolbars pattern)
        void disableAllOperationButtons();                               // Disable all VM/Host/Container operation buttons
        void disableAllOperationMenus();                                 // Disable all VM/Host operation menu items
        void updateContainerToolbarButtons(const QString& containerRef); // Enable/disable Container buttons

        // Helper methods for toolbar button handlers
        QString getSelectedObjectRef() const;
        QString getSelectedObjectName() const;
        QList<QSharedPointer<class VM>> getSelectedVMs() const;
        QList<QSharedPointer<class Host>> getSelectedHosts() const;

        // Search functionality
        void filterTreeItems(QTreeWidgetItem* item, const QString& searchText);
        bool itemMatchesSearch(QTreeWidgetItem* item, const QString& searchText);

        // Tab management - new system
        void showObjectTabs(QSharedPointer<XenObject> xen_obj);
        void showSearchPage(XenConnection *connection, class GroupingTag* groupingTag);
        void clearTabs();
        void updateTabPages(QSharedPointer<XenObject> xen_obj);
        void updatePlaceholderVisibility();

        // Get correct tab order for object type (C# GetNewTabPages equivalent)
        QList<BaseTabPage*> getNewTabPages(QSharedPointer<XenObject> xen_obj) const;

        // Settings management
        void saveSettings();
        void loadSettings();
        void restoreConnections();
        void updateConnectionProfileFromCache(XenConnection* connection, XenCache* cache);

    protected:
        void closeEvent(QCloseEvent* event) override;

        Ui::MainWindow* ui;

        // Debug window
        DebugWindow* m_debugWindow = nullptr;

        // Title bar widget
        TitleBar* m_titleBar = nullptr;

        // Console panels (matches C# MainWindow fields)
        ConsolePanel* m_consolePanel = nullptr;       // For VM/Host consoles
        CvmConsolePanel* m_cvmConsolePanel = nullptr; // For CVM consoles (XCP-ng)

        // Navigation pane (matches C# MainWindow.navigationPane)
        NavigationPane* m_navigationPane = nullptr;

        // Tab pages
        QList<BaseTabPage*> m_tabPages;

        // Notification pages (matches C# _notificationPages)
        // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 107
        QList<class NotificationsBasePage*> m_notificationPages;

        // Tab container (for notification pages and tabs)
        QWidget* m_tabContainer = nullptr;
        QVBoxLayout* m_tabContainerLayout = nullptr;

        // Search tab page (special handling for grouping tags)
        class SearchTabPage* m_searchTabPage;

        // Placeholder widget
        PlaceholderWidget* m_placeholderWidget;

        // Command map (matches C# CommandToolStripMenuItem pattern)
        QMap<QString, class Command*> m_commands;
        void initializeCommands();
        void connectMenuActions();
        void updateMenuItems();

        // Toolbar (matches C# ToolStrip)
        QToolBar* m_toolBar = nullptr;
        QToolButton* m_backButton = nullptr;    // QToolButton for dropdown menu support
        QToolButton* m_forwardButton = nullptr; // QToolButton for dropdown menu support
        QMenu* m_createVmFromTemplateMenu = nullptr;
        QMenu* m_resumeOnServerMenu = nullptr;
        QMenu* m_migrateToServerMenu = nullptr;
        QMenu* m_startOnServerMenu = nullptr;
        QMenu* m_addServerToPoolMenu = nullptr;
        QMenu* m_removeServerFromPoolMenu = nullptr;

        SelectionManager* m_selectionManager = nullptr;

        // Navigation history (matches C# History static class)
        NavigationHistory* m_navigationHistory = nullptr;

        // Status bar progress (matches C# MainWindow.statusProgressBar)
        QProgressBar* m_statusProgressBar;
        QLabel* m_statusLabel;
        AsyncOperation* m_statusBarAction; // Currently tracked action in status bar

        QSharedPointer<XenObject> m_currentObject;

        // Selection deduplication - prevent multiple API calls for same selection
        QString m_lastSelectedRef;

        // Tab memory - remember last selected tab per object (maps objectRef -> tab type)
        QHash<QString, BaseTabPage::Type> m_selectedTabs;
};

#endif // MAINWINDOW_H
