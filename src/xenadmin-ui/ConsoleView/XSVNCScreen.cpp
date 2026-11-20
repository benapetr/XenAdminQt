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
#include "ConsoleView/RdpClient.h"
#include "VNCGraphicsClient.h"
#include "VNCTabView.h"
#include "IRemoteConsole.h"
#include <QDebug>
#include <QThread>
#include <QThreadPool>
#include <QTcpSocket>
#include <QImage>
#include <QPalette>
#include <QApplication>
#include <QVBoxLayout>
#include "xenlib/xen/console.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/vif.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network.h"
#include "network/httpconnect.h"
#include "settingsmanager.h"
#include "dialogs/vncpassworddialog.h"
#include <QPointer>

// Defined with the connection code below
static QTcpSocket* connectGuest(const QString& ipAddress, int port, XenConnection* connection);

class VNCTabView;
class RdpClient;

/**
 * @brief Constructor - matches C# XSVNCScreen(VM source, EventHandler resizeHandler,
 *                      VNCTabView parent, string elevatedUsername, string elevatedPassword)
 *
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 147-177
 */
XSVNCScreen::XSVNCScreen(const QString& sourceRef, VNCTabView* parent, XenConnection *connection, const QString& elevatedUsername, const QString& elevatedPassword)
    : QWidget(nullptr), _sourceRef(sourceRef), _connection(connection), _parentVNCTabView(parent)
      , _elevatedUsername(elevatedUsername), _elevatedPassword(elevatedPassword), _focusColor(QApplication::palette().color(QPalette::Highlight))
{
    //qDebug() << "XSVNCScreen: Constructor for source:" << this->_sourceRef;

    // Force handle creation (equivalent to C#: var _ = Handle;)
    // This ensures the widget has a native window handle for rendering
    this->winId();

    // Create layout so VNCGraphicsClient fills the available space
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Determine if source is PV or HVM
    // Reference: C# XSVNCScreen.cs line 703: _sourceIsPv = !value.IsHVM();
    if (!this->_sourceRef.isEmpty() && this->_connection)
    {
        XenCache* cache = this->_connection->GetCache();
        QSharedPointer<VM> vm = cache ? cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef) : QSharedPointer<VM>();
        if (vm && vm->IsValid())
        {
            this->_sourceIsPv = !vm->IsHVM();
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
        if (this->_connection)
        {
            XenCache* cache = this->_connection->GetCache();
            QSharedPointer<VM> vm = cache ? cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef) : QSharedPointer<VM>();
            QString guestMetricsRef = vm ? vm->GetGuestMetricsRef() : QString();

            if (!guestMetricsRef.isEmpty() && guestMetricsRef != XENOBJECT_NULL)
            {
                QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
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

        // C# Source setter: start polling the guest right away. Queued, so the parent's
        // detection callbacks are set first.
        QTimer::singleShot(0, this, &XSVNCScreen::StartPolling);
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
        this->_remoteConsole->DisconnectAndDispose();
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
QSize XSVNCScreen::GetDesktopSize() const
{
    return this->_remoteConsole ? this->_remoteConsole->DesktopSize() : QSize();
}

/**
 * @brief Check if RDP version warning is needed
 * Equivalent to C#: public bool RdpVersionWarningNeeded => _rdpClient != null && _rdpClient.needsRdpVersionWarning;
 */
bool XSVNCScreen::IsRDPVersionWarningNeeded() const
{
    // Note: RdpClient uses FreeRDP which doesn't have Windows RDP version issues
    // This was for Windows native RDP ActiveX control version compatibility
    return false;
}

/**
 * @brief Set VNC password (stored securely)
 * Equivalent to C#: private char[] _vncPassword
 */
void XSVNCScreen::SetVNCPassword(const QString& password)
{
    this->_vncPassword = password.toUtf8();
}

/**
 * @brief Get VNC password
 */
QString XSVNCScreen::GetVNCPassword() const
{
    return QString::fromUtf8(this->_vncPassword);
}

/**
 * @brief Set whether to use VNC (true) or RDP (false)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 652-659
 */
void XSVNCScreen::SetUseVNC(bool value)
{
    // C# UseVNC setter
    if (value == this->_useVNC)
        return;

    this->_connectionRetries = 0;
    this->_useVNC = value;
    qDebug() << "XSVNCScreen: UseVNC changed to:" << this->_useVNC;

    // C# InitSubControl doesn't switch to RDP without an RDP address, and this port
    // has no RDP client to switch to: then the switch didn't happen and is undone (CA-102755)
    if (!this->_useVNC && (this->_rdpIp.isEmpty() || !this->_rdpClient))
    {
        this->_useVNC = true;
        return;
    }

    this->resetSubControl();
}

/**
 * @brief Set whether to use source/hosted console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 666-675
 */
void XSVNCScreen::SetUseSource(bool value)
{
    // C# UseSource setter: the hosted (text) console or the guest's own VNC server
    if (value == this->_useSource)
        return;

    this->_useSource = value;
    this->_connectionRetries = 0;
    qDebug() << "XSVNCScreen: UseSource changed to:" << this->_useSource;
    this->resetSubControl();
}

void XSVNCScreen::resetSubControl()
{
    // C# InitSubControl: drop the current client, make a new one for the new mode and
    // connect it. Workers still holding the old client see it was replaced and stop.
    if (this->_vncClient)
    {
        VNCGraphicsClient* old = this->_vncClient;
        this->_vncClient = nullptr;
        if (this->_remoteConsole == old)
            this->_remoteConsole = nullptr;
        QObject::disconnect(old, nullptr, this, nullptr);
        old->DisconnectAndDispose();
        if (layout())
            layout()->removeWidget(old);
        old->hide();
        old->deleteLater();
    }

    this->_vncPassword.clear();
    this->_haveTriedLoginWithoutPassword = false;
    this->m_lastAuthFailed = false;

    this->initSubControl();
    if (this->_vncClient)
    {
        this->_vncClient->SetUseSource(this->_useSource);
        if (this->_wasPaused)
            this->_vncClient->Pause();
        else
            this->_vncClient->Unpause();
    }

    this->connectToRemoteConsole();
}

/**
 * @brief Pause the remote console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 234-241
 */
void XSVNCScreen::Pause()
{
    if (this->_remoteConsole)
    {
        this->_wasPaused = true;
        this->_remoteConsole->Pause();
    }
}

/**
 * @brief Unpause the remote console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 243-251
 */
void XSVNCScreen::Unpause()
{
    if (this->_remoteConsole)
    {
        this->_wasPaused = false;
        this->_remoteConsole->Unpause();
    }
}

/**
 * @brief Send Ctrl+Alt+Del to remote console
 */
void XSVNCScreen::SendCAD()
{
    if (this->_remoteConsole)
    {
        this->_remoteConsole->SendCAD();
    }
}

void XSVNCScreen::SendFunctionKeyWithModifiers(bool ctrl, bool alt, int functionNumber)
{
    if (this->_remoteConsole)
    {
        this->_remoteConsole->SendFunctionKeyWithModifiers(ctrl, alt, functionNumber);
    }
}

/**
 * @brief Capture screenshot from console
 */
QImage XSVNCScreen::GetSnapshot()
{
    if (this->_remoteConsole)
    {
        return this->_remoteConsole->Snapshot();
    }
    return QImage();
}

/**
 * @brief Set console scaling mode
 */
void XSVNCScreen::SetScaling(bool enabled)
{
    qDebug() << "XSVNCScreen: setScaling:" << enabled;

    if (this->_remoteConsole)
        this->_remoteConsole->SetScaling(enabled);
}

/**
 * @brief Get console IsScaling mode
 */
bool XSVNCScreen::IsScaling() const
{
    if (this->_remoteConsole)
        return this->_remoteConsole->IsScaling();

    return false;
}

/**
 * @brief Check if must connect via remote desktop (GPU passthrough)
 */
bool XSVNCScreen::MustConnectRemoteDesktop() const
{
    qDebug() << "XSVNCScreen: mustConnectRemoteDesktop()";

#ifndef HAVE_FREERDP
    // RDP support not available - always use VNC
    return false;
#else
    // Check if VM has GPU passthrough (requires remote desktop connection)
    if (hasGPUPassthrough(this->_sourceRef))
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
void XSVNCScreen::CaptureKeyboardAndMouse()
{
    qDebug() << "XSVNCScreen: captureKeyboardAndMouse()";

    // Activate the remote console control
    if (this->_remoteConsole)
    {
        this->_remoteConsole->Activate();

        // Enable keyboard/mouse capture if auto-capture is enabled
        if (this->_autoCaptureKeyboardAndMouse)
        {
            this->setKeyboardAndMouseCapture(true);
        }

        this->Unpause();
    }

    // C#: DisableMenuShortcuts() - disable main window menu shortcuts
    // This allows all keyboard input to go to the console
    // Note: In Qt, we handle this through focus and event filtering
}

/**
 * @brief Release keyboard and mouse capture from console
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1276-1286
 */
void XSVNCScreen::UncaptureKeyboardAndMouse()
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
        QWidget* consoleWidget = this->_remoteConsole->ConsoleControl();
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
        QWidget* consoleWidget = this->_remoteConsole->ConsoleControl();
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
void XSVNCScreen::DisconnectAndDispose()
{
    {
        QMutexLocker locker(&this->_activeSessionLock);
        this->_activeSessionRef.clear();
    }
    this->m_hostedConsoleConnectionPending = false;
    this->setPendingVNCConnection(nullptr);

    if (this->_remoteConsole)
    {
        this->_remoteConsole->DisconnectAndDispose();
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
        QObject::connect(this->_vncClient, &VNCGraphicsClient::authenticationFailed,
                         this, &XSVNCScreen::onVncClientAuthenticationFailed);

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
        this->_remoteConsole->SetKeyHandler(this->_keyHandler);

        // C# XSVNCScreen: PV VMs use keysyms, HVM VMs use scan codes. Scan codes are only
        // sent once the server confirms QEMU extended key events (VNCGraphicsClient); before,
        // enabling them sent the old XenCenter scan event, which QEMU answered by dropping
        // the connection.
        this->_remoteConsole->SetSendScanCodes(!this->_sourceIsPv);
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
    if (this->_sourceRef.isEmpty() || !this->_connection)
        return;

    // Connect to cache's objectChanged signal for real-time updates (e.g., power_state changes)
    // This is the key signal for detecting when a VM powers on/off
    XenCache* cache = this->_connection->GetCache();
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

    XenCache* cache = this->_connection ? this->_connection->GetCache() : nullptr;
    if (cache)
    {
        disconnect(cache, &XenCache::objectChanged, this, &XSVNCScreen::onCacheObjectChanged);
    }

    qDebug() << "XSVNCScreen: Event listeners unregistered for" << this->_sourceRef;
}

// ========== Private Slots - Event Handlers ==========

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
        this->StartPolling();

        // Also try to connect hosted console immediately (for dom0/control domains)
        if (isControlDomainZero(this->_sourceRef))
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
void XSVNCScreen::onCacheObjectChanged(QSharedPointer<XenObject> object)
{
    if (!object)
        return;

    XenConnection* connection = object->GetConnection();
    Q_ASSERT(this->_connection == connection);

    const XenObjectType type = object->GetObjectType();
    const QString objectRef = object->OpaqueRef();

    if (type == XenObjectType::VM && objectRef == this->_sourceRef)
    {
        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object);
        if (!vm || !vm->IsValid())
            return;

        this->onVMDataChanged(vm->GetData());
        return;
    }

    if (type == XenObjectType::VMGuestMetrics && objectRef == this->_guestMetricsRef)
    {
        QVariantMap metricsData = object->GetData();
        if (!metricsData.isEmpty())
        {
            this->onGuestMetricsChanged(metricsData);
        }
    }
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
            this->StartPolling();
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
        this->StartPolling();
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

    if (this->_pendingVNCConnection == stream)
        return;

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

QTcpSocket* XSVNCScreen::takePendingVNCConnection()
{
    QMutexLocker locker(&_pendingVNCConnectionLock);
    QTcpSocket* stream = this->_pendingVNCConnection;
    this->_pendingVNCConnection = nullptr;
    return stream;
}

QString XSVNCScreen::currentConnectionSessionId()
{
    const QString liveSessionId = this->_connection && this->_connection->GetSession()
                                      ? this->_connection->GetSession()->GetSessionID()
                                      : QString();

    QMutexLocker locker(&this->_activeSessionLock);
    if (!liveSessionId.isEmpty())
        this->_activeSessionRef = liveSessionId;
    return this->_activeSessionRef;
}

XenCache* XSVNCScreen::cache() const
{
    return this->_connection ? this->_connection->GetCache() : nullptr;
}

QSharedPointer<VM> XSVNCScreen::resolveVM(const QString& vmRef) const
{
    XenCache* cache = this->cache();
    if (!cache || vmRef.isEmpty())
        return QSharedPointer<VM>();

    return cache->ResolveObject<VM>(XenObjectType::VM, vmRef);
}

QSharedPointer<Host> XSVNCScreen::resolveHost(const QString& hostRef) const
{
    XenCache* cache = this->cache();
    if (!cache || hostRef.isEmpty())
        return QSharedPointer<Host>();

    return cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
}

bool XSVNCScreen::isControlDomainZero(const QString& vmRef, QString* outHostRef) const
{
    QSharedPointer<VM> vm = resolveVM(vmRef);
    if (!vm || !vm->IsValid())
        return false;

    if (!vm->IsControlDomain())
        return false;

    QString hostRef = vm->GetResidentOnRef();
    if (hostRef.isEmpty() || hostRef == XENOBJECT_NULL)
        return false;

    if (outHostRef)
        *outHostRef = hostRef;

    QSharedPointer<Host> host = resolveHost(hostRef);
    if (!host || !host->IsValid())
        return false;

    QString controlDomain = host->ControlDomainRef();
    if (!controlDomain.isEmpty() && controlDomain != XENOBJECT_NULL)
        return controlDomain == vmRef;

    return vm->Domid() == 0;
}

bool XSVNCScreen::hasGPUPassthrough(const QString& vmRef) const
{
    QSharedPointer<VM> vm = resolveVM(vmRef);
    if (!vm || !vm->IsValid())
        return false;

    XenCache* cache = this->cache();
    if (!cache)
        return false;

    QStringList vgpuRefs = vm->VGPURefs();
    for (const QString& vgpuRef : vgpuRefs)
    {
        if (vgpuRef.isEmpty() || vgpuRef == XENOBJECT_NULL)
            continue;

        QVariantMap vgpuData = cache->ResolveObjectData(XenObjectType::VGPU, vgpuRef);
        if (vgpuData.isEmpty())
            continue;

        QString vgpuTypeRef = vgpuData.value("type").toString();
        if (vgpuTypeRef.isEmpty() || vgpuTypeRef == XENOBJECT_NULL)
            continue;

        QVariantMap vgpuTypeData = cache->ResolveObjectData("vgpu_type", vgpuTypeRef);
        if (vgpuTypeData.isEmpty())
            continue;

        if (vgpuTypeData.value("implementation").toString() == "passthrough")
            return true;
    }

    return false;
}

/**
 * @brief Check if source has RDP capability
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs line 125
 */
bool XSVNCScreen::hasRDP() const
{
    if (this->_sourceRef.isEmpty() || !this->_connection)
        return false;

    QSharedPointer<VM> vm = resolveVM(this->_sourceRef);
    if (!vm)
        return false;

    QString guestMetricsRef = vm->GetGuestMetricsRef();
    if (guestMetricsRef.isEmpty() || guestMetricsRef == XENOBJECT_NULL)
        return false;

    return false;
}

// ========== CONTINUATION MARKER ==========
// Continuing with polling and connection management logic

// ========== Public Methods - Polling ==========

/**
 * @brief Start polling for VNC/RDP ports on guest IP addresses
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 746-775
 */
void XSVNCScreen::StartPolling()
{
    qDebug() << "XSVNCScreen: startPolling()";

    // C#: disable the toggle button first, but only in the default (text) console, so
    // there is always a way back to it (CA-70314); detecting VNC/RDP enables it again
    if (this->IsInDefaultConsole() && this->_parentVNCTabView)
        this->_parentVNCTabView->DisableToggleVNCButton();

    if (this->_parentVNCTabView && this->_parentVNCTabView->IsRDPControlEnabled())
        return;

    if (this->_sourceRef.isEmpty())
        return;

    // The guest is polled in the text console too: that is how C# finds the guest's own
    // VNC server and offers "Switch to Graphical Console". The probes run off the GUI thread.

#ifndef HAVE_FREERDP
    // HVM guests are only polled for RDP (C# PollRDPPort), which this build can't use
    if (!this->_sourceIsPv)
        return;
#endif

    // Don't poll if this is a control domain (dom0) - they use hosted consoles only
    if (isControlDomainZero(this->_sourceRef))
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

    // Determine polling strategy based on VM virtualization mode: PV guests may run a VNC
    // server, HVM guests are polled for RDP
    if (this->_sourceIsPv)
        QObject::connect(this->_connectionPoller, &QTimer::timeout, this, &XSVNCScreen::pollVNCPort);
    else
        QObject::connect(this->_connectionPoller, &QTimer::timeout, this, &XSVNCScreen::pollRDPPort);

    // C#: first after RETRY_SLEEP_TIME, then every RDP_POLL_INTERVAL
    QTimer* poller = this->_connectionPoller;
    QObject::connect(poller, &QTimer::timeout, poller, [poller]() { poller->setInterval(RDP_POLL_INTERVAL); });
    this->_connectionPoller->start(RETRY_SLEEP_TIME);

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
    // No RDP support in this build (StartPolling doesn't poll HVM guests then)
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
        // Scan for RDP port on guest IPs (C# clears RdpIp first)
        this->_rdpIp.clear();
        this->pollGuestPort(RDP_PORT, false);
    }
}

/**
 * @brief Poll VNC port on guest IPs (timer callback)
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 310-324
 */
void XSVNCScreen::pollVNCPort()
{
    qDebug() << "XSVNCScreen: pollVNCPort()";
    this->pollGuestPort(VNC_PORT, true);
}

void XSVNCScreen::pollGuestPort(int port, bool vnc)
{
    if (this->m_guestPollRunning)
        return; // the previous probe is still going

    const QStringList addresses = this->guestAddresses();
    if (addresses.isEmpty())
    {
        this->onGuestPortPolled(port, vnc, QString(), nullptr);
        return;
    }

    this->m_guestPollRunning = true;
    QPointer<XSVNCScreen> self(this);
    QThreadPool::globalInstance()->start([self, addresses, port, vnc]()
    {
        QString found;
        QTcpSocket* socket = nullptr;
        for (const QString& address : addresses)
        {
            try
            {
                qDebug() << "XSVNCScreen: Polling" << address << ":" << port;
                QTcpSocket* connected = connectGuest(address, port, nullptr); // now on the GUI thread
                found = address;
                if (vnc)
                    socket = connected; // kept for the VNC client (C# _pendingVNCConnection)
                else
                    connected->deleteLater(); // RDP: only a probe
                break;
            } catch (const std::exception&)
            {
                // try the next address
            }
        }

        QMetaObject::invokeMethod(qApp, [self, port, vnc, found, socket]()
        {
            if (!self)
            {
                if (socket)
                    socket->deleteLater();
                return;
            }
            self->onGuestPortPolled(port, vnc, found, socket);
        }, Qt::QueuedConnection);
    });
}

void XSVNCScreen::onGuestPortPolled(int port, bool vnc, const QString& address, QTcpSocket* socket)
{
    Q_UNUSED(port);
    this->m_guestPollRunning = false;

    if (!vnc)
    {
        // C# PollRDPPort
        this->_rdpIp = address;
        if (!address.isEmpty())
        {
            qDebug() << "XSVNCScreen: Detected RDP on IP:" << address;
            if (onDetectRDP)
                onDetectRDP();
        }
        return;
    }

    // C# PollVNCPort. Keep the previous address if nothing answered this time, so a
    // reconnect still has one.
    if (address.isEmpty())
    {
        qDebug() << "XSVNCScreen: No VNC listener detected" << (this->_vncIp.isEmpty() ? "yet" : "now, keeping " + this->_vncIp);
        return;
    }

    this->_vncIp = address;
    this->setPendingVNCConnection(socket);
    qDebug() << "XSVNCScreen: Detected VNC on IP:" << this->_vncIp;

    // Stop polling once VNC is found (C#: _connectionPoller?.Change(Timeout.Infinite, Timeout.Infinite))
    if (this->_connectionPoller)
        this->_connectionPoller->stop();

    if (onDetectVNC)
        onDetectVNC();
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

void XSVNCScreen::onVncClientAuthenticationFailed(QObject* sender, const QString& error)
{
    Q_UNUSED(sender);
    qWarning() << "XSVNCScreen: VNC password rejected:" << error;

    // C# ErrorHandler, VNCAuthenticationException: forget the stored password and retry,
    // which asks for it (saying it was wrong, unless it was the passwordless attempt)
    SettingsManager::instance().SetVNCPassword(this->sourceVmUuid(), nullptr);
    this->_vncPassword.clear();
    this->m_lastAuthFailed = true;
    this->retryConnection(this->_vncClient, error);
}

bool XSVNCScreen::promptForPassword(const QString& vmUuid, bool passwordIncorrect, QString* password)
{
    // C# PromptForPassword
    VNCPasswordDialog dialog(passwordIncorrect, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    *password = dialog.GetPassword();
    SettingsManager::instance().SetVNCPassword(vmUuid, password); // stored for next time
    return true;
}

QString XSVNCScreen::sourceVmUuid() const
{
    QSharedPointer<VM> vm = this->resolveVM(this->_sourceRef);
    return vm ? vm->GetUUID() : QString();
}

/**
 * @brief Handle desktop resize event from VNC client
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 262-270
 */
void XSVNCScreen::onDesktopResized()
{
    qDebug() << "XSVNCScreen: Desktop resized to:" << this->GetDesktopSize();

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
static QTcpSocket* connectGuest(const QString& ipAddress, int port, XenConnection* connection)
{
    Q_UNUSED(connection); // Will be used when HTTPConnect is integrated

    qDebug() << "XSVNCScreen: Trying to connect to:" << ipAddress << ":" << port;

    // C#: HTTP.ConnectStream(new Uri(uriString), XenAdminConfigManager.Provider.GetProxyFromSettings(connection), true, 0);
    // Qt equivalent: Use QTcpSocket directly (HTTPConnect for HTTP CONNECT proxy will come later)

    QTcpSocket* socket = new QTcpSocket();
    socket->connectToHost(ipAddress, port);

    // Keep this short to avoid UI stalls when multiple addresses are probed
    if (!socket->waitForConnected(1000)) // 1 second timeout
    {
        const QString error = socket->errorString();
        qDebug() << "XSVNCScreen: Connection failed:" << error;
        delete socket; // worker threads have no event loop for deleteLater
        throw std::runtime_error(error.toStdString());
    }

    // Made on a worker thread, used by the VNC client on the GUI thread
    if (QThread::currentThread() != qApp->thread())
        socket->moveToThread(qApp->thread());

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
QStringList XSVNCScreen::guestAddresses() const
{
    try
    {
        if (this->_sourceRef.isEmpty() || !this->_connection)
            return QStringList();

        // Get VM record from cache
        XenCache* cache = this->_connection->GetCache();
        if (!cache)
            return QStringList();

        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef);
        if (!vm || !vm->IsValid())
            return QStringList();

        // Get guest_metrics reference
        QString guestMetricsRef = vm->GetGuestMetricsRef();
        if (guestMetricsRef.isEmpty() || guestMetricsRef == XENOBJECT_NULL)
            return QStringList();

        // Get guest metrics record
        QVariantMap guestMetrics = cache->ResolveObjectData(XenObjectType::VMGuestMetrics, guestMetricsRef);
        if (guestMetrics.isEmpty())
            return QStringList();

        // Get networks dictionary (format: "0/ip" -> "192.168.1.100", "0/ipv4/0" -> "192.168.1.100", etc.)
        QVariantMap networks = guestMetrics.value("networks").toMap();
        if (networks.isEmpty())
            return QStringList();

        // Lists to organize IP addresses by priority
        QStringList ipv4Addresses;      // IPv4 on connected PIFs
        QStringList ipv6Addresses;      // IPv6 on connected PIFs
        QStringList ipv4AddressesNoPif; // IPv4 without PIFs
        QStringList ipv6AddressesNoPif; // IPv6 without PIFs

        // Get VIFs for this VM
        QList<QSharedPointer<VIF>> vifs = vm->GetVIFs();
        //QStringList vifs = vm->GetVIFRefs();
        QSharedPointer<Host> resident_on = vm->GetResidentOnHost();

        // Process each VIF to extract IPs
        foreach (QSharedPointer<VIF> vif, vifs)
        {
            QString vifDevice = vif->GetDevice();
            QSharedPointer<Network> network = vif->GetNetwork();

            // Find PIF for this network on the host where VM is running
            bool hasPif = false;
            bool pifConnected = false;

            if (!network.isNull() && !resident_on.isNull())
            {
                QList<QSharedPointer<PIF>> pifs = network->GetPIFs();
                foreach (QSharedPointer<PIF> pif, pifs)
                {
                    if (pif->GetHost() == resident_on)
                    {
                        hasPif = true;
                        pifConnected = pif->IsCurrentlyAttached();
                        break;
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

        // IPv6 link-local addresses typically fail and waste time
        QStringList result;
        for (const QString& ipAddress : allAddresses)
        {
            if (!ipAddress.startsWith("[fe80", Qt::CaseInsensitive))
                result.append(ipAddress);
        }
        return result;
    } catch (...)
    {
        qWarning() << "XSVNCScreen: Exception in guestAddresses()";
    }

    return QStringList();
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
            this->connect();
        });
    } else if (this->_rdpClient)
    {
#ifdef HAVE_FREERDP
        // Connect using RDP
        this->_rdpClient->Connect(this->_rdpIp);
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
            QObject::connect(this->_vncClient, &VNCGraphicsClient::connectionSuccess, this, &XSVNCScreen::onVncClientConnected);
            QObject::connect(this->_vncClient, &VNCGraphicsClient::errorOccurred, this, &XSVNCScreen::onVncClientError);
            QObject::connect(this->_vncClient, &VNCGraphicsClient::desktopResized, this, &XSVNCScreen::onDesktopResized);
        }

        QThreadPool::globalInstance()->start([this]() { this->connect(); });
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
void XSVNCScreen::connect()
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
        if (this->GetUseSource())
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

            // C#: _vncPassword = Settings.GetVNCPassword(_sourceVm.uuid). Settings and dialogs
            // belong to the GUI thread.
            const QString vmUuid = this->sourceVmUuid();
            QString password;
            bool known = false;
            QMetaObject::invokeMethod(this, [&]() { known = SettingsManager::instance().GetVNCPassword(vmUuid, &password); },
                                      Qt::BlockingQueuedConnection);

            if (!known)
            {
                // C#: no prompt while a lifecycle operation (start, reboot...) is running
                bool lifecycleOperationInProgress = false;
                if (XenCache* cache = this->_connection ? this->_connection->GetCache() : nullptr)
                {
                    const QVariantMap operations = cache->ResolveObjectData(XenObjectType::VM, this->_sourceRef)
                                                       .value("current_operations").toMap();
                    for (const QVariant& operation : operations)
                    {
                        // C# VM.is_lifecycle_operation: anything but memory changes
                        const QString op = operation.toString();
                        if (op != "changing_dynamic_range" && op != "changing_static_range" && op != "changing_memory_limits")
                            lifecycleOperationInProgress = true;
                    }
                }

                if (this->_haveTriedLoginWithoutPassword && !lifecycleOperationInProgress)
                {
                    const bool passwordIncorrect = this->m_lastAuthFailed && !this->_ignoreNextError;
                    bool entered = false;
                    QMetaObject::invokeMethod(this, [&]() { entered = this->promptForPassword(vmUuid, passwordIncorrect, &password); },
                                              Qt::BlockingQueuedConnection);
                    this->_ignoreNextError = false;
                    this->m_lastAuthFailed = false;
                    if (!entered)
                    {
                        qDebug() << "XSVNCScreen: User cancelled VNC password prompt: aborting connection attempt";
                        QMetaObject::invokeMethod(this, [this]() { emit this->userCancelledAuth(); }, Qt::QueuedConnection);
                        return;
                    }
                } else
                {
                    qDebug() << "XSVNCScreen: Attempting passwordless VNC login";
                    password.clear();
                    this->_ignoreNextError = true;
                    this->_haveTriedLoginWithoutPassword = true;
                }
            }
            this->_vncPassword = password.toUtf8();

            // Get or create TCP connection
            QTcpSocket* stream = this->takePendingVNCConnection();

            if (stream)
            {
                qDebug() << "XSVNCScreen: Using pending VNC connection";
            } else
            {
                qDebug() << "XSVNCScreen: Connecting to vncIP=" << this->_vncIp << ", port=" << VNC_PORT;
                stream = connectGuest(this->_vncIp, VNC_PORT, this->_connection);
                qDebug() << "XSVNCScreen: Connected to vncIP=" << this->_vncIp << ", port=" << VNC_PORT;
            }

            // Invoke connection with the stream
            this->invokeConnection(this->_vncClient, stream, QString());

            // C#: store the empty password after a passwordless login; a rejection clears it
            if (this->_haveTriedLoginWithoutPassword && password.isEmpty())
            {
                QMetaObject::invokeMethod(this, [vmUuid]()
                {
                    const QString empty;
                    SettingsManager::instance().SetVNCPassword(vmUuid, &empty);
                }, Qt::QueuedConnection);
            }
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

    if (!this->shouldRetryConnection())
    {
        qDebug() << "XSVNCScreen: Source not in runnable state, skipping hosted console connect";
        return;
    }

    if (this->m_hostedConsoleConnectionPending)
    {
        qDebug() << "XSVNCScreen: Hosted console connection already pending";
        return;
    }

    if (!this->_vncClient || !this->_connection || this->_sourceRef.isEmpty())
    {
        qWarning() << "XSVNCScreen: Cannot connect - invalid state";
        return;
    }

    // Check if we should use source (hosted console) vs detected IP
    if (!this->GetUseSource())
    {
        qDebug() << "XSVNCScreen: Not using source, skipping hosted console";
        return;
    }

    try
    {
        // Get VM record from cache
        XenCache* cache = this->_connection->GetCache();
        if (!cache)
        {
            qWarning() << "XSVNCScreen: No cache available";
            return;
        }

        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef);
        if (!vm || !vm->IsValid())
        {
            qWarning() << "XSVNCScreen: Cannot resolve VM record";
            return;
        }
        QString powerState = vm->GetPowerState();
        qDebug() << "XSVNCScreen: VM power_state=" << powerState;

        // Get consoles list (list of OpaqueRefs)
        //QStringList consoles = vm->GetConsoleRefs();
        QList<QSharedPointer<Console>> consoles = vm->GetConsoles();
        if (consoles.isEmpty())
        {
            qDebug() << "XSVNCScreen: No consoles found for VM (consoles list empty in cache)";
            this->retryConnection(dynamic_cast<VNCGraphicsClient*>(this->_vncClient), "No consoles found");
            return;
        }

        qDebug() << "XSVNCScreen: Found" << consoles.size() << "console refs";

        // Search for RFB (VNC) console
        foreach (QSharedPointer<Console> console, consoles)
        {
            qDebug() << "XSVNCScreen: Inspecting console" << console->OpaqueRef();

            // Check if VNC client has been replaced
            if (!this->_vncClient)
            {
                qDebug() << "XSVNCScreen: VNC client replaced, aborting";
                return;
            }

            // Check protocol - we want "rfb" for VNC
            QString protocol = console->GetProtocol();
            if (protocol != "rfb")
            {
                qDebug() << "XSVNCScreen: Skipping console with protocol:" << protocol;
                continue;
            }

            // Found VNC console! Try to connect
            qDebug() << "XSVNCScreen: Found RFB console:" << console->OpaqueRef();
            qDebug() << "XSVNCScreen: Console location:" << console->GetLocation();
            try
            {
                if (this->connectHostedConsole(this->_vncClient, console->OpaqueRef()))
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

    if (!vncClient || !this->_connection || consoleRef.isEmpty())
    {
        qWarning() << "XSVNCScreen: Invalid parameters for connectHostedConsole";
        return false;
    }

    try
    {
        // Get console record from cache
        XenCache* cache = this->_connection->GetCache();
        if (!cache)
        {
            throw std::runtime_error("No cache available");
        }

        QVariantMap consoleRecord = cache->ResolveObjectData(XenObjectType::Console, consoleRef);
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
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef);
        QString residentOnRef = vm ? vm->GetResidentOnRef() : QString();

        if (residentOnRef.isEmpty() || residentOnRef == XENOBJECT_NULL)
        {
            throw std::runtime_error("VM is not running on any host");
        }

        // Verify host exists
        QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, residentOnRef);
        if (!host || !host->IsValid())
        {
            throw std::runtime_error("Cannot resolve host where VM is running");
        }
        qDebug() << "XSVNCScreen: Resident host:" << residentOnRef << "name:" << host->GetName();

        // Get current session ID
        // C#: Uses elevated credentials if available (CA-91132), otherwise duplicates session
        QString sessionId = this->currentConnectionSessionId();
        if (sessionId.isEmpty())
        {
            throw std::runtime_error("No active session");
        }
        const QString requestSessionId = sessionId;

        qDebug() << "XSVNCScreen: Establishing HTTP CONNECT tunnel";
        qDebug() << "XSVNCScreen: Session ID prefix:" << sessionId.left(12) + "...";

        this->m_hostedConsoleConnectionPending = true;

        // Create HTTPConnect instance for async connection
        HTTPConnect* httpConnect = new HTTPConnect(this);

        // Connect success signal
        QObject::connect(httpConnect, &HTTPConnect::connectedToConsole, this, [this, vncClient, consoleRef, httpConnect, requestSessionId](QSslSocket* socket, const QByteArray& initialData)
        {
             qDebug() << "XSVNCScreen: HTTP CONNECT tunnel established";
             this->m_hostedConsoleConnectionPending = false;

             // Clean up HTTPConnect object
             httpConnect->deleteLater();

             // Drop stale callbacks from an old session/connection cycle.
             if (this->currentConnectionSessionId() != requestSessionId || !this->GetUseSource())
             {
                 qDebug() << "XSVNCScreen: Ignoring stale hosted-console success callback";
                 if (socket)
                 {
                     socket->disconnectFromHost();
                     socket->deleteLater();
                 }
                 return;
             }

             // Pass socket to invokeConnection
             this->invokeConnection(vncClient, socket, consoleRef, initialData);
         });

        // Connect error signal
        QObject::connect(httpConnect, &HTTPConnect::error, this, [this, vncClient, httpConnect, requestSessionId](const QString& error)
        {
             qWarning() << "XSVNCScreen: HTTP CONNECT failed:" << error;
             this->m_hostedConsoleConnectionPending = false;

             // Clean up HTTPConnect object
             httpConnect->deleteLater();

             if (this->currentConnectionSessionId() != requestSessionId || !this->GetUseSource())
             {
                 qDebug() << "XSVNCScreen: Ignoring stale hosted-console error callback";
                 return;
             }

             // Retry connection
             this->retryConnection(vncClient, error);
         });

        // Start async connection
        httpConnect->connectToConsoleAsync(consoleUrl, sessionId);
        return true;
    } catch (const std::exception& e)
    {
        qWarning() << "XSVNCScreen: Exception in connectHostedConsole:" << e.what();
        this->m_hostedConsoleConnectionPending = false;
        this->retryConnection(vncClient, QString::fromStdString(e.what()));
    } catch (...)
    {
        qWarning() << "XSVNCScreen: Unknown exception in connectHostedConsole";
        this->m_hostedConsoleConnectionPending = false;
        this->retryConnection(vncClient, "Unknown error");
    }
    return false;
}

/**
 * @brief Invoke connection with open stream
 * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs lines 1093-1115
 */
void XSVNCScreen::invokeConnection(VNCGraphicsClient* vncClient, QTcpSocket* stream, const QString& consoleRef, const QByteArray& initialData)
{
    qDebug() << "XSVNCScreen: invokeConnection()";

    if (!vncClient || !stream)
    {
        qWarning() << "XSVNCScreen: Invalid VNC client or stream";
        return;
    }

    // Invoke on main thread (equivalent to C# Program.Invoke)
    QMetaObject::invokeMethod(this, [this, vncClient, stream, consoleRef, initialData]() {
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
            vncClient->DisconnectAndDispose();
            
            // Small delay to ensure dispose completes and widget repaints black screen
            QThread::msleep(10);
            
            // C#: v.SendScanCodes = UseSource && !_sourceIsPv; (not for in-guest VNC servers)
            vncClient->SetSendScanCodes(this->_useSource && !this->_sourceIsPv);

            // Connect the VNC client with the stream
            // C#: v.Connect(stream, console?.location);
            QString password = QString::fromUtf8(this->_vncPassword);
            vncClient->Connect(stream, password, initialData);

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
    if (this->_sourceRef.isEmpty() || !this->_connection)
        return false;

    XenCache* cache = this->_connection->GetCache();
    if (!cache)
        return false;

    // Check if source is a VM (hosts don't have power_state in the same way)
    QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, this->_sourceRef);
    if (vm && vm->IsValid())
    {
        QString powerState = vm->GetPowerState();
        return powerState == "Running";
    }

    // For hosts, check if the host is enabled/connected
    QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, this->_sourceRef);
    if (host && host->IsValid())
    {
        return host->IsEnabled();
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

    if (this->m_hostedConsoleConnectionPending)
    {
        qDebug() << "XSVNCScreen: Hosted console request already pending, suppressing retry storm";
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
            this->connect();
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
