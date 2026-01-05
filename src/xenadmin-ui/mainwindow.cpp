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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialogs/addserverdialog.h"
#include "dialogs/debugwindow.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/optionsdialog.h"
#include "dialogs/warningdialogs/closexencenterwarningdialog.h"
#include "tabpages/generaltabpage.h"
#include "tabpages/vmstoragetabpage.h"
#include "tabpages/srstoragetabpage.h"
#include "tabpages/physicalstoragetabpage.h"
#include "tabpages/networktabpage.h"
#include "tabpages/nicstabpage.h"
#include "tabpages/consoletabpage.h"
#include "tabpages/cvmconsoletabpage.h"
#include "tabpages/snapshotstabpage.h"
#include "tabpages/performancetabpage.h"
#include "tabpages/bootoptionstab.h"
#include "tabpages/memorytabpage.h"
#include "tabpages/searchtabpage.h"
#include "tabpages/alertsummarypage.h"
#include "tabpages/eventspage.h"
#include "alerts/alertmanager.h"
#include "alerts/messagealert.h"
#include "ConsoleView/ConsolePanel.h"
#include "placeholderwidget.h"
#include "settingsmanager.h"
#include "connectionprofile.h"
#include "navigation/navigationhistory.h"
#include "navigation/navigationpane.h"
#include "navigation/navigationview.h"
#include "network/xenconnectionui.h"
#include "xen/network/certificatemanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/sr.h"
#include "metricupdater.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "operations/operationmanager.h"
#include "actions/meddlingaction.h"
#include "commands/contextmenubuilder.h"

// Polymorphic commands (handle VMs and Hosts)
#include "commands/shutdowncommand.h"
#include "commands/rebootcommand.h"

// Host commands
#include "commands/host/reconnecthostcommand.h"
#include "commands/host/disconnecthostcommand.h"
#include "commands/host/connectallhostscommand.h"
#include "commands/host/disconnectallhostscommand.h"
#include "commands/host/restarttoolstackcommand.h"
#include "commands/host/hostreconnectascommand.h"
#include "commands/host/reboothostcommand.h"
#include "commands/host/shutdownhostcommand.h"
#include "commands/host/poweronhostcommand.h"
#include "commands/host/hostmaintenancemodecommand.h"
#include "commands/host/hostpropertiescommand.h"

// Pool commands
#include "commands/pool/newpoolcommand.h"
#include "commands/pool/deletepoolcommand.h"
#include "commands/pool/haconfigurecommand.h"
#include "commands/pool/hadisablecommand.h"
#include "commands/pool/poolpropertiescommand.h"
#include "commands/pool/joinpoolcommand.h"
#include "commands/pool/ejecthostfrompoolcommand.h"

// VM commands
#include "commands/vm/importvmcommand.h"
#include "commands/vm/exportvmcommand.h"
#include "commands/vm/startvmcommand.h"
#include "commands/vm/stopvmcommand.h"
#include "commands/vm/restartvmcommand.h"
#include "commands/vm/resumevmcommand.h"
#include "commands/vm/suspendvmcommand.h"
#include "commands/vm/pausevmcommand.h"
#include "commands/vm/unpausevmcommand.h"
#include "commands/vm/forceshutdownvmcommand.h"
#include "commands/vm/forcerebootvmcommand.h"
#include "commands/vm/migratevmcommand.h"
#include "commands/vm/clonevmcommand.h"
#include "commands/vm/vmlifecyclecommand.h"
#include "commands/vm/copyvmcommand.h"
#include "commands/vm/movevmcommand.h"
#include "commands/vm/installtoolscommand.h"
#include "commands/vm/uninstallvmcommand.h"
#include "commands/vm/deletevmcommand.h"
#include "commands/vm/convertvmtotemplatecommand.h"
#include "commands/vm/newvmcommand.h"
#include "commands/vm/vmpropertiescommand.h"
#include "commands/vm/takesnapshotcommand.h"
#include "commands/vm/deletesnapshotcommand.h"
#include "commands/vm/reverttosnapshotcommand.h"

// Template commands
#include "commands/template/createvmfromtemplatecommand.h"
#include "commands/template/newvmfromtemplatecommand.h"
#include "commands/template/instantvmfromtemplatecommand.h"
#include "commands/template/copytemplatecommand.h"
#include "commands/template/deletetemplatecommand.h"
#include "commands/template/exporttemplatecommand.h"

// Storage commands
#include "commands/storage/newsrcommand.h"
#include "commands/storage/repairsrcommand.h"
#include "commands/storage/detachsrcommand.h"
#include "commands/storage/setdefaultsrcommand.h"
#include "commands/storage/storagepropertiescommand.h"
#include "commands/storage/addvirtualdiskcommand.h"
#include "commands/storage/attachvirtualdiskcommand.h"
#include "commands/storage/reattachsrcommand.h"
#include "commands/storage/forgetsrcommand.h"
#include "commands/storage/destroysrcommand.h"

// Network commands
#include "commands/network/newnetworkcommand.h"
#include "commands/network/networkpropertiescommand.h"

#include "actions/meddlingactionmanager.h"
#include <QApplication>
#include <QMessageBox>
#include <QProgressBar>
#include <QLabel>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QProgressDialog>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <QToolButton>
#include <QToolBar>
#include <QDebug>
#include <QCloseEvent>
#include <QShortcut>
#include <QLineEdit>
#include <QDateTime>
#include <QDockWidget>
#include "titlebar.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_debugWindow(nullptr), m_titleBar(nullptr),
      m_consolePanel(nullptr), m_cvmConsolePanel(nullptr), m_navigationPane(nullptr), m_tabContainer(nullptr), m_tabContainerLayout(nullptr),
      m_navigationHistory(nullptr), m_poolsTreeItem(nullptr), m_hostsTreeItem(nullptr), m_vmsTreeItem(nullptr), m_storageTreeItem(nullptr),
      m_currentObjectType(""), m_currentObjectRef("")
{
    this->ui->setupUi(this);

    // Set application icon
    this->setWindowIcon(QIcon(":/icons/app.ico"));

    // Create title bar and integrate it with tab widget
    // We need to wrap the tab widget in a container to add the title bar above it
    this->m_titleBar = new TitleBar(this);

    // Get the splitter and the index where mainTabWidget is located
    QSplitter* splitter = this->ui->centralSplitter;
    int tabWidgetIndex = splitter->indexOf(this->ui->mainTabWidget);

    // Remove the tab widget from the splitter temporarily
    this->ui->mainTabWidget->setParent(nullptr);

    // Create a container widget with vertical layout
    QWidget* tabContainer = new QWidget(this);
    QVBoxLayout* containerLayout = new QVBoxLayout(tabContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // Add title bar and tab widget to the container
    containerLayout->addWidget(this->m_titleBar);
    containerLayout->addWidget(this->ui->mainTabWidget);

    // Insert the container back into the splitter at the same position
    splitter->insertWidget(tabWidgetIndex, tabContainer);
    
    // Store the tab container for later use with notification pages
    this->m_tabContainer = tabContainer;
    this->m_tabContainerLayout = containerLayout;

    // Status bar widgets (matches C# MainWindow.statusProgressBar and statusLabel)
    this->m_statusLabel = new QLabel(this);
    this->m_statusProgressBar = new QProgressBar(this);
    this->m_statusProgressBar->setMaximumWidth(200);
    this->m_statusProgressBar->setVisible(false); // Hidden by default
    this->m_statusBarAction = nullptr;

    this->ui->statusbar->addPermanentWidget(this->m_statusLabel);
    this->ui->statusbar->addPermanentWidget(this->m_statusProgressBar);

    XenCertificateManager::instance()->setValidationPolicy(true, false); // Allow self-signed, not expired

    // Connect to OperationManager for progress tracking (matches C# History_CollectionChanged)
    connect(OperationManager::instance(), &OperationManager::newOperation, this, &MainWindow::onNewOperation);

    this->m_titleBar->clear(); // Start with empty title

    // Wire UI to ConnectionsManager (C# model), keep XenLib only as active-connection facade.
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    connect(connMgr, &Xen::ConnectionsManager::connectionAdded, this, &MainWindow::onConnectionAdded);

    // Connect XenAPI message signals to AlertManager for alert system
    // C# Reference: MainWindow.cs line 703 - connection.Cache.RegisterCollectionChanged<Message>

    // Get NavigationPane from UI (matches C# MainWindow.navigationPane)
    this->m_navigationPane = this->ui->navigationPane;

    // Connect NavigationPane events (matches C# MainWindow.navigationPane_* event handlers)
    connect(this->m_navigationPane, &NavigationPane::navigationModeChanged, this, &MainWindow::onNavigationModeChanged);
    connect(this->m_navigationPane, &NavigationPane::notificationsSubModeChanged, this, &MainWindow::onNotificationsSubModeChanged);
    connect(this->m_navigationPane, &NavigationPane::treeViewSelectionChanged, this, &MainWindow::onNavigationPaneTreeViewSelectionChanged);
    connect(this->m_navigationPane, &NavigationPane::treeNodeRightClicked, this, &MainWindow::onNavigationPaneTreeNodeRightClicked);
    connect(this->m_navigationPane, &NavigationPane::connectToServerRequested, this, &MainWindow::connectToServer);

    // Get tree widget from NavigationPane's NavigationView for legacy code compatibility
    // TODO: Refactor to use NavigationPane API instead of direct tree access
    auto* navView = this->m_navigationPane->GetNavigationView();
    if (navView)
    {
        QTreeWidget* treeWidget = navView->treeWidget();
        if (treeWidget)
        {
            // Enable context menus
            treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
        }
    }

    // Connect tab change signal to notify tab pages
    connect(this->ui->mainTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // Create Ctrl+F shortcut for search
    QShortcut* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, &MainWindow::focusSearch);

    // Set splitter proportions
    QList<int> sizes = {250, 550};
    this->ui->centralSplitter->setSizes(sizes);

    // Initialize debug window and message handler
    this->m_debugWindow = new DebugWindow(this);
    DebugWindow::installDebugHandler();

    // Create console panels (matches C# MainWindow.cs lines 85-86)
    // - ConsolePanel for VM/Host consoles (shown in TabPageConsole)
    // - CvmConsolePanel for SR driver domain consoles (shown in TabPageCvmConsole)
    this->m_consolePanel = new ConsolePanel(this);
    this->m_cvmConsolePanel = new CvmConsolePanel(this);

    // Initialize tab pages (without parent - they will be parented to QTabWidget when added)
    // Order matches C# MainWindow.Designer.cs lines 326-345
    // Note: We don't implement all C# tabs yet (Home, Ballooning, HA, WLB, AD, GPU, Docker, USB)
    this->m_tabPages.append(new GeneralTabPage()); // C#: TabPageGeneral
    // Ballooning - not implemented yet
    // Console tabs are added below after initialization
    this->m_tabPages.append(new VMStorageTabPage()); // C#: TabPageStorage (Virtual Disks for VMs)
    this->m_tabPages.append(new SrStorageTabPage()); // C#: TabPageSR (for SRs)
    this->m_tabPages.append(new PhysicalStorageTabPage()); // C#: TabPagePhysicalStorage (for Hosts/Pools)
    this->m_tabPages.append(new NetworkTabPage());     // C#: TabPageNetwork
    this->m_tabPages.append(new NICsTabPage());        // C#: TabPageNICs
    this->m_tabPages.append(new PerformanceTabPage()); // C#: TabPagePerformance
    // HA - not implemented yet
    this->m_tabPages.append(new SnapshotsTabPage()); // C#: TabPageSnapshots
    // WLB - not implemented yet
    // AD - not implemented yet
    // GPU - not implemented yet
    // Docker pages - not implemented yet
    // USB - not implemented yet
    this->m_tabPages.append(new MemoryTabPage());  // Custom Qt tab (not in C# as separate tab)
    this->m_tabPages.append(new BootOptionsTab()); // Custom Qt tab (not in C# as separate tab)

    // Create console tab and wire up ConsolePanel (matches C# AddTabContents line 186)
    ConsoleTabPage* consoleTab = new ConsoleTabPage();
    consoleTab->setConsolePanel(this->m_consolePanel);
    this->m_tabPages.append(consoleTab);

    // Create CVM console tab and wire up CvmConsolePanel (matches C# AddTabContents line 187)
    CvmConsoleTabPage* cvmConsoleTab = new CvmConsoleTabPage();
    cvmConsoleTab->setConsolePanel(this->m_cvmConsolePanel);
    this->m_tabPages.append(cvmConsoleTab);

    // Create search tab page (matches C# TabPageSearch)
    // This is shown when clicking grouping tags in Objects view
    SearchTabPage* searchTab = new SearchTabPage();
    this->m_searchTabPage = searchTab;

    this->m_tabPages.append(searchTab);

    // Connect SearchTabPage objectSelected signal to navigate to that object
    connect(searchTab, &SearchTabPage::objectSelected, this, &MainWindow::onSearchTabPageObjectSelected);

    // Initialize notification pages (matches C# _notificationPages initialization)
    // C# Reference: xenadmin/XenAdmin/MainWindow.Designer.cs lines 304-306
    // In C#: splitContainer1.Panel2.Controls.Add(this.alertPage);
    // These pages are shown in the same area as tabs (Panel2 of the main splitter)
    AlertSummaryPage* alertPage = new AlertSummaryPage(this);
    this->m_notificationPages.append(alertPage);

    EventsPage* eventsPage = new EventsPage(this);
    this->m_notificationPages.append(eventsPage);

    // Add notification pages to the tab container (same area as tabs)
    // They will be shown/hidden based on notifications sub-mode selection
    // In C#, all pages are added to Panel2 and visibility is controlled
    this->m_tabContainerLayout->addWidget(alertPage);
    this->m_tabContainerLayout->addWidget(eventsPage);
    alertPage->hide();
    eventsPage->hide();

    // Create placeholder widget
    this->m_placeholderWidget = new PlaceholderWidget();

    // Initialize toolbar (matches C# MainWindow.Designer.cs ToolStrip)
    this->initializeToolbar();

    // Initialize commands (matches C# SelectionManager.BindTo pattern)
    this->initializeCommands();
    this->connectMenuActions();
    this->updateToolbarsAndMenus(); // Set initial toolbar and menu states (matches C# UpdateToolbars)

    // Initialize navigation history (matches C# History static class)
    this->m_navigationHistory = new NavigationHistory(this, this);

    qDebug() << "XenAdmin Qt: Application initialized successfully";
    qInfo() << "XenAdmin Qt: Debug console available via View -> Debug Console (F12)";

    this->updateActions();

    // Show placeholder initially since we have no tabs yet
    this->updatePlaceholderVisibility();

    // Load saved settings
    this->loadSettings();

    // Restore saved connections
    this->restoreConnections();
}

MainWindow::~MainWindow()
{
    // Cleanup debug handler
    DebugWindow::uninstallDebugHandler();

    // Clean up tab pages
    qDeleteAll(this->m_tabPages);
    this->m_tabPages.clear();

    delete this->ui;
}

void MainWindow::updateActions()
{
    bool is_connected = this->IsConnected();

    // Actions available only when connected
    this->ui->disconnectAction->setEnabled(is_connected);
    this->ui->importAction->setEnabled(is_connected);
    this->ui->exportAction->setEnabled(is_connected);
    this->ui->newNetworkAction->setEnabled(is_connected);
    this->ui->newStorageRepositoryAction->setEnabled(is_connected);

    // Connect action available only when not connected
    this->ui->connectAction->setEnabled(!is_connected);

    // Update toolbar and menu states (matches C# UpdateToolbars)
    this->updateToolbarsAndMenus();
}

void MainWindow::connectToServer()
{
    AddServerDialog dialog(nullptr, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString serverInput = dialog.serverInput();
    QString hostname = serverInput;
    int port = 443;
    const int lastColon = serverInput.lastIndexOf(':');
    if (lastColon > 0 && lastColon < serverInput.size() - 1)
    {
        bool ok = false;
        int parsedPort = serverInput.mid(lastColon + 1).toInt(&ok);
        if (ok)
        {
            hostname = serverInput.left(lastColon).trimmed();
            port = parsedPort;
        }
    }

    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
        return;

    XenConnection* connection = new XenConnection(nullptr);
    connMgr->addConnection(connection);

    connection->SetHostname(hostname);
    connection->SetPort(port);
    connection->SetUsername(dialog.username());
    connection->SetPassword(dialog.password());
    connection->setExpectPasswordIsCorrect(false);
    connection->setFromDialog(true);

    XenConnectionUI::BeginConnect(connection, true, this, false);
}

void MainWindow::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::showDebugWindow()
{
    if (this->m_debugWindow)
    {
        this->m_debugWindow->show();
        this->m_debugWindow->raise();
        this->m_debugWindow->activateWindow();
    }
}

/**
 * @brief Send Ctrl+Alt+Delete to active console
 * Reference: C# MainWindow.cs lines 2051-2054
 *
 * This sends Ctrl+Alt+Del to the currently active console (VNC or RDP).
 * Useful for logging into Windows VMs that require Ctrl+Alt+Del.
 *
 * To wire this up to a menu action:
 * 1. Add menu action in mainwindow.ui (e.g., "actionSendCAD" under VM menu)
 * 2. Connect signal: connect(ui->actionSendCAD, &QAction::triggered, this, &MainWindow::sendCADToConsole);
 */
void MainWindow::sendCADToConsole()
{
    if (this->m_consolePanel)
    {
        this->m_consolePanel->SendCAD();
    }
}

void MainWindow::showOptions()
{
    // Matches C# MainWindow showing OptionsDialog
    OptionsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        // Settings were saved, might need to apply some changes
        // (In C# this happens in OptionsDialog.okButton_Click)
    }
}

void MainWindow::showImportWizard()
{
    qDebug() << "MainWindow: Showing Import Wizard";

    // Use the ImportVMCommand to show the wizard
    ImportVMCommand importCmd(this);
    importCmd.Run();
}

void MainWindow::showExportWizard()
{
    qDebug() << "MainWindow: Showing Export Wizard";

    ExportVMCommand exportCmd(this);
    exportCmd.Run();
}

void MainWindow::showNewNetworkWizard()
{
    qDebug() << "MainWindow: Showing New Network Wizard";

    NewNetworkCommand newNetCmd(this);
    newNetCmd.Run();
}

void MainWindow::showNewStorageRepositoryWizard()
{
    qDebug() << "MainWindow: Showing New Storage Repository Wizard";

    NewSRCommand newSRCmd(this);
    newSRCmd.Run();
}

void MainWindow::onConnectionStateChanged(XenConnection *conn, bool connected)
{
    this->updateActions();

    if (connected)
    {
        qDebug() << "XenAdmin Qt: Successfully connected to Xen server";
        this->ui->statusbar->showMessage("Connected", 2000);

        // Note: Tree refresh happens in onCachePopulated() after initial data load
        // Don't refresh here - cache is empty at this point

        // Trigger task rehydration after successful reconnect
        auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
        if (rehydrationMgr && conn)
        {
            rehydrationMgr->rehydrateTasks(conn);
        }
    } else
    {
        qDebug() << "XenAdmin Qt: Disconnected from Xen server";
        this->ui->statusbar->showMessage("Disconnected", 2000);
        this->clearTabs();
        if (this->m_navigationPane)
        {
            this->m_navigationPane->RequestRefreshTreeView();
        }
        this->updatePlaceholderVisibility();
    }
}

void MainWindow::onCachePopulated()
{
    XenConnection* connection = qobject_cast<XenConnection*>(sender());

    if (!connection)
    {
        qDebug() << "MainWindow::onCachePopulated(): nullptr XenConnection";
        return;
    }

    qDebug() << "MainWindow: Cache populated, refreshing tree view";
    XenCache* cache = connection->GetCache();
    qDebug() << "MainWindow: Cache counts"
             << "hosts=" << cache->Count("host")
             << "pools=" << cache->Count("pool")
             << "vms=" << cache->Count("vm")
             << "srs=" << cache->Count("sr");

    this->updateConnectionProfileFromCache(connection, cache);

    // Refresh tree now that cache has data
    if (this->m_navigationPane)
    {
        this->m_navigationPane->RequestRefreshTreeView();
    }

    // Start MetricUpdater to begin fetching RRD performance metrics
    // C# Equivalent: MetricUpdater.Start() called after connection established
    // C# Reference: xenadmin/XenModel/XenConnection.cs line 780

    if (connection->GetMetricUpdater())
    {
        qDebug() << "MainWindow: Starting MetricUpdater for performance metrics";
        connection->GetMetricUpdater()->start();
    }
}

void MainWindow::onConnectionAdded(XenConnection* connection)
{
    if (!connection)
        return;

    XenConnection *conn = connection;

    connect(connection, &XenConnection::connectionResult, this, [this, conn](bool connected, const QString&)
    {
        this->onConnectionStateChanged(conn, connected);
    });
    connect(connection, &XenConnection::connectionClosed, this, [this, conn]()
    {
        this->onConnectionStateChanged(conn, false);
    });
    connect(connection, &XenConnection::connectionLost, this, [this, conn]()
    {
        this->onConnectionStateChanged(conn, false);
    });
    connect(connection, &XenConnection::cachePopulated, this, &MainWindow::onCachePopulated);

    XenCache* cache = connection->GetCache();
    if (cache)
    {
        connect(cache, &XenCache::objectChanged, this, &MainWindow::onCacheObjectChanged);
    }

    connect(connection, &XenConnection::taskAdded, this, &MainWindow::onConnectionTaskAdded);
    connect(connection, &XenConnection::taskModified, this, &MainWindow::onConnectionTaskModified);
    connect(connection, &XenConnection::taskDeleted, this, &MainWindow::onConnectionTaskDeleted);
    connect(connection, &XenConnection::messageReceived, this, &MainWindow::onMessageReceived);
    connect(connection, &XenConnection::messageRemoved, this, &MainWindow::onMessageRemoved);
}

void MainWindow::onConnectionTaskAdded(const QString& taskRef, const QVariantMap& taskData)
{
    auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (rehydrationMgr && connection)
        rehydrationMgr->handleTaskAdded(connection, taskRef, taskData);
}

void MainWindow::onConnectionTaskModified(const QString& taskRef, const QVariantMap& taskData)
{
    auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (rehydrationMgr && connection)
        rehydrationMgr->handleTaskUpdated(connection, taskRef, taskData);
}

void MainWindow::onConnectionTaskDeleted(const QString& taskRef)
{
    auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (rehydrationMgr && connection)
        rehydrationMgr->handleTaskRemoved(connection, taskRef);
}

void MainWindow::onTreeItemSelected()
{
    QList<QTreeWidgetItem*> selectedItems = this->getServerTreeWidget()->selectedItems();
    if (selectedItems.isEmpty())
    {
        this->ui->statusbar->showMessage("Ready", 2000);
        this->clearTabs();
        this->updatePlaceholderVisibility();
        this->m_titleBar->clear();
        this->m_lastSelectedRef.clear(); // Clear selection tracking

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        this->updateToolbarsAndMenus();

        return;
    }

    QTreeWidgetItem* item = selectedItems.first();
    QString itemText = item->text(0);
    QVariant itemData = item->data(0, Qt::UserRole);
    QIcon itemIcon = item->icon(0);
    
    // Extract object type and ref from QSharedPointer<XenObject>
    QString objectType;
    QString objectRef;
    QSharedPointer<XenObject> xenObject;
    XenConnection *connection = nullptr;
    
    xenObject = itemData.value<QSharedPointer<XenObject>>();
    if (xenObject)
    {
        objectType = xenObject->GetObjectType();
        objectRef = xenObject->OpaqueRef();
        connection = xenObject->GetConnection();
    } else if (itemData.canConvert<XenConnection*>())
    {
        // Disconnected server - handle specially
        objectType = "disconnected_host";
        objectRef = QString();
    }

    // Check if this is a GroupingTag node (matches C# MainWindow.SwitchToTab line 1718)
    // GroupingTag is stored in Qt::UserRole + 3
    QVariant groupingTagVar = item->data(0, Qt::UserRole + 3);
    if (groupingTagVar.canConvert<GroupingTag*>())
    {
        GroupingTag* groupingTag = groupingTagVar.value<GroupingTag*>();
        if (groupingTag)
        {
            // Show SearchTabPage with results for this grouping
            // Matches C# MainWindow.cs line 1771: SearchPage.Search = Search.SearchForNonVappGroup(...)
            this->showSearchPage(connection, groupingTag);
            return;
        }
    }

    // Update title bar with selected object
    this->m_titleBar->setTitle(itemText, itemIcon);

    if (!objectRef.isEmpty() && connection)
    {
        // Prevent duplicate API calls for same selection
        // This fixes the double-call issue when Qt emits itemSelectionChanged multiple times
        if (objectRef == this->m_lastSelectedRef && !objectRef.isEmpty())
        {
            //qDebug() << "MainWindow::onTreeItemSelected - Same object already selected, skipping duplicate API call";
            return;
        }

        this->m_lastSelectedRef = objectRef;

        this->ui->statusbar->showMessage("Selected: " + itemText + " (Ref: " + objectRef + ")", 5000);

        // Store context for async handler
        this->m_currentObjectType = objectType;
        this->m_currentObjectRef = objectRef;
        this->m_currentObjectText = itemText;
        this->m_currentObjectIcon = itemIcon;
        this->m_currentObjectConn = connection;

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        this->updateToolbarsAndMenus();

        // Now we have the data, show the tabs
        auto objectData = connection->GetCache()->ResolveObjectData(objectType, objectRef);
        this->showObjectTabs(connection, objectType, objectRef, objectData);

        // Add to navigation history (matches C# MainWindow.TreeView_SelectionsChanged)
        // Get current tab name (first tab is shown by default)
        QString currentTabName = "General"; // Default tab
        if (this->ui->mainTabWidget->count() > 0 && this->ui->mainTabWidget->currentIndex() >= 0)
        {
            currentTabName = this->ui->mainTabWidget->tabText(this->ui->mainTabWidget->currentIndex());
        }

        if (this->m_navigationHistory && !this->m_navigationHistory->isInHistoryNavigation())
        {
            HistoryItemPtr historyItem(new XenModelObjectHistoryItem(objectRef, objectType, this->m_currentObjectText, this->m_currentObjectIcon, currentTabName));
            this->m_navigationHistory->newHistoryItem(historyItem);
        }
    } else
    {
        this->ui->statusbar->showMessage("Selected: " + itemText, 3000);
        this->clearTabs();
        this->updatePlaceholderVisibility();
        this->m_lastSelectedRef.clear(); // Clear selection tracking

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        this->updateToolbarsAndMenus();
    }
}

void MainWindow::showObjectTabs(XenConnection *connection, const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    this->clearTabs();
    this->updateTabPages(connection, objectType, objectRef, objectData);
    this->updatePlaceholderVisibility();
}

// C# Equivalent: MainWindow.SwitchToTab(TabPageSearch) with SearchPage.Search = Search.SearchForNonVappGroup(...)
// C# Reference: xenadmin/XenAdmin/MainWindow.cs lines 1771-1775
void MainWindow::showSearchPage(XenConnection *connection, GroupingTag* groupingTag)
{
    if (!groupingTag || !this->m_searchTabPage)
        return;

    if (!connection)
    {
        Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
        if (connMgr)
        {
            const QList<XenConnection*> connections = connMgr->getAllConnections();
            for (XenConnection* candidate : connections)
            {
                if (candidate && candidate->GetCache())
                {
                    connection = candidate;
                    break;
                }
            }
        }
    }

    // Create Search object for this grouping
    // Matches C# MainWindow.cs line 1771: SearchPage.Search = Search.SearchForNonVappGroup(gt.Grouping, gt.Parent, gt.Group);
    Search* search = Search::SearchForNonVappGroup(groupingTag->getGrouping(), groupingTag->getParent(), groupingTag->getGroup());

    this->m_searchTabPage->SetXenObject(connection, QString(), QString(), QVariantMap());
    this->m_searchTabPage->setSearch(search); // SearchTabPage takes ownership

    // Clear existing tabs and show only SearchTabPage
    this->clearTabs();
    this->ui->mainTabWidget->addTab(this->m_searchTabPage, this->m_searchTabPage->GetTitle());
    this->updatePlaceholderVisibility();

    // Update status bar
    QString groupName = groupingTag->getGrouping()->getGroupName(groupingTag->getGroup());
    this->ui->statusbar->showMessage(tr("Showing overview: %1").arg(groupName), 3000);
}

// C# Equivalent: SearchPage double-click handler navigates to object
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 653-660
void MainWindow::onSearchTabPageObjectSelected(const QString& objectType, const QString& objectRef)
{
    // Find the object in the tree and select it
    // This matches C# behavior where double-clicking in search results navigates to General tab
    QTreeWidget* tree = this->getServerTreeWidget();
    if (!tree)
        return;

    // Search for the item in the tree
    QTreeWidgetItemIterator it(tree);
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        QVariant data = item->data(0, Qt::UserRole);
        
        QString itemType;
        QString itemRef;
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
        {
            itemType = obj->GetObjectType();
            itemRef = obj->OpaqueRef();
        }

        if (itemType == objectType && itemRef == objectRef)
        {
            // Found the item - select it (this will trigger onTreeItemSelected)
            tree->setCurrentItem(item);
            tree->scrollToItem(item);

            // Switch to General tab if it exists (matches C# behavior)
            // C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs line 659
            for (int i = 0; i < this->ui->mainTabWidget->count(); ++i)
            {
                BaseTabPage* page = qobject_cast<BaseTabPage*>(this->ui->mainTabWidget->widget(i));
                if (page && page->GetTitle() == tr("General"))
                {
                    this->ui->mainTabWidget->setCurrentIndex(i);
                    break;
                }
            }

            break;
        }

        ++it;
    }
}

void MainWindow::clearTabs()
{
    // Block signals to prevent spurious onTabChanged() calls during tab removal
    // Each removeTab() would otherwise trigger currentChanged with shifting indices
    bool oldState = this->ui->mainTabWidget->blockSignals(true);
    
    // Remove all tabs without destroying the underlying widgets.
    // The BaseTabPage instances are owned by m_tabPages and reused, so we
    // just detach them from the QTabWidget here.
    while (this->ui->mainTabWidget->count() > 0)
    {
        QWidget* widget = this->ui->mainTabWidget->widget(0);
        this->ui->mainTabWidget->removeTab(0);
        if (widget && widget != this->m_placeholderWidget)
        {
            widget->setParent(nullptr);
        }
    }
    
    // Restore signal state
    this->ui->mainTabWidget->blockSignals(oldState);
}

// C# Equivalent: GetNewTabPages() - builds list of tabs based on object type
// C# Reference: xenadmin/XenAdmin/MainWindow.cs lines 1293-1393
QList<BaseTabPage*> MainWindow::getNewTabPages(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) const
{
    QList<BaseTabPage*> newTabs;

    bool isHost = (objectType == "host");
    bool isVM = (objectType == "vm");
    bool isPool = (objectType == "pool");
    bool isSR = (objectType == "sr");
    bool isNetwork = (objectType == "network");

    // Get tab pointers from m_tabPages
    BaseTabPage* generalTab = nullptr;
    BaseTabPage* memoryTab = nullptr;
    BaseTabPage* vmStorageTab = nullptr;
    BaseTabPage* srStorageTab = nullptr;
    BaseTabPage* physicalStorageTab = nullptr;
    BaseTabPage* networkTab = nullptr;
    BaseTabPage* nicsTab = nullptr;
    BaseTabPage* performanceTab = nullptr;
    BaseTabPage* snapshotsTab = nullptr;
    BaseTabPage* bootTab = nullptr;
    BaseTabPage* consoleTab = nullptr;
    BaseTabPage* cvmConsoleTab = nullptr;
    BaseTabPage* searchTab = nullptr;

    for (BaseTabPage* tab : this->m_tabPages)
    {
        if (qobject_cast<VMStorageTabPage*>(tab))
            vmStorageTab = tab;
        else if (qobject_cast<SrStorageTabPage*>(tab))
            srStorageTab = tab;
        else if (qobject_cast<PhysicalStorageTabPage*>(tab))
            physicalStorageTab = tab;

        QString title = tab->GetTitle();
        if (title == "General")
            generalTab = tab;
        else if (title == "Memory")
            memoryTab = tab;
        else if (title == "Networking")
            networkTab = tab;
        else if (title == "NICs")
            nicsTab = tab;
        else if (title == "Performance")
            performanceTab = tab;
        else if (title == "Snapshots")
            snapshotsTab = tab;
        else if (title == "Boot Options")
            bootTab = tab;
        else if (title == "Console")
            consoleTab = tab;
        else if (title == "CVM Console")
            cvmConsoleTab = tab;
        else if (title == "Search")
            searchTab = tab;
    }

    // Host tab order: General, Memory, Storage, Networking, NICs, [GPU], Console, Performance
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs lines 1327-1379
    if (isHost)
    {
        if (generalTab)
            newTabs.append(generalTab);
        if (memoryTab)
            newTabs.append(memoryTab);
        if (physicalStorageTab)
            newTabs.append(physicalStorageTab);
        if (networkTab)
            newTabs.append(networkTab);
        if (nicsTab)
            newTabs.append(nicsTab);
        // TODO: Add GPU tab when implemented
        if (consoleTab)
            newTabs.append(consoleTab);
        if (performanceTab)
            newTabs.append(performanceTab);
    }
    // VM tab order: General, Memory, Storage, Networking, Snapshots, Boot Options, Console, Performance
    // C# Reference: TabPageBallooning is shown for VMs (MainWindow.cs line 1328)
    else if (isVM)
    {
        if (generalTab)
            newTabs.append(generalTab);
        if (memoryTab)
            newTabs.append(memoryTab);
        if (vmStorageTab)
            newTabs.append(vmStorageTab);
        if (networkTab)
            newTabs.append(networkTab);
        if (snapshotsTab)
            newTabs.append(snapshotsTab);
        if (bootTab)
            newTabs.append(bootTab);
        if (consoleTab)
            newTabs.append(consoleTab);
        if (performanceTab)
            newTabs.append(performanceTab);
    }
    // Pool tab order: General, Memory, Storage, Network, Performance
    else if (isPool)
    {
        if (generalTab)
            newTabs.append(generalTab);
        if (memoryTab)
            newTabs.append(memoryTab);
        if (physicalStorageTab)
            newTabs.append(physicalStorageTab);
        if (networkTab)
            newTabs.append(networkTab);
        if (performanceTab)
            newTabs.append(performanceTab);
    }
    // SR tab order: General, Storage, CVM Console (if applicable), Search
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs lines 1324, 1333, 1376, 1391
    else if (isSR)
    {
        if (generalTab)
            newTabs.append(generalTab);
        if (srStorageTab)
            newTabs.append(srStorageTab);
        // CVM Console only shown if SR has driver domain
        // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1376
        if (cvmConsoleTab)
        {
            XenConnection* connection = this->m_currentObjectConn;
            XenCache* cache = connection ? connection->GetCache() : nullptr;
            QSharedPointer<SR> srObj = cache ? cache->ResolveObject<SR>("sr", objectRef) : QSharedPointer<SR>();
            if (srObj && srObj->HasDriverDomain())
                newTabs.append(cvmConsoleTab);
        }
        // Note: Performance tab is NOT shown for SR in C#
    }
    // Network tab order: General, Network
    else if (isNetwork)
    {
        if (generalTab)
            newTabs.append(generalTab);
        if (networkTab)
            newTabs.append(networkTab);
    }
    // Default: show applicable tabs
    else
    {
        for (BaseTabPage* tab : this->m_tabPages)
        {
            if (tab->IsApplicableForObjectType(objectType))
                newTabs.append(tab);
        }
    }

    // Always add Search tab last
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1391
    if (searchTab)
        newTabs.append(searchTab);

    return newTabs;
}

void MainWindow::updateTabPages(XenConnection *connection, const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    // Get the correct tabs in order for this object type
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1432 (ChangeToNewTabs)
    QList<BaseTabPage*> newTabs = this->getNewTabPages(objectType, objectRef, objectData);

    // Get the last selected tab for this object (before adding tabs)
    // C# Reference: MainWindow.cs line 1434 - GetLastSelectedPage(SelectionManager.Selection.First)
    QString rememberedTabTitle = this->m_selectedTabs.value(objectRef);
    int pageToSelectIndex = -1;

    // Block signals during tab reconstruction to prevent premature onTabChanged calls
    // C# Reference: MainWindow.cs line 1438 - IgnoreTabChanges = true
    bool oldState = this->ui->mainTabWidget->blockSignals(true);

    // Add tabs in the correct order
    for (int i = 0; i < newTabs.size(); ++i)
    {
        BaseTabPage* tabPage = newTabs[i];

        // Set the object data on the tab page
        tabPage->SetXenObject(connection, objectType, objectRef, objectData);

        // Add the tab to the widget
        this->ui->mainTabWidget->addTab(tabPage, tabPage->GetTitle());

        // Check if this is the remembered tab
        // C# Reference: MainWindow.cs line 1460 - if (newTab == pageToSelect)
        if (!rememberedTabTitle.isEmpty() && tabPage->GetTitle() == rememberedTabTitle)
        {
            pageToSelectIndex = i;
        }
    }

    // If no remembered tab found or not applicable, default to first tab
    // C# Reference: MainWindow.cs line 1464 - if (pageToSelect == null) TheTabControl.SelectedTab = TheTabControl.TabPages[0]
    if (pageToSelectIndex < 0 && this->ui->mainTabWidget->count() > 0)
    {
        pageToSelectIndex = 0;
    }

    // Set the selected tab
    if (pageToSelectIndex >= 0)
    {
        this->ui->mainTabWidget->setCurrentIndex(pageToSelectIndex);
    }

    // Re-enable signals
    this->ui->mainTabWidget->blockSignals(oldState);

    // Save the final selection back to the map
    // C# Reference: MainWindow.cs line 1471 - SetLastSelectedPage(SelectionManager.Selection.First, TheTabControl.SelectedTab)
    if (this->ui->mainTabWidget->currentIndex() >= 0)
    {
        QString currentTabTitle = this->ui->mainTabWidget->tabText(this->ui->mainTabWidget->currentIndex());
        this->m_selectedTabs[objectRef] = currentTabTitle;
    }

    // Trigger onPageShown for the initially visible tab
    if (this->ui->mainTabWidget->count() > 0 && this->ui->mainTabWidget->currentIndex() >= 0)
    {
        QWidget* currentWidget = this->ui->mainTabWidget->currentWidget();
        BaseTabPage* currentPage = qobject_cast<BaseTabPage*>(currentWidget);
        
        if (currentPage)
        {
            // Handle console tabs specially - need to switch console to current object
            // Reference: C# TheTabControl_SelectedIndexChanged - console logic
            ConsoleTabPage* consoleTab = qobject_cast<ConsoleTabPage*>(currentPage);
            if (consoleTab && consoleTab->consolePanel())
            {
                // Pause CVM console
                if (this->m_cvmConsolePanel)
                {
                    this->m_cvmConsolePanel->PauseAllDockedViews();
                }

                // Set current source based on object type
                if (objectType == "vm")
                {
                    consoleTab->consolePanel()->SetCurrentSource(connection, objectRef);
                    consoleTab->consolePanel()->UnpauseActiveView(true);
                }
                else if (objectType == "host")
                {
                    consoleTab->consolePanel()->SetCurrentSourceHost(connection, objectRef);
                    consoleTab->consolePanel()->UnpauseActiveView(true);
                }

                // Update RDP resolution
                consoleTab->consolePanel()->UpdateRDPResolution();
            }
            else
            {
                // Check for CVM console tab
                CvmConsoleTabPage* cvmConsoleTab = qobject_cast<CvmConsoleTabPage*>(currentPage);
                if (cvmConsoleTab && cvmConsoleTab->consolePanel())
                {
                    // Pause regular console
                    if (this->m_consolePanel)
                    {
                        this->m_consolePanel->PauseAllDockedViews();
                    }

                    // Set current source for SR
                    if (objectType == "sr")
                    {
                        cvmConsoleTab->consolePanel()->SetCurrentSource(connection, objectRef);
                        cvmConsoleTab->consolePanel()->UnpauseActiveView(true);
                    }
                }
                else
                {
                    // Not a console tab - pause all consoles
                    if (this->m_consolePanel)
                    {
                        this->m_consolePanel->PauseAllDockedViews();
                    }
                    if (this->m_cvmConsolePanel)
                    {
                        this->m_cvmConsolePanel->PauseAllDockedViews();
                    }
                }
            }
            
            currentPage->OnPageShown();
        }
    }
}

void MainWindow::updatePlaceholderVisibility()
{
    // Count real tabs (excluding placeholder)
    int realTabCount = 0;
    for (int i = 0; i < this->ui->mainTabWidget->count(); ++i)
    {
        if (this->ui->mainTabWidget->widget(i) != this->m_placeholderWidget)
        {
            realTabCount++;
        }
    }

    // If we have real tabs, remove placeholder and show tab bar
    if (realTabCount > 0)
    {
        // Find and remove placeholder if it exists
        int placeholderIndex = this->ui->mainTabWidget->indexOf(this->m_placeholderWidget);
        if (placeholderIndex >= 0)
        {
            this->ui->mainTabWidget->removeTab(placeholderIndex);
        }
        this->ui->mainTabWidget->tabBar()->show();
    } else
    {
        // No real tabs - ensure placeholder is shown and tab bar is hidden
        int placeholderIndex = this->ui->mainTabWidget->indexOf(this->m_placeholderWidget);
        if (placeholderIndex < 0)
        {
            // Placeholder not present, add it
            this->ui->mainTabWidget->addTab(this->m_placeholderWidget, "");
        }
        this->ui->mainTabWidget->tabBar()->hide();
    }
}

/**
 * @brief Handle tab changes - pause/unpause console panels
 * Reference: MainWindow.cs TheTabControl_SelectedIndexChanged (lines 1642-1690)
 */
void MainWindow::onTabChanged(int index)
{
    // Notify the previous tab that it's being hidden
    static int previousIndex = -1;
    if (previousIndex >= 0 && previousIndex < this->ui->mainTabWidget->count())
    {
        QWidget* previousWidget = this->ui->mainTabWidget->widget(previousIndex);
        BaseTabPage* previousPage = qobject_cast<BaseTabPage*>(previousWidget);
        if (previousPage)
        {
            previousPage->OnPageHidden();
        }
    }

    // Notify the new tab that it's being shown
    if (index >= 0 && index < this->ui->mainTabWidget->count())
    {
        QWidget* currentWidget = this->ui->mainTabWidget->widget(index);
        BaseTabPage* currentPage = qobject_cast<BaseTabPage*>(currentWidget);

        // Check if this is the regular console tab (VM/Host consoles)
        // Reference: C# TheTabControl_SelectedIndexChanged lines 1652-1667
        ConsoleTabPage* consoleTab = qobject_cast<ConsoleTabPage*>(currentPage);

        if (consoleTab && consoleTab->consolePanel())
        {
            // Console tab selected - handle console panel logic
            qDebug() << "MainWindow: Console tab selected";

            // Pause CVM console (other console panel)
            if (this->m_cvmConsolePanel)
            {
                this->m_cvmConsolePanel->PauseAllDockedViews();
            }

            // Set current source based on selection
            if (this->m_currentObjectType == "vm")
            {
                consoleTab->consolePanel()->SetCurrentSource(this->m_currentObjectConn, this->m_currentObjectRef);
                consoleTab->consolePanel()->UnpauseActiveView(true); // Focus console
            } else if (this->m_currentObjectType == "host")
            {
                consoleTab->consolePanel()->SetCurrentSourceHost(this->m_currentObjectConn, this->m_currentObjectRef);
                consoleTab->consolePanel()->UnpauseActiveView(true); // Focus console
            }

            // Update RDP resolution
            consoleTab->consolePanel()->UpdateRDPResolution();
        } else
        {
            // Check if this is the CVM console tab (SR driver domain consoles)
            // Reference: C# TheTabControl_SelectedIndexChanged lines 1669-1677
            CvmConsoleTabPage* cvmConsoleTab = qobject_cast<CvmConsoleTabPage*>(currentPage);

            if (cvmConsoleTab && cvmConsoleTab->consolePanel())
            {
                // CVM Console tab selected
                qDebug() << "MainWindow: CVM Console tab selected";

                // Pause regular console (other console panel)
                if (this->m_consolePanel)
                {
                    this->m_consolePanel->PauseAllDockedViews();
                }

                // Set current source - CvmConsolePanel expects SR with driver domain
                // The CvmConsolePanel will look up the driver domain VM internally
                if (this->m_currentObjectType == "sr")
                {
                    // CvmConsolePanel.setCurrentSource() will look up driver domain VM
                    cvmConsoleTab->consolePanel()->SetCurrentSource(this->m_currentObjectConn, this->m_currentObjectRef);
                    cvmConsoleTab->consolePanel()->UnpauseActiveView(true); // Focus console
                }
            } else
            {
                // Not any console tab - pause all console panels
                // Reference: C# TheTabControl_SelectedIndexChanged lines 1681-1682
                if (this->m_consolePanel)
                {
                    this->m_consolePanel->PauseAllDockedViews();
                }
                if (this->m_cvmConsolePanel)
                {
                    this->m_cvmConsolePanel->PauseAllDockedViews();
                }
            }
        }

        if (currentPage)
        {
            currentPage->OnPageShown();
        }
    }

    // Save the selected tab for the current object (tab memory)
    // Reference: C# SetLastSelectedPage() - stores tab per object
    if (index >= 0 && !this->m_currentObjectRef.isEmpty())
    {
        QString tabTitle = this->ui->mainTabWidget->tabText(index);
        this->m_selectedTabs[this->m_currentObjectRef] = tabTitle;
    }

    previousIndex = index;
}

void MainWindow::showTreeContextMenu(const QPoint& position)
{
    QTreeWidget* tree = this->getServerTreeWidget();
    if (!tree)
        return;

    QTreeWidgetItem* item = tree->itemAt(position);
    if (!item)
        return;

    tree->setCurrentItem(item);

    // Use ContextMenuBuilder to create the appropriate menu
    ContextMenuBuilder builder(this);
    QMenu* contextMenu = builder.buildContextMenu(item, this);

    if (!contextMenu)
        return;

    // The context menu logic has been moved to ContextMenuBuilder

    // Show the context menu at the requested position
    contextMenu->exec(this->getServerTreeWidget()->mapToGlobal(position));

    // Clean up the menu
    contextMenu->deleteLater();
}

// Public interface methods for Command classes
QTreeWidget* MainWindow::getServerTreeWidget() const
{
    // Get tree widget from NavigationPane's NavigationView
    if (this->m_navigationPane)
    {
        auto* navView = this->m_navigationPane->GetNavigationView();
        if (navView)
        {
            return navView->treeWidget();
        }
    }
    return nullptr;
}

void MainWindow::showStatusMessage(const QString& message, int timeout)
{
    if (timeout > 0)
        this->ui->statusbar->showMessage(message, timeout);
    else
        this->ui->statusbar->showMessage(message);
}

void MainWindow::refreshServerTree()
{
    // Delegate tree building to NavigationView which respects current navigation mode
    if (this->m_navigationPane)
    {
        this->m_navigationPane->RequestRefreshTreeView();
    }
}

// Settings management
void MainWindow::saveSettings()
{
    SettingsManager& settings = SettingsManager::instance();

    // Save window geometry and state
    settings.saveMainWindowGeometry(saveGeometry());
    settings.saveMainWindowState(saveState());
    settings.saveSplitterState(ui->centralSplitter->saveState());

    // Save debug console visibility
    if (this->m_debugWindow)
    {
        settings.setDebugConsoleVisible(this->m_debugWindow->isVisible());
    }

    // Save expanded tree items
    QStringList expandedItems;
    QTreeWidgetItemIterator it(this->getServerTreeWidget());
    while (*it)
    {
        if ((*it)->isExpanded())
        {
            QSharedPointer<XenObject> obj = (*it)->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
            QString ref = obj ? obj->OpaqueRef() : QString();
            if (!ref.isEmpty())
            {
                expandedItems.append(ref);
            }
        }
        ++it;
    }
    settings.setExpandedTreeItems(expandedItems);

    settings.sync();
    qDebug() << "Settings saved to:" << settings.getValue("").toString();
}

void MainWindow::SaveConnections()
{
    SaveServerList();
}

bool MainWindow::IsConnected()
{
    Xen::ConnectionsManager *manager = Xen::ConnectionsManager::instance();
    return manager && !manager->getConnectedConnections().isEmpty();
}

void MainWindow::loadSettings()
{
    SettingsManager& settings = SettingsManager::instance();

    // Restore window geometry and state
    QByteArray geometry = settings.loadMainWindowGeometry();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }

    QByteArray state = settings.loadMainWindowState();
    if (!state.isEmpty())
    {
        restoreState(state);
    }

    QByteArray splitterState = settings.loadSplitterState();
    if (!splitterState.isEmpty())
    {
        this->ui->centralSplitter->restoreState(splitterState);
    }

    // Restore debug console visibility
    if (settings.getDebugConsoleVisible() && this->m_debugWindow)
    {
        this->m_debugWindow->show();
    }

    this->applyViewSettingsToMenu();
    if (this->m_navigationPane)
    {
        this->updateViewMenu(this->m_navigationPane->GetCurrentMode());
    }

    qDebug() << "Settings loaded from:" << settings.getValue("").toString();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Match C# behavior: prompt only when there are running operations
    bool hasRunningOperations = false;
    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->records();
    for (OperationManager::OperationRecord* record : records)
    {
        AsyncOperation* operation = record ? record->operation.data() : nullptr;
        if (!operation)
            continue;
        if (qobject_cast<MeddlingAction*>(operation))
            continue;
        if (record->state != AsyncOperation::Completed)
        {
            hasRunningOperations = true;
            break;
        }
    }

    if (hasRunningOperations)
    {
        CloseXenCenterWarningDialog dlg(false, nullptr, this);
        if (dlg.exec() != QDialog::Accepted)
        {
            event->ignore();
            return;
        }
    }

    // Save settings before closing
    this->saveSettings();

    // Save current connections
    SaveServerList();

    // Clean up operation UUIDs before exit (matches C# MainWindow.OnClosing)
    OperationManager::instance()->prepareAllOperationsForRestart();

    // Disconnect active connections (new flow via ConnectionsManager)
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (connMgr)
    {
        const QList<XenConnection*> connections = connMgr->getAllConnections();
        for (XenConnection* connection : connections)
        {
            if (connection && (connection->IsConnected() || connection->InProgress()))
                connection->EndConnect(true, true);
        }
    }

    event->accept();
}

// Search functionality
void MainWindow::onSearchTextChanged(const QString& text)
{
    QTreeWidget* treeWidget = this->getServerTreeWidget();
    if (!treeWidget)
        return;

    // If search is empty, show all items
    if (text.isEmpty())
    {
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = treeWidget->topLevelItem(i);
            this->filterTreeItems(item, "");
        }
        return;
    }

    // Filter tree items based on search text
    for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = treeWidget->topLevelItem(i);
        this->filterTreeItems(item, text);
    }
}
void MainWindow::focusSearch()
{
    // Focus search box in NavigationPane
    if (this->m_navigationPane)
    {
        this->m_navigationPane->FocusTreeView();
        // TODO: Also focus the search line edit when NavigationView exposes it
    }
}

// NavigationPane event handlers (matches C# MainWindow.navigationPane_* methods)

void MainWindow::onNavigationModeChanged(int mode)
{
    // C# Reference: MainWindow.navigationPane_NavigationModeChanged line 2570
    // Handle navigation mode changes (Infrastructure/Objects/Organization/Searches/Notifications)
    NavigationPane::NavigationMode navMode = static_cast<NavigationPane::NavigationMode>(mode);

    if (navMode == NavigationPane::Notifications)
    {
        // Hide main tabs when in notifications mode
        // C# Reference: line 2572
        this->ui->mainTabWidget->setVisible(false);
        
        // Auto-select Alerts sub-mode when entering Notifications mode
        // This ensures something is displayed instead of showing empty area
        this->m_navigationPane->SwitchToNotificationsView(NavigationPane::Alerts);
        
        // Notification pages are shown via onNotificationsSubModeChanged
    } else
    {
        // Remember if tab control was hidden before restore
        bool tabControlWasVisible = this->ui->mainTabWidget->isVisible();

        // Restore main tabs
        // C# Reference: line 2577
        this->ui->mainTabWidget->setVisible(true);

        // Hide all notification pages when switching away from Notifications mode
        // C# Reference: foreach (var page in _notificationPages) line 2579
        for (NotificationsBasePage* page : this->m_notificationPages)
        {
            if (page->isVisible())
            {
                page->hidePage();
            }
        }

        // Force tab refresh when switching back from Notification view
        // Some tabs ignore updates when not visible (e.g., Snapshots, HA)
        // C# Reference: line 2585
        if (!tabControlWasVisible)
        {
            this->onTabChanged(this->ui->mainTabWidget->currentIndex());
        }
    }

    // Update search for new mode (matches C# ViewSettingsChanged line 1981)
    this->m_navigationPane->UpdateSearch();

    // TODO: SetFiltersLabel() - update filters indicator in title bar
    this->updateViewMenu(navMode);

    // Update tree view for new mode
    this->refreshServerTree();
}

void MainWindow::onViewTemplatesToggled(bool checked)
{
    SettingsManager::instance().setDefaultTemplatesVisible(checked);
    this->onViewSettingsChanged();
}

void MainWindow::onViewCustomTemplatesToggled(bool checked)
{
    SettingsManager::instance().setUserTemplatesVisible(checked);
    this->onViewSettingsChanged();
}

void MainWindow::onViewLocalStorageToggled(bool checked)
{
    SettingsManager::instance().setLocalSRsVisible(checked);
    this->onViewSettingsChanged();
}

void MainWindow::onViewShowHiddenObjectsToggled(bool checked)
{
    SettingsManager::instance().setShowHiddenObjects(checked);
    this->onViewSettingsChanged();
}

void MainWindow::onViewSettingsChanged()
{
    this->m_navigationPane->UpdateSearch();
}

void MainWindow::applyViewSettingsToMenu()
{
    SettingsManager& settings = SettingsManager::instance();
    this->ui->viewTemplatesAction->setChecked(settings.getDefaultTemplatesVisible());
    this->ui->viewCustomTemplatesAction->setChecked(settings.getUserTemplatesVisible());
    this->ui->viewLocalStorageAction->setChecked(settings.getLocalSRsVisible());
    this->ui->viewShowHiddenObjectsAction->setChecked(settings.getShowHiddenObjects());
}

void MainWindow::updateViewMenu(NavigationPane::NavigationMode mode)
{
    const bool isInfrastructure = mode == NavigationPane::Infrastructure;
    const bool isNotifications = mode == NavigationPane::Notifications;

    this->ui->viewTemplatesAction->setVisible(isInfrastructure);
    this->ui->viewCustomTemplatesAction->setVisible(isInfrastructure);
    this->ui->viewLocalStorageAction->setVisible(isInfrastructure);
    this->ui->viewMenuSeparator1->setVisible(isInfrastructure);

    const bool showHiddenVisible = !isNotifications;
    this->ui->viewShowHiddenObjectsAction->setVisible(showHiddenVisible);
    this->ui->viewMenuSeparator2->setVisible(showHiddenVisible);
}

void MainWindow::onNotificationsSubModeChanged(int subMode)
{
    // C# Reference: MainWindow.navigationPane_NotificationsSubModeChanged line 2551
    // Show the correct notification page based on sub-mode selection
    
    NavigationPane::NotificationsSubMode mode = static_cast<NavigationPane::NotificationsSubMode>(subMode);
    
    // Show the page matching this sub-mode, hide all others
    // C# Reference: foreach (var page in _notificationPages)
    for (NotificationsBasePage* page : this->m_notificationPages)
    {
        if (page->notificationsSubMode() == mode)
        {
            page->showPage(); // C# ShowPage()
        }
        else if (page->isVisible())
        {
            page->hidePage(); // C# HidePage()
        }
    }

    // Hide tab control when showing notification pages
    // C# Reference: line 2565
    this->ui->mainTabWidget->setVisible(false);

    // Update title label and icon for notification pages
    // C# Reference: TitleLabel.Text = submodeItem.Text; line 2567
    // C# Reference: TitleIcon.Image = submodeItem.Image; line 2568
    QString title;
    QIcon icon;
    
    switch (mode)
    {
        case NavigationPane::Alerts:
            title = tr("Alerts");
            icon = QIcon(":/icons/alert.png"); // TODO: Use correct alert icon
            break;
        case NavigationPane::Events:
            title = tr("Events");
            icon = QIcon(":/icons/events.png"); // TODO: Use correct events icon
            break;
        case NavigationPane::Updates:
            title = tr("Updates");
            icon = QIcon(":/icons/updates.png"); // TODO: Use correct updates icon
            break;
    }
    
    // Update the title bar with notification sub-mode info
    if (this->m_titleBar)
    {
        this->m_titleBar->setTitle(title);
        this->m_titleBar->setIcon(icon);
    }

    // TODO: Update filters label in title bar
    // C# SetFiltersLabel();
    
    qDebug() << "Switched to notifications sub-mode:" << subMode;
}

void MainWindow::onNavigationPaneTreeViewSelectionChanged()
{
    // Ignore tree view selection changes when in Notifications mode
    // The title should show the notification sub-mode (Alerts/Events), not tree selection
    if (this->m_navigationPane && this->m_navigationPane->GetCurrentMode() == NavigationPane::Notifications)
        return;
    
    // Forward to existing tree selection handler
    this->onTreeItemSelected();
}

void MainWindow::onNavigationPaneTreeNodeRightClicked()
{
    // Matches C# MainWindow.navigationPane_TreeNodeRightClicked
    // Context menu is already handled via customContextMenuRequested signal
}

void MainWindow::filterTreeItems(QTreeWidgetItem* item, const QString& searchText)
{
    if (!item)
        return;

    // Check if this item or any of its children match
    bool itemMatches = searchText.isEmpty() || this->itemMatchesSearch(item, searchText);
    bool hasVisibleChild = false;

    // Recursively filter children
    for (int i = 0; i < item->childCount(); ++i)
    {
        QTreeWidgetItem* child = item->child(i);
        this->filterTreeItems(child, searchText);
        if (!child->isHidden())
        {
            hasVisibleChild = true;
        }
    }

    // Show item if it matches or has visible children
    item->setHidden(!itemMatches && !hasVisibleChild);

    // Expand items that have visible children when searching
    if (!searchText.isEmpty() && hasVisibleChild)
    {
        item->setExpanded(true);
    }
}

bool MainWindow::itemMatchesSearch(QTreeWidgetItem* item, const QString& searchText)
{
    if (!item || searchText.isEmpty())
        return true;

    // Case-insensitive search in item text
    QString itemText = item->text(0).toLower();
    QString search = searchText.toLower();

    if (itemText.contains(search))
        return true;

    // Also search in item data (uuid, type, etc.)
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
        {
            QString objectType = obj->GetObjectType().toLower();
            QString uuid = obj->GetUUID().toLower();
            if (objectType.contains(search) || uuid.contains(search))
                return true;
        }
    }

    return false;
}

void MainWindow::restoreConnections()
{
    qDebug() << "XenAdmin Qt: Restoring saved connections...";

    // Always restore profiles into the ConnectionsManager; only auto-connect if enabled.
    bool autoConnect = SettingsManager::instance().getAutoConnect();

    // Load all saved connection profiles
    QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();

    if (profiles.isEmpty())
    {
        qDebug() << "XenAdmin Qt: No saved connection profiles found";
        return;
    }

    qDebug() << "XenAdmin Qt: Found" << profiles.size() << "saved connection profile(s)";

    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
    {
        qWarning() << "XenAdmin Qt: ConnectionsManager not available, skipping restore";
        return;
    }

    // Restore connections that have autoConnect enabled or were previously connected
    for (const ConnectionProfile& profile : profiles)
    {
        // Only auto-connect if the profile has autoConnect enabled
        // or if save session is enabled and the connection wasn't explicitly disconnected
        bool shouldConnect = profile.autoConnect() ||
                             (SettingsManager::instance().getSaveSession() && !profile.saveDisconnected());

        if (shouldConnect)
        {
            qDebug() << "XenAdmin Qt: Restoring connection to" << profile.displayName();
        }
        else
        {
            qDebug() << "XenAdmin Qt: Adding disconnected profile" << profile.displayName();
        }

        XenConnection* connection = new XenConnection(nullptr);
        connection->SetHostname(profile.hostname());
        connection->SetPort(profile.port());
        connection->SetUsername(profile.username());
        connection->SetPassword(profile.password());
        connection->setSaveDisconnected(profile.saveDisconnected());
        connection->SetPoolMembers(profile.poolMembers());
        connection->setExpectPasswordIsCorrect(!profile.password().isEmpty());
        connection->setFromDialog(false);

        connMgr->addConnection(connection);

        if (shouldConnect)
            XenConnectionUI::BeginConnect(connection, true, this, true);
    }
}

void MainWindow::SaveServerList()
{
    qDebug() << "XenAdmin Qt: Saving server list...";

    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
    {
        qWarning() << "XenAdmin Qt: ConnectionsManager not available, skipping save";
        return;
    }

    const bool saveSession = SettingsManager::instance().getSaveSession();

    QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
    QMap<QString, ConnectionProfile> existing;
    for (const ConnectionProfile& profile : profiles)
    {
        const QString key = profile.hostname() + ":" + QString::number(profile.port());
        existing.insert(key, profile);
        if (!profile.name().isEmpty())
            SettingsManager::instance().removeConnectionProfile(profile.name());
    }

    const QList<XenConnection*> connections = connMgr->getAllConnections();
    for (XenConnection* connection : connections)
    {
        if (!connection)
            continue;

        const QString hostname = connection->GetHostname();
        const int port = connection->GetPort();
        const QString key = hostname + ":" + QString::number(port);
        const QString profileName = port == 443
            ? hostname
            : QString("%1:%2").arg(hostname).arg(port);

        ConnectionProfile profile = existing.value(key, ConnectionProfile(profileName, hostname, port,
                                                                          connection->GetUsername(), false));

        profile.setName(profileName);
        profile.setHostname(hostname);
        profile.setPort(port);
        profile.setUsername(connection->GetUsername());
        profile.setSaveDisconnected(!connection->IsConnected());
        profile.setPoolMembers(connection->GetPoolMembers());

        const bool rememberPassword = saveSession && !connection->GetPassword().isEmpty();
        profile.setRememberPassword(rememberPassword);
        if (rememberPassword)
            profile.setPassword(connection->GetPassword());
        else
            profile.setPassword(QString());

        QString friendlyName = profile.friendlyName();
        XenCache* cache = connection->GetCache();
        if (cache)
        {
            const QList<QVariantMap> pools = cache->GetAllData("pool");
            if (!pools.isEmpty())
            {
                friendlyName = pools.first().value("name_label").toString();
                if (friendlyName.isEmpty())
                    friendlyName = pools.first().value("name").toString();
            }
        }

        if (!friendlyName.isEmpty())
            profile.setFriendlyName(friendlyName);

        SettingsManager::instance().saveConnectionProfile(profile);
    }

    qDebug() << "XenAdmin Qt: Saved" << connections.size() << "connection profile(s)";
    SettingsManager::instance().sync();
}

void MainWindow::updateConnectionProfileFromCache(XenConnection* connection, XenCache* cache)
{
    if (!connection || !cache)
        return;

    if (!SettingsManager::instance().getSaveSession())
        return;

    const QString hostname = connection->GetHostname();
    const int port = connection->GetPort();
    const QString profileName = port == 443
        ? hostname
        : QString("%1:%2").arg(hostname).arg(port);

    QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
    ConnectionProfile targetProfile;
    bool found = false;

    for (const ConnectionProfile& profile : profiles)
    {
        if (profile.hostname() == hostname && profile.port() == port)
        {
            targetProfile = profile;
            found = true;
            break;
        }
    }

    if (!found)
    {
        targetProfile = ConnectionProfile(profileName, hostname, port, connection->GetUsername(),
                                          !connection->GetPassword().isEmpty());
    }

    targetProfile.setName(profileName);
    targetProfile.setHostname(hostname);
    targetProfile.setPort(port);
    targetProfile.setUsername(connection->GetUsername());
    targetProfile.setSaveDisconnected(false);

    const bool rememberPassword = !connection->GetPassword().isEmpty();
    targetProfile.setRememberPassword(rememberPassword);
    if (rememberPassword)
        targetProfile.setPassword(connection->GetPassword());

    QString poolName;
    const QList<QVariantMap> pools = cache->GetAllData("pool");
    if (!pools.isEmpty())
    {
        poolName = pools.first().value("name_label").toString();
        if (poolName.isEmpty())
            poolName = pools.first().value("name").toString();
    }

    if (!poolName.isEmpty())
        targetProfile.setFriendlyName(poolName);

    SettingsManager::instance().saveConnectionProfile(targetProfile);
    SettingsManager::instance().updateServerHistory(profileName);
    SettingsManager::instance().sync();
}

void MainWindow::onCacheObjectChanged(XenConnection* connection, const QString& objectType, const QString& objectRef)
{
    if (!connection)
        return;

    // If the changed object is the currently displayed one, refresh the tabs
    if (objectType == this->m_currentObjectType && objectRef == this->m_currentObjectRef)
    {
        // Get updated data from cache
        QVariantMap objectData = connection->GetCache()->ResolveObjectData(objectType, objectRef);
        if (!objectData.isEmpty())
        {
            // Update tab pages with new data
            for (int i = 0; i < this->ui->mainTabWidget->count(); ++i)
            {
                BaseTabPage* tabPage = qobject_cast<BaseTabPage*>(this->ui->mainTabWidget->widget(i));
                if (tabPage)
                {
                    tabPage->SetXenObject(connection, objectType, objectRef, objectData);
                }
            }
        }
    }
}

void MainWindow::onMessageReceived(const QString& messageRef, const QVariantMap& messageData)
{
    (void) messageRef;
    // C# Reference: MainWindow.cs line 1000 - Alert.AddAlert(MessageAlert.ParseMessage(m))
    // Create alert from XenAPI message and add to AlertManager
    
    // Use factory method to create appropriate alert type
    XenConnection* connection = qobject_cast<XenConnection*>(sender());
    if (!connection)
        return;

    Alert* alert = MessageAlert::parseMessage(connection, messageData);
    if (alert)
    {
        AlertManager::instance()->addAlert(alert);
    }
}

void MainWindow::onMessageRemoved(const QString& messageRef)
{
    // C# Reference: MainWindow.cs line 1013 - MessageAlert.RemoveAlert(m)
    // Remove alert when XenAPI message is deleted
    
    MessageAlert::removeAlert(messageRef);
}

// Connection handler implementations
void MainWindow::handleConnectionSuccess(ConnectionContext* context, bool connected)
{
    if (!connected)
        return; // Ignore disconnection, wait for specific error signals

    // Clean up connections
    this->cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    this->ui->statusbar->showMessage("Connected to " + context->hostname, 5000);

    // Delegate tree building to NavigationView which respects current navigation mode
    if (this->m_navigationPane)
    {
        this->m_navigationPane->RequestRefreshTreeView();
    }

    // Save profile if requested
    if (context->saveProfile)
    {
        SettingsManager::instance().saveConnectionProfile(*context->profile);
        SettingsManager::instance().setLastConnectionProfile(context->profile->name());
        qDebug() << "XenAdmin Qt: Saved connection profile for" << context->hostname;
    }

    delete context->profile;
    delete context;
}

void MainWindow::handleConnectionError(ConnectionContext* context, const QString& error)
{
    // Clean up connections
    this->cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    QString errorMsg = "Failed to connect to " + context->hostname + ".\n\nError: " + error +
                       "\n\nPlease check your connection details and try again.";
    QMessageBox::critical(this, "Connection Failed", errorMsg);
    this->ui->statusbar->showMessage("Connection failed", 5000);

    delete context->profile;
    delete context;
}

void MainWindow::handleInitialAuthFailed(ConnectionContext* context)
{
    // Clean up these initial connection handlers
    this->cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    delete context->profile;
    delete context;

    // Don't show any error here - onAuthenticationFailed() will handle it
}

void MainWindow::handleRetryAuthFailed(ConnectionContext* context)
{
    // Clean up retry connections before showing retry dialog again
    this->cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    delete context->profile;
    delete context;

    // The signal will trigger onAuthenticationFailed() again, creating a new retry dialog
}

void MainWindow::cleanupConnectionContext(ConnectionContext* context)
{
    if (context->successConn)
    {
        disconnect(*context->successConn);
        delete context->successConn;
        context->successConn = nullptr;
    }
    if (context->errorConn)
    {
        disconnect(*context->errorConn);
        delete context->errorConn;
        context->errorConn = nullptr;
    }
    if (context->authFailedConn)
    {
        disconnect(*context->authFailedConn);
        delete context->authFailedConn;
        context->authFailedConn = nullptr;
    }
}

// Operation progress tracking (matches C# History_CollectionChanged pattern)
void MainWindow::onNewOperation(AsyncOperation* operation)
{
    if (!operation)
        return;

    // Set this operation as the one to track in status bar (matches C# statusBarAction = action)
    this->m_statusBarAction = operation;

    // Connect to operation's progress and completion signals (matches C# action.Changed/Completed events)
    connect(operation, &AsyncOperation::progressChanged, this, &MainWindow::onOperationProgressChanged);
    connect(operation, &AsyncOperation::completed, this, &MainWindow::onOperationCompleted);
    connect(operation, &AsyncOperation::failed, this, &MainWindow::onOperationFailed);
    connect(operation, &AsyncOperation::cancelled, this, &MainWindow::onOperationCancelled);

    // Show initial status
    this->m_statusLabel->setText(operation->title());
    this->m_statusProgressBar->setValue(0);
    this->m_statusProgressBar->setVisible(true);
}

void MainWindow::onOperationProgressChanged(int percent)
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    if (!operation || operation != this->m_statusBarAction)
        return; // Not the operation we're tracking

    // Update progress bar (matches C# UpdateStatusProgressBar)
    if (percent < 0)
        percent = 0;
    else if (percent > 100)
        percent = 100;

    this->m_statusProgressBar->setValue(percent);
    this->m_statusLabel->setText(operation->title());
}

void MainWindow::onOperationCompleted()
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    this->finalizeOperation(operation, AsyncOperation::Completed);
}

void MainWindow::onOperationFailed(const QString& error)
{
    Q_UNUSED(error);
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    this->finalizeOperation(operation, AsyncOperation::Failed);
}

void MainWindow::onOperationCancelled()
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    this->finalizeOperation(operation, AsyncOperation::Cancelled);
}

void MainWindow::finalizeOperation(AsyncOperation* operation, AsyncOperation::OperationState state, const QString& errorMessage)
{
    if (!operation)
        return;

    // Disconnect signals
    disconnect(operation, &AsyncOperation::progressChanged, this, &MainWindow::onOperationProgressChanged);
    disconnect(operation, &AsyncOperation::completed, this, &MainWindow::onOperationCompleted);
    disconnect(operation, &AsyncOperation::failed, this, &MainWindow::onOperationFailed);
    disconnect(operation, &AsyncOperation::cancelled, this, &MainWindow::onOperationCancelled);

    // Only update status bar if this is the tracked action
    if (this->m_statusBarAction == operation)
    {
        this->m_statusProgressBar->setVisible(false);

        QString title = operation->title();
        switch (state)
        {
            case AsyncOperation::Completed:
                this->m_statusLabel->setText(QString("%1 completed successfully").arg(title));
                this->ui->statusbar->showMessage(QString("%1 completed successfully").arg(title), 5000);
                break;
            case AsyncOperation::Failed:
            {
                QString errorText = !errorMessage.isEmpty() ? errorMessage : operation->errorMessage();
                if (errorText.isEmpty())
                    errorText = tr("Unknown error");
                QString shortError = operation->shortErrorMessage();
                QString statusErrorText = shortError.isEmpty() ? errorText : shortError;
                this->m_statusLabel->setText(QString("%1 failed").arg(title));
                this->ui->statusbar->showMessage(QString("%1 failed: %2").arg(title, statusErrorText), 10000);
                break;
            }
            case AsyncOperation::Cancelled:
                this->m_statusLabel->setText(QString("%1 cancelled").arg(title));
                this->ui->statusbar->showMessage(QString("%1 was cancelled").arg(title), 5000);
                break;
            default:
                break;
        }

        this->m_statusBarAction = nullptr;
    }

    // C# relies on event poller updates; no explicit cache refresh here.
}

void MainWindow::initializeToolbar()
{
    // Get toolbar from UI file (matches C# ToolStrip in MainWindow.Designer.cs)
    this->m_toolBar = this->ui->mainToolBar;

    QAction* firstToolbarAction = this->m_toolBar->actions().isEmpty() ? nullptr : this->m_toolBar->actions().first();

    // Add Back button with dropdown at the beginning (C# backButton - ToolStripSplitButton)
    this->m_backButton = new QToolButton(this);
    this->m_backButton->setIcon(QIcon(":/icons/back.png"));
    this->m_backButton->setText("Back");
    this->m_backButton->setToolTip("Back");
    this->m_backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    this->m_backButton->setPopupMode(QToolButton::MenuButtonPopup); // Split button style
    QMenu* backMenu = new QMenu(this->m_backButton);
    this->m_backButton->setMenu(backMenu);
    connect(this->m_backButton, &QToolButton::clicked, this, &MainWindow::onBackButton);
    connect(backMenu, &QMenu::aboutToShow, this, [this, backMenu]() {
        if (this->m_navigationHistory)
        {
            this->m_navigationHistory->populateBackDropDown(backMenu);
        }
    });
    this->m_toolBar->insertWidget(firstToolbarAction, this->m_backButton);

    // Add Forward button with dropdown (C# forwardButton - ToolStripSplitButton)
    this->m_forwardButton = new QToolButton(this);
    this->m_forwardButton->setIcon(QIcon(":/icons/forward.png"));
    this->m_forwardButton->setText("Forward");
    this->m_forwardButton->setToolTip("Forward");
    this->m_forwardButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    this->m_forwardButton->setPopupMode(QToolButton::MenuButtonPopup); // Split button style
    QMenu* forwardMenu = new QMenu(this->m_forwardButton);
    this->m_forwardButton->setMenu(forwardMenu);
    connect(this->m_forwardButton, &QToolButton::clicked, this, &MainWindow::onForwardButton);
    connect(forwardMenu, &QMenu::aboutToShow, this, [this, forwardMenu]() {
        if (this->m_navigationHistory)
        {
            this->m_navigationHistory->populateForwardDropDown(forwardMenu);
        }
    });
    this->m_toolBar->insertWidget(firstToolbarAction, this->m_forwardButton);

    // Add separator after navigation buttons
    this->m_toolBar->insertSeparator(this->m_toolBar->actions().first());

    // Connect toolbar actions to slots (actions defined in mainwindow.ui)
    connect(ui->addServerAction, &QAction::triggered, this, &MainWindow::connectToServer);
    connect(ui->newStorageAction, &QAction::triggered, this, &MainWindow::showNewStorageRepositoryWizard);
    connect(ui->newVmAction, &QAction::triggered, this, &MainWindow::onNewVM);
    connect(ui->shutDownAction, &QAction::triggered, this, &MainWindow::onShutDownButton);
    connect(ui->powerOnHostAction, &QAction::triggered, this, &MainWindow::onPowerOnHostButton);
    connect(ui->startVMAction, &QAction::triggered, this, &MainWindow::onStartVMButton);
    connect(ui->rebootAction, &QAction::triggered, this, &MainWindow::onRebootButton);
    connect(ui->resumeAction, &QAction::triggered, this, &MainWindow::onResumeButton);
    connect(ui->suspendAction, &QAction::triggered, this, &MainWindow::onSuspendButton);
    connect(ui->pauseAction, &QAction::triggered, this, &MainWindow::onPauseButton);
    connect(ui->unpauseAction, &QAction::triggered, this, &MainWindow::onUnpauseButton);
    connect(ui->forceShutdownAction, &QAction::triggered, this, &MainWindow::onForceShutdownButton);
    connect(ui->forceRebootAction, &QAction::triggered, this, &MainWindow::onForceRebootButton);

    // TODO: Add Pool connections when implemented
    // TODO: Add Docker container buttons when needed

    // Initial state - disable all action buttons
    this->updateToolbarsAndMenus();
}

void MainWindow::updateToolbarsAndMenus()
{
    // Matches C# MainWindow.UpdateToolbars() which calls UpdateToolbarsCore() and MainMenuBar_MenuActivate()
    // This is the SINGLE source of truth for both toolbar AND menu item states
    // Both read from the same Command objects

    // Management buttons - driven by active connections (C# uses selection-based model)
    this->ui->addServerAction->setEnabled(true); // Always enabled
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    const bool anyConnected = connMgr && !connMgr->getConnectedConnections().isEmpty();
    this->ui->addPoolAction->setEnabled(anyConnected);
    this->ui->newStorageAction->setEnabled(anyConnected);
    this->ui->newVmAction->setEnabled(anyConnected);

    // Get current selection
    QTreeWidgetItem* currentItem = this->getServerTreeWidget()->currentItem();
    if (!currentItem)
    {
        // No selection - disable all operation buttons and menu items
        this->disableAllOperationButtons();
        this->disableAllOperationMenus();
        return;
    }

    QString objectType;
    QString objectRef;
    XenConnection* connection = nullptr;
    QVariant data = currentItem->data(0, Qt::UserRole);
    if (data.canConvert<QSharedPointer<XenObject>>())
    {
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
        {
            objectType = obj->GetObjectType();
            objectRef = obj->OpaqueRef();
            connection = obj->GetConnection();
        }
    }
    else if (data.canConvert<XenConnection*>())
    {
        objectType = "disconnected_host";
        objectRef = QString();
    }
    else
    {
        this->disableAllOperationButtons();
        this->disableAllOperationMenus();
        return;
    }

    // ========================================================================
    // TOOLBAR BUTTONS - Read from Command.canRun() (matches C# CommandToolStripButton.Update)
    // ========================================================================

    // Polymorphic commands (work for both VMs and Hosts)
    bool canShutdown = this->m_commands["Shutdown"]->CanRun();
    bool canReboot = this->m_commands["Reboot"]->CanRun();

    // VM-specific commands
    bool canStartVM = this->m_commands["StartVM"]->CanRun();
    bool canResume = this->m_commands["ResumeVM"]->CanRun();
    bool canSuspend = this->m_commands["SuspendVM"]->CanRun();
    bool canPause = this->m_commands["PauseVM"]->CanRun();
    bool canUnpause = this->m_commands["UnpauseVM"]->CanRun();
    bool canForceShutdown = this->m_commands["ForceShutdownVM"]->CanRun();
    bool canForceReboot = this->m_commands["ForceRebootVM"]->CanRun();

    // Host-specific commands
    bool canPowerOnHost = this->m_commands["PowerOnHost"]->CanRun();

    // Container buttons availability (for future Docker support)
    bool containerButtonsAvailable = false; // TODO: Docker support

    // Update button states (matches C# UpdateToolbarsCore visibility logic)

    // Start VM - visible when enabled
    this->ui->startVMAction->setEnabled(canStartVM);
    this->ui->startVMAction->setVisible(canStartVM);

    // Power On Host - visible when enabled
    this->ui->powerOnHostAction->setEnabled(canPowerOnHost);
    this->ui->powerOnHostAction->setVisible(canPowerOnHost);

    // Shutdown - show when enabled OR as fallback when no start buttons available
    // Matches C#: shutDownToolStripButton.Available = shutDownToolStripButton.Enabled || (!startVMToolStripButton.Available && !powerOnHostToolStripButton.Available && !containerButtonsAvailable)
    bool showShutdown = canShutdown || (!canStartVM && !canPowerOnHost && !containerButtonsAvailable);
    this->ui->shutDownAction->setEnabled(canShutdown);
    this->ui->shutDownAction->setVisible(showShutdown);

    // Reboot - show when enabled OR as fallback
    // Matches C#: RebootToolbarButton.Available = RebootToolbarButton.Enabled || !containerButtonsAvailable
    bool showReboot = canReboot || !containerButtonsAvailable;
    this->ui->rebootAction->setEnabled(canReboot);
    this->ui->rebootAction->setVisible(showReboot);

    // Resume - show when enabled
    this->ui->resumeAction->setEnabled(canResume);
    this->ui->resumeAction->setVisible(canResume);

    // Suspend - show if enabled OR if resume not visible
    // Matches C#: SuspendToolbarButton.Available = SuspendToolbarButton.Enabled || (!resumeToolStripButton.Available && !containerButtonsAvailable)
    bool showSuspend = canSuspend || (!canResume && !containerButtonsAvailable);
    this->ui->suspendAction->setEnabled(canSuspend);
    this->ui->suspendAction->setVisible(showSuspend);

    // Pause - show if enabled OR if unpause not visible
    // Matches C#: PauseVmToolbarButton.Available = PauseVmToolbarButton.Enabled || !UnpauseVmToolbarButton.Available
    bool showPause = canPause || !canUnpause;
    this->ui->pauseAction->setEnabled(canPause);
    this->ui->pauseAction->setVisible(showPause);

    // Unpause - show when enabled
    this->ui->unpauseAction->setEnabled(canUnpause);
    this->ui->unpauseAction->setVisible(canUnpause);

    // Force Shutdown - show based on Command.ShowOnMainToolBar property
    // Matches C#: ForceShutdownToolbarButton.Available = ((ForceVMShutDownCommand)ForceShutdownToolbarButton.Command).ShowOnMainToolBar
    bool hasCleanShutdown = false;
    bool hasCleanReboot = false;
    if (objectType == "vm" && connection && connection->GetCache())
    {
        QVariantMap vmData = connection->GetCache()->ResolveObjectData("vm", objectRef);
        QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
        for (const QVariant& op : allowedOps)
        {
            if (op.toString() == "clean_shutdown")
                hasCleanShutdown = true;
            if (op.toString() == "clean_reboot")
                hasCleanReboot = true;
        }
    }
    bool showForceShutdown = canForceShutdown && !hasCleanShutdown;
    bool showForceReboot = canForceReboot && !hasCleanReboot;

    this->ui->forceShutdownAction->setEnabled(canForceShutdown);
    this->ui->forceShutdownAction->setVisible(showForceShutdown);

    this->ui->forceRebootAction->setEnabled(canForceReboot);
    this->ui->forceRebootAction->setVisible(showForceReboot);

    // ========================================================================
    // MENU ITEMS - Read from Command.canRun() (matches C# MainMenuBar_MenuActivate)
    // ========================================================================

    // Server menu - use the polymorphic Shutdown/Reboot commands
    this->ui->ReconnectToolStripMenuItem1->setEnabled(this->m_commands["ReconnectHost"]->CanRun());
    this->ui->DisconnectToolStripMenuItem->setEnabled(this->m_commands["DisconnectHost"]->CanRun());
    this->ui->connectAllToolStripMenuItem->setEnabled(this->m_commands["ConnectAllHosts"]->CanRun());
    this->ui->disconnectAllToolStripMenuItem->setEnabled(this->m_commands["DisconnectAllHosts"]->CanRun());
    this->ui->restartToolstackAction->setEnabled(this->m_commands["RestartToolstack"]->CanRun());
    this->ui->reconnectAsToolStripMenuItem->setEnabled(this->m_commands["HostReconnectAs"]->CanRun());
    this->ui->rebootAction->setEnabled(canReboot);           // Use same variable as toolbar
    this->ui->shutDownAction->setEnabled(canShutdown);       // Use same variable as toolbar
    this->ui->powerOnHostAction->setEnabled(canPowerOnHost); // Use same variable as toolbar
    this->ui->maintenanceModeToolStripMenuItem1->setEnabled(this->m_commands["HostMaintenanceMode"]->CanRun());
    this->ui->ServerPropertiesToolStripMenuItem->setEnabled(this->m_commands["HostProperties"]->CanRun());

    // Pool menu
    this->ui->AddPoolToolStripMenuItem->setEnabled(this->m_commands["NewPool"]->CanRun());
    this->ui->deleteToolStripMenuItem->setEnabled(this->m_commands["DeletePool"]->CanRun());
    this->ui->toolStripMenuItemHaConfigure->setEnabled(this->m_commands["HAConfigure"]->CanRun());
    this->ui->toolStripMenuItemHaDisable->setEnabled(this->m_commands["HADisable"]->CanRun());
    this->ui->PoolPropertiesToolStripMenuItem->setEnabled(this->m_commands["PoolProperties"]->CanRun());
    this->ui->addServerToPoolMenuItem->setEnabled(this->m_commands["JoinPool"]->CanRun());
    this->ui->menuItemRemoveFromPool->setEnabled(this->m_commands["EjectHostFromPool"]->CanRun());

    // VM menu
    this->ui->newVmAction->setEnabled(this->m_commands["NewVM"]->CanRun());
    this->ui->startShutdownToolStripMenuItem->setEnabled(this->m_commands["VMLifeCycle"]->CanRun());
    this->ui->copyVMtoSharedStorageMenuItem->setEnabled(this->m_commands["CopyVM"]->CanRun());
    this->ui->MoveVMToolStripMenuItem->setEnabled(this->m_commands["MoveVM"]->CanRun());
    this->ui->installToolsToolStripMenuItem->setEnabled(this->m_commands["InstallTools"]->CanRun());
    this->ui->uninstallToolStripMenuItem->setEnabled(this->m_commands["UninstallVM"]->CanRun());
    this->ui->VMPropertiesToolStripMenuItem->setEnabled(this->m_commands["VMProperties"]->CanRun());
    this->ui->snapshotToolStripMenuItem->setEnabled(this->m_commands["TakeSnapshot"]->CanRun());
    this->ui->convertToTemplateToolStripMenuItem->setEnabled(this->m_commands["ConvertVMToTemplate"]->CanRun());
    this->ui->exportToolStripMenuItem->setEnabled(this->m_commands["ExportVM"]->CanRun());

    // Update dynamic menu text for VMLifeCycle command
    this->ui->startShutdownToolStripMenuItem->setText(this->m_commands["VMLifeCycle"]->MenuText());

    // Template menu
    this->ui->newVMFromTemplateToolStripMenuItem->setEnabled(this->m_commands["NewVMFromTemplate"]->CanRun());
    this->ui->InstantVmToolStripMenuItem->setEnabled(this->m_commands["InstantVMFromTemplate"]->CanRun());
    this->ui->exportTemplateToolStripMenuItem->setEnabled(this->m_commands["ExportTemplate"]->CanRun());
    this->ui->duplicateTemplateToolStripMenuItem->setEnabled(this->m_commands["CopyTemplate"]->CanRun());
    this->ui->uninstallTemplateToolStripMenuItem->setEnabled(this->m_commands["DeleteTemplate"]->CanRun());
    this->ui->templatePropertiesToolStripMenuItem->setEnabled(this->m_commands["VMProperties"]->CanRun());

    // Storage menu
    this->ui->addVirtualDiskToolStripMenuItem->setEnabled(this->m_commands["AddVirtualDisk"]->CanRun());
    this->ui->attachVirtualDiskToolStripMenuItem->setEnabled(this->m_commands["AttachVirtualDisk"]->CanRun());
    this->ui->DetachStorageToolStripMenuItem->setEnabled(this->m_commands["DetachSR"]->CanRun());
    this->ui->ReattachStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["ReattachSR"]->CanRun());
    this->ui->ForgetStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["ForgetSR"]->CanRun());
    this->ui->DestroyStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["DestroySR"]->CanRun());
    this->ui->RepairStorageToolStripMenuItem->setEnabled(this->m_commands["RepairSR"]->CanRun());
    this->ui->DefaultSRToolStripMenuItem->setEnabled(this->m_commands["SetDefaultSR"]->CanRun());
    this->ui->newStorageRepositoryAction->setEnabled(this->m_commands["NewSR"]->CanRun());
    this->ui->virtualDisksToolStripMenuItem->setEnabled(this->m_commands["StorageProperties"]->CanRun());

    // Network menu
    this->ui->newNetworkAction->setEnabled(this->m_commands["NewNetwork"]->CanRun());
}

void MainWindow::disableAllOperationButtons()
{
    // Disable and hide all VM/Host operation toolbar actions
    this->ui->shutDownAction->setEnabled(false);
    this->ui->shutDownAction->setVisible(false);
    this->ui->powerOnHostAction->setEnabled(false);
    this->ui->powerOnHostAction->setVisible(false);
    this->ui->startVMAction->setEnabled(false);
    this->ui->startVMAction->setVisible(false);
    this->ui->rebootAction->setEnabled(false);
    this->ui->rebootAction->setVisible(false);
    this->ui->resumeAction->setEnabled(false);
    this->ui->resumeAction->setVisible(false);
    this->ui->suspendAction->setEnabled(false);
    this->ui->suspendAction->setVisible(false);
    this->ui->pauseAction->setEnabled(false);
    this->ui->pauseAction->setVisible(false);
    this->ui->unpauseAction->setEnabled(false);
    this->ui->unpauseAction->setVisible(false);
    this->ui->forceShutdownAction->setEnabled(false);
    this->ui->forceShutdownAction->setVisible(false);
    this->ui->forceRebootAction->setEnabled(false);
    this->ui->forceRebootAction->setVisible(false);
}

void MainWindow::disableAllOperationMenus()
{
    // Disable operation menu items (matches toolbar button disable)
    this->ui->shutDownAction->setEnabled(false);
    this->ui->rebootAction->setEnabled(false);
    this->ui->powerOnHostAction->setEnabled(false);
    this->ui->startShutdownToolStripMenuItem->setEnabled(false);
    // Add more as needed
}

void MainWindow::onBackButton()
{
    // Matches C# MainWindow.backButton_Click (History.Back(1))
    if (this->m_navigationHistory)
    {
        this->m_navigationHistory->back(1);
    }
}

void MainWindow::onForwardButton()
{
    // Matches C# MainWindow.forwardButton_Click (History.Forward(1))
    if (this->m_navigationHistory)
    {
        this->m_navigationHistory->forward(1);
    }
}

void MainWindow::updateHistoryButtons(bool canGoBack, bool canGoForward)
{
    // Update toolbar button enabled state based on history availability
    if (this->m_backButton)
        this->m_backButton->setEnabled(canGoBack);
    if (this->m_forwardButton)
        this->m_forwardButton->setEnabled(canGoForward);
}

// Navigation support for history (matches C# MainWindow)
void MainWindow::selectObjectInTree(const QString& objectRef, const QString& objectType)
{
    // Find and select the tree item with matching objectRef
    QTreeWidgetItemIterator it(this->getServerTreeWidget());
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        QVariant data = item->data(0, Qt::UserRole);
        
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj && obj->OpaqueRef() == objectRef && obj->GetObjectType() == objectType)
        {
            // Found the item - select it
            this->getServerTreeWidget()->setCurrentItem(item);
            this->getServerTreeWidget()->scrollToItem(item);
            return;
        }
        ++it;
    }

    qWarning() << "NavigationHistory: Could not find object in tree:" << objectRef << "type:" << objectType;
}

void MainWindow::setCurrentTab(const QString& tabName)
{
    // Find and activate tab by name
    for (int i = 0; i < this->ui->mainTabWidget->count(); ++i)
    {
        if (this->ui->mainTabWidget->tabText(i) == tabName)
        {
            this->ui->mainTabWidget->setCurrentIndex(i);
            return;
        }
    }

    // Tab not found - just keep current tab
    qDebug() << "NavigationHistory: Could not find tab:" << tabName;
}

// Toolbar VM operation button handlers (matches C# MainWindow toolbar command execution)
void MainWindow::onStartVMButton()
{
    // Matches C# StartVMCommand execution from toolbar
    StartVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onShutDownButton()
{
    // Use polymorphic Shutdown command (handles both VMs and Hosts - matches C# ShutDownCommand)
    if (this->m_commands.contains("Shutdown"))
        this->m_commands["Shutdown"]->Run();
}

void MainWindow::onRebootButton()
{
    // Use polymorphic Reboot command (handles both VMs and Hosts - matches C# RebootCommand pattern)
    if (this->m_commands.contains("Reboot"))
        this->m_commands["Reboot"]->Run();
}

void MainWindow::onResumeButton()
{
    // Matches C# ResumeVMCommand execution from toolbar
    ResumeVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onSuspendButton()
{
    // Matches C# SuspendVMCommand execution from toolbar
    SuspendVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onPauseButton()
{
    // Matches C# PauseVMCommand execution from toolbar
    PauseVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onUnpauseButton()
{
    // Matches C# UnPauseVMCommand execution from toolbar
    UnpauseVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onForceShutdownButton()
{
    // Matches C# ForceVMShutDownCommand execution from toolbar
    ForceShutdownVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

void MainWindow::onForceRebootButton()
{
    // Matches C# ForceVMRebootCommand execution from toolbar
    ForceRebootVMCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

// Toolbar Host operation button handlers
void MainWindow::onPowerOnHostButton()
{
    // Matches C# PowerOnHostCommand execution from toolbar
    PowerOnHostCommand cmd(this);
    if (cmd.CanRun())
        cmd.Run();
}

// Toolbar Container operation button handlers
void MainWindow::onStartContainerButton()
{
    // Matches C# StartDockerContainerCommand execution from toolbar
    // TODO: Implement Docker container commands
    QMessageBox::information(this, "Not Implemented", "Docker container operations will be implemented in a future update.");
}

void MainWindow::onStopContainerButton()
{
    // Matches C# StopDockerContainerCommand execution from toolbar
    QMessageBox::information(this, "Not Implemented", "Docker container operations will be implemented in a future update.");
}

void MainWindow::onRestartContainerButton()
{
    // Matches C# RestartDockerContainerCommand execution from toolbar
    QMessageBox::information(this, "Not Implemented", "Docker container operations will be implemented in a future update.");
}

void MainWindow::onPauseContainerButton()
{
    // Matches C# PauseDockerContainerCommand execution from toolbar
    QMessageBox::information(this, "Not Implemented", "Docker container operations will be implemented in a future update.");
}

void MainWindow::onResumeContainerButton()
{
    // Matches C# ResumeDockerContainerCommand execution from toolbar
    QMessageBox::information(this, "Not Implemented", "Docker container operations will be implemented in a future update.");
}

// ============================================================================
// Menu action slot handlers
// ============================================================================

// Server menu slots
void MainWindow::onReconnectHost()
{
    if (this->m_commands.contains("ReconnectHost"))
        this->m_commands["ReconnectHost"]->Run();
}

void MainWindow::onDisconnectHost()
{
    if (this->m_commands.contains("DisconnectHost"))
        this->m_commands["DisconnectHost"]->Run();
}

void MainWindow::onConnectAllHosts()
{
    if (this->m_commands.contains("ConnectAllHosts"))
        this->m_commands["ConnectAllHosts"]->Run();
}

void MainWindow::onDisconnectAllHosts()
{
    if (this->m_commands.contains("DisconnectAllHosts"))
        this->m_commands["DisconnectAllHosts"]->Run();
}

void MainWindow::onRestartToolstack()
{
    if (this->m_commands.contains("RestartToolstack"))
        this->m_commands["RestartToolstack"]->Run();
}

void MainWindow::onReconnectAs()
{
    if (this->m_commands.contains("HostReconnectAs"))
        this->m_commands["HostReconnectAs"]->Run();
}

void MainWindow::onMaintenanceMode()
{
    if (this->m_commands.contains("HostMaintenanceMode"))
        this->m_commands["HostMaintenanceMode"]->Run();
}

void MainWindow::onServerProperties()
{
    if (this->m_commands.contains("HostProperties"))
        this->m_commands["HostProperties"]->Run();
}

// Pool menu slots
void MainWindow::onNewPool()
{
    if (this->m_commands.contains("NewPool"))
        this->m_commands["NewPool"]->Run();
}

void MainWindow::onDeletePool()
{
    if (this->m_commands.contains("DeletePool"))
        this->m_commands["DeletePool"]->Run();
}

void MainWindow::onHAConfigure()
{
    if (this->m_commands.contains("HAConfigure"))
        this->m_commands["HAConfigure"]->Run();
}

void MainWindow::onHADisable()
{
    if (this->m_commands.contains("HADisable"))
        this->m_commands["HADisable"]->Run();
}

void MainWindow::onPoolProperties()
{
    if (this->m_commands.contains("PoolProperties"))
        this->m_commands["PoolProperties"]->Run();
}

void MainWindow::onJoinPool()
{
    if (this->m_commands.contains("JoinPool"))
        this->m_commands["JoinPool"]->Run();
}

void MainWindow::onEjectFromPool()
{
    if (this->m_commands.contains("EjectHostFromPool"))
        this->m_commands["EjectHostFromPool"]->Run();
}

// VM menu slots
void MainWindow::onNewVM()
{
    if (this->m_commands.contains("NewVM"))
        this->m_commands["NewVM"]->Run();
}

void MainWindow::onStartShutdownVM()
{
    if (this->m_commands.contains("VMLifeCycle"))
        this->m_commands["VMLifeCycle"]->Run();
}

void MainWindow::onCopyVM()
{
    if (this->m_commands.contains("CopyVM"))
        this->m_commands["CopyVM"]->Run();
}

void MainWindow::onMoveVM()
{
    if (this->m_commands.contains("MoveVM"))
        this->m_commands["MoveVM"]->Run();
}

void MainWindow::onInstallTools()
{
    if (this->m_commands.contains("InstallTools"))
        this->m_commands["InstallTools"]->Run();
}

void MainWindow::onUninstallVM()
{
    if (this->m_commands.contains("UninstallVM"))
        this->m_commands["UninstallVM"]->Run();
}

void MainWindow::onVMProperties()
{
    if (this->m_commands.contains("VMProperties"))
        this->m_commands["VMProperties"]->Run();
}

void MainWindow::onTakeSnapshot()
{
    if (this->m_commands.contains("TakeSnapshot"))
        this->m_commands["TakeSnapshot"]->Run();
}

void MainWindow::onConvertToTemplate()
{
    if (this->m_commands.contains("ConvertVMToTemplate"))
        this->m_commands["ConvertVMToTemplate"]->Run();
}

void MainWindow::onExportVM()
{
    if (this->m_commands.contains("ExportVM"))
        this->m_commands["ExportVM"]->Run();
}

// Template menu slots
void MainWindow::onNewVMFromTemplate()
{
    if (this->m_commands.contains("NewVMFromTemplate"))
        this->m_commands["NewVMFromTemplate"]->Run();
}

void MainWindow::onInstantVM()
{
    if (this->m_commands.contains("InstantVMFromTemplate"))
        this->m_commands["InstantVMFromTemplate"]->Run();
}

void MainWindow::onExportTemplate()
{
    if (this->m_commands.contains("ExportTemplate"))
        this->m_commands["ExportTemplate"]->Run();
}

void MainWindow::onDuplicateTemplate()
{
    if (this->m_commands.contains("CopyTemplate"))
        this->m_commands["CopyTemplate"]->Run();
}

void MainWindow::onDeleteTemplate()
{
    if (this->m_commands.contains("DeleteTemplate"))
        this->m_commands["DeleteTemplate"]->Run();
}

void MainWindow::onTemplateProperties()
{
    if (this->m_commands.contains("VMProperties"))
        this->m_commands["VMProperties"]->Run(); // Use VMProperties for templates too
}

// Storage menu slots
void MainWindow::onAddVirtualDisk()
{
    if (this->m_commands.contains("AddVirtualDisk"))
        this->m_commands["AddVirtualDisk"]->Run();
}

void MainWindow::onAttachVirtualDisk()
{
    if (this->m_commands.contains("AttachVirtualDisk"))
        this->m_commands["AttachVirtualDisk"]->Run();
}

void MainWindow::onDetachSR()
{
    if (this->m_commands.contains("DetachSR"))
        this->m_commands["DetachSR"]->Run();
}

void MainWindow::onReattachSR()
{
    if (this->m_commands.contains("ReattachSR"))
        this->m_commands["ReattachSR"]->Run();
}

void MainWindow::onForgetSR()
{
    if (this->m_commands.contains("ForgetSR"))
        this->m_commands["ForgetSR"]->Run();
}

void MainWindow::onDestroySR()
{
    if (this->m_commands.contains("DestroySR"))
        this->m_commands["DestroySR"]->Run();
}

void MainWindow::onRepairSR()
{
    if (this->m_commands.contains("RepairSR"))
        this->m_commands["RepairSR"]->Run();
}

void MainWindow::onSetDefaultSR()
{
    if (this->m_commands.contains("SetDefaultSR"))
        this->m_commands["SetDefaultSR"]->Run();
}

void MainWindow::onNewSR()
{
    if (this->m_commands.contains("NewSR"))
        this->m_commands["NewSR"]->Run();
}

void MainWindow::onStorageProperties()
{
    if (this->m_commands.contains("StorageProperties"))
        this->m_commands["StorageProperties"]->Run();
}

// Network menu slots
void MainWindow::onNewNetwork()
{
    if (this->m_commands.contains("NewNetwork"))
        this->m_commands["NewNetwork"]->Run();
}

QString MainWindow::getSelectedObjectRef() const
{
    QTreeWidgetItem* item = this->getServerTreeWidget()->currentItem();
    if (!item)
        return QString();

    QSharedPointer<XenObject> obj = item->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
    return obj ? obj->OpaqueRef() : QString();
}

QString MainWindow::getSelectedObjectName() const
{
    QTreeWidgetItem* item = this->getServerTreeWidget()->currentItem();
    if (!item)
        return QString();

    return item->text(0);
}

// ============================================================================
// Command System (matches C# SelectionManager.BindTo pattern)
// ============================================================================

void MainWindow::initializeCommands()
{
    // Polymorphic commands (handle both VMs and Hosts - matches C# ShutDownCommand/RebootCommand)
    this->m_commands["Shutdown"] = new ShutdownCommand(this, this);
    this->m_commands["Reboot"] = new RebootCommand(this, this);

    // Server/Host commands
    this->m_commands["ReconnectHost"] = new ReconnectHostCommand(this, this);
    this->m_commands["DisconnectHost"] = new DisconnectHostCommand(this, this);
    this->m_commands["ConnectAllHosts"] = new ConnectAllHostsCommand(this, this);
    this->m_commands["DisconnectAllHosts"] = new DisconnectAllHostsCommand(this, this);
    this->m_commands["RestartToolstack"] = new RestartToolstackCommand(this, this);
    this->m_commands["HostReconnectAs"] = new HostReconnectAsCommand(this, this);
    this->m_commands["RebootHost"] = new RebootHostCommand(this, this);
    this->m_commands["ShutdownHost"] = new ShutdownHostCommand(this, this);
    this->m_commands["PowerOnHost"] = new PowerOnHostCommand(this, this);
    this->m_commands["HostMaintenanceMode"] = new HostMaintenanceModeCommand(this, true, this);
    this->m_commands["HostProperties"] = new HostPropertiesCommand(this, this);

    // Pool commands
    this->m_commands["NewPool"] = new NewPoolCommand(this, this);
    this->m_commands["DeletePool"] = new DeletePoolCommand(this, this);
    this->m_commands["HAConfigure"] = new HAConfigureCommand(this, this);
    this->m_commands["HADisable"] = new HADisableCommand(this, this);
    this->m_commands["PoolProperties"] = new PoolPropertiesCommand(this, this);
    this->m_commands["JoinPool"] = new JoinPoolCommand(this, this);
    this->m_commands["EjectHostFromPool"] = new EjectHostFromPoolCommand(this, this);

    // VM commands
    this->m_commands["StartVM"] = new StartVMCommand(this, this);
    this->m_commands["StopVM"] = new StopVMCommand(this, this);
    this->m_commands["RestartVM"] = new RestartVMCommand(this, this);
    this->m_commands["SuspendVM"] = new SuspendVMCommand(this, this);
    this->m_commands["ResumeVM"] = new ResumeVMCommand(this, this);
    this->m_commands["PauseVM"] = new PauseVMCommand(this, this);
    this->m_commands["UnpauseVM"] = new UnpauseVMCommand(this, this);
    this->m_commands["ForceShutdownVM"] = new ForceShutdownVMCommand(this, this);
    this->m_commands["ForceRebootVM"] = new ForceRebootVMCommand(this, this);
    this->m_commands["MigrateVM"] = new MigrateVMCommand(this, this);
    this->m_commands["CloneVM"] = new CloneVMCommand(this, this);
    this->m_commands["VMLifeCycle"] = new VMLifeCycleCommand(this, this);
    this->m_commands["CopyVM"] = new CopyVMCommand(this, this);
    this->m_commands["MoveVM"] = new MoveVMCommand(this, this);
    this->m_commands["InstallTools"] = new InstallToolsCommand(this, this);
    this->m_commands["UninstallVM"] = new UninstallVMCommand(this, this);
    this->m_commands["DeleteVM"] = new DeleteVMCommand(this, this);
    this->m_commands["ConvertVMToTemplate"] = new ConvertVMToTemplateCommand(this, this);
    this->m_commands["ExportVM"] = new ExportVMCommand(this, this);
    this->m_commands["NewVM"] = new NewVMCommand(this);
    this->m_commands["VMProperties"] = new VMPropertiesCommand(this);
    this->m_commands["TakeSnapshot"] = new TakeSnapshotCommand(this);
    this->m_commands["DeleteSnapshot"] = new DeleteSnapshotCommand(this);
    this->m_commands["RevertToSnapshot"] = new RevertToSnapshotCommand(this);
    this->m_commands["ImportVM"] = new ImportVMCommand(this);

    // Template commands
    this->m_commands["CreateVMFromTemplate"] = new CreateVMFromTemplateCommand(this, this);
    this->m_commands["NewVMFromTemplate"] = new NewVMFromTemplateCommand(this, this);
    this->m_commands["InstantVMFromTemplate"] = new InstantVMFromTemplateCommand(this, this);
    this->m_commands["CopyTemplate"] = new CopyTemplateCommand(this, this);
    this->m_commands["DeleteTemplate"] = new DeleteTemplateCommand(this, this);
    this->m_commands["ExportTemplate"] = new ExportTemplateCommand(this, this);

    // Storage commands
    this->m_commands["RepairSR"] = new RepairSRCommand(this, this);
    this->m_commands["DetachSR"] = new DetachSRCommand(this, this);
    this->m_commands["SetDefaultSR"] = new SetDefaultSRCommand(this, this);
    this->m_commands["NewSR"] = new NewSRCommand(this, this);
    this->m_commands["StorageProperties"] = new StoragePropertiesCommand(this, this);
    this->m_commands["AddVirtualDisk"] = new AddVirtualDiskCommand(this, this);
    this->m_commands["AttachVirtualDisk"] = new AttachVirtualDiskCommand(this, this);
    this->m_commands["ReattachSR"] = new ReattachSRCommand(this, this);
    this->m_commands["ForgetSR"] = new ForgetSRCommand(this, this);
    this->m_commands["DestroySR"] = new DestroySRCommand(this, this);

    // Network commands
    this->m_commands["NewNetwork"] = new NewNetworkCommand(this, this);
    this->m_commands["NetworkProperties"] = new NetworkPropertiesCommand(this, this);

    qDebug() << "Initialized" << this->m_commands.size() << "commands";
}

void MainWindow::connectMenuActions()
{
    // Server menu actions
    connect(this->ui->ReconnectToolStripMenuItem1, &QAction::triggered, this, &MainWindow::onReconnectHost);
    connect(this->ui->DisconnectToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDisconnectHost);
    connect(this->ui->connectAllToolStripMenuItem, &QAction::triggered, this, &MainWindow::onConnectAllHosts);
    connect(this->ui->disconnectAllToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDisconnectAllHosts);
    connect(this->ui->restartToolstackAction, &QAction::triggered, this, &MainWindow::onRestartToolstack);
    connect(this->ui->reconnectAsToolStripMenuItem, &QAction::triggered, this, &MainWindow::onReconnectAs);
    // Note: rebootAction, shutDownAction, powerOnHostAction are connected in initializeToolbar()
    // to avoid duplicate connections (toolbar and menu share the same QAction)
    connect(this->ui->maintenanceModeToolStripMenuItem1, &QAction::triggered, this, &MainWindow::onMaintenanceMode);
    connect(this->ui->ServerPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onServerProperties);

    // Pool menu actions
    connect(this->ui->AddPoolToolStripMenuItem, &QAction::triggered, this, &MainWindow::onNewPool);
    connect(this->ui->deleteToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDeletePool);
    connect(this->ui->toolStripMenuItemHaConfigure, &QAction::triggered, this, &MainWindow::onHAConfigure);
    connect(this->ui->toolStripMenuItemHaDisable, &QAction::triggered, this, &MainWindow::onHADisable);
    connect(this->ui->PoolPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onPoolProperties);
    connect(this->ui->addServerToPoolMenuItem, &QAction::triggered, this, &MainWindow::onJoinPool);
    connect(this->ui->menuItemRemoveFromPool, &QAction::triggered, this, &MainWindow::onEjectFromPool);

    // VM menu actions
    // Note: newVmAction is connected in initializeToolbar() (toolbar and menu share the same QAction)
    connect(this->ui->startShutdownToolStripMenuItem, &QAction::triggered, this, &MainWindow::onStartShutdownVM);
    connect(this->ui->copyVMtoSharedStorageMenuItem, &QAction::triggered, this, &MainWindow::onCopyVM);
    connect(this->ui->MoveVMToolStripMenuItem, &QAction::triggered, this, &MainWindow::onMoveVM);
    connect(this->ui->installToolsToolStripMenuItem, &QAction::triggered, this, &MainWindow::onInstallTools);
    connect(this->ui->uninstallToolStripMenuItem, &QAction::triggered, this, &MainWindow::onUninstallVM);
    connect(this->ui->VMPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onVMProperties);
    connect(this->ui->snapshotToolStripMenuItem, &QAction::triggered, this, &MainWindow::onTakeSnapshot);
    connect(this->ui->convertToTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onConvertToTemplate);
    connect(this->ui->exportToolStripMenuItem, &QAction::triggered, this, &MainWindow::onExportVM);

    // Template menu actions
    connect(this->ui->newVMFromTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onNewVMFromTemplate);
    connect(this->ui->InstantVmToolStripMenuItem, &QAction::triggered, this, &MainWindow::onInstantVM);
    connect(this->ui->exportTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onExportTemplate);
    connect(this->ui->duplicateTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDuplicateTemplate);
    connect(this->ui->uninstallTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDeleteTemplate);
    connect(this->ui->templatePropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onTemplateProperties);

    // Storage menu actions
    connect(this->ui->addVirtualDiskToolStripMenuItem, &QAction::triggered, this, &MainWindow::onAddVirtualDisk);
    connect(this->ui->attachVirtualDiskToolStripMenuItem, &QAction::triggered, this, &MainWindow::onAttachVirtualDisk);
    connect(this->ui->DetachStorageToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDetachSR);
    connect(this->ui->ReattachStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onReattachSR);
    connect(this->ui->ForgetStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onForgetSR);
    connect(this->ui->DestroyStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDestroySR);
    connect(this->ui->RepairStorageToolStripMenuItem, &QAction::triggered, this, &MainWindow::onRepairSR);
    connect(this->ui->DefaultSRToolStripMenuItem, &QAction::triggered, this, &MainWindow::onSetDefaultSR);
    connect(this->ui->newStorageRepositoryAction, &QAction::triggered, this, &MainWindow::onNewSR);
    connect(this->ui->virtualDisksToolStripMenuItem, &QAction::triggered, this, &MainWindow::onStorageProperties);

    // Network menu actions
    connect(this->ui->newNetworkAction, &QAction::triggered, this, &MainWindow::onNewNetwork);

    // View menu actions (filters)
    connect(this->ui->viewTemplatesAction, &QAction::toggled, this, &MainWindow::onViewTemplatesToggled);
    connect(this->ui->viewCustomTemplatesAction, &QAction::toggled, this, &MainWindow::onViewCustomTemplatesToggled);
    connect(this->ui->viewLocalStorageAction, &QAction::toggled, this, &MainWindow::onViewLocalStorageToggled);
    connect(this->ui->viewShowHiddenObjectsAction, &QAction::toggled, this, &MainWindow::onViewShowHiddenObjectsToggled);

    qDebug() << "Connected menu actions to command slots";
}

void MainWindow::updateMenuItems()
{
    // Update menu items based on command canRun() state
    // This matches C# CommandToolStripMenuItem.Update() pattern

    // Server menu
    this->ui->ReconnectToolStripMenuItem1->setEnabled(this->m_commands["ReconnectHost"]->CanRun());
    this->ui->DisconnectToolStripMenuItem->setEnabled(this->m_commands["DisconnectHost"]->CanRun());
    this->ui->connectAllToolStripMenuItem->setEnabled(this->m_commands["ConnectAllHosts"]->CanRun());
    this->ui->disconnectAllToolStripMenuItem->setEnabled(this->m_commands["DisconnectAllHosts"]->CanRun());
    this->ui->restartToolstackAction->setEnabled(this->m_commands["RestartToolstack"]->CanRun());
    this->ui->reconnectAsToolStripMenuItem->setEnabled(this->m_commands["HostReconnectAs"]->CanRun());
    this->ui->rebootAction->setEnabled(this->m_commands["Reboot"]->CanRun());     // Use polymorphic Reboot command
    this->ui->shutDownAction->setEnabled(this->m_commands["Shutdown"]->CanRun()); // Use polymorphic Shutdown command
    this->ui->powerOnHostAction->setEnabled(this->m_commands["PowerOnHost"]->CanRun());
    this->ui->maintenanceModeToolStripMenuItem1->setEnabled(this->m_commands["HostMaintenanceMode"]->CanRun());
    this->ui->ServerPropertiesToolStripMenuItem->setEnabled(this->m_commands["HostProperties"]->CanRun());

    // Pool menu
    this->ui->AddPoolToolStripMenuItem->setEnabled(this->m_commands["NewPool"]->CanRun());
    this->ui->deleteToolStripMenuItem->setEnabled(this->m_commands["DeletePool"]->CanRun());
    this->ui->toolStripMenuItemHaConfigure->setEnabled(this->m_commands["HAConfigure"]->CanRun());
    this->ui->toolStripMenuItemHaDisable->setEnabled(this->m_commands["HADisable"]->CanRun());
    this->ui->PoolPropertiesToolStripMenuItem->setEnabled(this->m_commands["PoolProperties"]->CanRun());
    this->ui->addServerToPoolMenuItem->setEnabled(this->m_commands["JoinPool"]->CanRun());
    this->ui->menuItemRemoveFromPool->setEnabled(this->m_commands["EjectHostFromPool"]->CanRun());

    // VM menu
    this->ui->newVmAction->setEnabled(this->m_commands["NewVM"]->CanRun());
    this->ui->startShutdownToolStripMenuItem->setEnabled(this->m_commands["VMLifeCycle"]->CanRun());
    this->ui->copyVMtoSharedStorageMenuItem->setEnabled(this->m_commands["CopyVM"]->CanRun());
    this->ui->MoveVMToolStripMenuItem->setEnabled(this->m_commands["MoveVM"]->CanRun());
    this->ui->installToolsToolStripMenuItem->setEnabled(this->m_commands["InstallTools"]->CanRun());
    this->ui->uninstallToolStripMenuItem->setEnabled(this->m_commands["UninstallVM"]->CanRun());
    this->ui->VMPropertiesToolStripMenuItem->setEnabled(this->m_commands["VMProperties"]->CanRun());
    this->ui->snapshotToolStripMenuItem->setEnabled(this->m_commands["TakeSnapshot"]->CanRun());
    this->ui->convertToTemplateToolStripMenuItem->setEnabled(this->m_commands["ConvertVMToTemplate"]->CanRun());
    this->ui->exportToolStripMenuItem->setEnabled(this->m_commands["ExportVM"]->CanRun());

    // Update dynamic menu text for VMLifeCycle command
    this->ui->startShutdownToolStripMenuItem->setText(this->m_commands["VMLifeCycle"]->MenuText());

    // Template menu
    this->ui->newVMFromTemplateToolStripMenuItem->setEnabled(this->m_commands["NewVMFromTemplate"]->CanRun());
    this->ui->InstantVmToolStripMenuItem->setEnabled(this->m_commands["InstantVMFromTemplate"]->CanRun());
    this->ui->exportTemplateToolStripMenuItem->setEnabled(this->m_commands["ExportTemplate"]->CanRun());
    this->ui->duplicateTemplateToolStripMenuItem->setEnabled(this->m_commands["CopyTemplate"]->CanRun());
    this->ui->uninstallTemplateToolStripMenuItem->setEnabled(this->m_commands["DeleteTemplate"]->CanRun());
    this->ui->templatePropertiesToolStripMenuItem->setEnabled(this->m_commands["VMProperties"]->CanRun());

    // Storage menu
    this->ui->addVirtualDiskToolStripMenuItem->setEnabled(this->m_commands["AddVirtualDisk"]->CanRun());
    this->ui->attachVirtualDiskToolStripMenuItem->setEnabled(this->m_commands["AttachVirtualDisk"]->CanRun());
    this->ui->DetachStorageToolStripMenuItem->setEnabled(this->m_commands["DetachSR"]->CanRun());
    this->ui->ReattachStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["ReattachSR"]->CanRun());
    this->ui->ForgetStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["ForgetSR"]->CanRun());
    this->ui->DestroyStorageRepositoryToolStripMenuItem->setEnabled(this->m_commands["DestroySR"]->CanRun());
    this->ui->RepairStorageToolStripMenuItem->setEnabled(this->m_commands["RepairSR"]->CanRun());
    this->ui->DefaultSRToolStripMenuItem->setEnabled(this->m_commands["SetDefaultSR"]->CanRun());
    this->ui->newStorageRepositoryAction->setEnabled(this->m_commands["NewSR"]->CanRun());
    this->ui->virtualDisksToolStripMenuItem->setEnabled(this->m_commands["StorageProperties"]->CanRun());

    // Network menu
    this->ui->newNetworkAction->setEnabled(this->m_commands["NewNetwork"]->CanRun());
    // Note: NetworkProperties will be added when action exists
}
