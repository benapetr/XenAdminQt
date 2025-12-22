/* Copyright (c) Petr Bena
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * *   Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "VNCTabView.h"
#include "ui_VNCTabView.h"
#include "XSVNCScreen.h"
#include "xenlib.h"
#include "../widgets/isodropdownbox.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/vm.h"
#include "xen/actions/vm/vmstartaction.h"
#include "xen/actions/vm/vmresumeaction.h"
#include "../commands/vm/startvmcommand.h"
#include "../commands/vm/resumevmcommand.h"
#include "../mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QApplication>
#include <QThread>
#include <QSettings>
#include <QProcess>
#include <QStandardPaths>
#include <QtCore/QTimer>

// Forward declare VNCView (will be implemented later)
class VNCView;

/**
 * @brief Constructor
 * Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 94-228
 */
VNCTabView::VNCTabView(VNCView* parent,
                       const QString& vmRef,
                       const QString& elevatedUsername,
                       const QString& elevatedPassword,
                       XenLib* xenLib,
                       QWidget* parentWidget)
    : QWidget(parentWidget), ui(new Ui::VNCTabView), _vncScreen(nullptr), _parentVNCView(parent), _xenLib(xenLib), _vmRef(vmRef), _lastDesktopSize(QSize()), _switchOnTabOpened(false), _ignoringResizes(false), _ignoreScaleChange(false), _inToggleDockUnDock(false), _inToggleFullscreen(false), _inToggleConsoleFocus(false), _oldScaleValue(false), _tryToConnectRDP(false), _keyHandler(), _insKeyTimer(nullptr), _toggleToXVNCorRDP(RDP), _inCdRefresh(false), _currentCdVbdRef()
{
    qDebug() << "VNCTabView: Constructor for VM:" << this->_vmRef;

    this->ui->setupUi(this);

    // C#: Force handle creation to receive resize events
    // Qt equivalent: widget is already created

    // Initialize scaling - enabled by default for better usability
    this->ui->scaleCheckBox->setChecked(true);

    // Get guest metrics from XenLib
    // C#: guestMetrics = source.Connection.Resolve(source.guest_metrics);
    // if (guestMetrics != null)
    //     guestMetrics.PropertyChanged += guestMetrics_PropertyChanged;
    QString guestMetricsRef = this->_xenLib->getGuestMetricsRef(vmRef);
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        // TODO: Wire up guestMetrics.PropertyChanged event handler
        // Will be implemented in registerEventListeners()
        qDebug() << "VNCTabView: VM has guest_metrics:" << guestMetricsRef;
    }

    // Register event listeners
    this->registerEventListeners();

    // Check if VM is control domain zero or SR driver domain
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 127-159
    QString hostRef;
    QString srRef;

    if (this->_xenLib->isControlDomainZero(vmRef, &hostRef))
    {
        // This is dom0 (control domain zero)
        qDebug() << "VNCTabView: VM is control domain zero for host:" << hostRef;

        // Get host data to show name
        QVariantMap hostData = this->_xenLib->getCachedObjectData("host", hostRef);
        if (!hostData.isEmpty())
        {
            QString hostName = hostData.value("name_label").toString();
            // C#: HostLabel.Text = string.Format(Messages.CONSOLE_HOST, host.Name());
            this->ui->hostLabel->setText(tr("Host: %1").arg(hostName));
            this->ui->hostLabel->setVisible(true);

            // TODO: Register Server_PropertyChanged event listener on host
            // TODO: Register Server_PropertyChanged event listener on host.metrics
        }
    } else if (this->_xenLib->isSRDriverDomain(vmRef, &srRef))
    {
        // This is an SR driver domain
        qDebug() << "VNCTabView: VM is SR driver domain for SR:" << srRef;

        // Get SR data to show name
        QVariantMap srData = this->_xenLib->getCachedObjectData("sr", srRef);
        if (!srData.isEmpty())
        {
            QString srName = srData.value("name_label").toString();
            // C#: HostLabel.Text = string.Format(Messages.CONSOLE_SR_DRIVER_DOMAIN, sr.Name());
            this->ui->hostLabel->setText(tr("SR driver domain: %1").arg(srName));
            this->ui->hostLabel->setVisible(true);

            // TODO: Register Server_PropertyChanged event listener on SR
        }
    } else
    {
        // Regular VM (not control domain)
        this->ui->hostLabel->setVisible(false);

        // TODO: Register Host_CollectionChanged event listener
        // TODO: Get target host from VM.GetStorageHost(false)
        // TODO: Register Server_EnabledPropertyChanged on all hosts
    }

    // Update power state after initialization is complete
    // Use QTimer::singleShot to defer until event loop is running
    QTimer::singleShot(0, this, [this]() {
        updatePowerState();
    });

    // Create XSVNCScreen (sourceRef, parent, xenLib, elevatedUsername, elevatedPassword)
    _vncScreen = new XSVNCScreen(vmRef, this, xenLib, elevatedUsername, elevatedPassword);

    // Connect VNC screen signals
    connect(_vncScreen, &XSVNCScreen::gpuStatusChanged, this, &VNCTabView::showGpuWarningIfRequired);
    connect(_vncScreen, &XSVNCScreen::userCancelledAuth, this, &VNCTabView::onUserCancelledAuth);
    connect(_vncScreen, &XSVNCScreen::vncConnectionAttemptCancelled, this, &VNCTabView::onVncConnectionAttemptCancelled);
    connect(_vncScreen, &XSVNCScreen::resizeRequested, this, &VNCTabView::onRDPorVNCResizeHandler);

    // Set up callbacks
    _vncScreen->onDetectRDP = [this]() { onDetectRDP(); };
    _vncScreen->onDetectVNC = [this]() { onDetectVNC(); };

    showGpuWarningIfRequired(_vncScreen->mustConnectRemoteDesktop());

    // Check if control domain or Linux HVM (no RDP)
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs line 171
    // C#: if (source.IsControlDomainZero(out var _) || source.IsHVM() && !HasRDP)
    // Linux HVM guests should only have one console: the console switch button vanishes altogether.
    bool hideToggleButton = false;

    if (this->_xenLib->isControlDomainZero(vmRef))
    {
        // Control domain zero never has RDP
        hideToggleButton = true;
        qDebug() << "VNCTabView: Hiding toggle console button (control domain zero)";
    } else if (this->_xenLib->isHVM(vmRef) && !this->_xenLib->hasRDP(vmRef))
    {
        // Linux HVM without RDP (only VNC available)
        hideToggleButton = true;
        qDebug() << "VNCTabView: Hiding toggle console button (Linux HVM without RDP)";
    }

    if (hideToggleButton)
    {
        this->ui->toggleConsoleButton->setVisible(false);
    }

    // Get last desktop size
    _lastDesktopSize = _vncScreen->desktopSize();

    // Create INS key timer
    _insKeyTimer = new QTimer(this);
    _insKeyTimer->setSingleShot(true);
    _insKeyTimer->setInterval(INS_KEY_TIMEOUT);
    connect(_insKeyTimer, &QTimer::timeout, this, &VNCTabView::onInsKeyTimeout);

    // Register keyboard shortcuts
    registerShortcutKeys();

    // Add Ctrl+Alt+Ins → Ctrl+Alt+Del shortcut
    _keyHandler.addKeyHandler(ConsoleShortcutKey::CTRL_ALT_INS, [this]() {
        cancelWaitForInsKeyAndSendCAD();
    });

    // Add VNC screen to content panel (C#: contentPanel - VNC screen fills available space)
    // Use the layout from UI file (contentPanelLayout)
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->ui->contentPanel->layout());
    if (!layout)
    {
        layout = new QVBoxLayout(this->ui->contentPanel);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(_vncScreen); // Fill space (XSVNCScreen has Expanding size policy)

    // Set toggle console button label
    QString rdpLabel = guessNativeConsoleLabel();
    this->ui->toggleConsoleButton->setText(rdpLabel);

    // Update UI elements
    updateFullScreenButton();
    updateDockButton();
    setupCD();
    updateParentMinimumSize();
    updateTooltipOfToggleButton();
    updateOpenSSHConsoleButtonState();

    // Connect button signals
    connect(this->ui->sendCADButton, &QPushButton::clicked, this, &VNCTabView::onSendCADClicked);
    connect(this->ui->scaleCheckBox, &QCheckBox::toggled, this, &VNCTabView::onScaleCheckBoxChanged);
    connect(this->ui->dockButton, &QPushButton::clicked, this, &VNCTabView::onDockButtonClicked);
    connect(this->ui->fullscreenButton, &QPushButton::clicked, this, &VNCTabView::onFullscreenButtonClicked);
    connect(this->ui->toggleConsoleButton, &QPushButton::clicked, this, &VNCTabView::onToggleConsoleButtonClicked);
    connect(this->ui->sshButton, &QPushButton::clicked, this, &VNCTabView::onSSHButtonClicked);
    connect(this->ui->powerStateLabel, &QLabel::linkActivated, this, &VNCTabView::onPowerStateLabelClicked);

    // Auto-switch to RDP if setting enabled
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 186-187
    // C#: if (Properties.Settings.Default.AutoSwitchToRDP && RDPEnabled)
    //         vncScreen.AutoSwitchRDPLater = true;
    QSettings settings;
    bool autoSwitchToRDP = settings.value("Console/AutoSwitchToRDP", false).toBool();
    if (autoSwitchToRDP && this->_xenLib->isRDPEnabled(vmRef))
    {
        _vncScreen->setAutoSwitchRDPLater(true);
        qDebug() << "VNCTabView: Auto-switch to RDP enabled";
    }

    // Update power state UI and trigger connection if VM is running
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs line 166: updatePowerState()
    // This will call showTopBarContents() if running, hideTopBarContents() if not
    // The connection is started automatically by updatePowerState() -> showTopBarContents()
    updatePowerState();

    qDebug() << "VNCTabView: Constructor complete";
}

/**
 * @brief Destructor
 * Reference: XenAdmin/ConsoleView/VNCTabView.Designer.cs lines 14-37
 */
VNCTabView::~VNCTabView()
{
    qDebug() << "VNCTabView: Destructor";

    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    unregisterEventListeners();

    if (_vncScreen)
    {
        disconnect(_vncScreen, nullptr, this, nullptr);
        _vncScreen->deleteLater();
        _vncScreen = nullptr;
    }

    // Cleanup fullscreen form if it exists (when implemented)
    // Reference: C# VNCTabView.Designer.cs Dispose() method
    // Currently fullscreen is handled by Qt window state, not separate form

    delete ui;
}

// ========== Public Methods ==========

bool VNCTabView::isScaled() const
{
    return this->ui->scaleCheckBox->isChecked();
}

void VNCTabView::setScaled(bool scaled)
{
    this->ui->scaleCheckBox->setChecked(scaled);
}

bool VNCTabView::isRDPControlEnabled() const
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 90-92
    // C#: private bool RDPControlEnabled => source != null && source.RDPControlEnabled();
    // C#: public bool IsRDPControlEnabled() { return RDPControlEnabled; }

    if (!this->_xenLib || this->_vmRef.isEmpty())
        return false;

    return this->_xenLib->isRDPControlEnabled(this->_vmRef);
}

void VNCTabView::pause()
{
    qDebug() << "VNCTabView: pause()";
    if (_vncScreen)
        _vncScreen->pause();
}

void VNCTabView::unpause()
{
    qDebug() << "VNCTabView: unpause() - VM:" << this->_vmRef;

    // Update power state which may trigger connection
    updatePowerState();

    if (_vncScreen)
        _vncScreen->unpause();
}

void VNCTabView::disableToggleVNCButton()
{
    qDebug() << "VNCTabView: disableToggleVNCButton()";
    this->ui->toggleConsoleButton->setEnabled(false);
}

void VNCTabView::enableToggleVNCButton()
{
    qDebug() << "VNCTabView: enableToggleVNCButton()";
    this->ui->toggleConsoleButton->setEnabled(true);
}

void VNCTabView::updateDockButton()
{
    qDebug() << "VNCTabView: updateDockButton()";

    // Reference: C# checks if console is docked
    // C#: bool isDocked = parentVNCView != null && parentVNCView.isDocked;

    bool isDocked = true; // Default to docked
    if (_parentVNCView)
    {
        // VNCView tracks dock state - check if we have an undocked window
        // For now, assume docked (full implementation requires VNCView::isDocked() method)
        isDocked = true;
    }

    if (isDocked)
    {
        this->ui->dockButton->setToolTip(tr("Undock console to separate window"));
        // ui->dockButton->setIcon(QIcon(":/icons/detach-24.png"));
    } else
    {
        this->ui->dockButton->setToolTip(tr("Dock console back to main window"));
        // ui->dockButton->setIcon(QIcon(":/icons/attach-24.png"));
    }
}

void VNCTabView::updateFullScreenButton()
{
    qDebug() << "VNCTabView: updateFullScreenButton()";

    // Reference: C# checks if VM is running
    // C#: bool running = source != null && source.power_state == vm_power_state.Running;

    bool running = false;
    if (this->_xenLib && !this->_vmRef.isEmpty())
    {
        QString powerState = getCachedVmPowerState();
        running = (powerState == "Running");
    }

    this->ui->fullscreenButton->setEnabled(running);
}

void VNCTabView::setupCD()
{
    // Reference: XenAdmin/Controls/MultipleDvdIsoList.cs
    qDebug() << "VNCTabView: setupCD()";

    if (!this->_xenLib || this->_vmRef.isEmpty())
        return;

    // Connect CD control signals
    connect(this->ui->cdDriveComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VNCTabView::onCdDriveChanged);
    connect(this->ui->cdIsoComboBox, QOverload<int>::of(&QComboBox::activated),
            this, &VNCTabView::onCdIsoSelected);
    connect(this->ui->cdEjectButton, &QPushButton::clicked,
            this, &VNCTabView::onCdEject);
    connect(this->ui->cdNewDriveLabel, &QLabel::linkActivated,
            this, &VNCTabView::onCreateNewCdDrive);

    // Initial refresh
    refreshCdDrives();
}

void VNCTabView::updateParentMinimumSize()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 350-358
    qDebug() << "VNCTabView: updateParentMinimumSize()";

    QWidget* parent = this->parentWidget();
    if (parent)
    {
        // Calculate minimum width based on bottom toolbar
        // Get the toolbar layout and calculate required width
        int bottomPanelWidth = this->ui->bottomToolbar->sizeHint().width();

        // Set parent minimum size (toolbar width + margin, minimum height)
        QSize minSize(bottomPanelWidth + 100, 400);
        parent->setMinimumSize(minSize);

        qDebug() << "VNCTabView: Set parent minimum size to" << minSize;
    }
}

void VNCTabView::maybeScale()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 846-868
    qDebug() << "VNCTabView: maybeScale()";

    if (!_vncScreen)
        return;

    QSize desktopSize = _vncScreen->desktopSize();
    int contentWidth = this->ui->contentPanel->width();

    QSettings settings;
    bool preserveScale = settings.value("Console/PreserveScaleWhenSwitchBackToVNC", false).toBool();

    // C#: if (vncScreen.DesktopSize.Width > 10 && contentPanel.Width < vncScreen.DesktopSize.Width)
    if (desktopSize.width() > 10 && contentWidth < desktopSize.width())
    {
        if (!preserveScale)
        {
            // C#: scaleCheckBox.Checked = true;
            this->ui->scaleCheckBox->setChecked(true);
        } else
        {
            // C#: scaleCheckBox.Checked = oldScaleValue || firstTime;
            // Note: firstTime logic not implemented yet (would need member variable)
            this->ui->scaleCheckBox->setChecked(_oldScaleValue);
        }
    } else if (preserveScale)
    {
        // C#: scaleCheckBox.Checked = oldScaleValue;
        this->ui->scaleCheckBox->setChecked(_oldScaleValue);
    }

    // C#: scaleCheckBox_CheckedChanged(null, null);
    // Trigger the scale change handler
    onScaleCheckBoxChanged(this->ui->scaleCheckBox->isChecked());
}

QSize VNCTabView::growToFit()
{
    qDebug() << "VNCTabView: growToFit()";

    if (!_vncScreen)
        return QSize(640, 480);

    QSize desktopSize = _vncScreen->desktopSize();

    // Calculate total size including toolbars
    int toolbarHeight = this->ui->gradientPanel->height() + this->ui->bottomToolbar->height();
    if (this->ui->warningPanel->isVisible())
        toolbarHeight += this->ui->warningPanel->height();

    return QSize(desktopSize.width(), desktopSize.height() + toolbarHeight);
}

void VNCTabView::onTabOpened()
{
    qDebug() << "VNCTabView: onTabOpened()";

    // C#: if (switchOnTabOpened) { ... auto-switch to RDP ... }
    if (_switchOnTabOpened)
    {
        _switchOnTabOpened = false;

        // User opened the console tab - auto-switch to RDP if it was detected while tab was closed
        qDebug() << "VNCTabView: Switching to RDP on tab open (was detected while tab closed)";
        onToggleConsoleButtonClicked();
    }
}

// ========== Private Slots ==========

void VNCTabView::onVMPropertyChanged(const QString& propertyName)
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 511-563
    qDebug() << "VNCTabView: onVMPropertyChanged:" << propertyName;

    // C#: Handle power_state, live, allowed_operations
    if (propertyName == "power_state" || propertyName == "live" ||
        propertyName == "allowed_operations")
    {
        updatePowerState();
        updateFullScreenButton();
    }
    // C#: Handle VBDs (CD device may have changed)
    else if (propertyName == "VBDs")
    {
        setupCD();
    }
    // C#: Handle guest_metrics changes
    else if (propertyName == "guest_metrics")
    {
        // Re-register guest_metrics event handlers (handled by XenCache)
        enableRDPIfCapable();
        updateOpenSSHConsoleButtonState();
    }
    // C#: Handle VIFs/PIFs (network changes affect SSH IP)
    else if (propertyName == "VIFs" || propertyName == "PIFs")
    {
        updateOpenSSHConsoleButtonState();
    }
    // C#: Handle name_label (update window title for control domains)
    else if (propertyName == "name_label")
    {
        QString labelText;

        if (this->_xenLib && this->_xenLib->isControlDomainZero(this->_vmRef))
        {
            QVariantMap vmData = this->_xenLib->getCachedObjectData("vm", this->_vmRef);
            QString hostRef = vmData.value("resident_on").toString();

            if (!hostRef.isEmpty() && hostRef != "OpaqueRef:NULL")
            {
                QVariantMap hostData = this->_xenLib->getCachedObjectData("host", hostRef);
                QString hostName = hostData.value("name_label").toString();
                labelText = tr("Console - %1").arg(hostName);
                this->ui->hostLabel->setText(labelText);
            }
        } else if (this->_xenLib && this->_xenLib->isSRDriverDomain(this->_vmRef))
        {
            // Get SR name for driver domain
            QVariantMap vmData = this->_xenLib->getCachedObjectData("vm", this->_vmRef);
            // SR driver domain naming would require additional XenLib method
            labelText = tr("Console - Storage Driver Domain");
            this->ui->hostLabel->setText(labelText);
        }
    }
}

void VNCTabView::onGuestMetricsPropertyChanged(const QString& propertyName)
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 565-587
    qDebug() << "VNCTabView: onGuestMetricsPropertyChanged:" << propertyName;

    // C#: Handle "other" property (RDP status changes)
    if (propertyName == "other")
    {
        if (this->_xenLib && this->_xenLib->isRDPEnabled(this->_vmRef))
        {
            // RDP is enabled - maybe auto-switch
            // C#: if (vncScreen.UseVNC && (tryToConnectRDP || (!vncScreen.UserWantsToSwitchProtocol && AutoSwitchToRDP)))
            if (_vncScreen && _vncScreen->useVNC())
            {
                QSettings settings;
                bool autoSwitch = settings.value("Console/AutoSwitchToRDP", true).toBool();

                if (_tryToConnectRDP || (!_vncScreen->userWantsToSwitchProtocol() && autoSwitch))
                {
                    _tryToConnectRDP = false;

                    // Queue RDP connection attempt in background
                    // C#: ThreadPool.QueueUserWorkItem(TryToConnectRDP);
                    QTimer::singleShot(100, this, [this]() {
                        if (_vncScreen)
                        {
                            qDebug() << "VNCTabView: Attempting to connect to RDP after guest_metrics change";
                            // This will trigger the protocol switch
                            enableRDPIfCapable();
                        }
                    });
                }
            }
        } else
        {
            enableRDPIfCapable();
        }

        updateButtons();
    }
    // C#: Handle "networks" property (IP address changes)
    else if (propertyName == "networks")
    {
        updateOpenSSHConsoleButtonState();
    }
}

void VNCTabView::onSettingsPropertyChanged(const QString& settingName)
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 340-348
    qDebug() << "VNCTabView: onSettingsPropertyChanged:" << settingName;

    // C#: When preferences/options change, re-register shortcuts
    // Settings that affect shortcuts: FullScreenShortcutKey, DockShortcutKey, UncaptureShortcutKey

    // Re-register shortcuts with new settings
    deregisterShortcutKeys();
    registerShortcutKeys();

    // Update parent minimum size (if needed)
    // C#: UpdateParentMinimumSize();
    // Qt equivalent would update parent widget size constraints
}

void VNCTabView::onSendCADClicked()
{
    qDebug() << "VNCTabView: onSendCADClicked()";

    if (_vncScreen)
        _vncScreen->sendCAD();
}

/**
 * @brief Send Ctrl+Alt+Delete to the VM
 * Reference: VNCTabView.cs lines 1269-1273
 */
void VNCTabView::sendCAD()
{
    qDebug() << "VNCTabView: sendCAD()";

    if (_vncScreen)
        _vncScreen->sendCAD();
}

void VNCTabView::onScaleCheckBoxChanged(bool checked)
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 912-930
    qDebug() << "VNCTabView: onScaleCheckBoxChanged:" << checked;

    // C#: if (ignoreScaleChange) return;
    if (_ignoreScaleChange)
        return;

    // C#: try { ignoringResizes = true; vncScreen.Scaling = scaleCheckBox.Checked; }
    //     finally { ignoringResizes = false; }
    try
    {
        _ignoringResizes = true;

        if (_vncScreen)
        {
            _vncScreen->setScaling(checked);
        }
    } catch (...)
    {
        _ignoringResizes = false;
        throw;
    }
    _ignoringResizes = false;

    // C#: FocusVNC();
    if (_vncScreen)
        _vncScreen->setFocus();

    // Save scale preference to QSettings
    // C#: Properties.Settings.Default.PreserveScaleWhenSwitchBackToVNC = checked;
    // Note: In C#, this setting is saved globally and persists across sessions
    QSettings settings;
    settings.setValue("Console/PreserveScaleWhenSwitchBackToVNC", checked);
    qDebug() << "VNCTabView: Saved scale preference:" << checked;
}

void VNCTabView::onDockButtonClicked()
{
    qDebug() << "VNCTabView: onDockButtonClicked()";
    toggleDockUnDock();
}

void VNCTabView::onFullscreenButtonClicked()
{
    qDebug() << "VNCTabView: onFullscreenButtonClicked()";
    toggleFullscreen();
}

void VNCTabView::onToggleConsoleButtonClicked()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1143-1241
    qDebug() << "VNCTabView: onToggleConsoleButtonClicked()";

    if (!_vncScreen)
        return;

    bool rdp = (_toggleToXVNCorRDP == RDP);

    try
    {
        if (rdp)
        {
            // Switching to RDP
            qDebug() << "VNCTabView: Switching to RDP";

            // C#: if (vncScreen.UseVNC) oldScaleValue = scaleCheckBox.Checked;
            if (_vncScreen->useVNC())
            {
                _oldScaleValue = this->ui->scaleCheckBox->isChecked();
            }

            // C#: vncScreen.UseVNC = !vncScreen.UseVNC;
            _vncScreen->setUseVNC(!_vncScreen->useVNC());

            // C#: vncScreen.UserWantsToSwitchProtocol = true;
            _vncScreen->setUserWantsToSwitchProtocol(true);

            // C#: if (CanEnableRDP()) { show dialog to enable RDP }
            if (canEnableRDP())
            {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("Enable RDP"));
                msgBox.setText(tr("Do you want to enable RDP on this VM?\n\n"
                                  "This will allow you to connect using the Remote Desktop Protocol."));
                msgBox.setIcon(QMessageBox::Question);
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::Yes);

                if (msgBox.exec() == QMessageBox::Yes)
                {
                    qDebug() << "VNCTabView: Requesting RDP enable via guest-agent-operation";

                    // C#: VM.call_plugin(session, source.opaque_ref, "guest-agent-operation", "request-rdp-on", _arguments);
                    // TODO: Implement when XenRpcAPI has call_plugin method
                    // For now, just log that we would enable RDP
                    _tryToConnectRDP = true;

                    QMessageBox::information(this, tr("RDP Enable"),
                                             tr("RDP enable request sent.\n\n"
                                                "Note: Full RDP enable via guest-agent-operation not yet implemented.\n"
                                                "The VM must have RDP already enabled or XenServer Tools installed."));
                }
            }

            // C#: if (vncScreen.RdpIp == null) toggleConsoleButton.Enabled = false;
            // Disable button until RDP connection is established
            if (_vncScreen->rdpIp().isEmpty())
            {
                this->ui->toggleConsoleButton->setEnabled(false);
            }

            // C#: ThreadPool.QueueUserWorkItem(TryToConnectRDP);
            // TODO: Start RDP connection attempt in background
            // For now, XSVNCScreen will handle polling automatically
        } else
        {
            // Switching to text console (XVNC)
            qDebug() << "VNCTabView: Switching to text console";

            // C#: oldScaleValue = scaleCheckBox.Checked;
            _oldScaleValue = this->ui->scaleCheckBox->isChecked();

            // C#: vncScreen.UseSource = !vncScreen.UseSource;
            // Note: UseSource property not yet ported, so we toggle UseVNC instead
            _vncScreen->setUseVNC(!_vncScreen->useVNC());
        }

        // C#: Unpause();
        unpause();

        // C#: UpdateButtons();
        updateButtons();
    } catch (const std::exception& ex)
    {
        qWarning() << "VNCTabView: Exception in toggle console button:" << ex.what();
        this->ui->toggleConsoleButton->setEnabled(false);
    }
}

void VNCTabView::onSSHButtonClicked()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1469-1505
    qDebug() << "VNCTabView: onSSHButtonClicked()";

    if (!_xenLib || this->_vmRef.isEmpty())
        return;

    // Check if SSH is supported and can start
    bool isSSHSupported = !_xenLib->isVMWindows(this->_vmRef);
    QString powerState = getCachedVmPowerState();
    QString ipAddress = _xenLib->getVMIPAddressForSSH(this->_vmRef);

    if (!isSSHSupported || powerState != "Running" || ipAddress.isEmpty())
    {
        qDebug() << "VNCTabView: Cannot start SSH - unsupported or not ready";
        return;
    }

    // Read SSH client configuration from QSettings
    QSettings settings;
    QString sshClient = settings.value("SSH/Client", "openssh").toString(); // "openssh" or "putty"
    QString sshClientPath;

    if (sshClient == "putty")
    {
        sshClientPath = settings.value("SSH/PuttyPath", "putty").toString();
    } else // openssh
    {
// On Linux, try common terminal emulators
#ifdef Q_OS_LINUX
        // Try to find a terminal emulator
        QStringList terminals = {"x-terminal-emulator", "gnome-terminal", "konsole",
                                 "xfce4-terminal", "xterm"};
        for (const QString& term : terminals)
        {
            QString path = QStandardPaths::findExecutable(term);
            if (!path.isEmpty())
            {
                sshClientPath = term;
                break;
            }
        }
#endif

#ifdef Q_OS_WINDOWS
        sshClientPath = settings.value("SSH/OpenSSHPath", "ssh.exe").toString();
#endif

#ifdef Q_OS_MAC
        sshClientPath = "Terminal.app";
#endif
    }

    if (sshClientPath.isEmpty())
    {
        QMessageBox::warning(this, tr("SSH Client Not Found"),
                             tr("No SSH client configured. Please install an SSH client (like PuTTY or OpenSSH) "
                                "or configure one in the application settings."));
        return;
    }

    // Build SSH command
    QString command;
    QStringList arguments;

    if (sshClient == "putty")
    {
        // PuTTY: putty -ssh root@192.168.1.100
        arguments << "-ssh" << QString("root@%1").arg(ipAddress);
    } else // openssh
    {
        // Get username from session (or default to root)
        QString username = "root"; // TODO: Get from session if available

#ifdef Q_OS_LINUX
        // For Linux terminals, we need to wrap the SSH command
        // e.g., gnome-terminal -- ssh user@host
        if (sshClientPath == "gnome-terminal" || sshClientPath == "xfce4-terminal")
        {
            arguments << "--" << "ssh" << QString("%1@%2").arg(username, ipAddress);
        } else if (sshClientPath == "konsole")
        {
            arguments << "-e" << "ssh" << QString("%1@%2").arg(username, ipAddress);
        } else if (sshClientPath == "x-terminal-emulator" || sshClientPath == "xterm")
        {
            arguments << "-e" << QString("ssh %1@%2").arg(username, ipAddress);
        } else
        {
            // Direct SSH command (might not work without terminal)
            sshClientPath = "ssh";
            arguments << QString("%1@%2").arg(username, ipAddress);
        }
#else
        // Windows/Mac: direct SSH
        arguments << QString("%1@%2").arg(username, ipAddress);
#endif
    }

    // Launch SSH client
    qDebug() << "VNCTabView: Launching SSH:" << sshClientPath << arguments;

    bool success = QProcess::startDetached(sshClientPath, arguments);
    if (!success)
    {
        QMessageBox::warning(this, tr("SSH Launch Failed"),
                             tr("Failed to launch SSH client.\n\nClient: %1\nTarget: root@%2\n\n"
                                "Please check that the SSH client is installed and accessible.")
                                 .arg(sshClientPath, ipAddress));
    }
}

void VNCTabView::onPowerStateLabelClicked()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 738-770
    qDebug() << "VNCTabView: onPowerStateLabelClicked()";

    if (!this->ui->powerStateLabel->isEnabled() || !_xenLib || this->_vmRef.isEmpty())
        return;

    XenConnection* conn = _xenLib->getConnection();
    if (!conn || !conn->isConnected())
    {
        qWarning() << "VNCTabView: Not connected to XenServer";
        return;
    }

    QString powerState = getCachedVmPowerState();
    if (powerState.isEmpty())
    {
        qWarning() << "VNCTabView: Power state unavailable in cache for" << this->_vmRef;
        return;
    }

    // C#: Execute VM action based on current power state
    if (powerState == "Halted")
    {
        // C#: if (source.allowed_operations.Contains(vm_operations.start))
        //         new StartVMCommand(Program.MainWindow, source).Run();

        // Check if start operation is allowed
        QVariantMap vmData = this->_xenLib->getCachedObjectData("vm", this->_vmRef);
        QVariantList allowedOps = vmData.value("allowed_operations").toList();

        bool canStart = false;
        for (const QVariant& op : allowedOps)
        {
            if (op.toString() == "start")
            {
                canStart = true;
                break;
            }
        }

        if (canStart)
        {
            qDebug() << "VNCTabView: Starting VM from power state label click";
            this->ui->powerStateLabel->setEnabled(false);
            disablePowerStateLabel(tr("Starting VM..."));

            MainWindow* mainWin = qobject_cast<MainWindow*>(this->window());
            StartVMCommand startCmd(mainWin);
            if (!startCmd.runForVm(this->_vmRef))
            {
                enablePowerStateLabel(tr("Failed to start VM"));
            }
        }
    } else if (powerState == "Suspended")
    {
        // C#: if (source.allowed_operations.Contains(vm_operations.resume))
        //         new ResumeVMCommand(Program.MainWindow, source).Run();

        QVariantMap vmData = this->_xenLib->getCachedObjectData("vm", this->_vmRef);
        QVariantList allowedOps = vmData.value("allowed_operations").toList();

        bool canResume = false;
        for (const QVariant& op : allowedOps)
        {
            if (op.toString() == "resume")
            {
                canResume = true;
                break;
            }
        }

        if (canResume)
        {
            qDebug() << "VNCTabView: Resuming VM from power state label click";
            this->ui->powerStateLabel->setEnabled(false);
            disablePowerStateLabel(tr("Resuming VM..."));

            MainWindow* mainWin = qobject_cast<MainWindow*>(this->window());
            ResumeVMCommand resumeCmd(mainWin);
            if (!resumeCmd.runForVm(this->_vmRef))
            {
                enablePowerStateLabel(tr("Failed to resume VM"));
            }
        }
    } else if (powerState == "Paused")
    {
        // C#: Paused state - unpause operation commented out in C#
        qDebug() << "VNCTabView: VM is paused (unpause not implemented)";
    }
}

void VNCTabView::onRDPorVNCResizeHandler()
{
    qDebug() << "VNCTabView: onRDPorVNCResizeHandler()";
    vncResizeHandler();
}

void VNCTabView::onDetectRDP()
{
    qDebug() << "VNCTabView: onDetectRDP()";

    // Invoke on main thread
    QMetaObject::invokeMethod(this, [this]() {
        qDebug() << "VNCTabView: onDetectRDP_()";
        
        // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1089-1113
        // Enable RDP button, maybe auto-switch
        
        this->ui->toggleConsoleButton->setEnabled(true);
        updateTooltipOfToggleButton();
        
        // Auto-switch to RDP if setting enabled and user hasn't manually switched
        // C#: if (!vncScreen.UserWantsToSwitchProtocol && Properties.Settings.Default.AutoSwitchToRDP)
        if (_vncScreen && !_vncScreen->userWantsToSwitchProtocol())
        {
            QSettings settings;
            bool autoSwitchToRDP = settings.value("Console/AutoSwitchToRDP", true).toBool();
            
            if (autoSwitchToRDP && this->_xenLib && this->_xenLib->isRDPEnabled(this->_vmRef))
            {
                qDebug() << "VNCTabView: Auto-switching to RDP (setting enabled)";
                // Switch to RDP automatically
                // C#: if (Program.MainWindow.TheTabControl.SelectedTab == Program.MainWindow.TabPageConsole)
                //         toggleConsoleButton_Click(null, null);
                //     else
                //         switchOnTabOpened = true;
                
                // For now, always switch immediately (tab visibility check would require MainWindow integration)
                onToggleConsoleButtonClicked();
            }
        } }, Qt::QueuedConnection);
}

void VNCTabView::onDetectVNC()
{
    qDebug() << "VNCTabView: onDetectVNC()";

    // Invoke on main thread
    QMetaObject::invokeMethod(this, [this]() {
        qDebug() << "VNCTabView: onDetectVNC_()";
        
        // C#: Lines 1120-1141
        // VNC is available
        
        this->ui->toggleConsoleButton->setEnabled(true);
        updateTooltipOfToggleButton(); }, Qt::QueuedConnection);
}

void VNCTabView::onUserCancelledAuth()
{
    qDebug() << "VNCTabView: onUserCancelledAuth()";

    // C#: Lines 1252-1273
    // User cancelled VNC password prompt

    QMessageBox::information(this,
                             tr("Console Authentication"),
                             tr("Console connection cancelled by user."));
}

void VNCTabView::onVncConnectionAttemptCancelled()
{
    qDebug() << "VNCTabView: onVncConnectionAttemptCancelled()";

    // C#: Lines 1243-1250
    // VNC connection attempt was cancelled
}

void VNCTabView::onInsKeyTimeout()
{
    qDebug() << "VNCTabView: onInsKeyTimeout()";

    // C#: Lines 969-983
    // INS key timeout - reset fullscreen hint

    // TODO: Hide fullscreen hint
}

// ========== Private Methods ==========

void VNCTabView::registerShortcutKeys()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 360-421
    qDebug() << "VNCTabView: registerShortcutKeys()";

    if (!_vncScreen)
        return;

    QSettings settings;

    // Fullscreen shortcut
    // C#: Properties.Settings.Default.FullScreenShortcutKey
    int fullScreenKey = settings.value("Console/FullScreenShortcutKey", 0).toInt();

    if (fullScreenKey == 0)
    {
        // C#: Ctrl + Alt (wait for INS key to complete fullscreen)
        _keyHandler.addKeyHandler(ConsoleShortcutKey::CTRL_ALT, [this]() {
            waitForInsKey();
        });
    } else if (fullScreenKey == 1)
    {
        // C#: Ctrl + Alt + F
        _keyHandler.addKeyHandler(ConsoleShortcutKey::CTRL_ALT_F, [this]() {
            toggleFullscreen();
        });
    } else if (fullScreenKey == 2)
    {
        // C#: F12
        _keyHandler.addKeyHandler(ConsoleShortcutKey::F12, [this]() {
            toggleFullscreen();
        });
    } else if (fullScreenKey == 3)
    {
        // C#: Ctrl + Enter
        _keyHandler.addKeyHandler(ConsoleShortcutKey::CTRL_ENTER, [this]() {
            toggleFullscreen();
        });
    }

    updateFullScreenButton();

    // Dock/Undock shortcut
    // C#: CA-10943 - Properties.Settings.Default.DockShortcutKey
    int dockKey = settings.value("Console/DockShortcutKey", 1).toInt();

    if (dockKey == 1)
    {
        // C#: Alt + Shift + U
        _keyHandler.addKeyHandler(ConsoleShortcutKey::ALT_SHIFT_U, [this]() {
            toggleDockUnDock();
        });
    } else if (dockKey == 2)
    {
        // C#: F11
        _keyHandler.addKeyHandler(ConsoleShortcutKey::F11, [this]() {
            toggleDockUnDock();
        });
    } else if (dockKey == 0)
    {
        // C#: <none>
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::ALT_SHIFT_U);
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::F11);
    }

    updateDockButton();

    // Uncapture keyboard and mouse key
    // C#: Properties.Settings.Default.UncaptureShortcutKey
    int uncaptureKey = settings.value("Console/UncaptureShortcutKey", 0).toInt();

    if (uncaptureKey == 0)
    {
        // C#: Right Ctrl
        _keyHandler.addKeyHandler(ConsoleShortcutKey::RIGHT_CTRL, [this]() {
            toggleConsoleFocus();
        });
    } else if (uncaptureKey == 1)
    {
        // C#: Left Alt
        _keyHandler.addKeyHandler(ConsoleShortcutKey::LEFT_ALT, [this]() {
            toggleConsoleFocus();
        });
    }

    qDebug() << "VNCTabView: Registered shortcuts - Fullscreen:" << fullScreenKey
             << "Dock:" << dockKey << "Uncapture:" << uncaptureKey;
}

void VNCTabView::deregisterShortcutKeys()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 423-476
    qDebug() << "VNCTabView: deregisterShortcutKeys()";

    if (!_vncScreen)
        return;

    // C#: The deregister method removes shortcuts based on settings
    // Since we're clearing all, we can just use clearHandlers()
    // But let's match C# behavior for completeness

    QSettings settings;
    int fullScreenKey = settings.value("Console/FullScreenShortcutKey", 0).toInt();
    int dockKey = settings.value("Console/DockShortcutKey", 1).toInt();
    int uncaptureKey = settings.value("Console/UncaptureShortcutKey", 0).toInt();

    // C#: Remove shortcuts that are NOT the current setting
    // (This allows changing shortcuts without conflicts)

    if (fullScreenKey != 0)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::CTRL_ALT);
    }
    if (fullScreenKey != 1)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::CTRL_ALT_F);
    }
    if (fullScreenKey != 2)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::F12);
    }
    if (fullScreenKey != 3)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::CTRL_ENTER);
    }

    if (dockKey != 1)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::ALT_SHIFT_U);
    }
    if (dockKey != 2)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::F11);
    }

    if (uncaptureKey != 0)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::RIGHT_CTRL);
    }
    if (uncaptureKey != 1)
    {
        _keyHandler.removeKeyHandler(ConsoleShortcutKey::LEFT_ALT);
    }

    qDebug() << "VNCTabView: Deregistered shortcuts";
}

void VNCTabView::registerEventListeners()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 117-159
    qDebug() << "VNCTabView: registerEventListeners()";

    if (!_xenLib || this->_vmRef.isEmpty())
    {
        qWarning() << "VNCTabView: Cannot register event listeners - xenLib or vmRef is null";
        return;
    }

    // Get cache from XenLib
    XenCache* cache = _xenLib->getCache();
    if (!cache)
    {
        qWarning() << "VNCTabView: Cannot register event listeners - cache is null";
        return;
    }

    // C#: source.PropertyChanged += Server_PropertyChanged;
    // Qt: Connect to cache objectChanged signal for this VM
    connect(cache, &XenCache::objectChanged, this, [this](const QString& type, const QString& ref) {
        if (type == "vm" && ref == this->_vmRef)
        {
            // Get updated VM data from cache
            QVariantMap vmData = this->_xenLib->getCache()->ResolveObjectData("vm", ref);
            if (!vmData.isEmpty())
            {
                // Check what changed and update UI accordingly
                // C#: Server_PropertyChanged checks specific properties
                updatePowerState();
                // TODO: Check other properties like allowed_operations, is_control_domain, etc.
            }
        }
    });

    // C#: guestMetrics.PropertyChanged += guestMetrics_PropertyChanged;
    // Use deferred update to avoid infinite loop (don't call cache queries inside cache callbacks)
    QString guestMetricsRef = this->_xenLib->getGuestMetricsRef(this->_vmRef);
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        connect(cache, &XenCache::objectChanged, this, [this, guestMetricsRef](const QString& type, const QString& ref) {
            if (type == "vm_guest_metrics" && ref == guestMetricsRef)
            {
                // Guest metrics changed - update RDP/SSH availability
                // C#: guestMetrics_PropertyChanged updates RDP button state
                qDebug() << "VNCTabView: Guest metrics changed for" << this->_vmRef;

                // Defer update to next event loop iteration to avoid calling cache queries
                // inside cache update callback (which causes infinite loops)
                QTimer::singleShot(0, this, [this]() {
                    // Update RDP availability based on new guest metrics
                    // Reference: C# VNCTabView.cs lines 565-580
                    // Check if RDP became available and auto-switch if enabled
                    onDetectRDP();
                });
            }
        });
    }

    // C#: For control domain, register host property changes
    // CRITICAL: Check isControlDomainZero() ONCE before creating connection, not inside lambda!
    // Otherwise it triggers API calls on every cache update → infinite loop
    QString hostRef;
    bool isControlDomain = this->_xenLib->isControlDomainZero(this->_vmRef, &hostRef);
    if (isControlDomain && !hostRef.isEmpty())
    {
        qDebug() << "VNCTabView: Registering host property listener for control domain on" << hostRef;
        connect(cache, &XenCache::objectChanged, this, [this, hostRef](const QString& type, const QString& ref) {
            if (type == "host" && ref == hostRef)
            {
                // Host changed - update power state
                updatePowerState();
            }
        });

        // C#: Also register host_metrics property changes
        QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
        QString hostMetricsRef = hostData.value("metrics").toString();
        if (!hostMetricsRef.isEmpty() && hostMetricsRef != "OpaqueRef:NULL")
        {
            qDebug() << "VNCTabView: Registering host_metrics listener for" << hostMetricsRef;
            connect(cache, &XenCache::objectChanged, this, [this, hostMetricsRef](const QString& type, const QString& ref) {
                if (type == "host_metrics" && ref == hostMetricsRef)
                {
                    // Host metrics changed - update power state (check 'live' field)
                    updatePowerState();
                }
            });
        }
    }

    // C#: For SR driver domain, register SR property changes
    // Use deferred update to avoid infinite loop (isSRDriverDomain() triggers cache queries)
    QString srRef;
    bool isSRDriver = this->_xenLib->isSRDriverDomain(this->_vmRef, &srRef);
    if (isSRDriver && !srRef.isEmpty())
    {
        qDebug() << "VNCTabView: Registering SR property listener for SR driver domain on" << srRef;
        connect(cache, &XenCache::objectChanged, this, [this, srRef](const QString& type, const QString& ref) {
            if (type == "sr" && ref == srRef)
            {
                // SR changed - may need to update labels
                // Defer update to avoid calling cache queries inside cache callback
                QTimer::singleShot(0, this, [this]() {
                    // Refresh VM data to get updated SR info
                    if (_xenLib)
                    {
                        _xenLib->requestObjectData("vm", this->_vmRef);
                    }
                });
            }
        });
    }

    // Note: C# also registers Host_CollectionChanged and VM_CollectionChanged for migration targets
    // This is more complex and involves tracking all hosts for migration capability
    // We'll implement this when migration UI is ported

    qDebug() << "VNCTabView: Event listeners registered for" << this->_vmRef;
}

void VNCTabView::unregisterEventListeners()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 290-338
    qDebug() << "VNCTabView: unregisterEventListeners()";

    if (!_xenLib)
    {
        qDebug() << "VNCTabView: xenLib is null, nothing to unregister";
        return;
    }

    // Get cache from XenLib
    XenCache* cache = _xenLib->getCache();
    if (!cache)
    {
        qDebug() << "VNCTabView: cache is null, nothing to unregister";
        return;
    }

    // C#: source.PropertyChanged -= Server_PropertyChanged;
    // C#: guestMetrics.PropertyChanged -= guestMetrics_PropertyChanged;
    // C#: host.PropertyChanged -= Server_PropertyChanged;
    // C#: hostMetrics.PropertyChanged -= Server_PropertyChanged;
    // C#: sr.PropertyChanged -= Server_PropertyChanged;
    // C#: Connection.Cache.DeregisterCollectionChanged<VM>(...)
    // C#: Connection.Cache.DeregisterCollectionChanged<Host>(...)

    // Qt: Disconnect all signals from cache to this object
    // This disconnects ALL connections from cache to this VNCTabView instance
    disconnect(cache, 0, this, 0);

    qDebug() << "VNCTabView: Event listeners unregistered for" << this->_vmRef;
}

void VNCTabView::updatePowerState()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 623-660
    qDebug() << "VNCTabView: updatePowerState() - VM:" << this->_vmRef;

    QString hostRef;
    if (this->_xenLib->isControlDomainZero(this->_vmRef, &hostRef))
    {
        qDebug() << "VNCTabView: VM is control domain for host:" << hostRef;

        // For control domain zero, check host metrics
        QVariantMap hostData = this->_xenLib->getCachedObjectData("host", hostRef);
        if (hostData.isEmpty())
        {
            qDebug() << "VNCTabView: Host data is empty";
            return;
        }

        QString metricsRef = hostData.value("metrics").toString();
        if (!metricsRef.isEmpty() && metricsRef != "OpaqueRef:NULL")
        {
            // Resolve host_metrics and check 'live' field
            QVariantMap metricsData = this->_xenLib->getCachedObjectData("host_metrics", metricsRef);
            bool isLive = metricsData.value("live", false).toBool();

            qDebug() << "VNCTabView: Host metrics live:" << isLive;

            if (isLive)
            {
                showTopBarContents();
            } else
            {
                hideTopBarContents();
            }
        } else
        {
            qDebug() << "VNCTabView: Host metrics not available, hiding top bar";
            hideTopBarContents();
        }
    } else
    {
        // Regular VM - check power_state
        QVariantMap vmData = getCachedVmData();
        QString powerState = vmData.value("power_state").toString();
        if (powerState.isEmpty())
        {
            qDebug() << "VNCTabView: VM data missing from cache for" << this->_vmRef << "- skipping power state update";
            return;
        }
        qDebug() << "VNCTabView: VM power_state:" << powerState;

        if (powerState == "Halted" || powerState == "Paused" || powerState == "Suspended")
        {
            qDebug() << "VNCTabView: VM is not running, hiding top bar";
            hideTopBarContents();
        } else if (powerState == "Running")
        {
            qDebug() << "VNCTabView: VM is running, showing top bar and enabling button";
            showTopBarContents();
            maybeEnableButton();
        } else
        {
            qDebug() << "VNCTabView: Unknown power state:" << powerState << ", hiding top bar";
            // Unknown state
            hideTopBarContents();
        }
    }

    updateOpenSSHConsoleButtonState();
}

void VNCTabView::maybeEnableButton()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 662-668
    // CA-8966: No way to get from graphical to text console if vm's networking is broken on startup
    qDebug() << "VNCTabView: maybeEnableButton()";

    // C#: if (vncScreen != null && (!vncScreen.UseVNC || !vncScreen.UseSource))
    // UseSource property not yet ported, so just check UseVNC
    if (_vncScreen && !_vncScreen->useVNC())
    {
        this->ui->toggleConsoleButton->setEnabled(true);
    }
}

void VNCTabView::enablePowerStateLabel(const QString& label)
{
    qDebug() << "VNCTabView: enablePowerStateLabel:" << label;

    // C#: Lines 670-675
    // EnablePowerStateLabel sets Enabled=true, Text=label, Cursor=Hand
    // The label becomes clickable
    this->ui->powerStateLabel->setEnabled(true);
    this->ui->powerStateLabel->setText(QString("<a href='action'>%1</a>").arg(label));
    this->ui->powerStateLabel->setCursor(Qt::PointingHandCursor);
    this->ui->powerStateLabel->setVisible(true);
    this->ui->warningPanel->setVisible(true);
}

void VNCTabView::disablePowerStateLabel(const QString& label)
{
    qDebug() << "VNCTabView: disablePowerStateLabel:" << label;

    // C#: Lines 677-682
    // DisablePowerStateLabel sets Enabled=false, Text=label, Cursor=Default
    // The label is NOT clickable but IS visible (shows power state)
    this->ui->powerStateLabel->setEnabled(false);
    this->ui->powerStateLabel->setText(label);
    this->ui->powerStateLabel->setCursor(Qt::ArrowCursor);
    this->ui->powerStateLabel->setVisible(true);
    this->ui->warningPanel->setVisible(true);
}

void VNCTabView::hideTopBarContents()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 684-728
    qDebug() << "VNCTabView: hideTopBarContents()";

    // VMPowerOff() - disable console controls
    vmPowerOff();

    if (_xenLib->isControlDomainZero(this->_vmRef))
    {
        // Control domain zero: show "Host is unavailable"
        // C#: DisablePowerStateLabel(Messages.CONSOLE_HOST_DEAD);
        qDebug() << "VNCTabView: Hide top bar contents, server is unavailable";
        disablePowerStateLabel(tr("This server is unavailable."));
    } else
    {
        // Regular VM: show power state
        QString powerState = getCachedVmPowerState();
        qDebug() << "VNCTabView: Hide top bar contents, powerstate=" << powerState;
        if (powerState.isEmpty())
        {
            disablePowerStateLabel(tr("Power state unknown."));
            this->ui->powerStateLabel->show();
            return;
        }

        // Get VM data to check allowed_operations and is_control_domain
        QVariantMap vmData = _xenLib->getCachedObjectData("vm", this->_vmRef);
        QVariantList allowedOps = vmData.value("allowed_operations").toList();
        bool isControlDomain = vmData.value("is_control_domain", false).toBool();

        // C#: Also check Helpers.EnabledTargetExists(targetHost, source.Connection)
        // For now, assume target exists if not control domain

        if (powerState == "Halted")
        {
            // Check if VM can be started
            bool canStart = false;
            for (const QVariant& op : allowedOps)
            {
                if (op.toString() == "start")
                {
                    canStart = true;
                    break;
                }
            }

            if (canStart && !isControlDomain)
            {
                // C#: EnablePowerStateLabel(Messages.CONSOLE_POWER_STATE_HALTED_START);
                enablePowerStateLabel(tr("This VM is currently shut down.  Click here to start it."));
            } else
            {
                // C#: DisablePowerStateLabel(Messages.CONSOLE_POWER_STATE_HALTED);
                disablePowerStateLabel(tr("This VM is currently shut down."));
            }
        } else if (powerState == "Paused")
        {
            // CA-12637: Pause/UnPause is not supported in the GUI
            // C#: DisablePowerStateLabel(Messages.CONSOLE_POWER_STATE_PAUSED);
            disablePowerStateLabel(tr("This VM is currently paused."));
        } else if (powerState == "Suspended")
        {
            // Check if VM can be resumed
            bool canResume = false;
            for (const QVariant& op : allowedOps)
            {
                if (op.toString() == "resume")
                {
                    canResume = true;
                    break;
                }
            }

            if (canResume && !isControlDomain)
            {
                // C#: EnablePowerStateLabel(Messages.CONSOLE_POWER_STATE_SUSPENDED_RESUME);
                enablePowerStateLabel(tr("This VM is currently suspended.  Click here to resume it."));
            } else
            {
                // C#: DisablePowerStateLabel(Messages.CONSOLE_POWER_STATE_SUSPENDED);
                disablePowerStateLabel(tr("This VM is currently suspended."));
            }
        } else
        {
            disablePowerStateLabel(powerState);
        }
    }

    // C#: powerStateLabel.Show();
    this->ui->powerStateLabel->show();
}

void VNCTabView::showTopBarContents()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 730-736
    qDebug() << "VNCTabView: Show top bar contents, VM is running";

    // C#: VMPowerOn();
    vmPowerOn();

    // C#: powerStateLabel.Hide();
    this->ui->powerStateLabel->hide();

    // Hide warning panel if no other warnings are visible
    if (!this->ui->gpuWarningLabel->isVisible())
        this->ui->warningPanel->setVisible(false);

    // Trigger console connection for Running VM
    // Reference: C# starts connection automatically when VM is running
    // Brief delay allows UI to stabilize before starting connection
    if (_vncScreen)
    {
        QTimer::singleShot(100, _vncScreen, &XSVNCScreen::connectNewHostedConsole);
        qDebug() << "VNCTabView: Triggering console connection for running VM";
    }
}

void VNCTabView::vmPowerOff()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1290-1302
    qDebug() << "VNCTabView: vmPowerOff()";

    // C#: toggleConsoleButton.Enabled = false;
    this->ui->toggleConsoleButton->setEnabled(false);

    // C#: VBD cdDrive = source.FindVMCDROM();
    // C#: multipleDvdIsoList1.Enabled = cdDrive == null ||
    //         source.power_state == vm_power_state.Halted &&
    //         (cdDrive.allowed_operations.Contains(vbd_operations.eject) ||
    //          cdDrive.allowed_operations.Contains(vbd_operations.insert));
    // For now, allow CD operations if VM is halted
    QString powerState = getCachedVmPowerState();
    bool enableCd = (powerState == "Halted");
    this->ui->cdIsoComboBox->setEnabled(enableCd);
    this->ui->cdEjectButton->setEnabled(enableCd);

    // C#: sendCAD.Enabled = false;
    this->ui->sendCADButton->setEnabled(false);
}

void VNCTabView::vmPowerOn()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1304-1308
    qDebug() << "VNCTabView: vmPowerOn()";

    // C#: No need to reenable toggleConsoleButton, polling will do it.
    // C#: multipleDvdIsoList1.Enabled = true;
    this->ui->cdIsoComboBox->setEnabled(true);
    this->ui->cdEjectButton->setEnabled(true);

    // C#: sendCAD.Enabled = true;
    this->ui->sendCADButton->setEnabled(true);
}

bool VNCTabView::canEnableRDP() const
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 784-788
    // C#: return RDPControlEnabled && !RDPEnabled;

    if (!_xenLib || this->_vmRef.isEmpty())
        return false;

    return this->_xenLib->isRDPControlEnabled(this->_vmRef) && !this->_xenLib->isRDPEnabled(this->_vmRef);
}

void VNCTabView::enableRDPIfCapable()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 592-598
    qDebug() << "VNCTabView: enableRDPIfCapable()";

    // C#: var enable = source.CanUseRDP();
    bool enable = this->_xenLib->canEnableRDP(this->_vmRef);

    if (enable)
    {
        qDebug() << "VNCTabView: Enabling RDP button, because RDP capability has appeared.";
    }

    // C#: toggleConsoleButton.Visible = toggleConsoleButton.Enabled = enable;
    this->ui->toggleConsoleButton->setVisible(enable);
    this->ui->toggleConsoleButton->setEnabled(enable);
}

/**
 * @brief Update button states and labels based on protocol
 * Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1194-1219
 */
void VNCTabView::updateButtons()
{
    qDebug() << "VNCTabView: updateButtons()";

    if (!_vncScreen)
        return;

    bool rdp = (_toggleToXVNCorRDP == RDP);

    // C#: toggleConsoleButton.Text = vncScreen.UseVNC
    //         ? CanEnableRDP() ? enableRDP : UseRDP
    //         : UseStandardDesktop;
    if (rdp)
    {
        if (_vncScreen->useVNC())
        {
            // Using VNC, button should switch to RDP
            this->ui->toggleConsoleButton->setText(canEnableRDP() ? tr("Enable Remote Desktop") : tr("Switch to Remote Desktop"));
        } else
        {
            // Using RDP, button should switch to VNC
            this->ui->toggleConsoleButton->setText(tr("Switch to Standard Desktop"));
        }
    } else
    {
        // Text console mode (XVNC)
        // C#: toggleConsoleButton.Text = vncScreen.UseSource ? UseXVNC : UseVNC;
        // UseSource not ported yet, so just show VNC label
        this->ui->toggleConsoleButton->setText(_vncScreen->useVNC() ? tr("Switch to Text Console") : tr("Switch to Graphical Console"));
    }

    // C#: UpdateTooltipOfToggleButton();
    updateTooltipOfToggleButton();

    // C#: scaleCheckBox.Visible = !rdp || vncScreen.UseVNC;
    this->ui->scaleCheckBox->setVisible(!rdp || _vncScreen->useVNC());

    // C#: sendCAD.Enabled = !rdp || vncScreen.UseVNC;
    this->ui->sendCADButton->setEnabled(!rdp || _vncScreen->useVNC());

    // C#: FocusVNC();
    // Focus the VNC screen widget
    if (_vncScreen)
        _vncScreen->setFocus();

    // C#: ignoreScaleChange = true;
    // C#: scaleCheckBox.Checked = (!rdp || vncScreen.UseVNC) ? (scaleCheckBox.Checked = oldScaleValue) : false;
    _ignoreScaleChange = true;
    if (!rdp || _vncScreen->useVNC())
    {
        this->ui->scaleCheckBox->setChecked(_oldScaleValue);
    } else
    {
        this->ui->scaleCheckBox->setChecked(false);
    }

    // C#: ignoreScaleChange = false;
    _ignoreScaleChange = false;
}

QString VNCTabView::guessNativeConsoleLabel() const
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 790-824
    // Make the 'enable RDP' button show something sensible if we can...

    QString label = tr("Looking for guest console...");

    if (!_xenLib || this->_vmRef.isEmpty())
        return label;

    // C#: XenRef<VM_guest_metrics> gm = source.guest_metrics;
    QString guestMetricsRef = this->_xenLib->getGuestMetricsRef(this->_vmRef);
    if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
        return label;

    // C#: VM_guest_metrics gmo = Connection.Resolve<VM_guest_metrics>(gm);
    QVariantMap guestMetrics = this->_xenLib->getCachedObjectData("vm_guest_metrics", guestMetricsRef);
    if (guestMetrics.isEmpty())
        return label;

    // C#: if (gmo != null && gmo.os_version != null)
    QVariantMap osVersion = guestMetrics.value("os_version").toMap();
    if (!osVersion.isEmpty())
    {
        // C#: if (gmo.os_version.ContainsKey("name"))
        if (osVersion.contains("name"))
        {
            QString osString = osVersion.value("name").toString();
            if (!osString.isEmpty())
            {
                // C#: if (osString.Contains("Microsoft"))
                //         label = CanEnableRDP() ? enableRDP : UseRDP;
                //     else
                //         label = UseXVNC;
                if (osString.contains("Microsoft", Qt::CaseInsensitive))
                {
                    label = canEnableRDP() ? tr("Enable Remote Desktop") : tr("Switch to Remote Desktop");
                } else
                {
                    // Linux or other Unix-like OS - offer text console
                    label = tr("Switch to Text Console");
                }
            }
        }
    }

    return label;
}

void VNCTabView::vncResizeHandler()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 836-843
    qDebug() << "VNCTabView: vncResizeHandler()";

    // C#: if (!ignoringResizes && DesktopSizeHasChanged() && !scaleCheckBox.Checked)
    //         MaybeScale();

    if (_ignoringResizes)
        return;

    if (!desktopSizeHasChanged())
        return;

    if (!ui->scaleCheckBox->isChecked())
    {
        qDebug() << "VNCTabView: Desktop size changed, auto-scaling...";
        maybeScale();
    }

    updateParentMinimumSize();
    emit consoleResized();
}

bool VNCTabView::desktopSizeHasChanged()
{
    // C#: Lines 870-881

    if (!_vncScreen)
        return false;

    QSize currentSize = _vncScreen->desktopSize();

    if (currentSize != _lastDesktopSize)
    {
        _lastDesktopSize = currentSize;
        return true;
    }

    return false;
}

void VNCTabView::waitForInsKey()
{
    qDebug() << "VNCTabView: waitForInsKey()";

    // C#: Lines 950-956
    // Start timer to wait for INS key

    _insKeyTimer->start(INS_KEY_TIMEOUT);

    // TODO: Show fullscreen hint
}

void VNCTabView::cancelWaitForInsKeyAndSendCAD()
{
    qDebug() << "VNCTabView: cancelWaitForInsKeyAndSendCAD()";

    // C#: Lines 958-967
    // Cancel INS key wait and send Ctrl+Alt+Del

    _insKeyTimer->stop();

    // TODO: Hide fullscreen hint

    if (_vncScreen)
        _vncScreen->sendCAD();
}

void VNCTabView::updateTooltipOfToggleButton()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1221-1225
    qDebug() << "VNCTabView: updateTooltipOfToggleButton()";

    if (!_xenLib || _vmRef.isEmpty())
    {
        ui->toggleConsoleButton->setToolTip("");
        return;
    }

    // C#: if (RDPEnabled || RDPControlEnabled)
    //         tip.SetToolTip(toggleConsoleButton, null);

    if (_xenLib->isRDPEnabled(_vmRef) || _xenLib->isRDPControlEnabled(_vmRef))
    {
        // Clear tooltip when RDP is available
        ui->toggleConsoleButton->setToolTip("");
    } else
    {
        // Show informative tooltip when RDP is not available
        QString tooltip;

        if (_xenLib->isHVM(_vmRef))
        {
            // HVM VM without RDP - explain why
            tooltip = tr("Remote Desktop is not available.\n"
                         "Install XenServer Tools in the VM to enable Remote Desktop support.");
        } else
        {
            // PV VM - RDP not supported
            tooltip = tr("Remote Desktop is only available for Windows VMs with XenServer Tools installed.");
        }

        ui->toggleConsoleButton->setToolTip(tooltip);
    }
}

void VNCTabView::updateOpenSSHConsoleButtonState()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1560-1591
    qDebug() << "VNCTabView: updateOpenSSHConsoleButtonState()";

    if (!_xenLib || _vmRef.isEmpty())
    {
        ui->sshButton->setVisible(false);
        return;
    }

    // Check if SSH console is supported
    // C#: IsSSHConsoleSupported - return false for Windows, true for Linux
    bool isSSHSupported = !_xenLib->isVMWindows(_vmRef);

    // For control domain, check if host metrics are live
    if (_xenLib->isControlDomainZero(_vmRef))
    {
        QVariantMap vmData = _xenLib->getCachedObjectData("vm", _vmRef);
        QString hostRef = vmData.value("resident_on").toString();

        if (!hostRef.isEmpty() && hostRef != "OpaqueRef:NULL")
        {
            QVariantMap hostData = _xenLib->getCachedObjectData("host", hostRef);
            QString metricsRef = hostData.value("metrics").toString();

            if (!metricsRef.isEmpty() && metricsRef != "OpaqueRef:NULL")
            {
                QVariantMap metricsData = _xenLib->getCachedObjectData("host_metrics", metricsRef);
                bool live = metricsData.value("live").toBool();

                if (!live)
                {
                    isSSHSupported = false;
                }
            } else
            {
                isSSHSupported = false;
            }
        }
    }

    // C#: buttonSSH.Visible = isSshConsoleSupported && source.power_state != vm_power_state.Halted;
    QString powerState = getCachedVmPowerState();
    if (powerState.isEmpty())
    {
        ui->sshButton->setVisible(false);
        return;
    }
    bool visible = isSSHSupported && (powerState != "Halted");
    ui->sshButton->setVisible(visible);

    // C#: buttonSSH.Enabled = isSshConsoleSupported && CanStartSSHConsole;
    // CanStartSSHConsole = power_state == Running && !string.IsNullOrEmpty(IPAddressForSSH())
    bool canStart = (powerState == "Running") && !_xenLib->getVMIPAddressForSSH(_vmRef).isEmpty();
    ui->sshButton->setEnabled(canStart && isSSHSupported);
}

void VNCTabView::showOrHideRdpVersionWarning()
{
    qDebug() << "VNCTabView: showOrHideRdpVersionWarning()";

    // C#: Lines 220-223

    bool showWarning = _vncScreen && _vncScreen->rdpVersionWarningNeeded();

    ui->rdpWarningIcon->setVisible(showWarning);
    ui->rdpWarningLabel->setVisible(showWarning);

    if (showWarning)
    {
        ui->rdpWarningLabel->setText(tr("Using compatibility RDP version"));
    }
}

void VNCTabView::showGpuWarningIfRequired(bool mustConnectRemoteDesktop)
{
    qDebug() << "VNCTabView: showGpuWarningIfRequired:" << mustConnectRemoteDesktop;

    // C#: Lines 1367-1379

    if (mustConnectRemoteDesktop)
    {
        ui->gpuWarningLabel->setText(tr("This VM has GPU passthrough enabled. You must use Remote Desktop to connect."));
        ui->gpuWarningLabel->setVisible(true);
        ui->warningPanel->setVisible(true);
    } else
    {
        ui->gpuWarningLabel->setVisible(false);

        // Hide warning panel if no other warnings
        if (!ui->powerStateLabel->isVisible())
            ui->warningPanel->setVisible(false);
    }
}

void VNCTabView::toggleDockUnDock()
{
    qDebug() << "VNCTabView: toggleDockUnDock()";

    // C#: Lines 995-1005

    if (_inToggleDockUnDock)
        return;

    _inToggleDockUnDock = true;

    emit toggleDockRequested();

    _inToggleDockUnDock = false;
}

void VNCTabView::toggleFullscreen()
{
    qDebug() << "VNCTabView: toggleFullscreen()";

    // C#: Lines 1008-1066

    if (_inToggleFullscreen)
        return;

    _inToggleFullscreen = true;

    emit toggleFullscreenRequested();

    _inToggleFullscreen = false;
}

void VNCTabView::toggleConsoleFocus()
{
    // Reference: XenAdmin/ConsoleView/VNCTabView.cs lines 1436-1456
    qDebug() << "VNCTabView: toggleConsoleFocus()";

    if (_inToggleConsoleFocus)
        return;

    _inToggleConsoleFocus = true;

    if (_vncScreen)
    {
        // C#: if (vncScreen.Focused && vncScreen.ActiveControl == null)
        //         vncScreen.CaptureKeyboardAndMouse();
        //     else
        //     {
        //         vncScreen.UncaptureKeyboardAndMouse();
        //         vncScreen.Refresh();
        //     }

        // Check if VNC screen is focused and no active control (i.e., console has focus)
        if (_vncScreen->hasFocus())
        {
            // Console is focused - capture keyboard and mouse
            _vncScreen->captureKeyboardAndMouse();
        } else
        {
            // Console is not focused - release keyboard and mouse
            _vncScreen->uncaptureKeyboardAndMouse();
            _vncScreen->update(); // C# Refresh() → Qt update()
        }
    }

    _inToggleConsoleFocus = false;
}

QVariantMap VNCTabView::getCachedVmData() const
{
    if (!_xenLib || _vmRef.isEmpty())
        return QVariantMap();

    return _xenLib->getCachedObjectData("vm", _vmRef);
}

QString VNCTabView::getCachedVmPowerState() const
{
    return getCachedVmData().value("power_state").toString();
}

// ========== CD/DVD Management ==========

void VNCTabView::refreshCdDrives()
{
    // Reference: MultipleDvdIsoList.cs lines 116-187
    qDebug() << "VNCTabView: refreshCdDrives()";

    if (_inCdRefresh || !_xenLib || _vmRef.isEmpty())
        return;

    _inCdRefresh = true;

    // Save current selection
    QString prevSelectedVbd = _currentCdVbdRef;

    // Clear drives combo box
    ui->cdDriveComboBox->clear();

    // Get VM data
    QVariantMap vmData = _xenLib->getCachedObjectData("vm", _vmRef);
    bool isControlDomain = vmData.value("is_control_domain", false).toBool();

    if (isControlDomain)
    {
        // Control domains don't have CD drives
        _inCdRefresh = false;
        ui->cdDriveLabel->setVisible(false);
        ui->cdDriveComboBox->setVisible(false);
        ui->cdIsoComboBox->setVisible(false);
        ui->cdEjectButton->setVisible(false);
        ui->cdNewDriveLabel->setVisible(false);
        return;
    }

    // Get all VBDs for this VM
    QVariantList vbdRefs = vmData.value("VBDs").toList();
    QList<QVariantMap> cdDrives;

    int dvdCount = 0;
    for (const QVariant& vbdRefVar : vbdRefs)
    {
        QString vbdRef = vbdRefVar.toString();
        QVariantMap vbdData = _xenLib->getCachedObjectData("vbd", vbdRef);

        QString type = vbdData.value("type").toString();
        if (type == "CD")
        {
            dvdCount++;
            QString driveName = QString("DVD Drive %1").arg(dvdCount);

            ui->cdDriveComboBox->addItem(driveName, vbdRef);
            cdDrives.append(vbdData);
        }
    }

    // Update UI visibility
    bool hasDrives = dvdCount > 0;
    ui->cdDriveLabel->setVisible(hasDrives && dvdCount > 1);
    ui->cdDriveComboBox->setVisible(dvdCount > 1);
    ui->cdIsoComboBox->setVisible(hasDrives);
    ui->cdEjectButton->setVisible(hasDrives);
    ui->cdNewDriveLabel->setVisible(!hasDrives);

    // Restore previous selection or select first drive
    if (!prevSelectedVbd.isEmpty())
    {
        int index = ui->cdDriveComboBox->findData(prevSelectedVbd);
        if (index >= 0)
        {
            ui->cdDriveComboBox->setCurrentIndex(index);
        } else if (ui->cdDriveComboBox->count() > 0)
        {
            ui->cdDriveComboBox->setCurrentIndex(0);
        }
    } else if (ui->cdDriveComboBox->count() > 0)
    {
        ui->cdDriveComboBox->setCurrentIndex(0);
    }

    _inCdRefresh = false;

    // Refresh ISO list for selected drive
    refreshIsoList();
}

void VNCTabView::refreshIsoList()
{
    // Reference: ISODropDownBox.cs (base class of CDChanger)
    qDebug() << "VNCTabView: refreshIsoList()";

    if (!_xenLib || _vmRef.isEmpty())
        return;

    // Get currently selected VBD
    QString vbdRef = ui->cdDriveComboBox->currentData().toString();
    if (vbdRef.isEmpty() && ui->cdDriveComboBox->count() > 0)
    {
        vbdRef = ui->cdDriveComboBox->itemData(0).toString();
    }

    _currentCdVbdRef = vbdRef;

    if (vbdRef.isEmpty())
    {
        ui->cdIsoComboBox->clear();
        return;
    }

    // Get VBD data
    QVariantMap vbdData = _xenLib->getCachedObjectData("vbd", vbdRef);
    bool isEmpty = vbdData.value("empty", true).toBool();
    QString currentVdiRef = vbdData.value("VDI").toString();

    IsoDropDownBox* isoBox = qobject_cast<IsoDropDownBox*>(ui->cdIsoComboBox);
    if (!isoBox)
        return;

    isoBox->setXenLib(_xenLib);
    isoBox->setVMRef(_vmRef);
    isoBox->refresh();

    if (!isEmpty && !currentVdiRef.isEmpty())
        isoBox->setSelectedVdiRef(currentVdiRef);
    else
        isoBox->setSelectedVdiRef(QString());
}

void VNCTabView::onCdDriveChanged(int index)
{
    // Reference: MultipleDvdIsoList.cs lines 233-241
    qDebug() << "VNCTabView: onCdDriveChanged:" << index;

    if (_inCdRefresh)
        return;

    QString vbdRef = ui->cdDriveComboBox->itemData(index).toString();
    _currentCdVbdRef = vbdRef;

    // Refresh ISO list for new drive
    refreshIsoList();
}

void VNCTabView::onCdIsoSelected(int index)
{
    // Reference: CDChanger.cs lines 66-87
    qDebug() << "VNCTabView: onCdIsoSelected:" << index;

    if (_inCdRefresh || _currentCdVbdRef.isEmpty())
        return;

    QString vdiRef = ui->cdIsoComboBox->itemData(index).toString();

    // Get current VBD state
    QVariantMap vbdData = _xenLib->getCachedObjectData("vbd", _currentCdVbdRef);
    QString currentVdiRef = vbdData.value("VDI").toString();
    bool isEmpty = vbdData.value("empty", true).toBool();

    // Don't change if selecting same ISO or both empty
    if (vdiRef == currentVdiRef)
        return;
    if (vdiRef.isEmpty() && isEmpty)
        return;

    // Change the ISO
    changeCdIso(vdiRef);
}

void VNCTabView::onCdEject()
{
    // Reference: MultipleDvdIsoList.cs lines 267-273
    qDebug() << "VNCTabView: onCdEject()";

    if (_currentCdVbdRef.isEmpty())
        return;

    // Eject = mount empty
    changeCdIso(QString());
}

void VNCTabView::changeCdIso(const QString& vdiRef)
{
    // Reference: CDChanger.cs lines 89-108
    qDebug() << "VNCTabView: changeCdIso:" << vdiRef;

    if (_currentCdVbdRef.isEmpty() || !_xenLib)
        return;

    // Disable controls during change
    ui->cdIsoComboBox->setEnabled(false);
    ui->cdEjectButton->setEnabled(false);

    // Use ChangeVMISOAction equivalent
    // For now, use direct XenAPI call (proper action implementation would be better)
    QVariantMap vbdData = _xenLib->getCachedObjectData("vbd", _currentCdVbdRef);
    QString vmRef = vbdData.value("VM").toString();

    // Call xenLib method to change CD
    bool success = _xenLib->changeVMISO(vmRef, _currentCdVbdRef, vdiRef);

    // Re-enable controls
    ui->cdIsoComboBox->setEnabled(true);
    ui->cdEjectButton->setEnabled(true);

    if (success)
    {
        qDebug() << "VNCTabView: Successfully changed CD/DVD";
        // Refresh to show new state
        QTimer::singleShot(500, this, [this]() {
            refreshIsoList();
        });
    } else
    {
        qWarning() << "VNCTabView: Failed to change CD/DVD";
        // Restore UI state
        refreshIsoList();
    }
}

void VNCTabView::onCreateNewCdDrive()
{
    // Reference: MultipleDvdIsoList.cs lines 243-265
    qDebug() << "VNCTabView: onCreateNewCdDrive()";

    if (!_xenLib || _vmRef.isEmpty())
        return;

    // Get VM data to check if HVM
    QVariantMap vmData = _xenLib->getCachedObjectData("vm", _vmRef);
    bool isHVM = _xenLib->isHVM(_vmRef);
    QString vmName = vmData.value("name_label").toString();

    if (isHVM)
    {
        // Warn user about HVM CD drive creation
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Create CD/DVD Drive"),
            tr("Creating a new CD/DVD drive for HVM VM '%1' requires a reboot. Continue?").arg(vmName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (reply != QMessageBox::Yes)
            return;
    }

    // Create new CD drive via XenLib
    bool success = _xenLib->createCdDrive(_vmRef);

    if (success)
    {
        qDebug() << "VNCTabView: Successfully created CD/DVD drive";
        // Refresh drives list
        QTimer::singleShot(1000, this, [this]() {
            refreshCdDrives();
        });
    } else
    {
        QMessageBox::warning(
            this,
            tr("Create CD/DVD Drive"),
            tr("Failed to create CD/DVD drive"));
    }
}
