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

#include "XSVNCScreen.h"
#include "VNCGraphicsClient.h"
#include "RdpClient.h"
#include "IRemoteConsole.h"
#include "ConsoleKeyHandler.h"
#include <QDebug>
#include <QThread>
#include <QThreadPool>
#include <QTcpSocket>
#include <QImage>
#include <QPalette>
#include <QApplication>
#include <QVBoxLayout>
#include "xenlib.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xencache.h"
#include "network/httpconnect.h"

// TODO: Forward declare VNCTabView and RdpClient when they exist
class VNCTabView;
class RdpClient;

/**
 * @brief Constructor - matches C# XSVNCScreen(VM source, EventHandler resizeHandler,
 *                      VNCTabView parent, string elevatedUsername, string elevatedPassword)
 *
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 147-177
 */
XSVNCScreen::XSVNCScreen(const QString& sourceRef, VNCTabView* parent, XenLib* xenLib,
                         const QString& elevatedUsername, const QString& elevatedPassword)
    : QWidget(nullptr), _sourceRef(sourceRef), _sourceIsPv(false), _xenLib(xenLib), _parentVNCTabView(parent), _keyHandler(nullptr) // TODO: Get from parent when VNCTabView exists
      ,
      _vncClient(nullptr), _rdpClient(nullptr), _remoteConsole(nullptr), _useVNC(true), _useSource(true) // Default to hosted/source console (C#: _useSource = true)
      ,
      _autoSwitchRDPLater(false), _connectionRetries(0), _wasPaused(true), _haveTriedLoginWithoutPassword(false), _ignoreNextError(false), _userWantsToSwitchProtocol(false), _elevatedUsername(elevatedUsername), _elevatedPassword(elevatedPassword), _connectionPoller(nullptr), _hostedConsoleConnectionPending(false), _pendingVNCConnection(nullptr), _autoCaptureKeyboardAndMouse(true), _focusColor(QApplication::palette().color(QPalette::Highlight))
{
    qDebug() << "XSVNCScreen: Constructor for source:" << this->_sourceRef;

    // Force handle creation (equivalent to C#: var _ = Handle;)
    // This ensures the widget has a native window handle for rendering
    this->winId();

    // Create layout so VNCGraphicsClient fills the available space
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Determine if source is PV or HVM
    // Reference: C# XSVNCScreen.cs line 703: _sourceIsPv = !value.IsHVM();
    if (!this->_sourceRef.isEmpty() && this->_xenLib && this->_xenLib->getConnection())
    {
        XenCache* cache = this->_xenLib->getConnection()->GetCache();
        QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
        if (!vmRecord.isEmpty())
        {
            // Check if HVM (true) or PV (false)
            bool isHvm = vmRecord.value("is_hvm", false).toBool();
            this->_sourceIsPv = !isHvm;
            qDebug() << "XSVNCScreen: VM" << this->_sourceRef << "is" << (this->_sourceIsPv ? "PV" : "HVM");
        } else
        {
            qWarning() << "XSVNCScreen: Could not resolve VM record for" << this->_sourceRef;
        }
    }

    // Initialize console control (VNC or RDP client)
    this->initSubControl();

    // Register event listeners for VM/metrics changes
    if (!this->_sourceRef.isEmpty())
    {
        this->registerEventListeners();

        // Cache initial network info from guest metrics
        // C#: var guestMetrics = Source.Connection.Resolve(Source.guest_metrics);
        // C#: _cachedNetworks = guestMetrics.networks;
        if (this->_xenLib && this->_xenLib->getConnection())
        {
            XenCache* cache = this->_xenLib->getConnection()->GetCache();
            QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
            QString guestMetricsRef = vmRecord.value("guest_metrics").toString();

            if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
            {
                QVariantMap guestMetrics = cache->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
                QVariantMap networks = guestMetrics.value("networks").toMap();

                // Convert QVariantMap to QMap<QString, QString>
                this->_cachedNetworks.clear();
                for (auto it = networks.begin(); it != networks.end(); ++it)
                {
                    this->_cachedNetworks.insert(it.key(), it.value().toString());
                }

                qDebug() << "XSVNCScreen: Cached" << this->_cachedNetworks.size() << "network entries";
            }
        }
    }
}

/**
 * @brief Destructor - matches C# Dispose(bool disposing)
 *
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 256-284
 */
XSVNCScreen::~XSVNCScreen()
{
    qDebug() << "XSVNCScreen: Destructor";

    // Unregister all event listeners
    this->unregisterEventListeners();

    // Stop polling timer
    if (this->_connectionPoller)
    {
        this->_connectionPoller->stop();
        delete this->_connectionPoller;
        this->_connectionPoller = nullptr;
    }

    // Disconnect and cleanup remote console
    if (this->_remoteConsole)
    {
        this->_remoteConsole->disconnectAndDispose();
        this->_remoteConsole = nullptr;
    }

    // Cleanup VNC client
    if (this->_vncClient)
    {
        delete this->_vncClient;
        this->_vncClient = nullptr;
    }

    // Cleanup RDP client
    if (this->_rdpClient)
    {
        delete this->_rdpClient;
        this->_rdpClient = nullptr;
    }

    // Clear pending VNC connection
    qDebug() << "XSVNCScreen: Set pending VNC connection to null";
    this->setPendingVNCConnection(nullptr);
}

// ========== Public Property Accessors ==========

/**
 * @brief Get desktop size from active console
 * Equivalent to C#: public Size DesktopSize => RemoteConsole?.DesktopSize ?? Size.Empty;
 */
QSize XSVNCScreen::desktopSize() const
{
    return this->_remoteConsole ? this->_remoteConsole->desktopSize() : QSize();
}

/**
 * @brief Check if RDP version warning is needed
 * Equivalent to C#: public bool RdpVersionWarningNeeded => _rdpClient != null && _rdpClient.needsRdpVersionWarning;
 */
bool XSVNCScreen::rdpVersionWarningNeeded() const
{
    // Note: RdpClient uses FreeRDP which doesn't have Windows RDP version issues
    // This was for Windows native RDP ActiveX control version compatibility
    return false;
}

/**
 * @brief Set VNC password (stored securely)
 * Equivalent to C#: private char[] _vncPassword
 */
void XSVNCScreen::setVncPassword(const QString& password)
{
    this->_vncPassword = password.toUtf8();
}

/**
 * @brief Get VNC password
 */
QString XSVNCScreen::vncPassword() const
{
    return QString::fromUtf8(this->_vncPassword);
}

/**
 * @brief Set whether to use VNC (true) or RDP (false)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 652-659
 */
void XSVNCScreen::setUseVNC(bool value)
{
    if (value != this->_useVNC)
    {
        this->_useVNC = value;
        qDebug() << "XSVNCScreen: UseVNC changed to:" << this->_useVNC;
        // TODO: Trigger reconnection if needed
    }
}

/**
 * @brief Set whether to use source/hosted console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 666-675
 */
void XSVNCScreen::setUseSource(bool value)
{
    if (value != this->_useSource)
    {
        this->_useSource = value;
        qDebug() << "XSVNCScreen: UseSource changed to:" << this->_useSource;

        // Update VNC client if it exists
        if (this->_vncClient)
        {
            this->_vncClient->setUseSource(this->_useSource);
        }

        // Trigger reconnection
        // C#: connectNewHostedConsole();
        QTimer::singleShot(0, this, &XSVNCScreen::connectNewHostedConsole);
    }
}

// ========== Public Methods ==========

/**
 * @brief Pause the remote console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 234-241
 */
void XSVNCScreen::pause()
{
    if (this->_remoteConsole)
    {
        this->_wasPaused = true;
        this->_remoteConsole->pause();
    }
}

/**
 * @brief Unpause the remote console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 243-251
 */
void XSVNCScreen::unpause()
{
    if (this->_remoteConsole)
    {
        this->_wasPaused = false;
        this->_remoteConsole->unpause();
    }
}

/**
 * @brief Send Ctrl+Alt+Del to remote console
 */
void XSVNCScreen::sendCAD()
{
    if (this->_remoteConsole)
    {
        this->_remoteConsole->sendCAD();
    }
}

/**
 * @brief Capture screenshot from console
 */
QImage XSVNCScreen::snapshot()
{
    if (this->_remoteConsole)
    {
        return this->_remoteConsole->snapshot();
    }
    return QImage();
}

/**
 * @brief Set console scaling mode
 */
void XSVNCScreen::setScaling(bool enabled)
{
    qDebug() << "XSVNCScreen: setScaling:" << enabled;

    if (this->_remoteConsole)
        this->_remoteConsole->setScaling(enabled);
}

/**
 * @brief Get console scaling mode
 */
bool XSVNCScreen::scaling() const
{
    if (this->_remoteConsole)
        return this->_remoteConsole->scaling();

    return false;
}

/**
 * @brief Check if must connect via remote desktop (GPU passthrough)
 */
bool XSVNCScreen::mustConnectRemoteDesktop() const
{
    qDebug() << "XSVNCScreen: mustConnectRemoteDesktop()";

#ifndef HAVE_FREERDP
    // RDP support not available - always use VNC
    return false;
#else
    // Check if VM has GPU passthrough (requires remote desktop connection)
    if (this->_xenLib && this->_xenLib->hasGPUPassthrough(this->_sourceRef))
    {
        return true;
    }

    return false;
#endif
}

/**
 * @brief Capture keyboard and mouse input to console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1288-1303
 */
void XSVNCScreen::captureKeyboardAndMouse()
{
    qDebug() << "XSVNCScreen: captureKeyboardAndMouse()";

    // Activate the remote console control
    if (this->_remoteConsole)
    {
        this->_remoteConsole->activate();

        // Enable keyboard/mouse capture if auto-capture is enabled
        if (this->_autoCaptureKeyboardAndMouse)
        {
            this->setKeyboardAndMouseCapture(true);
        }

        this->unpause();
    }

    // C#: DisableMenuShortcuts() - disable main window menu shortcuts
    // This allows all keyboard input to go to the console
    // Note: In Qt, we handle this through focus and event filtering
}

/**
 * @brief Release keyboard and mouse capture from console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1276-1286
 */
void XSVNCScreen::uncaptureKeyboardAndMouse()
{
    qDebug() << "XSVNCScreen: uncaptureKeyboardAndMouse()";

    // Release keyboard/mouse capture if auto-capture is enabled
    if (this->_autoCaptureKeyboardAndMouse)
    {
        this->setKeyboardAndMouseCapture(false);
    }

    // C#: ActiveControl = null; - clear active control
    // In Qt, this means releasing focus
    if (this->_remoteConsole)
    {
        QWidget* consoleWidget = this->_remoteConsole->consoleControl();
        if (consoleWidget)
        {
            consoleWidget->clearFocus();
        }
    }

    // C#: EnableMenuShortcuts() - re-enable main window menu shortcuts
    // This allows keyboard shortcuts to work in the main window again
}

/**
 * @brief Set keyboard and mouse capture state
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 599-604
 */
void XSVNCScreen::setKeyboardAndMouseCapture(bool enabled)
{
    // C#: if (RemoteConsole?.ConsoleControl != null)
    //         RemoteConsole.ConsoleControl.TabStop = value;

    if (this->_remoteConsole)
    {
        QWidget* consoleWidget = this->_remoteConsole->consoleControl();
        if (consoleWidget)
        {
            // Qt equivalent of TabStop - control whether widget can receive focus
            consoleWidget->setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);

            if (enabled)
            {
                // Grab keyboard and mouse input when capturing
                consoleWidget->setFocus(Qt::OtherFocusReason);
                consoleWidget->grabKeyboard();
                consoleWidget->grabMouse();
            } else
            {
                // Release keyboard and mouse input when uncapturing
                consoleWidget->releaseKeyboard();
                consoleWidget->releaseMouse();
            }
        }
    }
}

/**
 * @brief Disconnect and dispose all connections
 */
void XSVNCScreen::disconnectAndDispose()
{
    if (this->_remoteConsole)
    {
        this->_remoteConsole->disconnectAndDispose();
        this->_remoteConsole = nullptr;
    }
}

// ========== Private Methods - Initialization ==========

/**
 * @brief Initialize the console sub-control (VNC or RDP client)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 469-514
 */
void XSVNCScreen::initSubControl()
{
    qDebug() << "XSVNCScreen: initSubControl() - creating VNC/RDP client";

    // C# checks Source != null and Source.is_control_domain
    // For now, always create VNC client (RDP support comes later)

    if (!this->_vncClient)
    {
        this->_vncClient = new VNCGraphicsClient(this);

        // Add to layout so it fills the available space
        if (layout())
        {
            layout()->addWidget(this->_vncClient);
        }

        // Wire up VNC client signals (C# XSVNCScreen.cs lines 240-270)
        // These handle connection success/failure, resizing, etc.

        // Connection success handler
        QObject::connect(this->_vncClient, &VNCGraphicsClient::connectionSuccess,
                         this, &XSVNCScreen::onVncClientConnected);

        // Connection error handler
        QObject::connect(this->_vncClient, &VNCGraphicsClient::errorOccurred,
                         this, &XSVNCScreen::onVncClientError);

        // Desktop resize handler (for updating parent window size)
        QObject::connect(this->_vncClient, &VNCGraphicsClient::desktopResized,
                         this, &XSVNCScreen::onDesktopResized);

        qDebug() << "XSVNCScreen: VNC client signals connected";
    }

    this->_remoteConsole = this->_vncClient;

    // Configure VNC client based on VM type (matches C# XSVNCScreen.cs line 576)
    if (this->_remoteConsole)
    {
        // Set key handler
        this->_remoteConsole->setKeyHandler(this->_keyHandler);

        // PV VMs use keysyms, HVM VMs use scan codes
        // This is critical for proper keyboard input!
        this->_remoteConsole->setSendScanCodes(!this->_sourceIsPv);

        qDebug() << "XSVNCScreen: SendScanCodes set to" << !this->_sourceIsPv
                 << "(PV:" << this->_sourceIsPv << ")";
    }

    // Layout the console widget to fill parent
    if (this->_remoteConsole)
    {
        QWidget* consoleWidget = dynamic_cast<QWidget*>(this->_remoteConsole);
        if (consoleWidget)
        {
            consoleWidget->setParent(this);
            consoleWidget->setGeometry(rect());
            consoleWidget->show();
        }
    }

    // C# sets AutoScaleMode, AutoSize, margins, etc.
    // Qt equivalent: setSizePolicy, margins
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setContentsMargins(0, 0, 0, 0);
}

/**
 * @brief Register event listeners for VM/metrics changes
 */
void XSVNCScreen::registerEventListeners()
{
    if (this->_sourceRef.isEmpty() || !this->_xenLib)
        return;

    // Connect to XenLib signals for object changes (forwarded from EventPoller)
    QObject::connect(this->_xenLib, &XenLib::objectDataReceived, this, &XSVNCScreen::onObjectDataReceived);

    // Connect to cache's objectChanged signal for real-time updates (e.g., power_state changes)
    // This is the key signal for detecting when a VM powers on/off
    XenCache* cache = this->_xenLib->getConnection() ? this->_xenLib->getConnection()->GetCache() : nullptr;
    if (cache)
    {
        QObject::connect(cache, &XenCache::objectChanged, this, &XSVNCScreen::onCacheObjectChanged);
        qDebug() << "XSVNCScreen: Connected to cache objectChanged signal";
    }

    qDebug() << "XSVNCScreen: Event listeners registered for" << this->_sourceRef;
}

/**
 * @brief Unregister all event listeners
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 181-195
 */
void XSVNCScreen::unregisterEventListeners()
{
    if (this->_sourceRef.isEmpty())
        return;

    // Disconnect from XenLib signals
    if (this->_xenLib)
    {
        disconnect(this->_xenLib, &XenLib::objectDataReceived, this, &XSVNCScreen::onObjectDataReceived);

        // Disconnect from cache signals
        XenCache* cache = this->_xenLib->getConnection() ? this->_xenLib->getConnection()->GetCache() : nullptr;
        if (cache)
        {
            disconnect(cache, &XenCache::objectChanged, this, &XSVNCScreen::onCacheObjectChanged);
        }
    }

    qDebug() << "XSVNCScreen: Event listeners unregistered for" << this->_sourceRef;
}

// ========== Private Slots - Event Handlers ==========

/**
 * @brief Handle VM property changes
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs (VM_PropertyChanged not shown in excerpt)
 */
void XSVNCScreen::onVMPropertyChanged(const QString& vmRef, const QString& propertyName)
{
    if (vmRef != this->_sourceRef)
        return;

    qDebug() << "XSVNCScreen: VM property changed:" << propertyName;

    // TODO: Handle power state changes, name changes, etc.
    // If power state changed to running, might need to start polling
}

/**
 * @brief Handle guest metrics property changes (IP address changes trigger re-polling)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 197-217
 */
void XSVNCScreen::onGuestMetricsPropertyChanged(const QString& propertyName)
{
    if (this->_sourceRef.isEmpty())
        return;

    if (propertyName == "networks")
    {
        // TODO: Get new networks from guest metrics
        // QMap<QString, QString> newNetworks = ...;

        // if (!equateDictionary(newNetworks, _cachedNetworks))
        // {
        //     qDebug() << "XSVNCScreen: Detected IP address change, repolling for VNC/RDP...";
        //     _cachedNetworks = newNetworks;
        //     startPolling();
        // }
    }
}

/**
 * @brief Handle object data changes from EventPoller
 * Routes to specific handlers based on object type
 */
void XSVNCScreen::onObjectDataReceived(const QString& objectType, const QString& objectRef, const QVariantMap& data)
{
    if (objectType == "vm" && objectRef == this->_sourceRef)
    {
        // VM property changed - check if it affects console
        this->onVMDataChanged(data);
    } else if (objectType == "vm_guest_metrics" && objectRef == this->_guestMetricsRef)
    {
        // Guest metrics changed - check for IP address changes
        this->onGuestMetricsChanged(data);
    }
}

/**
 * @brief Handle VM data changes (power state, name, etc.)
 */
void XSVNCScreen::onVMDataChanged(const QVariantMap& vmData)
{
    // Check if guest_metrics ref changed (might indicate new metrics object)
    QString newGuestMetricsRef = vmData.value("guest_metrics").toString();
    if (newGuestMetricsRef != this->_guestMetricsRef)
    {
        this->_guestMetricsRef = newGuestMetricsRef;
        qDebug() << "XSVNCScreen: Guest metrics ref changed to" << newGuestMetricsRef;
    }

    // Check power state changes
    QString powerState = vmData.value("power_state").toString();

    // Only react when power state actually changes to Running
    if (powerState == "Running" && this->_lastPowerState != "Running")
    {
        qDebug() << "XSVNCScreen: VM power state changed to Running, resetting connection state";

        // Reset retry counter - critical for allowing reconnection attempts
        this->_connectionRetries = 0;

        // Stop any existing polling to start fresh
        if (this->_connectionPoller)
        {
            this->_connectionPoller->stop();
            delete this->_connectionPoller;
            this->_connectionPoller = nullptr;
        }

        // Start fresh connection attempt
        this->startPolling();

        // Also try to connect hosted console immediately (for dom0/control domains)
        if (this->_xenLib && this->_xenLib->isControlDomainZero(this->_sourceRef))
        {
            QTimer::singleShot(500, this, &XSVNCScreen::connectNewHostedConsole);
        }
    } else if (powerState != "Running" && this->_lastPowerState == "Running")
    {
        qDebug() << "XSVNCScreen: VM power state changed from Running to" << powerState << ", stopping console";
        if (this->_connectionPoller)
        {
            this->_connectionPoller->stop();
            delete this->_connectionPoller;
            this->_connectionPoller = nullptr;
        }
    }

    this->_lastPowerState = powerState;
}

/**
 * @brief Handle cache object changes from EventPoller
 * This is the primary handler for real-time VM power state changes
 */
void XSVNCScreen::onCacheObjectChanged(const QString& objectType, const QString& objectRef)
{
    // Only handle VM changes for our source
    if (objectType != "vm" || objectRef != this->_sourceRef)
        return;

    // Get the updated data from cache
    XenCache* cache = this->_xenLib->getConnection() ? this->_xenLib->getConnection()->GetCache() : nullptr;
    if (!cache)
        return;

    QVariantMap vmData = cache->ResolveObjectData("vm", objectRef);
    if (vmData.isEmpty())
        return;

    // Forward to existing handler
    this->onVMDataChanged(vmData);
}

/**
 * @brief Handle guest metrics changes (IP address updates)
 */
void XSVNCScreen::onGuestMetricsChanged(const QVariantMap& metricsData)
{
    QVariantMap networks = metricsData.value("networks").toMap();

    // Convert QVariantMap to QMap<QString, QString>
    QMap<QString, QString> networksString;
    for (auto it = networks.constBegin(); it != networks.constEnd(); ++it)
    {
        networksString[it.key()] = it.value().toString();
    }

    // Check if networks changed compared to cached value
    if (networksString != this->_cachedNetworks)
    {
        qDebug() << "XSVNCScreen: Detected IP address change, repolling for VNC/RDP...";
        this->_cachedNetworks = networksString;

        // Restart polling to pick up new IP address
        if (this->_connectionPoller)
        {
            this->startPolling();
        }
    }
}

/**
 * @brief Handle settings property changes
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 173-179
 */
void XSVNCScreen::onSettingsPropertyChanged(const QString& propertyName)
{
    if (propertyName == "EnableRDPPolling")
    {
        this->startPolling();
    }
}

// ========== Template Method Implementations ==========

/**
 * @brief Compare two QMap dictionaries for equality
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 219-232
 */
template <typename K, typename V>
bool XSVNCScreen::equateDictionary(const QMap<K, V>& d1, const QMap<K, V>& d2)
{
    if (d1.size() != d2.size())
        return false;

    for (auto it = d1.constBegin(); it != d1.constEnd(); ++it)
    {
        if (!d2.contains(it.key()) || d2[it.key()] != it.value())
            return false;
    }

    return true;
}

// Explicit template instantiations for common types
template bool XSVNCScreen::equateDictionary(const QMap<QString, QString>&, const QMap<QString, QString>&);

// ========== Pending VNC Connection Management ==========

/**
 * @brief Set pending VNC connection (thread-safe)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 455-467
 */
void XSVNCScreen::setPendingVNCConnection(QTcpSocket* stream)
{
    QMutexLocker locker(&_pendingVNCConnectionLock);

    if (this->_pendingVNCConnection)
    {
        qDebug() << "XSVNCScreen: Closing old pending VNC connection";
        this->_pendingVNCConnection->disconnectFromHost();
        this->_pendingVNCConnection->deleteLater();
    }

    this->_pendingVNCConnection = stream;
}

/**
 * @brief Get pending VNC connection (thread-safe)
 */
QTcpSocket* XSVNCScreen::getPendingVNCConnection()
{
    QMutexLocker locker(&_pendingVNCConnectionLock);
    return this->_pendingVNCConnection;
}

/**
 * @brief Check if source has RDP capability
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs line 125
 */
bool XSVNCScreen::hasRDP() const
{
    if (this->_sourceRef.isEmpty() || !this->_xenLib)
        return false;

    return this->_xenLib->hasRDP(this->_sourceRef);
}

// ========== CONTINUATION MARKER ==========
// Continuing with polling and connection management logic

// ========== Public Methods - Polling ==========

/**
 * @brief Start polling for VNC/RDP ports on guest IP addresses
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 746-775
 */
void XSVNCScreen::startPolling()
{
    qDebug() << "XSVNCScreen: startPolling()";

    // TODO: Disable toggle VNC button if in default console
    // C#: if (InDefaultConsole()) ParentVNCTabView.DisableToggleVNCButton();

    // TODO: Check if RDP control is enabled
    // C#: if (ParentVNCTabView.IsRDPControlEnabled()) return;

    if (this->_sourceRef.isEmpty())
        return;

    // If we're using hosted consoles (UseSource), skip guest polling entirely.
    // Hosted console discovery/connection is handled separately and polling the
    // guest IPs just burns sockets and UI time.
    if (this->useSource())
    {
        qDebug() << "XSVNCScreen: Hosted console in use, skipping guest port polling";
        return;
    }

    // Don't poll if this is a control domain (dom0) - they use hosted consoles only
    if (this->_xenLib && this->_xenLib->isControlDomainZero(this->_sourceRef))
    {
        qDebug() << "XSVNCScreen: Source is control domain, no polling needed";
        return;
    }

    // Stop existing poller
    if (this->_connectionPoller)
    {
        this->_connectionPoller->stop();
        delete this->_connectionPoller;
        this->_connectionPoller = nullptr;
    }

    // Start new polling timer
    // C#: Timer(PollRDPPort, null, RETRY_SLEEP_TIME, RDP_POLL_INTERVAL) for HVM
    // C#: Timer(PollVNCPort, null, RETRY_SLEEP_TIME, RDP_POLL_INTERVAL) for PV
    this->_connectionPoller = new QTimer(this);

    // Determine polling strategy based on VM virtualization mode
    if (this->_sourceIsPv)
    {
        // PV VMs only have VNC
        QObject::connect(this->_connectionPoller, &QTimer::timeout, this, &XSVNCScreen::pollVNCPort);
        this->_connectionPoller->start(RETRY_SLEEP_TIME);
    } else
    {
        // HVM VMs can have both VNC and RDP
        QObject::connect(this->_connectionPoller, &QTimer::timeout, this, &XSVNCScreen::pollRDPPort);
        this->_connectionPoller->start(RETRY_SLEEP_TIME);
    }

    qDebug() << "XSVNCScreen: Polling started with interval:" << RETRY_SLEEP_TIME << "ms";
}

/**
 * @brief Poll RDP port on guest IPs (timer callback)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 289-308
 */
void XSVNCScreen::pollRDPPort()
{
    qDebug() << "XSVNCScreen: pollRDPPort()";

#ifndef HAVE_FREERDP
    // RDP not available - fall back to VNC polling instead
    qDebug() << "XSVNCScreen: RDP not available, switching to VNC polling";
    this->pollVNCPort();
    return;
#endif

    if (this->hasRDP())
    {
        // VM has RDP capability built-in
        if (onDetectRDP)
        {
            // Invoke callback on main thread (equivalent to Program.Invoke)
            QMetaObject::invokeMethod(this, [this]() { onDetectRDP(); }, Qt::QueuedConnection);
        }
    } else
    {
        // Scan for RDP port on guest IPs
        this->_rdpIp.clear();
        QString openIp = this->pollPort(RDP_PORT, false);

        if (openIp.isEmpty())
            return;

        this->_rdpIp = openIp;
        qDebug() << "XSVNCScreen: Detected RDP on IP:" << this->_rdpIp;

        if (onDetectRDP)
        {
            QMetaObject::invokeMethod(this, [this]() { onDetectRDP(); }, Qt::QueuedConnection);
        }
    }
}

/**
 * @brief Poll VNC port on guest IPs (timer callback)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 310-324
 */
void XSVNCScreen::pollVNCPort()
{
    qDebug() << "XSVNCScreen: pollVNCPort()";

    QString openIp = this->pollPort(VNC_PORT, true);

    if (openIp.isEmpty())
    {
        // Keep the previously detected IP so reconnect attempts still have an address.
        if (this->_vncIp.isEmpty())
        {
            qDebug() << "XSVNCScreen: No VNC listener detected yet";
        } else
        {
            qDebug() << "XSVNCScreen: VNC listener not reachable, preserving" << this->_vncIp;
        }
        return;
    }

    if (this->_vncIp != openIp)
    {
        this->_vncIp = openIp;
        qDebug() << "XSVNCScreen: Detected VNC on IP:" << this->_vncIp;
    } else
    {
        qDebug() << "XSVNCScreen: VNC IP unchanged:" << this->_vncIp;
    }

    // Stop polling once VNC is found (C#: _connectionPoller?.Change(Timeout.Infinite, Timeout.Infinite))
    if (this->_connectionPoller)
    {
        this->_connectionPoller->stop();
    }

    if (onDetectVNC)
    {
        QMetaObject::invokeMethod(this, [this]() { onDetectVNC(); }, Qt::QueuedConnection);
    }
}

/**
 * @brief Handle VNC client successful connection
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 242-250
 */
void XSVNCScreen::onVncClientConnected()
{
    qDebug() << "XSVNCScreen: VNC client connected successfully";

    // Reset retry counter on successful connection
    this->_connectionRetries = 0;

    // Clear "have tried passwordless" flag for next time
    this->_haveTriedLoginWithoutPassword = false;

    // Notify parent that connection succeeded (e.g., update status text)
    // The parent can then enable/disable buttons, show/hide error messages, etc.

    // If scaling was enabled before connection, re-apply it
    if (this->_remoteConsole)
    {
        // C#: RemoteConsole.Scaling = Scaling;
        // This is handled by the individual console implementations
    }

    // Trigger resize event for parent window to adjust to console size
    emit resizeRequested();
}

/**
 * @brief Handle VNC client connection error
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 252-260
 */
void XSVNCScreen::onVncClientError(QObject* sender, const QString& error)
{
    Q_UNUSED(sender);
    qWarning() << "XSVNCScreen: VNC client error:" << error;

    // Handle connection failures by retrying
    // C#: calls RetryConnection or SleepAndRetryConnection
    this->retryConnection(this->_vncClient, error);
}

/**
 * @brief Handle desktop resize event from VNC client
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 262-270
 */
void XSVNCScreen::onDesktopResized()
{
    qDebug() << "XSVNCScreen: Desktop resized to:" << this->desktopSize();

    // Notify parent window that console size changed
    // Parent may need to adjust window size or scroll areas
    emit resizeRequested();

    // C#: Program.Invoke(this, delegate
    // {
    //     if (ResizeHandler != null)
    //         ResizeHandler(this, null);
    // });
}

// ========== Helper Functions (private static) ==========

/**
 * @brief Connect guest helper - establish TCP connection to guest IP:port
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1058-1064
 */
static QTcpSocket* connectGuest(const QString& ipAddress, int port, XenLib* xenLib)
{
    Q_UNUSED(xenLib); // Will be used when HTTPConnect is integrated

    qDebug() << "XSVNCScreen: Trying to connect to:" << ipAddress << ":" << port;

    // C#: HTTP.ConnectStream(new Uri(uriString), XenAdminConfigManager.Provider.GetProxyFromSettings(connection), true, 0);
    // Qt equivalent: Use QTcpSocket directly (HTTPConnect for HTTP CONNECT proxy will come later)

    QTcpSocket* socket = new QTcpSocket();
    socket->connectToHost(ipAddress, port);

    // Keep this short to avoid UI stalls when multiple addresses are probed
    if (!socket->waitForConnected(1000)) // 1 second timeout
    {
        qDebug() << "XSVNCScreen: Connection failed:" << socket->errorString();
        socket->deleteLater();
        throw std::runtime_error(socket->errorString().toStdString());
    }

    qDebug() << "XSVNCScreen: Connected successfully";
    return socket;
}

/**
 * @brief Scan a specific port on all guest IP addresses
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 326-448
 *
 * This method fetches guest network info from XenCache and tries to connect
 * to each IP address on the specified port. It prioritizes:
 * 1. IPv4 addresses on connected PIFs
 * 2. IPv6 addresses on connected PIFs
 * 3. IPv4 addresses without PIFs
 * 4. IPv6 addresses without PIFs
 *
 * @param port Port number to scan
 * @param vnc If true, keep connection open for VNC; if false, close immediately (RDP probe)
 * @return First responsive IP address, or empty string if none found
 */
QString XSVNCScreen::pollPort(int port, bool vnc)
{
    qDebug() << "XSVNCScreen: pollPort() - scanning port:" << port;

    try
    {
        if (this->_sourceRef.isEmpty() || !this->_xenLib)
            return QString();

        // Get VM record from cache
        XenCache* cache = this->_xenLib->getConnection()->GetCache();
        if (!cache)
            return QString();

        QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
        if (vmRecord.isEmpty())
            return QString();

        // Get guest_metrics reference
        QString guestMetricsRef = vmRecord.value("guest_metrics").toString();
        if (guestMetricsRef.isEmpty() || guestMetricsRef == "OpaqueRef:NULL")
            return QString();

        // Get guest metrics record
        QVariantMap guestMetrics = cache->ResolveObjectData("vm_guest_metrics", guestMetricsRef);
        if (guestMetrics.isEmpty())
            return QString();

        // Get networks dictionary (format: "0/ip" -> "192.168.1.100", "0/ipv4/0" -> "192.168.1.100", etc.)
        QVariantMap networks = guestMetrics.value("networks").toMap();
        if (networks.isEmpty())
            return QString();

        // Lists to organize IP addresses by priority
        QStringList ipv4Addresses;      // IPv4 on connected PIFs
        QStringList ipv6Addresses;      // IPv6 on connected PIFs
        QStringList ipv4AddressesNoPif; // IPv4 without PIFs
        QStringList ipv6AddressesNoPif; // IPv6 without PIFs

        // Get VIFs for this VM
        QVariantList vifs = vmRecord.value("VIFs").toList();
        QString residentOnRef = vmRecord.value("resident_on").toString();

        // Process each VIF to extract IPs
        foreach (const QVariant& vifRefVar, vifs)
        {
            QString vifRef = vifRefVar.toString();
            if (vifRef.isEmpty())
                continue;

            QVariantMap vif = cache->ResolveObjectData("vif", vifRef);
            if (vif.isEmpty())
                continue;

            QString vifDevice = vif.value("device").toString();
            QString networkRef = vif.value("network").toString();

            // Find PIF for this network on the host where VM is running
            QVariantMap pif;
            bool hasPif = false;
            bool pifConnected = false;

            if (!networkRef.isEmpty() && !residentOnRef.isEmpty())
            {
                QVariantMap network = cache->ResolveObjectData("network", networkRef);
                if (!network.isEmpty())
                {
                    QVariantList pifs = network.value("PIFs").toList();
                    foreach (const QVariant& pifRefVar, pifs)
                    {
                        QString pifRef = pifRefVar.toString();
                        QVariantMap pifRecord = cache->ResolveObjectData("pif", pifRef);
                        if (pifRecord.value("host").toString() == residentOnRef)
                        {
                            pif = pifRecord;
                            hasPif = true;
                            pifConnected = pifRecord.value("currently_attached", false).toBool();
                            break;
                        }
                    }
                }
            }

            // Extract IP addresses from networks dictionary for this VIF
            // Format: "0/ip", "0/ipv4/0", "0/ipv6/0", etc.
            QMapIterator<QString, QVariant> it(networks);
            while (it.hasNext())
            {
                it.next();
                QString key = it.key();
                QString value = it.value().toString();

                // Check if this network entry belongs to this VIF device
                if (!key.startsWith(vifDevice + "/"))
                    continue;

                // Determine if IPv4 or IPv6
                bool isIPv4 = key.endsWith("/ip") || key.contains("/ipv4");
                bool isIPv6 = key.contains("/ipv6");

                if (isIPv4)
                {
                    if (!hasPif)
                        ipv4AddressesNoPif.append(value);
                    else if (pifConnected)
                        ipv4Addresses.append(value);
                } else if (isIPv6)
                {
                    // IPv6 addresses need square brackets for connection
                    QString ipv6WithBrackets = QString("[%1]").arg(value);
                    if (!hasPif)
                        ipv6AddressesNoPif.append(ipv6WithBrackets);
                    else if (pifConnected)
                        ipv6Addresses.append(ipv6WithBrackets);
                }
            }
        }

        // Remove duplicates
        ipv4Addresses.removeDuplicates();
        ipv6Addresses.removeDuplicates();
        ipv4AddressesNoPif.removeDuplicates();
        ipv6AddressesNoPif.removeDuplicates();

        // Combine all addresses in priority order
        QStringList allAddresses;
        allAddresses << ipv4Addresses << ipv6Addresses << ipv4AddressesNoPif << ipv6AddressesNoPif;

        // Try connecting to each IP address
        foreach (const QString& ipAddress, allAddresses)
        {
            // Skip IPv6 link-local addresses which typically fail and waste time
            if (ipAddress.startsWith("[fe80", Qt::CaseInsensitive))
                continue;

            try
            {
                qDebug() << "XSVNCScreen: Polling" << ipAddress << ":" << port;

                QTcpSocket* socket = connectGuest(ipAddress, port, this->_xenLib);

                if (socket && socket->state() == QAbstractSocket::ConnectedState)
                {
                    qDebug() << "XSVNCScreen: Connected to" << ipAddress << ":" << port;

                    if (vnc)
                    {
                        // Keep connection open and pass to VNC client
                        qDebug() << "XSVNCScreen: Setting pending VNC connection";
                        this->setPendingVNCConnection(socket);
                    } else
                    {
                        // RDP probe - just close it
                        socket->close();
                        delete socket;
                    }

                    return ipAddress;
                } else if (socket)
                {
                    socket->close();
                    delete socket;
                }
            } catch (...)
            {
                qDebug() << "XSVNCScreen: Failed to connect to" << ipAddress << ":" << port;
            }
        }
    } catch (...)
    {
        qWarning() << "XSVNCScreen: Exception in pollPort()";
    }

    return QString();
}

// ========== Connection Management ==========

/**
 * @brief Connect to remote console (main entry point)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 605-611
 */
void XSVNCScreen::connectToRemoteConsole()
{
    qDebug() << "XSVNCScreen: connectToRemoteConsole()";

    if (this->_vncClient)
    {
        // Queue connection attempt on thread pool (C#: ThreadPool.QueueUserWorkItem)
        // Pass VNCGraphicsClient* and null exception
        QThreadPool::globalInstance()->start([this]() {
            this->connect(nullptr);
        });
    } else if (this->_rdpClient)
    {
#ifdef HAVE_FREERDP
        // Connect using RDP
        this->_rdpClient->connect(this->_rdpIp);
#else
        // RDP support not available - fall back to VNC
        qWarning() << "XSVNCScreen: RDP not available, falling back to VNC";
        this->_useVNC = true;
        this->_rdpClient = nullptr;

        // Create VNC client and retry
        if (!this->_vncClient)
        {
            this->_vncClient = new VNCGraphicsClient(this);

            // Add to layout so it fills the available space
            if (layout())
            {
                layout()->addWidget(this->_vncClient);
            }

            // Wire up VNC client signals
            QObject::connect(this->_vncClient, &VNCGraphicsClient::connectionSuccess,
                             this, &XSVNCScreen::onVncClientConnected);
            QObject::connect(this->_vncClient, &VNCGraphicsClient::errorOccurred,
                             this, &XSVNCScreen::onVncClientError);
            QObject::connect(this->_vncClient, &VNCGraphicsClient::desktopResized,
                             this, &XSVNCScreen::onDesktopResized);
        }

        QThreadPool::globalInstance()->start([this]() {
            this->connect(nullptr);
        });
#endif
    }
}

/**
 * @brief Connection succeeded handler
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 613-625
 */
void XSVNCScreen::connectionSuccess()
{
    qDebug() << "XSVNCScreen: connectionSuccess()";
    this->_connectionRetries = 0;

    // TODO: Implement auto-switch to RDP logic
    // C#: if (AutoSwitchRDPLater) { ... }
}

/**
 * @brief Main connection logic (runs on background thread)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 886-1011
 */
void XSVNCScreen::connect(void* data)
{
    qDebug() << "XSVNCScreen: connect() - background thread";

    // C#: Program.AssertOffEventThread();
    Q_ASSERT(QThread::currentThread() != QApplication::instance()->thread());

    if (!this->_vncClient)
    {
        qDebug() << "XSVNCScreen: VNC client is null, aborting";
        return;
    }

    try
    {
        // C#: if (UseSource) { ConnectHostedConsole logic }
        if (this->useSource())
        {
            // Use hosted console connection (console objects from XenAPI)
            // This tries all available rfb consoles until one works
            qDebug() << "XSVNCScreen: Using hosted console connection (UseSource=true)";

            // Invoke connectNewHostedConsole on main thread
            // It will handle trying all hosted consoles with retry logic
            QMetaObject::invokeMethod(this, &XSVNCScreen::connectNewHostedConsole, Qt::QueuedConnection);
            return;
        }

        // Else branch: direct VNC connection (when UseSource is false)
        {
            if (this->_vncIp.isEmpty())
            {
                qDebug() << "XSVNCScreen: vncIP is null. Abort VNC connection attempt";
                QMetaObject::invokeMethod(this, &XSVNCScreen::onVncConnectionAttemptCancelled, Qt::QueuedConnection);
                return;
            }

            // TODO: Get VNC password from settings
            // C#: _vncPassword = Settings.GetVNCPassword(_sourceVm.uuid);

            if (this->_vncPassword.isEmpty())
            {
                // TODO: Check if lifecycle operation is in progress
                // C#: var lifecycleOperationInProgress = _sourceVm.current_operations.Values.Any(VM.is_lifecycle_operation);

                if (this->_haveTriedLoginWithoutPassword)
                {
                    // TODO: Prompt for password
                    // C#: Program.Invoke(this, delegate { PromptForPassword(_ignoreNextError ? null : error); });
                    qDebug() << "XSVNCScreen: Would prompt for VNC password (not implemented)";

                    if (this->_vncPassword.isEmpty())
                    {
                        qDebug() << "XSVNCScreen: User cancelled VNC password prompt: aborting connection attempt";
                        // TODO: Emit userCancelledAuth signal
                        return;
                    }
                } else
                {
                    qDebug() << "XSVNCScreen: Attempting passwordless VNC login";
                    this->_vncPassword.clear(); // Empty password
                    this->_ignoreNextError = true;
                    this->_haveTriedLoginWithoutPassword = true;
                }
            }

            // Get or create TCP connection
            QTcpSocket* stream = this->getPendingVNCConnection();

            if (stream)
            {
                qDebug() << "XSVNCScreen: Using pending VNC connection";
                this->setPendingVNCConnection(nullptr); // Clear pending connection
            } else
            {
                qDebug() << "XSVNCScreen: Connecting to vncIP=" << this->_vncIp << ", port=" << VNC_PORT;
                stream = connectGuest(this->_vncIp, VNC_PORT, this->_xenLib);
                qDebug() << "XSVNCScreen: Connected to vncIP=" << this->_vncIp << ", port=" << VNC_PORT;
            }

            // Invoke connection with the stream
            this->invokeConnection(this->_vncClient, stream, QString());

            // TODO: Store empty VNC password after successful passwordless login
            // C#: if (_haveTriedLoginWithoutPassword && _vncPassword.Length == 0)
            //         Program.Invoke(this, () => Settings.SetVNCPassword(_sourceVm.uuid, _vncPassword));
        }
    } catch (const std::exception& exn)
    {
        qWarning() << "XSVNCScreen: Exception during connection:" << exn.what();
        this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), exn.what());
    }
}

/**
 * @brief Connect new hosted console (console object from XenAPI)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 827-884
 *
 * This method attempts to establish a VNC connection using the VM's console objects.
 * It searches for a console with protocol "rfb" (VNC) and tries to connect through it.
 */
void XSVNCScreen::connectNewHostedConsole()
{
    qDebug() << "XSVNCScreen: connectNewHostedConsole() sourceRef=" << this->_sourceRef;

    if (this->_hostedConsoleConnectionPending)
    {
        qDebug() << "XSVNCScreen: Hosted console connection already pending";
        return;
    }

    if (!this->_vncClient || !this->_xenLib || this->_sourceRef.isEmpty())
    {
        qWarning() << "XSVNCScreen: Cannot connect - invalid state";
        return;
    }

    // Check if we should use source (hosted console) vs detected IP
    if (!this->useSource())
    {
        qDebug() << "XSVNCScreen: Not using source, skipping hosted console";
        return;
    }

    try
    {
        // Get VM record from cache
        XenCache* cache = this->_xenLib->getConnection()->GetCache();
        if (!cache)
        {
            qWarning() << "XSVNCScreen: No cache available";
            return;
        }

        QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
        if (vmRecord.isEmpty())
        {
            qWarning() << "XSVNCScreen: Cannot resolve VM record";
            return;
        }
        QString powerState = vmRecord.value("power_state").toString();
        qDebug() << "XSVNCScreen: VM power_state=" << powerState;

        // Get consoles list (list of OpaqueRefs)
        QVariantList consoles = vmRecord.value("consoles").toList();
        if (consoles.isEmpty())
        {
            qDebug() << "XSVNCScreen: No consoles found for VM (consoles list empty in cache)";
            this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), "No consoles found");
            return;
        }

        qDebug() << "XSVNCScreen: Found" << consoles.size() << "console refs";

        // Search for RFB (VNC) console
        foreach (const QVariant& consoleRefVar, consoles)
        {
            QString consoleRef = consoleRefVar.toString();
            if (consoleRef.isEmpty())
                continue;
            qDebug() << "XSVNCScreen: Inspecting console" << consoleRef;

            // Check if VNC client has been replaced
            if (!this->_vncClient)
            {
                qDebug() << "XSVNCScreen: VNC client replaced, aborting";
                return;
            }

            // Get console record
            QVariantMap consoleRecord = cache->ResolveObjectData("console", consoleRef);
            if (consoleRecord.isEmpty())
            {
                qWarning() << "XSVNCScreen: Console record missing in cache for" << consoleRef;
                continue;
            }

            // Check protocol - we want "rfb" for VNC
            QString protocol = consoleRecord.value("protocol").toString();
            if (protocol != "rfb")
            {
                qDebug() << "XSVNCScreen: Skipping console with protocol:" << protocol;
                continue;
            }

            // Found VNC console! Try to connect
            qDebug() << "XSVNCScreen: Found RFB console:" << consoleRef;
            qDebug() << "XSVNCScreen: Console location:" << consoleRecord.value("location").toString();
            try
            {
                if (this->connectHostedConsole(this->_vncClient, consoleRef))
                    return; // Success!
            } catch (const std::exception& e)
            {
                qWarning() << "XSVNCScreen: Failed to connect to console:" << e.what();
                // Continue trying other consoles
            } catch (...)
            {
                qWarning() << "XSVNCScreen: Unknown error connecting to console";
            }
        }

        // If we got here, no consoles worked
        qDebug() << "XSVNCScreen: Did not find any working hosted consoles";
        this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), "No working consoles");
    } catch (const std::exception& e)
    {
        qWarning() << "XSVNCScreen: Exception in connectNewHostedConsole:" << e.what();
        this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), e.what());
    } catch (...)
    {
        qWarning() << "XSVNCScreen: Unknown exception in connectNewHostedConsole";
        this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), "Unknown exception");
    }
}

/**
 * @brief Connect hosted console with VNC client
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1066-1091
 *
 * This method establishes an HTTP CONNECT tunnel through XenServer to the VM's console.
 * It uses the console's location URL and current session for authentication.
 */
bool XSVNCScreen::connectHostedConsole(VNCGraphicsClient* vncClient, const QString& consoleRef)
{
    qDebug() << "XSVNCScreen: connectHostedConsole() - console:" << consoleRef;

    if (!vncClient || !this->_xenLib || consoleRef.isEmpty())
    {
        qWarning() << "XSVNCScreen: Invalid parameters for connectHostedConsole";
        return false;
    }

    try
    {
        // Get console record from cache
        XenCache* cache = this->_xenLib->getConnection()->GetCache();
        if (!cache)
        {
            throw std::runtime_error("No cache available");
        }

        QVariantMap consoleRecord = cache->ResolveObjectData("console", consoleRef);
        if (consoleRecord.isEmpty())
        {
            throw std::runtime_error("Cannot resolve console record");
        }

        // Get console location URL
        QString location = consoleRecord.value("location").toString();
        if (location.isEmpty())
        {
            throw std::runtime_error("Console location is empty");
        }

        QUrl consoleUrl(location);
        if (!consoleUrl.isValid())
        {
            throw std::runtime_error(QString("Invalid console URL: %1").arg(location).toStdString());
        }

        qDebug() << "XSVNCScreen: Console location:" << location;

        // Get VM record to check resident_on (host where VM is running)
        QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
        QString residentOnRef = vmRecord.value("resident_on").toString();

        if (residentOnRef.isEmpty() || residentOnRef == "OpaqueRef:NULL")
        {
            throw std::runtime_error("VM is not running on any host");
        }

        // Verify host exists
        QVariantMap hostRecord = cache->ResolveObjectData("host", residentOnRef);
        if (hostRecord.isEmpty())
        {
            throw std::runtime_error("Cannot resolve host where VM is running");
        }
        qDebug() << "XSVNCScreen: Resident host:" << residentOnRef
                 << "name:" << hostRecord.value("name_label").toString();

        // Get current session ID
        // C#: Uses elevated credentials if available (CA-91132), otherwise duplicates session
        QString sessionId = this->_xenLib->getConnection()->GetSession()->getSessionId();
        if (sessionId.isEmpty())
        {
            throw std::runtime_error("No active session");
        }

        qDebug() << "XSVNCScreen: Establishing HTTP CONNECT tunnel";
        qDebug() << "XSVNCScreen: Session ID prefix:" << sessionId.left(12) + "...";

        this->_hostedConsoleConnectionPending = true;

        // Create HTTPConnect instance for async connection
        HTTPConnect* httpConnect = new HTTPConnect(this);

        // Connect success signal
        QObject::connect(httpConnect, &HTTPConnect::connectedToConsole, this,
                         [this, vncClient, consoleRef, httpConnect](QSslSocket* socket) {
                             qDebug() << "XSVNCScreen: HTTP CONNECT tunnel established";
                             this->_hostedConsoleConnectionPending = false;

                             // Clean up HTTPConnect object
                             httpConnect->deleteLater();

                             // Pass socket to invokeConnection
                             this->invokeConnection(vncClient, socket, consoleRef);
                         });

        // Connect error signal
        QObject::connect(httpConnect, &HTTPConnect::error, this,
                         [this, vncClient, httpConnect](const QString& error) {
                             qWarning() << "XSVNCScreen: HTTP CONNECT failed:" << error;
                             this->_hostedConsoleConnectionPending = false;

                             // Clean up HTTPConnect object
                             httpConnect->deleteLater();

                             // Retry connection
                             this->retryConnection(vncClient, error);
                         });

        // Start async connection
        httpConnect->connectToConsoleAsync(consoleUrl, sessionId);
        return true;
    } catch (const std::exception& e)
    {
        qWarning() << "XSVNCScreen: Exception in connectHostedConsole:" << e.what();
        this->_hostedConsoleConnectionPending = false;
        this->retryConnection(vncClient, QString::fromStdString(e.what()));
    } catch (...)
    {
        qWarning() << "XSVNCScreen: Unknown exception in connectHostedConsole";
        this->_hostedConsoleConnectionPending = false;
        this->retryConnection(vncClient, "Unknown error");
    }
    return false;
}

/**
 * @brief Invoke connection with open stream
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1093-1115
 */
void XSVNCScreen::invokeConnection(VNCGraphicsClient* vncClient, QTcpSocket* stream, const QString& consoleRef)
{
    qDebug() << "XSVNCScreen: invokeConnection()";

    if (!vncClient || !stream)
    {
        qWarning() << "XSVNCScreen: Invalid VNC client or stream";
        return;
    }

    // Invoke on main thread (equivalent to C# Program.Invoke)
    QMetaObject::invokeMethod(this, [this, vncClient, stream, consoleRef]() {
        if (this->_vncClient != vncClient)
        {
            // We've been replaced
            qDebug() << "XSVNCScreen: VNC client was replaced, aborting connection";
            stream->disconnectFromHost();
            stream->deleteLater();
            return;
        }

        try
        {
            // C# ALWAYS calls v.DisconnectAndDispose() before connecting to ensure clean state
            // This clears any stale backbuffer, sockets, or flags from previous session
            qDebug() << "XSVNCScreen: Disposing old VNC client state before reconnect";
            vncClient->disconnectAndDispose();
            
            // Small delay to ensure dispose completes and widget repaints black screen
            QThread::msleep(10);
            
            // Connect the VNC client with the stream
            // C#: v.Connect(stream, console?.location);
            QString password = QString::fromUtf8(this->_vncPassword);
            vncClient->connect(stream, password);

            qDebug() << "XSVNCScreen: VNC client connected successfully";
        }
        catch (const std::exception& exn)
        {
            qWarning() << "XSVNCScreen: Error during VNC connection:" << exn.what();
            this->retryConnection(vncClient, QString::fromStdString(exn.what()));
        } }, Qt::QueuedConnection);
}

/**
 * @brief Check if the source VM/host is running and should allow retry
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs RetryConnection check: Source.power_state == vm_power_state.Running
 */
bool XSVNCScreen::shouldRetryConnection() const
{
    if (this->_sourceRef.isEmpty() || !this->_xenLib)
        return false;

    XenCache* cache = this->_xenLib->getConnection() ? this->_xenLib->getConnection()->GetCache() : nullptr;
    if (!cache)
        return false;

    // Check if source is a VM (hosts don't have power_state in the same way)
    QVariantMap vmRecord = cache->ResolveObjectData("vm", this->_sourceRef);
    if (!vmRecord.isEmpty())
    {
        QString powerState = vmRecord.value("power_state").toString();
        return powerState == "Running";
    }

    // For hosts, check if the host is enabled/connected
    QVariantMap hostRecord = cache->ResolveObjectData("host", this->_sourceRef);
    if (!hostRecord.isEmpty())
    {
        bool enabled = hostRecord.value("enabled", false).toBool();
        return enabled;
    }

    return false;
}

/**
 * @brief Retry connection after failure with exponential backoff
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1117-1164
 */
void XSVNCScreen::retryConnection(VNCGraphicsClient* vncClient, const QString& errorMessage)
{
    qDebug() << "XSVNCScreen: retryConnection() - error:" << errorMessage;

    // C#: Program.AssertOnEventThread();
    Q_ASSERT(QThread::currentThread() == QApplication::instance()->thread());

    if (this->_vncClient != vncClient)
    {
        qDebug() << "XSVNCScreen: VNC client was replaced, not retrying";
        return;
    }

    // C# check: Source.power_state == vm_power_state.Running
    if (!this->shouldRetryConnection())
    {
        qDebug() << "XSVNCScreen: Source not running/enabled, stopping retry";
        return;
    }

    // Increment retry counter
    this->_connectionRetries++;

    // Add a maximum retry limit to prevent infinite loops
    static const int MAX_RETRY_COUNT = 60; // ~5 minutes at 5s intervals after initial 10
    if (this->_connectionRetries > MAX_RETRY_COUNT)
    {
        qDebug() << "XSVNCScreen: Maximum retry count reached (" << MAX_RETRY_COUNT << "), giving up";
        return;
    }

    if (this->_connectionRetries < SHORT_RETRY_COUNT)
    {
        qDebug() << "XSVNCScreen: Short retry #" << this->_connectionRetries << " of" << SHORT_RETRY_COUNT;

        // Short retry: sleep briefly and reconnect
        QTimer::singleShot(SHORT_RETRY_SLEEP_TIME, this, [this, vncClient]() {
            this->sleepAndRetryConnection(vncClient);
        });
    } else
    {
        qDebug() << "XSVNCScreen: Long retry #" << this->_connectionRetries << " (interval:" << RETRY_SLEEP_TIME << "ms)";

        // Long retry: sleep longer
        QTimer::singleShot(RETRY_SLEEP_TIME, this, [this, vncClient]() {
            this->sleepAndRetryConnection(vncClient);
        });
    }
}

/**
 * @brief Sleep and retry connection (background thread)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1166-1186
 */
void XSVNCScreen::sleepAndRetryConnection(IRemoteConsole* console)
{
    qDebug() << "XSVNCScreen: sleepAndRetryConnection()";

    if (!console)
        return;

    // Queue retry on thread pool (C#: ThreadPool.QueueUserWorkItem)
    QThreadPool::globalInstance()->start([this, console]() {
        // Sleep determined by caller (already done via QTimer)

        // Retry connection
        if (this->_vncClient == console)
        {
            qDebug() << "XSVNCScreen: Retrying VNC connection...";
            this->connect(nullptr);
        } else
        {
            qDebug() << "XSVNCScreen: Console was replaced, not retrying";
        }
    });
}

/**
 * @brief Auto-switch to RDP after delay
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs (AutoSwitchRDPLater logic)
 */
void XSVNCScreen::autoSwitchRDPLater()
{
    qDebug() << "XSVNCScreen: autoSwitchRDPLater()";

#ifndef HAVE_FREERDP
    // RDP not available - stay with VNC
    qDebug() << "XSVNCScreen: RDP not available, staying with VNC";
    return;
#else
    // TODO: Implement auto-switch logic when RdpClient is fully integrated
    // C#: Sets up a timer to switch to RDP automatically after initial VNC connection
    qDebug() << "XSVNCScreen: RDP auto-switch not yet fully implemented";
#endif
}

/**
 * @brief Trigger VNC connection attempt cancelled event
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1049-1056
 */
void XSVNCScreen::onVncConnectionAttemptCancelled()
{
    qDebug() << "XSVNCScreen: Cancelled VNC connection attempt";
    emit vncConnectionAttemptCancelled();
}
