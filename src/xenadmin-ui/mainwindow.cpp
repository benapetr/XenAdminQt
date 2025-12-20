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
#include "dialogs/connectdialog.h"
#include "dialogs/debugwindow.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/optionsdialog.h"
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
#include "iconmanager.h"
#include "connectionprofile.h"
#include "navigation/navigationhistory.h"
#include "widgets/navigationpane.h"
#include "widgets/navigationview.h"
#include "xenlib.h"
#include "xencache.h"      // Need full definition for resolve() calls
#include "metricupdater.h" // Need full definition for start() call
#include "search.h"
#include "groupingtag.h"
#include "grouping.h"
#include "xen/api.h"
#include "xen/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmrebootaction.h"
#include "operations/operationmanager.h"
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
    : QMainWindow(parent), ui(new Ui::MainWindow), m_xenLib(nullptr), m_debugWindow(nullptr), m_titleBar(nullptr),
      m_consolePanel(nullptr), m_cvmConsolePanel(nullptr), m_navigationPane(nullptr),
      m_connected(false),
      m_navigationHistory(nullptr),
      m_tabContainer(nullptr), m_tabContainerLayout(nullptr),
      m_poolsTreeItem(nullptr), m_hostsTreeItem(nullptr), m_vmsTreeItem(nullptr), m_storageTreeItem(nullptr),
      m_currentObjectType(""), m_currentObjectRef("")
{
    this->ui->setupUi(this);

    // Set application icon
    setWindowIcon(QIcon(":/icons/app.ico"));

    // Create title bar and integrate it with tab widget
    // We need to wrap the tab widget in a container to add the title bar above it
    this->m_titleBar = new TitleBar(this);

    // Get the splitter and the index where mainTabWidget is located
    QSplitter* splitter = ui->centralSplitter;
    int tabWidgetIndex = splitter->indexOf(ui->mainTabWidget);

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
    m_tabContainer = tabContainer;
    m_tabContainerLayout = containerLayout;

    // Status bar widgets (matches C# MainWindow.statusProgressBar and statusLabel)
    m_statusLabel = new QLabel(this);
    m_statusProgressBar = new QProgressBar(this);
    m_statusProgressBar->setMaximumWidth(200);
    m_statusProgressBar->setVisible(false); // Hidden by default
    m_statusBarAction = nullptr;

    ui->statusbar->addPermanentWidget(m_statusLabel);
    ui->statusbar->addPermanentWidget(m_statusProgressBar);

    // Connect to OperationManager for progress tracking (matches C# History_CollectionChanged)
    connect(OperationManager::instance(), &OperationManager::newOperation, this, &MainWindow::onNewOperation);

    this->m_titleBar->clear(); // Start with empty title

    // Initialize XenLib
    this->m_xenLib = new XenLib(this);
    connect(this->m_xenLib, &XenLib::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);

    // Connect cache populated signal to refresh tree after initial data load
    connect(this->m_xenLib, &XenLib::cachePopulated, this, &MainWindow::onCachePopulated);

    // Connect authentication failure signal for retry prompt
    connect(this->m_xenLib, &XenLib::authenticationFailed, this, &MainWindow::onAuthenticationFailed);

    // Connect redirect to master signal
    connect(this->m_xenLib, &XenLib::redirectedToMaster, this, [this](const QString& masterAddress) {
        QString msg = QString("Redirecting to pool master: %1").arg(masterAddress);
        qDebug() << "MainWindow:" << msg;
        this->ui->statusbar->showMessage(msg, 5000);
    });

    // Connect cache objectChanged signal to refresh tabs when selected object updates
    if (m_xenLib->getCache())
    {
        connect(m_xenLib->getCache(), &XenCache::objectChanged,
                this, &MainWindow::onCacheObjectChanged);
    }

    // Connect task rehydration signals to MeddlingActionManager
    auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
    if (rehydrationMgr)
    {
        connect(this->m_xenLib, &XenLib::taskAdded, rehydrationMgr, &MeddlingActionManager::handleTaskAdded);
        connect(this->m_xenLib, &XenLib::taskModified, rehydrationMgr, &MeddlingActionManager::handleTaskUpdated);
        connect(this->m_xenLib, &XenLib::taskDeleted, rehydrationMgr, &MeddlingActionManager::handleTaskRemoved);
    }

    // Connect XenAPI message signals to AlertManager for alert system
    // C# Reference: MainWindow.cs line 703 - connection.Cache.RegisterCollectionChanged<Message>
    connect(this->m_xenLib, &XenLib::messageReceived, this, &MainWindow::onMessageReceived);
    connect(this->m_xenLib, &XenLib::messageRemoved, this, &MainWindow::onMessageRemoved);

    // Get NavigationPane from UI (matches C# MainWindow.navigationPane)
    m_navigationPane = ui->navigationPane;

    // Connect NavigationPane events (matches C# MainWindow.navigationPane_* event handlers)
    connect(m_navigationPane, &NavigationPane::navigationModeChanged,
            this, &MainWindow::onNavigationModeChanged);
    connect(m_navigationPane, &NavigationPane::notificationsSubModeChanged,
            this, &MainWindow::onNotificationsSubModeChanged);
    connect(m_navigationPane, &NavigationPane::treeViewSelectionChanged,
            this, &MainWindow::onNavigationPaneTreeViewSelectionChanged);
    connect(m_navigationPane, &NavigationPane::treeNodeRightClicked,
            this, &MainWindow::onNavigationPaneTreeNodeRightClicked);

    // Get tree widget from NavigationPane's NavigationView for legacy code compatibility
    // TODO: Refactor to use NavigationPane API instead of direct tree access
    auto* navView = m_navigationPane->navigationView();
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

    // Set XenLib instance for console panels
    this->m_consolePanel->setXenLib(this->m_xenLib);
    this->m_cvmConsolePanel->setXenLib(this->m_xenLib);

    // Set XenLib instance for navigation pane (for tree building)
    if (m_navigationPane)
    {
        m_navigationPane->setXenLib(this->m_xenLib);
    }

    // Initialize tab pages (without parent - they will be parented to QTabWidget when added)
    // Order matches C# MainWindow.Designer.cs lines 326-345
    // Note: We don't implement all C# tabs yet (Home, Ballooning, HA, WLB, AD, GPU, Docker, USB)
    this->m_tabPages.append(new GeneralTabPage()); // C#: TabPageGeneral
    // Ballooning - not implemented yet
    // Console tabs are added below after initialization
    this->m_tabPages.append(new VMStorageTabPage()); // C#: TabPageStorage (Virtual Disks for VMs)
    this->m_tabPages.append(new SrStorageTabPage()); // C#: TabPageSR (for SRs)
    this->m_tabPages.append(new PhysicalStorageTabPage()); // C#: TabPagePhysicalStorage (for Hosts/Pools)
    this->m_tabPages.append(new NetworkTabPage());     // C#: TabPageNetwork (name changed to "Networking")
    this->m_tabPages.append(new NICsTabPage());        // C#: TabPageNICs
    this->m_tabPages.append(new PerformanceTabPage()); // C#: TabPagePerformance
    // HA - not implemented yet
    this->m_tabPages.append(new SnapshotsTabPage()); // C#: TabPageSnapshots
    // WLB - not implemented yet
    // AD - not implemented yet
    // GPU - not implemented yet
    // Search - not implemented yet
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
    searchTab->setXenLib(this->m_xenLib);
    this->m_searchTabPage = searchTab;
    this->m_tabPages.append(searchTab);

    // Connect SearchTabPage objectSelected signal to navigate to that object
    connect(searchTab, &SearchTabPage::objectSelected, this, &MainWindow::onSearchTabPageObjectSelected);

    // Initialize notification pages (matches C# _notificationPages initialization)
    // C# Reference: xenadmin/XenAdmin/MainWindow.Designer.cs lines 304-306
    // In C#: splitContainer1.Panel2.Controls.Add(this.alertPage);
    // These pages are shown in the same area as tabs (Panel2 of the main splitter)
    AlertSummaryPage* alertPage = new AlertSummaryPage(this);
    alertPage->setXenLib(this->m_xenLib);
    this->m_notificationPages.append(alertPage);

    EventsPage* eventsPage = new EventsPage(this);
    eventsPage->setXenLib(this->m_xenLib);
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

    this->m_xenLib->initialize();

    // Load saved settings
    this->loadSettings();

    // Restore saved connections
    this->restoreConnections();
}

MainWindow::~MainWindow()
{
    // Cleanup debug handler
    DebugWindow::uninstallDebugHandler();

    if (this->m_xenLib)
    {
        this->m_xenLib->cleanup();
    }

    // Clean up tab pages
    qDeleteAll(this->m_tabPages);
    this->m_tabPages.clear();

    delete this->ui;
}

void MainWindow::updateActions()
{
    // Actions available only when connected
    this->ui->disconnectAction->setEnabled(this->m_connected);
    this->ui->importAction->setEnabled(this->m_connected);
    this->ui->exportAction->setEnabled(this->m_connected);
    this->ui->newNetworkAction->setEnabled(this->m_connected);
    this->ui->newStorageRepositoryAction->setEnabled(this->m_connected);

    // Connect action available only when not connected
    this->ui->connectAction->setEnabled(!this->m_connected);

    // Update toolbar and menu states (matches C# UpdateToolbars)
    updateToolbarsAndMenus();
}

void MainWindow::connectToServer()
{
    ConnectDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString hostname = dialog.getHostname();
        int port = dialog.getPort();
        QString username = dialog.getUsername();
        QString password = dialog.getPassword();
        bool useSSL = dialog.useSSL();
        bool saveProfile = dialog.saveProfile();

        qDebug() << "XenAdmin Qt: Attempting to connect to" << hostname << "port" << port << "SSL:" << useSSL;
        this->ui->statusbar->showMessage("Connecting to " + hostname + "...");

        // Create progress dialog for connection attempt
        QProgressDialog* progressDialog = new QProgressDialog("Connecting to server...", "Cancel", 0, 0, this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        // Store the connection profile for later use if save is requested
        ConnectionProfile* profile = new ConnectionProfile(dialog.getConnectionProfile());

        // Create connection context
        ConnectionContext* context = new ConnectionContext{
            progressDialog,
            hostname,
            saveProfile,
            profile,
            new QMetaObject::Connection(),
            new QMetaObject::Connection(),
            new QMetaObject::Connection()};

        *context->successConn = connect(this->m_xenLib, &XenLib::connectionStateChanged, this,
                                        [this, context](bool connected) {
                                            handleConnectionSuccess(context, connected);
                                        });

        *context->errorConn = connect(this->m_xenLib, &XenLib::connectionError, this,
                                      [this, context](const QString& error) {
                                          handleConnectionError(context, error);
                                      });

        // Handle authentication failures - clean up and let onAuthenticationFailed() handle retry
        *context->authFailedConn = connect(this->m_xenLib, &XenLib::authenticationFailed, this,
                                           [this, context](const QString&, int, const QString&, const QString&) {
                                               handleInitialAuthFailed(context);
                                           });

        // Initiate async connection
        this->m_xenLib->connectToServer(hostname, port, username, password, useSSL);
    }
}

void MainWindow::disconnectFromServer()
{
    if (this->m_xenLib && this->m_xenLib->isConnected())
    {
        // Close all VNC/console connections before disconnecting
        // Reference: C# MainWindow.cs lines 745-762
        if (m_consolePanel)
        {
            // Get all VMs and close their console connections
            QList<QVariantMap> vms = m_xenLib->getCache()->getAll("vm");
            for (const QVariantMap& vm : vms)
            {
                QString vmRef = vm.value("ref").toString();
                if (!vmRef.isEmpty())
                {
                    m_consolePanel->closeVncForSource(vmRef);
                }
            }
        }

        if (m_cvmConsolePanel)
        {
            // Get all hosts and close their CVM console connections
            QList<QVariantMap> hosts = m_xenLib->getCache()->getAll("host");
            for (const QVariantMap& host : hosts)
            {
                QString hostRef = host.value("ref").toString();
                if (!hostRef.isEmpty())
                {
                    // Close control domain zero console
                    QString controlDomainRef = m_xenLib->getControlDomainForHost(hostRef);
                    if (!controlDomainRef.isEmpty())
                    {
                        m_consolePanel->closeVncForSource(controlDomainRef);
                    }

                    // Close SR driver domain consoles for this host
                    // Iterate through all VMs to find SR driver domains on this host
                    QList<QVariantMap> allVMs = m_xenLib->getCache()->getAll("vm");
                    for (const QVariantMap& vm : allVMs)
                    {
                        QString vmRef = vm.value("ref").toString();
                        QString vmHostRef = vm.value("resident_on").toString();

                        // Check if this VM is an SR driver domain on the current host
                        if (vmHostRef == hostRef && m_xenLib->isSRDriverDomain(vmRef))
                        {
                            if (m_cvmConsolePanel)
                            {
                                m_cvmConsolePanel->closeVncForSource(vmRef);
                            }
                        }
                    }
                }
            }
        }

        this->m_xenLib->disconnectFromServer();
        getServerTreeWidget()->clear();
        this->clearTabs();
        this->updatePlaceholderVisibility();
        this->ui->statusbar->showMessage("Disconnected", 5000);
    }
}

void MainWindow::showAbout()
{
    AboutDialog dialog(m_xenLib, this);
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
    if (m_consolePanel)
    {
        m_consolePanel->sendCAD();
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
    importCmd.run();
}

void MainWindow::showExportWizard()
{
    qDebug() << "MainWindow: Showing Export Wizard";

    ExportVMCommand exportCmd(this);
    exportCmd.run();
}

void MainWindow::showNewNetworkWizard()
{
    qDebug() << "MainWindow: Showing New Network Wizard";

    NewNetworkCommand newNetCmd(this);
    newNetCmd.run();
}

void MainWindow::showNewStorageRepositoryWizard()
{
    qDebug() << "MainWindow: Showing New Storage Repository Wizard";

    NewSRCommand newSRCmd(this);
    newSRCmd.run();
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    this->m_connected = connected;
    this->updateActions();

    if (connected)
    {
        qDebug() << "XenAdmin Qt: Successfully connected to Xen server";
        this->ui->statusbar->showMessage("Connected", 2000);

        // Note: Tree refresh happens in onCachePopulated() after initial data load
        // Don't refresh here - cache is empty at this point

        // Trigger task rehydration after successful reconnect
        auto* rehydrationMgr = OperationManager::instance()->meddlingActionManager();
        if (rehydrationMgr && this->m_xenLib && this->m_xenLib->getConnection())
        {
            rehydrationMgr->rehydrateTasks(this->m_xenLib->getConnection());
        }
    } else
    {
        qDebug() << "XenAdmin Qt: Disconnected from Xen server";
        this->ui->statusbar->showMessage("Disconnected", 2000);
        getServerTreeWidget()->clear();
        this->clearTabs();
        this->updatePlaceholderVisibility();
    }
}

void MainWindow::onCachePopulated()
{
    qDebug() << "MainWindow: Cache populated, refreshing tree view";

    // Refresh tree now that cache has data
    if (m_navigationPane)
    {
        m_navigationPane->requestRefreshTreeView();
    }

    // Start MetricUpdater to begin fetching RRD performance metrics
    // C# Equivalent: MetricUpdater.Start() called after connection established
    // C# Reference: xenadmin/XenModel/XenConnection.cs line 780
    if (m_xenLib && m_xenLib->getMetricUpdater())
    {
        qDebug() << "MainWindow: Starting MetricUpdater for performance metrics";
        m_xenLib->getMetricUpdater()->start();
    }
}

void MainWindow::onAuthenticationFailed(const QString& hostname, int port,
                                        const QString& username, const QString& errorMessage)
{
    qDebug() << "XenAdmin Qt: Authentication failed for" << hostname << ":" << port
             << "username:" << username << "error:" << errorMessage;

    // Show retry dialog with pre-filled credentials
    ConnectDialog retryDialog(hostname, port, username, errorMessage, this);

    if (retryDialog.exec() == QDialog::Accepted)
    {
        // User wants to retry with new credentials
        QString newUsername = retryDialog.getUsername();
        QString newPassword = retryDialog.getPassword();
        bool useSSL = retryDialog.useSSL();
        bool saveProfile = retryDialog.saveProfile();

        qDebug() << "XenAdmin Qt: Retrying connection to" << hostname << "with new credentials";
        this->ui->statusbar->showMessage("Retrying connection to " + hostname + "...");

        // Create progress dialog for retry attempt
        QProgressDialog* progressDialog = new QProgressDialog("Retrying connection...", "Cancel", 0, 0, this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        // Store the connection profile for later use if save is requested
        ConnectionProfile* profile = new ConnectionProfile(retryDialog.getConnectionProfile());

        // Create connection context for retry
        ConnectionContext* context = new ConnectionContext{
            progressDialog,
            hostname,
            saveProfile,
            profile,
            new QMetaObject::Connection(),
            new QMetaObject::Connection(),
            new QMetaObject::Connection()};

        *context->successConn = connect(this->m_xenLib, &XenLib::connectionStateChanged, this,
                                        [this, context](bool connected) {
                                            handleConnectionSuccess(context, connected);
                                        });

        *context->errorConn = connect(this->m_xenLib, &XenLib::connectionError, this,
                                      [this, context](const QString& error) {
                                          handleConnectionError(context, error);
                                      });

        // Handle recursive authentication failures (user can keep retrying)
        *context->authFailedConn = connect(this->m_xenLib, &XenLib::authenticationFailed, this,
                                           [this, context](const QString&, int, const QString&, const QString&) {
                                               handleRetryAuthFailed(context);
                                           });

        // Initiate async connection with new credentials
        this->m_xenLib->connectToServer(hostname, port, newUsername, newPassword, useSSL);
    } else
    {
        this->ui->statusbar->showMessage("Connection cancelled", 3000);
    }
}

void MainWindow::onTreeItemSelected()
{
    QList<QTreeWidgetItem*> selectedItems = getServerTreeWidget()->selectedItems();
    if (selectedItems.isEmpty())
    {
        this->ui->statusbar->showMessage("Ready", 2000);
        this->clearTabs();
        this->updatePlaceholderVisibility();
        this->m_titleBar->clear();
        m_lastSelectedRef.clear(); // Clear selection tracking

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        updateToolbarsAndMenus();

        return;
    }

    QTreeWidgetItem* item = selectedItems.first();
    QString itemText = item->text(0);
    QVariant itemRef = item->data(0, Qt::UserRole);
    QString objectType = item->data(0, Qt::UserRole + 1).toString();
    QIcon itemIcon = item->icon(0);

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
            this->showSearchPage(groupingTag);
            return;
        }
    }

    // Update title bar with selected object
    this->m_titleBar->setTitle(itemText, itemIcon);

    if (!itemRef.isNull())
    {
        QString refString = itemRef.toString();

        // Prevent duplicate API calls for same selection
        // This fixes the double-call issue when Qt emits itemSelectionChanged multiple times
        if (refString == m_lastSelectedRef && !refString.isEmpty())
        {
            //qDebug() << "MainWindow::onTreeItemSelected - Same object already selected, skipping duplicate API call";
            return;
        }

        m_lastSelectedRef = refString;

        this->ui->statusbar->showMessage("Selected: " + itemText + " (Ref: " + refString + ")", 5000);

        // Store context for async handler
        this->m_currentObjectType = objectType;
        this->m_currentObjectRef = refString;
        this->m_currentObjectText = itemText;
        this->m_currentObjectIcon = itemIcon;

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        this->updateToolbarsAndMenus();

        // This is no longer needed, we get everything from the cache now, stuff is being updated via event poller instead
        // Request full object data asynchronously - returns immediately, no UI freeze
        /*if (this->m_xenLib && this->m_xenLib->isConnected())
        {
            this->m_xenLib->requestObjectData(objectType, refString);
        }*/

        // Now we have the data, show the tabs
        auto objectData = this->m_xenLib->getCachedObjectData(objectType, refString);
        this->showObjectTabs(objectType, refString, objectData);

        // Add to navigation history (matches C# MainWindow.TreeView_SelectionsChanged)
        // Get current tab name (first tab is shown by default)
        QString currentTabName = "General"; // Default tab
        if (this->ui->mainTabWidget->count() > 0 && this->ui->mainTabWidget->currentIndex() >= 0)
        {
            currentTabName = this->ui->mainTabWidget->tabText(this->ui->mainTabWidget->currentIndex());
        }

        if (m_navigationHistory && !m_navigationHistory->isInHistoryNavigation())
        {
            HistoryItemPtr historyItem(new XenModelObjectHistoryItem(refString, objectType, m_currentObjectText, m_currentObjectIcon, currentTabName));
            m_navigationHistory->newHistoryItem(historyItem);
        }
    } else
    {
        this->ui->statusbar->showMessage("Selected: " + itemText, 3000);
        this->clearTabs();
        this->updatePlaceholderVisibility();
        m_lastSelectedRef.clear(); // Clear selection tracking

        // Update both toolbar and menu from Commands (matches C# UpdateToolbars)
        updateToolbarsAndMenus();
    }
}

void MainWindow::showObjectTabs(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    this->clearTabs();
    this->updateTabPages(objectType, objectRef, objectData);
    this->updatePlaceholderVisibility();
}

// C# Equivalent: MainWindow.SwitchToTab(TabPageSearch) with SearchPage.Search = Search.SearchForNonVappGroup(...)
// C# Reference: xenadmin/XenAdmin/MainWindow.cs lines 1771-1775
void MainWindow::showSearchPage(GroupingTag* groupingTag)
{
    if (!groupingTag || !m_searchTabPage)
        return;

    // Create Search object for this grouping
    // Matches C# MainWindow.cs line 1771: SearchPage.Search = Search.SearchForNonVappGroup(gt.Grouping, gt.Parent, gt.Group);
    Search* search = Search::searchForNonVappGroup(
        groupingTag->getGrouping(),
        groupingTag->getParent(),
        groupingTag->getGroup());

    // Set the search on SearchTabPage
    m_searchTabPage->setSearch(search); // SearchTabPage takes ownership
    m_searchTabPage->setXenLib(m_xenLib);
    m_searchTabPage->buildList();

    // Clear existing tabs and show only SearchTabPage
    clearTabs();
    ui->mainTabWidget->addTab(m_searchTabPage, m_searchTabPage->tabTitle());
    updatePlaceholderVisibility();

    // Update status bar
    QString groupName = groupingTag->getGrouping()->getGroupName(groupingTag->getGroup());
    ui->statusbar->showMessage(tr("Showing overview: %1").arg(groupName), 3000);
}

// C# Equivalent: SearchPage double-click handler navigates to object
// C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs lines 653-660
void MainWindow::onSearchTabPageObjectSelected(const QString& objectType, const QString& objectRef)
{
    // Find the object in the tree and select it
    // This matches C# behavior where double-clicking in search results navigates to General tab
    QTreeWidget* tree = getServerTreeWidget();
    if (!tree)
        return;

    // Search for the item in the tree
    QTreeWidgetItemIterator it(tree);
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        QString itemType = item->data(0, Qt::UserRole + 1).toString();
        QString itemRef = item->data(0, Qt::UserRole).toString();

        if (itemType == objectType && itemRef == objectRef)
        {
            // Found the item - select it (this will trigger onTreeItemSelected)
            tree->setCurrentItem(item);
            tree->scrollToItem(item);

            // Switch to General tab if it exists (matches C# behavior)
            // C# Reference: xenadmin/XenAdmin/Controls/XenSearch/QueryPanel.cs line 659
            for (int i = 0; i < ui->mainTabWidget->count(); ++i)
            {
                BaseTabPage* page = qobject_cast<BaseTabPage*>(ui->mainTabWidget->widget(i));
                if (page && page->tabTitle() == tr("General"))
                {
                    ui->mainTabWidget->setCurrentIndex(i);
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

    for (BaseTabPage* tab : m_tabPages)
    {
        if (qobject_cast<VMStorageTabPage*>(tab))
            vmStorageTab = tab;
        else if (qobject_cast<SrStorageTabPage*>(tab))
            srStorageTab = tab;
        else if (qobject_cast<PhysicalStorageTabPage*>(tab))
            physicalStorageTab = tab;

        QString title = tab->tabTitle();
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
        if (cvmConsoleTab && this->m_xenLib && this->m_xenLib->srHasDriverDomain(objectRef))
            newTabs.append(cvmConsoleTab);
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
        for (BaseTabPage* tab : m_tabPages)
        {
            if (tab->isApplicableForObjectType(objectType))
                newTabs.append(tab);
        }
    }

    // Always add Search tab last
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1391
    if (searchTab)
        newTabs.append(searchTab);

    return newTabs;
}

void MainWindow::updateTabPages(const QString& objectType, const QString& objectRef, const QVariantMap& objectData)
{
    // Get the correct tabs in order for this object type
    // C# Reference: xenadmin/XenAdmin/MainWindow.cs line 1432 (ChangeToNewTabs)
    QList<BaseTabPage*> newTabs = this->getNewTabPages(objectType, objectRef, objectData);

    // Get the last selected tab for this object (before adding tabs)
    // C# Reference: MainWindow.cs line 1434 - GetLastSelectedPage(SelectionManager.Selection.First)
    QString rememberedTabTitle = m_selectedTabs.value(objectRef);
    int pageToSelectIndex = -1;

    // Block signals during tab reconstruction to prevent premature onTabChanged calls
    // C# Reference: MainWindow.cs line 1438 - IgnoreTabChanges = true
    bool oldState = this->ui->mainTabWidget->blockSignals(true);

    // Add tabs in the correct order
    for (int i = 0; i < newTabs.size(); ++i)
    {
        BaseTabPage* tabPage = newTabs[i];

        // Set the XenLib so tab pages can access XenAPI
        tabPage->setXenLib(this->m_xenLib);

        // Set the object data on the tab page
        tabPage->setXenObject(objectType, objectRef, objectData);

        // Add the tab to the widget
        this->ui->mainTabWidget->addTab(tabPage, tabPage->tabTitle());

        // Check if this is the remembered tab
        // C# Reference: MainWindow.cs line 1460 - if (newTab == pageToSelect)
        if (!rememberedTabTitle.isEmpty() && tabPage->tabTitle() == rememberedTabTitle)
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
        m_selectedTabs[objectRef] = currentTabTitle;
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
                if (m_cvmConsolePanel)
                {
                    m_cvmConsolePanel->pauseAllDockedViews();
                }

                // Set current source based on object type
                if (objectType == "vm")
                {
                    consoleTab->consolePanel()->setCurrentSource(objectRef);
                    consoleTab->consolePanel()->unpauseActiveView(true);
                }
                else if (objectType == "host")
                {
                    consoleTab->consolePanel()->setCurrentSourceHost(objectRef);
                    consoleTab->consolePanel()->unpauseActiveView(true);
                }

                // Update RDP resolution
                consoleTab->consolePanel()->updateRDPResolution();
            }
            else
            {
                // Check for CVM console tab
                CvmConsoleTabPage* cvmConsoleTab = qobject_cast<CvmConsoleTabPage*>(currentPage);
                if (cvmConsoleTab && cvmConsoleTab->consolePanel())
                {
                    // Pause regular console
                    if (m_consolePanel)
                    {
                        m_consolePanel->pauseAllDockedViews();
                    }

                    // Set current source for SR
                    if (objectType == "sr")
                    {
                        cvmConsoleTab->consolePanel()->setCurrentSource(objectRef);
                        cvmConsoleTab->consolePanel()->unpauseActiveView(true);
                    }
                }
                else
                {
                    // Not a console tab - pause all consoles
                    if (m_consolePanel)
                    {
                        m_consolePanel->pauseAllDockedViews();
                    }
                    if (m_cvmConsolePanel)
                    {
                        m_cvmConsolePanel->pauseAllDockedViews();
                    }
                }
            }
            
            currentPage->onPageShown();
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
            previousPage->onPageHidden();
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
            if (m_cvmConsolePanel)
            {
                m_cvmConsolePanel->pauseAllDockedViews();
            }

            // Set current source based on selection
            if (m_currentObjectType == "vm")
            {
                consoleTab->consolePanel()->setCurrentSource(m_currentObjectRef);
                consoleTab->consolePanel()->unpauseActiveView(true); // Focus console
            } else if (m_currentObjectType == "host")
            {
                consoleTab->consolePanel()->setCurrentSourceHost(m_currentObjectRef);
                consoleTab->consolePanel()->unpauseActiveView(true); // Focus console
            }

            // Update RDP resolution
            consoleTab->consolePanel()->updateRDPResolution();
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
                if (m_consolePanel)
                {
                    m_consolePanel->pauseAllDockedViews();
                }

                // Set current source - CvmConsolePanel expects SR with driver domain
                // The CvmConsolePanel will look up the driver domain VM internally
                if (m_currentObjectType == "sr")
                {
                    // CvmConsolePanel.setCurrentSource() will look up driver domain VM
                    cvmConsoleTab->consolePanel()->setCurrentSource(m_currentObjectRef);
                    cvmConsoleTab->consolePanel()->unpauseActiveView(true); // Focus console
                }
            } else
            {
                // Not any console tab - pause all console panels
                // Reference: C# TheTabControl_SelectedIndexChanged lines 1681-1682
                if (m_consolePanel)
                {
                    m_consolePanel->pauseAllDockedViews();
                }
                if (m_cvmConsolePanel)
                {
                    m_cvmConsolePanel->pauseAllDockedViews();
                }
            }
        }

        if (currentPage)
        {
            currentPage->onPageShown();
        }
    }

    // Save the selected tab for the current object (tab memory)
    // Reference: C# SetLastSelectedPage() - stores tab per object
    if (index >= 0 && !m_currentObjectRef.isEmpty())
    {
        QString tabTitle = this->ui->mainTabWidget->tabText(index);
        m_selectedTabs[m_currentObjectRef] = tabTitle;
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
    contextMenu->exec(getServerTreeWidget()->mapToGlobal(position));

    // Clean up the menu
    contextMenu->deleteLater();
}

// Public interface methods for Command classes
QTreeWidget* MainWindow::getServerTreeWidget() const
{
    // Get tree widget from NavigationPane's NavigationView
    if (m_navigationPane)
    {
        auto* navView = m_navigationPane->navigationView();
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
        ui->statusbar->showMessage(message, timeout);
    else
        ui->statusbar->showMessage(message);
}

void MainWindow::refreshServerTree()
{
    // Delegate tree building to NavigationView which respects current navigation mode
    if (m_navigationPane)
    {
        m_navigationPane->requestRefreshTreeView();
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
    if (m_debugWindow)
    {
        settings.setDebugConsoleVisible(m_debugWindow->isVisible());
    }

    // Save expanded tree items
    QStringList expandedItems;
    QTreeWidgetItemIterator it(getServerTreeWidget());
    while (*it)
    {
        if ((*it)->isExpanded())
        {
            QString ref = (*it)->data(0, Qt::UserRole).toString();
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
        ui->centralSplitter->restoreState(splitterState);
    }

    // Restore debug console visibility
    if (settings.getDebugConsoleVisible() && m_debugWindow)
    {
        m_debugWindow->show();
    }

    qDebug() << "Settings loaded from:" << settings.getValue("").toString();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Check if user wants confirmation on exit
    if (SettingsManager::instance().getConfirmOnExit())
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Confirm Exit",
            "Are you sure you want to exit XenAdmin?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
    }

    // Save settings before closing
    saveSettings();

    // Save current connections
    saveConnections();

    // Clean up operation UUIDs before exit (matches C# MainWindow.OnClosing)
    OperationManager::instance()->prepareAllOperationsForRestart();

    // Disconnect if connected
    if (m_xenLib && m_xenLib->isConnected())
    {
        m_xenLib->disconnectFromServer();
    }

    event->accept();
}

// Search functionality
void MainWindow::onSearchTextChanged(const QString& text)
{
    QTreeWidget* treeWidget = getServerTreeWidget();
    if (!treeWidget)
        return;

    // If search is empty, show all items
    if (text.isEmpty())
    {
        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = treeWidget->topLevelItem(i);
            filterTreeItems(item, "");
        }
        return;
    }

    // Filter tree items based on search text
    for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = treeWidget->topLevelItem(i);
        filterTreeItems(item, text);
    }
}
void MainWindow::focusSearch()
{
    // Focus search box in NavigationPane
    if (m_navigationPane)
    {
        m_navigationPane->focusTreeView();
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
        this->m_navigationPane->switchToNotificationsView(NavigationPane::Alerts);
        
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
    this->m_navigationPane->updateSearch();

    // TODO: SetFiltersLabel() - update filters indicator in title bar
    // TODO: UpdateViewMenu(mode) - show/hide menu items based on mode

    // Update tree view for new mode
    this->refreshServerTree();
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
    if (this->m_navigationPane && this->m_navigationPane->currentMode() == NavigationPane::Notifications)
        return;
    
    // Forward to existing tree selection handler
    onTreeItemSelected();
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
    bool itemMatches = searchText.isEmpty() || itemMatchesSearch(item, searchText);
    bool hasVisibleChild = false;

    // Recursively filter children
    for (int i = 0; i < item->childCount(); ++i)
    {
        QTreeWidgetItem* child = item->child(i);
        filterTreeItems(child, searchText);
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
    QString objectType = item->data(0, Qt::UserRole + 1).toString().toLower();
    QString objectRef = item->data(0, Qt::UserRole + 2).toString().toLower();

    if (objectType.contains(search) || objectRef.contains(search))
        return true;

    return false;
}

void MainWindow::restoreConnections()
{
    qDebug() << "XenAdmin Qt: Restoring saved connections...";

    // Check if auto-connect is enabled
    bool autoConnect = SettingsManager::instance().getAutoConnect();
    if (!autoConnect && !SettingsManager::instance().getSaveSession())
    {
        qDebug() << "XenAdmin Qt: Auto-connect and save session are disabled, skipping connection restoration";
        return;
    }

    // Load all saved connection profiles
    QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();

    if (profiles.isEmpty())
    {
        qDebug() << "XenAdmin Qt: No saved connection profiles found";
        return;
    }

    qDebug() << "XenAdmin Qt: Found" << profiles.size() << "saved connection profile(s)";

    // Restore connections that have autoConnect enabled or were previously connected
    for (const ConnectionProfile& profile : profiles)
    {
        // Only auto-connect if the profile has autoConnect enabled
        // or if save session is enabled and the connection wasn't explicitly disconnected
        bool shouldConnect = profile.autoConnect() ||
                             (SettingsManager::instance().getSaveSession() && !profile.saveDisconnected());

        if (!shouldConnect)
        {
            qDebug() << "XenAdmin Qt: Skipping profile" << profile.displayName() << "(auto-connect disabled)";
            continue;
        }

        qDebug() << "XenAdmin Qt: Restoring connection to" << profile.displayName();

        // Attempt connection using the saved credentials
        bool success = m_xenLib->connectToServer(
            profile.hostname(),
            profile.port(),
            profile.username(),
            profile.password(),
            profile.useSSL());

        if (success)
        {
            qDebug() << "XenAdmin Qt: Successfully restored connection to" << profile.displayName();
            ui->statusbar->showMessage("Reconnected to " + profile.displayName(), 3000);

            // Delegate tree building to NavigationView which respects current navigation mode
            if (m_navigationPane)
            {
                m_navigationPane->requestRefreshTreeView();
            }

            // Update last connected timestamp
            ConnectionProfile updatedProfile = profile;
            updatedProfile.setLastConnected(QDateTime::currentSecsSinceEpoch());
            SettingsManager::instance().saveConnectionProfile(updatedProfile);
        } else
        {
            qWarning() << "XenAdmin Qt: Failed to restore connection to" << profile.displayName();

            QString errorMsg = m_xenLib->hasError() ? m_xenLib->getLastError() : "Unknown error";
            qWarning() << "XenAdmin Qt: Error:" << errorMsg;
        }
    }
}

void MainWindow::saveConnections()
{
    qDebug() << "XenAdmin Qt: Saving current connections...";

    // Check if save session is enabled
    if (!SettingsManager::instance().getSaveSession())
    {
        qDebug() << "XenAdmin Qt: Save session is disabled, skipping connection save";
        return;
    }

    // Get all current connection profiles and update their state
    QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();

    // Update connection states based on current connections
    // For now, we mark all as disconnected since we're closing
    // In a future version, we could track which connections are still active
    for (ConnectionProfile& profile : profiles)
    {
        // Mark as disconnected when app closes
        profile.setSaveDisconnected(!m_xenLib->isConnected());
        SettingsManager::instance().saveConnectionProfile(profile);
    }

    qDebug() << "XenAdmin Qt: Saved" << profiles.size() << "connection profile(s)";
    SettingsManager::instance().sync();
}

void MainWindow::onCacheObjectChanged(const QString& objectType, const QString& objectRef)
{
    // If the changed object is the currently displayed one, refresh the tabs
    if (objectType == m_currentObjectType && objectRef == m_currentObjectRef)
    {
        // Get updated data from cache
        QVariantMap objectData = m_xenLib->getCache()->resolve(objectType, objectRef);
        if (!objectData.isEmpty())
        {
            // Update tab pages with new data
            for (int i = 0; i < ui->mainTabWidget->count(); ++i)
            {
                BaseTabPage* tabPage = qobject_cast<BaseTabPage*>(ui->mainTabWidget->widget(i));
                if (tabPage)
                {
                    tabPage->setXenObject(objectType, objectRef, objectData);
                }
            }
        }
    }
}

void MainWindow::onMessageReceived(const QString& messageRef, const QVariantMap& messageData)
{
    // C# Reference: MainWindow.cs line 1000 - Alert.AddAlert(MessageAlert.ParseMessage(m))
    // Create alert from XenAPI message and add to AlertManager
    
    // Use factory method to create appropriate alert type
    Alert* alert = MessageAlert::parseMessage(m_xenLib->getConnection(), messageData);
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
    cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    this->ui->statusbar->showMessage("Connected to " + context->hostname, 5000);

    // Delegate tree building to NavigationView which respects current navigation mode
    if (m_navigationPane)
    {
        m_navigationPane->requestRefreshTreeView();
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
    cleanupConnectionContext(context);

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
    cleanupConnectionContext(context);

    context->progressDialog->close();
    context->progressDialog->deleteLater();

    delete context->profile;
    delete context;

    // Don't show any error here - onAuthenticationFailed() will handle it
}

void MainWindow::handleRetryAuthFailed(ConnectionContext* context)
{
    // Clean up retry connections before showing retry dialog again
    cleanupConnectionContext(context);

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
    m_statusBarAction = operation;

    // Connect to operation's progress and completion signals (matches C# action.Changed/Completed events)
    connect(operation, &AsyncOperation::progressChanged,
            this, &MainWindow::onOperationProgressChanged);
    connect(operation, &AsyncOperation::completed,
            this, &MainWindow::onOperationCompleted);
    connect(operation, &AsyncOperation::failed,
            this, &MainWindow::onOperationFailed);
    connect(operation, &AsyncOperation::cancelled,
            this, &MainWindow::onOperationCancelled);

    // Show initial status
    m_statusLabel->setText(operation->title());
    m_statusProgressBar->setValue(0);
    m_statusProgressBar->setVisible(true);
}

void MainWindow::onOperationProgressChanged(int percent)
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    if (!operation || operation != m_statusBarAction)
        return; // Not the operation we're tracking

    // Update progress bar (matches C# UpdateStatusProgressBar)
    if (percent < 0)
        percent = 0;
    else if (percent > 100)
        percent = 100;

    m_statusProgressBar->setValue(percent);
    m_statusLabel->setText(operation->title());
}

void MainWindow::onOperationCompleted()
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    finalizeOperation(operation, AsyncOperation::Completed);
}

void MainWindow::onOperationFailed(const QString& error)
{
    Q_UNUSED(error);
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    finalizeOperation(operation, AsyncOperation::Failed);
}

void MainWindow::onOperationCancelled()
{
    AsyncOperation* operation = qobject_cast<AsyncOperation*>(sender());
    finalizeOperation(operation, AsyncOperation::Cancelled);
}

void MainWindow::finalizeOperation(AsyncOperation* operation, AsyncOperation::OperationState state, const QString& errorMessage)
{
    if (!operation)
        return;

    // Disconnect signals
    disconnect(operation, &AsyncOperation::progressChanged,
               this, &MainWindow::onOperationProgressChanged);
    disconnect(operation, &AsyncOperation::completed,
               this, &MainWindow::onOperationCompleted);
    disconnect(operation, &AsyncOperation::failed,
               this, &MainWindow::onOperationFailed);
    disconnect(operation, &AsyncOperation::cancelled,
               this, &MainWindow::onOperationCancelled);

    // Only update status bar if this is the tracked action
    if (m_statusBarAction == operation)
    {
        m_statusProgressBar->setVisible(false);

        QString title = operation->title();
        switch (state)
        {
        case AsyncOperation::Completed:
            m_statusLabel->setText(QString("%1 completed successfully").arg(title));
            ui->statusbar->showMessage(QString("%1 completed successfully").arg(title), 5000);
            break;
        case AsyncOperation::Failed:
        {
            QString errorText = !errorMessage.isEmpty() ? errorMessage : operation->errorMessage();
            if (errorText.isEmpty())
                errorText = tr("Unknown error");
            m_statusLabel->setText(QString("%1 failed").arg(title));
            ui->statusbar->showMessage(QString("%1 failed: %2").arg(title, errorText), 10000);
            break;
        }
        case AsyncOperation::Cancelled:
            m_statusLabel->setText(QString("%1 cancelled").arg(title));
            ui->statusbar->showMessage(QString("%1 was cancelled").arg(title), 5000);
            break;
        default:
            break;
        }

        m_statusBarAction = nullptr;
    }

    // Refresh tree data after action completes (success/failure/cancel)
    if (m_connected && m_xenLib)
    {
        m_xenLib->requestVirtualMachines();
        m_xenLib->requestHosts();
        m_xenLib->requestPools();
        m_xenLib->requestStorageRepositories();
    }
}

void MainWindow::initializeToolbar()
{
    // Get toolbar from UI file (matches C# ToolStrip in MainWindow.Designer.cs)
    m_toolBar = ui->mainToolBar;

    QAction* firstToolbarAction = m_toolBar->actions().isEmpty() ? nullptr : m_toolBar->actions().first();

    // Add Back button with dropdown at the beginning (C# backButton - ToolStripSplitButton)
    m_backButton = new QToolButton(this);
    m_backButton->setIcon(QIcon(":/icons/back.png"));
    m_backButton->setText("Back");
    m_backButton->setToolTip("Back");
    m_backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_backButton->setPopupMode(QToolButton::MenuButtonPopup); // Split button style
    QMenu* backMenu = new QMenu(m_backButton);
    m_backButton->setMenu(backMenu);
    connect(m_backButton, &QToolButton::clicked, this, &MainWindow::onBackButton);
    connect(backMenu, &QMenu::aboutToShow, this, [this, backMenu]() {
        if (m_navigationHistory)
        {
            m_navigationHistory->populateBackDropDown(backMenu);
        }
    });
    m_toolBar->insertWidget(firstToolbarAction, m_backButton);

    // Add Forward button with dropdown (C# forwardButton - ToolStripSplitButton)
    m_forwardButton = new QToolButton(this);
    m_forwardButton->setIcon(QIcon(":/icons/forward.png"));
    m_forwardButton->setText("Forward");
    m_forwardButton->setToolTip("Forward");
    m_forwardButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_forwardButton->setPopupMode(QToolButton::MenuButtonPopup); // Split button style
    QMenu* forwardMenu = new QMenu(m_forwardButton);
    m_forwardButton->setMenu(forwardMenu);
    connect(m_forwardButton, &QToolButton::clicked, this, &MainWindow::onForwardButton);
    connect(forwardMenu, &QMenu::aboutToShow, this, [this, forwardMenu]() {
        if (m_navigationHistory)
        {
            m_navigationHistory->populateForwardDropDown(forwardMenu);
        }
    });
    m_toolBar->insertWidget(firstToolbarAction, m_forwardButton);

    // Add separator after navigation buttons
    m_toolBar->insertSeparator(m_toolBar->actions().first());

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
    updateToolbarsAndMenus();
}

void MainWindow::updateToolbarsAndMenus()
{
    // Matches C# MainWindow.UpdateToolbars() which calls UpdateToolbarsCore() and MainMenuBar_MenuActivate()
    // This is the SINGLE source of truth for both toolbar AND menu item states
    // Both read from the same Command objects

    // Management buttons - always available when connected
    ui->addServerAction->setEnabled(true); // Always enabled
    ui->addPoolAction->setEnabled(m_connected);
    ui->newStorageAction->setEnabled(m_connected);
    ui->newVmAction->setEnabled(m_connected);

    // Get current selection
    QTreeWidgetItem* currentItem = getServerTreeWidget()->currentItem();
    if (!currentItem || !m_connected)
    {
        // No selection - disable all operation buttons and menu items
        disableAllOperationButtons();
        disableAllOperationMenus();
        return;
    }

    QString objectType = currentItem->data(0, Qt::UserRole + 1).toString();
    QString objectRef = currentItem->data(0, Qt::UserRole).toString();

    // ========================================================================
    // TOOLBAR BUTTONS - Read from Command.canRun() (matches C# CommandToolStripButton.Update)
    // ========================================================================

    // Polymorphic commands (work for both VMs and Hosts)
    bool canShutdown = m_commands["Shutdown"]->canRun();
    bool canReboot = m_commands["Reboot"]->canRun();

    // VM-specific commands
    bool canStartVM = m_commands["StartVM"]->canRun();
    bool canResume = m_commands["ResumeVM"]->canRun();
    bool canSuspend = m_commands["SuspendVM"]->canRun();
    bool canPause = m_commands["PauseVM"]->canRun();
    bool canUnpause = m_commands["UnpauseVM"]->canRun();
    bool canForceShutdown = m_commands["ForceShutdownVM"]->canRun();
    bool canForceReboot = m_commands["ForceRebootVM"]->canRun();

    // Host-specific commands
    bool canPowerOnHost = m_commands["PowerOnHost"]->canRun();

    // Container buttons availability (for future Docker support)
    bool containerButtonsAvailable = false; // TODO: Docker support

    // Update button states (matches C# UpdateToolbarsCore visibility logic)

    // Start VM - visible when enabled
    ui->startVMAction->setEnabled(canStartVM);
    ui->startVMAction->setVisible(canStartVM);

    // Power On Host - visible when enabled
    ui->powerOnHostAction->setEnabled(canPowerOnHost);
    ui->powerOnHostAction->setVisible(canPowerOnHost);

    // Shutdown - show when enabled OR as fallback when no start buttons available
    // Matches C#: shutDownToolStripButton.Available = shutDownToolStripButton.Enabled || (!startVMToolStripButton.Available && !powerOnHostToolStripButton.Available && !containerButtonsAvailable)
    bool showShutdown = canShutdown || (!canStartVM && !canPowerOnHost && !containerButtonsAvailable);
    ui->shutDownAction->setEnabled(canShutdown);
    ui->shutDownAction->setVisible(showShutdown);

    // Reboot - show when enabled OR as fallback
    // Matches C#: RebootToolbarButton.Available = RebootToolbarButton.Enabled || !containerButtonsAvailable
    bool showReboot = canReboot || !containerButtonsAvailable;
    ui->rebootAction->setEnabled(canReboot);
    ui->rebootAction->setVisible(showReboot);

    // Resume - show when enabled
    ui->resumeAction->setEnabled(canResume);
    ui->resumeAction->setVisible(canResume);

    // Suspend - show if enabled OR if resume not visible
    // Matches C#: SuspendToolbarButton.Available = SuspendToolbarButton.Enabled || (!resumeToolStripButton.Available && !containerButtonsAvailable)
    bool showSuspend = canSuspend || (!canResume && !containerButtonsAvailable);
    ui->suspendAction->setEnabled(canSuspend);
    ui->suspendAction->setVisible(showSuspend);

    // Pause - show if enabled OR if unpause not visible
    // Matches C#: PauseVmToolbarButton.Available = PauseVmToolbarButton.Enabled || !UnpauseVmToolbarButton.Available
    bool showPause = canPause || !canUnpause;
    ui->pauseAction->setEnabled(canPause);
    ui->pauseAction->setVisible(showPause);

    // Unpause - show when enabled
    ui->unpauseAction->setEnabled(canUnpause);
    ui->unpauseAction->setVisible(canUnpause);

    // Force Shutdown - show based on Command.ShowOnMainToolBar property
    // Matches C#: ForceShutdownToolbarButton.Available = ((ForceVMShutDownCommand)ForceShutdownToolbarButton.Command).ShowOnMainToolBar
    QVariantMap vmData = m_xenLib->getCache()->resolve("vm", objectRef);
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    bool hasCleanShutdown = false;
    bool hasCleanReboot = false;
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "clean_shutdown")
            hasCleanShutdown = true;
        if (op.toString() == "clean_reboot")
            hasCleanReboot = true;
    }
    bool showForceShutdown = canForceShutdown && !hasCleanShutdown;
    bool showForceReboot = canForceReboot && !hasCleanReboot;

    ui->forceShutdownAction->setEnabled(canForceShutdown);
    ui->forceShutdownAction->setVisible(showForceShutdown);

    ui->forceRebootAction->setEnabled(canForceReboot);
    ui->forceRebootAction->setVisible(showForceReboot);

    // ========================================================================
    // MENU ITEMS - Read from Command.canRun() (matches C# MainMenuBar_MenuActivate)
    // ========================================================================

    // Server menu - use the polymorphic Shutdown/Reboot commands
    ui->ReconnectToolStripMenuItem1->setEnabled(m_commands["ReconnectHost"]->canRun());
    ui->DisconnectToolStripMenuItem->setEnabled(m_commands["DisconnectHost"]->canRun());
    ui->connectAllToolStripMenuItem->setEnabled(m_commands["ConnectAllHosts"]->canRun());
    ui->disconnectAllToolStripMenuItem->setEnabled(m_commands["DisconnectAllHosts"]->canRun());
    ui->restartToolstackAction->setEnabled(m_commands["RestartToolstack"]->canRun());
    ui->reconnectAsToolStripMenuItem->setEnabled(m_commands["HostReconnectAs"]->canRun());
    ui->rebootAction->setEnabled(canReboot);           // Use same variable as toolbar
    ui->shutDownAction->setEnabled(canShutdown);       // Use same variable as toolbar
    ui->powerOnHostAction->setEnabled(canPowerOnHost); // Use same variable as toolbar
    ui->maintenanceModeToolStripMenuItem1->setEnabled(m_commands["HostMaintenanceMode"]->canRun());
    ui->ServerPropertiesToolStripMenuItem->setEnabled(m_commands["HostProperties"]->canRun());

    // Pool menu
    ui->AddPoolToolStripMenuItem->setEnabled(m_commands["NewPool"]->canRun());
    ui->deleteToolStripMenuItem->setEnabled(m_commands["DeletePool"]->canRun());
    ui->toolStripMenuItemHaConfigure->setEnabled(m_commands["HAConfigure"]->canRun());
    ui->toolStripMenuItemHaDisable->setEnabled(m_commands["HADisable"]->canRun());
    ui->PoolPropertiesToolStripMenuItem->setEnabled(m_commands["PoolProperties"]->canRun());
    ui->addServerToPoolMenuItem->setEnabled(m_commands["JoinPool"]->canRun());
    ui->menuItemRemoveFromPool->setEnabled(m_commands["EjectHostFromPool"]->canRun());

    // VM menu
    ui->newVmAction->setEnabled(m_commands["NewVM"]->canRun());
    ui->startShutdownToolStripMenuItem->setEnabled(m_commands["VMLifeCycle"]->canRun());
    ui->copyVMtoSharedStorageMenuItem->setEnabled(m_commands["CopyVM"]->canRun());
    ui->MoveVMToolStripMenuItem->setEnabled(m_commands["MoveVM"]->canRun());
    ui->installToolsToolStripMenuItem->setEnabled(m_commands["InstallTools"]->canRun());
    ui->uninstallToolStripMenuItem->setEnabled(m_commands["UninstallVM"]->canRun());
    ui->VMPropertiesToolStripMenuItem->setEnabled(m_commands["VMProperties"]->canRun());
    ui->snapshotToolStripMenuItem->setEnabled(m_commands["TakeSnapshot"]->canRun());
    ui->convertToTemplateToolStripMenuItem->setEnabled(m_commands["ConvertVMToTemplate"]->canRun());
    ui->exportToolStripMenuItem->setEnabled(m_commands["ExportVM"]->canRun());

    // Update dynamic menu text for VMLifeCycle command
    ui->startShutdownToolStripMenuItem->setText(m_commands["VMLifeCycle"]->menuText());

    // Template menu
    ui->newVMFromTemplateToolStripMenuItem->setEnabled(m_commands["NewVMFromTemplate"]->canRun());
    ui->InstantVmToolStripMenuItem->setEnabled(m_commands["InstantVMFromTemplate"]->canRun());
    ui->exportTemplateToolStripMenuItem->setEnabled(m_commands["ExportTemplate"]->canRun());
    ui->duplicateTemplateToolStripMenuItem->setEnabled(m_commands["CopyTemplate"]->canRun());
    ui->uninstallTemplateToolStripMenuItem->setEnabled(m_commands["DeleteTemplate"]->canRun());
    ui->templatePropertiesToolStripMenuItem->setEnabled(m_commands["VMProperties"]->canRun());

    // Storage menu
    ui->addVirtualDiskToolStripMenuItem->setEnabled(m_commands["AddVirtualDisk"]->canRun());
    ui->attachVirtualDiskToolStripMenuItem->setEnabled(m_commands["AttachVirtualDisk"]->canRun());
    ui->DetachStorageToolStripMenuItem->setEnabled(m_commands["DetachSR"]->canRun());
    ui->ReattachStorageRepositoryToolStripMenuItem->setEnabled(m_commands["ReattachSR"]->canRun());
    ui->ForgetStorageRepositoryToolStripMenuItem->setEnabled(m_commands["ForgetSR"]->canRun());
    ui->DestroyStorageRepositoryToolStripMenuItem->setEnabled(m_commands["DestroySR"]->canRun());
    ui->RepairStorageToolStripMenuItem->setEnabled(m_commands["RepairSR"]->canRun());
    ui->DefaultSRToolStripMenuItem->setEnabled(m_commands["SetDefaultSR"]->canRun());
    ui->newStorageRepositoryAction->setEnabled(m_commands["NewSR"]->canRun());
    ui->virtualDisksToolStripMenuItem->setEnabled(m_commands["StorageProperties"]->canRun());

    // Network menu
    ui->newNetworkAction->setEnabled(m_commands["NewNetwork"]->canRun());
}

void MainWindow::disableAllOperationButtons()
{
    // Disable and hide all VM/Host operation toolbar actions
    ui->shutDownAction->setEnabled(false);
    ui->shutDownAction->setVisible(false);
    ui->powerOnHostAction->setEnabled(false);
    ui->powerOnHostAction->setVisible(false);
    ui->startVMAction->setEnabled(false);
    ui->startVMAction->setVisible(false);
    ui->rebootAction->setEnabled(false);
    ui->rebootAction->setVisible(false);
    ui->resumeAction->setEnabled(false);
    ui->resumeAction->setVisible(false);
    ui->suspendAction->setEnabled(false);
    ui->suspendAction->setVisible(false);
    ui->pauseAction->setEnabled(false);
    ui->pauseAction->setVisible(false);
    ui->unpauseAction->setEnabled(false);
    ui->unpauseAction->setVisible(false);
    ui->forceShutdownAction->setEnabled(false);
    ui->forceShutdownAction->setVisible(false);
    ui->forceRebootAction->setEnabled(false);
    ui->forceRebootAction->setVisible(false);
}

void MainWindow::disableAllOperationMenus()
{
    // Disable operation menu items (matches toolbar button disable)
    ui->shutDownAction->setEnabled(false);
    ui->rebootAction->setEnabled(false);
    ui->powerOnHostAction->setEnabled(false);
    ui->startShutdownToolStripMenuItem->setEnabled(false);
    // Add more as needed
}

void MainWindow::onBackButton()
{
    // Matches C# MainWindow.backButton_Click (History.Back(1))
    if (m_navigationHistory)
    {
        m_navigationHistory->back(1);
    }
}

void MainWindow::onForwardButton()
{
    // Matches C# MainWindow.forwardButton_Click (History.Forward(1))
    if (m_navigationHistory)
    {
        m_navigationHistory->forward(1);
    }
}

void MainWindow::updateHistoryButtons(bool canGoBack, bool canGoForward)
{
    // Update toolbar button enabled state based on history availability
    if (m_backButton)
        m_backButton->setEnabled(canGoBack);
    if (m_forwardButton)
        m_forwardButton->setEnabled(canGoForward);
}

// Navigation support for history (matches C# MainWindow)
void MainWindow::selectObjectInTree(const QString& objectRef, const QString& objectType)
{
    // Find and select the tree item with matching objectRef
    QTreeWidgetItemIterator it(getServerTreeWidget());
    while (*it)
    {
        QTreeWidgetItem* item = *it;
        QVariant itemRef = item->data(0, Qt::UserRole);
        QString itemType = item->data(0, Qt::UserRole + 1).toString();

        if (!itemRef.isNull() && itemRef.toString() == objectRef && itemType == objectType)
        {
            // Found the item - select it
            getServerTreeWidget()->setCurrentItem(item);
            getServerTreeWidget()->scrollToItem(item);
            return;
        }
        ++it;
    }

    qWarning() << "NavigationHistory: Could not find object in tree:" << objectRef << "type:" << objectType;
}

void MainWindow::setCurrentTab(const QString& tabName)
{
    // Find and activate tab by name
    for (int i = 0; i < ui->mainTabWidget->count(); ++i)
    {
        if (ui->mainTabWidget->tabText(i) == tabName)
        {
            ui->mainTabWidget->setCurrentIndex(i);
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
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onShutDownButton()
{
    // Use polymorphic Shutdown command (handles both VMs and Hosts - matches C# ShutDownCommand)
    if (m_commands.contains("Shutdown"))
        m_commands["Shutdown"]->run();
}

void MainWindow::onRebootButton()
{
    // Use polymorphic Reboot command (handles both VMs and Hosts - matches C# RebootCommand pattern)
    if (m_commands.contains("Reboot"))
        m_commands["Reboot"]->run();
}

void MainWindow::onResumeButton()
{
    // Matches C# ResumeVMCommand execution from toolbar
    ResumeVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onSuspendButton()
{
    // Matches C# SuspendVMCommand execution from toolbar
    SuspendVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onPauseButton()
{
    // Matches C# PauseVMCommand execution from toolbar
    PauseVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onUnpauseButton()
{
    // Matches C# UnPauseVMCommand execution from toolbar
    UnpauseVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onForceShutdownButton()
{
    // Matches C# ForceVMShutDownCommand execution from toolbar
    ForceShutdownVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

void MainWindow::onForceRebootButton()
{
    // Matches C# ForceVMRebootCommand execution from toolbar
    ForceRebootVMCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
}

// Toolbar Host operation button handlers
void MainWindow::onPowerOnHostButton()
{
    // Matches C# PowerOnHostCommand execution from toolbar
    PowerOnHostCommand cmd(this);
    if (cmd.canRun())
        cmd.run();
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
    if (m_commands.contains("ReconnectHost"))
        m_commands["ReconnectHost"]->run();
}

void MainWindow::onDisconnectHost()
{
    if (m_commands.contains("DisconnectHost"))
        m_commands["DisconnectHost"]->run();
}

void MainWindow::onConnectAllHosts()
{
    if (m_commands.contains("ConnectAllHosts"))
        m_commands["ConnectAllHosts"]->run();
}

void MainWindow::onDisconnectAllHosts()
{
    if (m_commands.contains("DisconnectAllHosts"))
        m_commands["DisconnectAllHosts"]->run();
}

void MainWindow::onRestartToolstack()
{
    if (m_commands.contains("RestartToolstack"))
        m_commands["RestartToolstack"]->run();
}

void MainWindow::onReconnectAs()
{
    if (m_commands.contains("HostReconnectAs"))
        m_commands["HostReconnectAs"]->run();
}

void MainWindow::onMaintenanceMode()
{
    if (m_commands.contains("HostMaintenanceMode"))
        m_commands["HostMaintenanceMode"]->run();
}

void MainWindow::onServerProperties()
{
    if (m_commands.contains("HostProperties"))
        m_commands["HostProperties"]->run();
}

// Pool menu slots
void MainWindow::onNewPool()
{
    if (m_commands.contains("NewPool"))
        m_commands["NewPool"]->run();
}

void MainWindow::onDeletePool()
{
    if (m_commands.contains("DeletePool"))
        m_commands["DeletePool"]->run();
}

void MainWindow::onHAConfigure()
{
    if (m_commands.contains("HAConfigure"))
        m_commands["HAConfigure"]->run();
}

void MainWindow::onHADisable()
{
    if (m_commands.contains("HADisable"))
        m_commands["HADisable"]->run();
}

void MainWindow::onPoolProperties()
{
    if (m_commands.contains("PoolProperties"))
        m_commands["PoolProperties"]->run();
}

void MainWindow::onJoinPool()
{
    if (m_commands.contains("JoinPool"))
        m_commands["JoinPool"]->run();
}

void MainWindow::onEjectFromPool()
{
    if (m_commands.contains("EjectHostFromPool"))
        m_commands["EjectHostFromPool"]->run();
}

// VM menu slots
void MainWindow::onNewVM()
{
    if (m_commands.contains("NewVM"))
        m_commands["NewVM"]->run();
}

void MainWindow::onStartShutdownVM()
{
    if (m_commands.contains("VMLifeCycle"))
        m_commands["VMLifeCycle"]->run();
}

void MainWindow::onCopyVM()
{
    if (m_commands.contains("CopyVM"))
        m_commands["CopyVM"]->run();
}

void MainWindow::onMoveVM()
{
    if (m_commands.contains("MoveVM"))
        m_commands["MoveVM"]->run();
}

void MainWindow::onInstallTools()
{
    if (m_commands.contains("InstallTools"))
        m_commands["InstallTools"]->run();
}

void MainWindow::onUninstallVM()
{
    if (m_commands.contains("UninstallVM"))
        m_commands["UninstallVM"]->run();
}

void MainWindow::onVMProperties()
{
    if (m_commands.contains("VMProperties"))
        m_commands["VMProperties"]->run();
}

void MainWindow::onTakeSnapshot()
{
    if (m_commands.contains("TakeSnapshot"))
        m_commands["TakeSnapshot"]->run();
}

void MainWindow::onConvertToTemplate()
{
    if (m_commands.contains("ConvertVMToTemplate"))
        m_commands["ConvertVMToTemplate"]->run();
}

void MainWindow::onExportVM()
{
    if (m_commands.contains("ExportVM"))
        m_commands["ExportVM"]->run();
}

// Template menu slots
void MainWindow::onNewVMFromTemplate()
{
    if (m_commands.contains("NewVMFromTemplate"))
        m_commands["NewVMFromTemplate"]->run();
}

void MainWindow::onInstantVM()
{
    if (m_commands.contains("InstantVMFromTemplate"))
        m_commands["InstantVMFromTemplate"]->run();
}

void MainWindow::onExportTemplate()
{
    if (m_commands.contains("ExportTemplate"))
        m_commands["ExportTemplate"]->run();
}

void MainWindow::onDuplicateTemplate()
{
    if (m_commands.contains("CopyTemplate"))
        m_commands["CopyTemplate"]->run();
}

void MainWindow::onDeleteTemplate()
{
    if (m_commands.contains("DeleteTemplate"))
        m_commands["DeleteTemplate"]->run();
}

void MainWindow::onTemplateProperties()
{
    if (m_commands.contains("VMProperties"))
        m_commands["VMProperties"]->run(); // Use VMProperties for templates too
}

// Storage menu slots
void MainWindow::onAddVirtualDisk()
{
    if (m_commands.contains("AddVirtualDisk"))
        m_commands["AddVirtualDisk"]->run();
}

void MainWindow::onAttachVirtualDisk()
{
    if (m_commands.contains("AttachVirtualDisk"))
        m_commands["AttachVirtualDisk"]->run();
}

void MainWindow::onDetachSR()
{
    if (m_commands.contains("DetachSR"))
        m_commands["DetachSR"]->run();
}

void MainWindow::onReattachSR()
{
    if (m_commands.contains("ReattachSR"))
        m_commands["ReattachSR"]->run();
}

void MainWindow::onForgetSR()
{
    if (m_commands.contains("ForgetSR"))
        m_commands["ForgetSR"]->run();
}

void MainWindow::onDestroySR()
{
    if (m_commands.contains("DestroySR"))
        m_commands["DestroySR"]->run();
}

void MainWindow::onRepairSR()
{
    if (m_commands.contains("RepairSR"))
        m_commands["RepairSR"]->run();
}

void MainWindow::onSetDefaultSR()
{
    if (m_commands.contains("SetDefaultSR"))
        m_commands["SetDefaultSR"]->run();
}

void MainWindow::onNewSR()
{
    if (m_commands.contains("NewSR"))
        m_commands["NewSR"]->run();
}

void MainWindow::onStorageProperties()
{
    if (m_commands.contains("StorageProperties"))
        m_commands["StorageProperties"]->run();
}

// Network menu slots
void MainWindow::onNewNetwork()
{
    if (m_commands.contains("NewNetwork"))
        m_commands["NewNetwork"]->run();
}

QString MainWindow::getSelectedObjectRef() const
{
    QTreeWidgetItem* item = getServerTreeWidget()->currentItem();
    if (!item)
        return QString();

    return item->data(0, Qt::UserRole).toString();
}

QString MainWindow::getSelectedObjectName() const
{
    QTreeWidgetItem* item = getServerTreeWidget()->currentItem();
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
    m_commands["Shutdown"] = new ShutdownCommand(this, this);
    m_commands["Reboot"] = new RebootCommand(this, this);

    // Server/Host commands
    m_commands["ReconnectHost"] = new ReconnectHostCommand(this, this);
    m_commands["DisconnectHost"] = new DisconnectHostCommand(this, this);
    m_commands["ConnectAllHosts"] = new ConnectAllHostsCommand(this, this);
    m_commands["DisconnectAllHosts"] = new DisconnectAllHostsCommand(this, this);
    m_commands["RestartToolstack"] = new RestartToolstackCommand(this, this);
    m_commands["HostReconnectAs"] = new HostReconnectAsCommand(this, this);
    m_commands["RebootHost"] = new RebootHostCommand(this, this);
    m_commands["ShutdownHost"] = new ShutdownHostCommand(this, this);
    m_commands["PowerOnHost"] = new PowerOnHostCommand(this, this);
    m_commands["HostMaintenanceMode"] = new HostMaintenanceModeCommand(this, this);
    m_commands["HostProperties"] = new HostPropertiesCommand(this, this);

    // Pool commands
    m_commands["NewPool"] = new NewPoolCommand(this, this);
    m_commands["DeletePool"] = new DeletePoolCommand(this, this);
    m_commands["HAConfigure"] = new HAConfigureCommand(this, this);
    m_commands["HADisable"] = new HADisableCommand(this, this);
    m_commands["PoolProperties"] = new PoolPropertiesCommand(this, this);
    m_commands["JoinPool"] = new JoinPoolCommand(this, this);
    m_commands["EjectHostFromPool"] = new EjectHostFromPoolCommand(this, this);

    // VM commands
    m_commands["StartVM"] = new StartVMCommand(this, this);
    m_commands["StopVM"] = new StopVMCommand(this, this);
    m_commands["RestartVM"] = new RestartVMCommand(this, this);
    m_commands["SuspendVM"] = new SuspendVMCommand(this, this);
    m_commands["ResumeVM"] = new ResumeVMCommand(this, this);
    m_commands["PauseVM"] = new PauseVMCommand(this, this);
    m_commands["UnpauseVM"] = new UnpauseVMCommand(this, this);
    m_commands["ForceShutdownVM"] = new ForceShutdownVMCommand(this, this);
    m_commands["ForceRebootVM"] = new ForceRebootVMCommand(this, this);
    m_commands["MigrateVM"] = new MigrateVMCommand(this, this);
    m_commands["CloneVM"] = new CloneVMCommand(this, this);
    m_commands["VMLifeCycle"] = new VMLifeCycleCommand(this, this);
    m_commands["CopyVM"] = new CopyVMCommand(this, this);
    m_commands["MoveVM"] = new MoveVMCommand(this, this);
    m_commands["InstallTools"] = new InstallToolsCommand(this, this);
    m_commands["UninstallVM"] = new UninstallVMCommand(this, this);
    m_commands["DeleteVM"] = new DeleteVMCommand(this, this);
    m_commands["ConvertVMToTemplate"] = new ConvertVMToTemplateCommand(this, this);
    m_commands["ExportVM"] = new ExportVMCommand(this, this);
    m_commands["NewVM"] = new NewVMCommand(this);
    m_commands["VMProperties"] = new VMPropertiesCommand(this);
    m_commands["TakeSnapshot"] = new TakeSnapshotCommand(this);
    m_commands["DeleteSnapshot"] = new DeleteSnapshotCommand(this);
    m_commands["RevertToSnapshot"] = new RevertToSnapshotCommand(this);
    m_commands["ImportVM"] = new ImportVMCommand(this);

    // Template commands
    m_commands["CreateVMFromTemplate"] = new CreateVMFromTemplateCommand(this, this);
    m_commands["NewVMFromTemplate"] = new NewVMFromTemplateCommand(this, this);
    m_commands["InstantVMFromTemplate"] = new InstantVMFromTemplateCommand(this, this);
    m_commands["CopyTemplate"] = new CopyTemplateCommand(this, this);
    m_commands["DeleteTemplate"] = new DeleteTemplateCommand(this, this);
    m_commands["ExportTemplate"] = new ExportTemplateCommand(this, this);

    // Storage commands
    m_commands["RepairSR"] = new RepairSRCommand(this, this);
    m_commands["DetachSR"] = new DetachSRCommand(this, this);
    m_commands["SetDefaultSR"] = new SetDefaultSRCommand(this, this);
    m_commands["NewSR"] = new NewSRCommand(this, this);
    m_commands["StorageProperties"] = new StoragePropertiesCommand(this, this);
    m_commands["AddVirtualDisk"] = new AddVirtualDiskCommand(this, this);
    m_commands["AttachVirtualDisk"] = new AttachVirtualDiskCommand(this, this);
    m_commands["ReattachSR"] = new ReattachSRCommand(this, this);
    m_commands["ForgetSR"] = new ForgetSRCommand(this, this);
    m_commands["DestroySR"] = new DestroySRCommand(this, this);

    // Network commands
    m_commands["NewNetwork"] = new NewNetworkCommand(this, this);
    m_commands["NetworkProperties"] = new NetworkPropertiesCommand(this, this);

    qDebug() << "Initialized" << m_commands.size() << "commands";
}

void MainWindow::connectMenuActions()
{
    // Server menu actions
    connect(ui->ReconnectToolStripMenuItem1, &QAction::triggered, this, &MainWindow::onReconnectHost);
    connect(ui->DisconnectToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDisconnectHost);
    connect(ui->connectAllToolStripMenuItem, &QAction::triggered, this, &MainWindow::onConnectAllHosts);
    connect(ui->disconnectAllToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDisconnectAllHosts);
    connect(ui->restartToolstackAction, &QAction::triggered, this, &MainWindow::onRestartToolstack);
    connect(ui->reconnectAsToolStripMenuItem, &QAction::triggered, this, &MainWindow::onReconnectAs);
    // Note: rebootAction, shutDownAction, powerOnHostAction are connected in initializeToolbar()
    // to avoid duplicate connections (toolbar and menu share the same QAction)
    connect(ui->maintenanceModeToolStripMenuItem1, &QAction::triggered, this, &MainWindow::onMaintenanceMode);
    connect(ui->ServerPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onServerProperties);

    // Pool menu actions
    connect(ui->AddPoolToolStripMenuItem, &QAction::triggered, this, &MainWindow::onNewPool);
    connect(ui->deleteToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDeletePool);
    connect(ui->toolStripMenuItemHaConfigure, &QAction::triggered, this, &MainWindow::onHAConfigure);
    connect(ui->toolStripMenuItemHaDisable, &QAction::triggered, this, &MainWindow::onHADisable);
    connect(ui->PoolPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onPoolProperties);
    connect(ui->addServerToPoolMenuItem, &QAction::triggered, this, &MainWindow::onJoinPool);
    connect(ui->menuItemRemoveFromPool, &QAction::triggered, this, &MainWindow::onEjectFromPool);

    // VM menu actions
    // Note: newVmAction is connected in initializeToolbar() (toolbar and menu share the same QAction)
    connect(ui->startShutdownToolStripMenuItem, &QAction::triggered, this, &MainWindow::onStartShutdownVM);
    connect(ui->copyVMtoSharedStorageMenuItem, &QAction::triggered, this, &MainWindow::onCopyVM);
    connect(ui->MoveVMToolStripMenuItem, &QAction::triggered, this, &MainWindow::onMoveVM);
    connect(ui->installToolsToolStripMenuItem, &QAction::triggered, this, &MainWindow::onInstallTools);
    connect(ui->uninstallToolStripMenuItem, &QAction::triggered, this, &MainWindow::onUninstallVM);
    connect(ui->VMPropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onVMProperties);
    connect(ui->snapshotToolStripMenuItem, &QAction::triggered, this, &MainWindow::onTakeSnapshot);
    connect(ui->convertToTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onConvertToTemplate);
    connect(ui->exportToolStripMenuItem, &QAction::triggered, this, &MainWindow::onExportVM);

    // Template menu actions
    connect(ui->newVMFromTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onNewVMFromTemplate);
    connect(ui->InstantVmToolStripMenuItem, &QAction::triggered, this, &MainWindow::onInstantVM);
    connect(ui->exportTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onExportTemplate);
    connect(ui->duplicateTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDuplicateTemplate);
    connect(ui->uninstallTemplateToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDeleteTemplate);
    connect(ui->templatePropertiesToolStripMenuItem, &QAction::triggered, this, &MainWindow::onTemplateProperties);

    // Storage menu actions
    connect(ui->addVirtualDiskToolStripMenuItem, &QAction::triggered, this, &MainWindow::onAddVirtualDisk);
    connect(ui->attachVirtualDiskToolStripMenuItem, &QAction::triggered, this, &MainWindow::onAttachVirtualDisk);
    connect(ui->DetachStorageToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDetachSR);
    connect(ui->ReattachStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onReattachSR);
    connect(ui->ForgetStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onForgetSR);
    connect(ui->DestroyStorageRepositoryToolStripMenuItem, &QAction::triggered, this, &MainWindow::onDestroySR);
    connect(ui->RepairStorageToolStripMenuItem, &QAction::triggered, this, &MainWindow::onRepairSR);
    connect(ui->DefaultSRToolStripMenuItem, &QAction::triggered, this, &MainWindow::onSetDefaultSR);
    connect(ui->newStorageRepositoryAction, &QAction::triggered, this, &MainWindow::onNewSR);
    connect(ui->virtualDisksToolStripMenuItem, &QAction::triggered, this, &MainWindow::onStorageProperties);

    // Network menu actions
    connect(ui->newNetworkAction, &QAction::triggered, this, &MainWindow::onNewNetwork);

    qDebug() << "Connected menu actions to command slots";
}

void MainWindow::updateMenuItems()
{
    // Update menu items based on command canRun() state
    // This matches C# CommandToolStripMenuItem.Update() pattern

    // Server menu
    ui->ReconnectToolStripMenuItem1->setEnabled(m_commands["ReconnectHost"]->canRun());
    ui->DisconnectToolStripMenuItem->setEnabled(m_commands["DisconnectHost"]->canRun());
    ui->connectAllToolStripMenuItem->setEnabled(m_commands["ConnectAllHosts"]->canRun());
    ui->disconnectAllToolStripMenuItem->setEnabled(m_commands["DisconnectAllHosts"]->canRun());
    ui->restartToolstackAction->setEnabled(m_commands["RestartToolstack"]->canRun());
    ui->reconnectAsToolStripMenuItem->setEnabled(m_commands["HostReconnectAs"]->canRun());
    ui->rebootAction->setEnabled(m_commands["Reboot"]->canRun());     // Use polymorphic Reboot command
    ui->shutDownAction->setEnabled(m_commands["Shutdown"]->canRun()); // Use polymorphic Shutdown command
    ui->powerOnHostAction->setEnabled(m_commands["PowerOnHost"]->canRun());
    ui->maintenanceModeToolStripMenuItem1->setEnabled(m_commands["HostMaintenanceMode"]->canRun());
    ui->ServerPropertiesToolStripMenuItem->setEnabled(m_commands["HostProperties"]->canRun());

    // Pool menu
    ui->AddPoolToolStripMenuItem->setEnabled(m_commands["NewPool"]->canRun());
    ui->deleteToolStripMenuItem->setEnabled(m_commands["DeletePool"]->canRun());
    ui->toolStripMenuItemHaConfigure->setEnabled(m_commands["HAConfigure"]->canRun());
    ui->toolStripMenuItemHaDisable->setEnabled(m_commands["HADisable"]->canRun());
    ui->PoolPropertiesToolStripMenuItem->setEnabled(m_commands["PoolProperties"]->canRun());
    ui->addServerToPoolMenuItem->setEnabled(m_commands["JoinPool"]->canRun());
    ui->menuItemRemoveFromPool->setEnabled(m_commands["EjectHostFromPool"]->canRun());

    // VM menu
    ui->newVmAction->setEnabled(m_commands["NewVM"]->canRun());
    ui->startShutdownToolStripMenuItem->setEnabled(m_commands["VMLifeCycle"]->canRun());
    ui->copyVMtoSharedStorageMenuItem->setEnabled(m_commands["CopyVM"]->canRun());
    ui->MoveVMToolStripMenuItem->setEnabled(m_commands["MoveVM"]->canRun());
    ui->installToolsToolStripMenuItem->setEnabled(m_commands["InstallTools"]->canRun());
    ui->uninstallToolStripMenuItem->setEnabled(m_commands["UninstallVM"]->canRun());
    ui->VMPropertiesToolStripMenuItem->setEnabled(m_commands["VMProperties"]->canRun());
    ui->snapshotToolStripMenuItem->setEnabled(m_commands["TakeSnapshot"]->canRun());
    ui->convertToTemplateToolStripMenuItem->setEnabled(m_commands["ConvertVMToTemplate"]->canRun());
    ui->exportToolStripMenuItem->setEnabled(m_commands["ExportVM"]->canRun());

    // Update dynamic menu text for VMLifeCycle command
    ui->startShutdownToolStripMenuItem->setText(m_commands["VMLifeCycle"]->menuText());

    // Template menu
    ui->newVMFromTemplateToolStripMenuItem->setEnabled(m_commands["NewVMFromTemplate"]->canRun());
    ui->InstantVmToolStripMenuItem->setEnabled(m_commands["InstantVMFromTemplate"]->canRun());
    ui->exportTemplateToolStripMenuItem->setEnabled(m_commands["ExportTemplate"]->canRun());
    ui->duplicateTemplateToolStripMenuItem->setEnabled(m_commands["CopyTemplate"]->canRun());
    ui->uninstallTemplateToolStripMenuItem->setEnabled(m_commands["DeleteTemplate"]->canRun());
    ui->templatePropertiesToolStripMenuItem->setEnabled(m_commands["VMProperties"]->canRun());

    // Storage menu
    ui->addVirtualDiskToolStripMenuItem->setEnabled(m_commands["AddVirtualDisk"]->canRun());
    ui->attachVirtualDiskToolStripMenuItem->setEnabled(m_commands["AttachVirtualDisk"]->canRun());
    ui->DetachStorageToolStripMenuItem->setEnabled(m_commands["DetachSR"]->canRun());
    ui->ReattachStorageRepositoryToolStripMenuItem->setEnabled(m_commands["ReattachSR"]->canRun());
    ui->ForgetStorageRepositoryToolStripMenuItem->setEnabled(m_commands["ForgetSR"]->canRun());
    ui->DestroyStorageRepositoryToolStripMenuItem->setEnabled(m_commands["DestroySR"]->canRun());
    ui->RepairStorageToolStripMenuItem->setEnabled(m_commands["RepairSR"]->canRun());
    ui->DefaultSRToolStripMenuItem->setEnabled(m_commands["SetDefaultSR"]->canRun());
    ui->newStorageRepositoryAction->setEnabled(m_commands["NewSR"]->canRun());
    ui->virtualDisksToolStripMenuItem->setEnabled(m_commands["StorageProperties"]->canRun());

    // Network menu
    ui->newNetworkAction->setEnabled(m_commands["NewNetwork"]->canRun());
    // Note: NetworkProperties will be added when action exists
}
