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

#ifndef XSVNCSCREEN_H
#define XSVNCSCREEN_H

#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QSize>
#include <QMap>
#include <QList>
#include <QTcpSocket>
#include <QSharedPointer>
#include <functional>

// Forward declarations
class VNCGraphicsClient;
class RdpClient;
class IRemoteConsole;
class ConsoleKeyHandler;
class VNCTabView;
class XenConnection;
class XenCache;
class VM;
class Host;

namespace XenAPI
{
    class Session;
} // namespace XenAPI

/**
 * @brief XSVNCScreen - Central controller for VNC/RDP console connections
 *
 * Ports the C# XSVNCScreen.cs class (1392 lines) exactly, maintaining all field names,
 * method names, and logic flow for easy comparison with upstream.
 *
 * Responsibilities:
 * - Protocol polling: Scan VNC (5900) and RDP (3389) ports on VM guest IPs
 * - Connection retry: Exponential backoff with configurable retry counts
 * - Auto-switching: Detect RDP availability and switch protocols automatically
 * - Session management: Own _vncClient and _rdpClient instances
 * - GPU passthrough detection: Monitor GPU status and emit warnings
 * - Event emission: Notify UI of connection state changes
 *
 * Threading model (matches C# exactly):
 * - Polling runs on QTimer background thread
 * - Connection attempts use Qt thread pool
 * - All UI updates via Qt signals (equivalent to C# Program.Invoke)
 *
 * Reference: xenadmin/XenAdmin/ConsoleView/XSVNCScreen.cs
 */
class XSVNCScreen : public QWidget
{
    Q_OBJECT

    public:
        // Constants matching C# exactly
        static const int SHORT_RETRY_COUNT = 10;
        static const int SHORT_RETRY_SLEEP_TIME = 100; // ms
        static const int RETRY_SLEEP_TIME = 5000;      // ms
        static const int RDP_POLL_INTERVAL = 30000;    // ms
        static const int RDP_PORT = 3389;
        static const int VNC_PORT = 5900;
        static const int CONSOLE_SIZE_OFFSET = 6;

        /**
         * @brief Constructor matching C# XSVNCScreen(VM, EventHandler, VNCTabView, string, string)
         * @param source The VM or Host to connect to
         * @param parent The parent VNCTabView (equivalent to C# parent parameter)
         * @param connection XenConnection instance for API access
         * @param elevatedUsername Username for elevated credential sessions (can be empty)
         * @param elevatedPassword Password for elevated credential sessions (can be empty)
         */
        explicit XSVNCScreen(const QString& sourceRef, VNCTabView* parent, XenConnection* connection,
                             const QString& elevatedUsername = QString(),
                             const QString& elevatedPassword = QString());

        ~XSVNCScreen() override;

        // ========== Public Properties (matching C# properties) ==========

        /**
         * @brief The VM or Host this console is connected to
         * Equivalent to C#: public VM Source { get; private set; }
         */
        QString source() const
        {
            return _sourceRef;
        }

        /**
         * @brief Desktop size of the remote console
         * Equivalent to C#: public Size DesktopSize
         */
        QSize desktopSize() const;

        /**
         * @brief Whether RDP version warning is needed
         * Equivalent to C#: public bool RdpVersionWarningNeeded
         */
        bool rdpVersionWarningNeeded() const;

        /**
         * @brief Whether user wants to manually switch protocol
         * Equivalent to C#: public bool UserWantsToSwitchProtocol { get; set; }
         */
        bool userWantsToSwitchProtocol() const
        {
            return _userWantsToSwitchProtocol;
        }
        void setUserWantsToSwitchProtocol(bool value)
        {
            _userWantsToSwitchProtocol = value;
        }

        /**
         * @brief The currently active remote console (VNC or RDP)
         * Equivalent to C#: private IRemoteConsole RemoteConsole
         */
        IRemoteConsole* remoteConsole() const
        {
            return _remoteConsole;
        }

        /**
         * @brief VNC password for this VM (stored as char array for security)
         * Equivalent to C#: private char[] _vncPassword
         */
        void setVncPassword(const QString& password);
        QString vncPassword() const;

        /**
         * @brief Elevated credentials for host console access
         */
        QString elevatedUsername() const
        {
            return _elevatedUsername;
        }
        QString elevatedPassword() const
        {
            return _elevatedPassword;
        }

        /**
         * @brief Whether to use VNC (true) or RDP (false)
         * Equivalent to C#: public bool UseVNC { get; set; }
         * Reference: XSVNCScreen.cs lines 652-659
         */
        bool useVNC() const
        {
            return _useVNC;
        }
        void setUseVNC(bool value);

        /**
         * @brief Whether to use hosted console (text/source console)
         * For Windows VMs: UseVNC controls default desktop (true) vs remote desktop (false)
         * For Linux VMs: UseSource controls text console (true) vs graphical VNC (false)
         * Equivalent to C#: public bool UseSource { get; set; }
         * Reference: XSVNCScreen.cs lines 666-675
         */
        bool useSource() const
        {
            return _useSource;
        }
        void setUseSource(bool value);

        /**
         * @brief Check if in default console mode
         * Windows VMs: UseVNC indicates default desktop (true) vs remote desktop (false)
         * Linux VMs: UseSource indicates text console (true) vs graphical VNC (false)
         * Equivalent to C#: private bool InDefaultConsole()
         * Reference: XSVNCScreen.cs lines 738-744
         */
        bool inDefaultConsole() const
        {
            return _useVNC && _useSource;
        }

        /**
         * @brief IP addresses detected for VNC/RDP
         * Equivalent to C#: public string RdpIp / public string VncIp
         */
        QString rdpIp() const
        {
            return _rdpIp;
        }
        QString vncIp() const
        {
            return _vncIp;
        }

        /**
         * @brief Auto-switch to RDP when it becomes available
         * Equivalent to C#: internal bool AutoSwitchRDPLater { get; set; }
         */
        bool autoSwitchRDPLater() const
        {
            return _autoSwitchRDPLater;
        }
        void setAutoSwitchRDPLater(bool value)
        {
            _autoSwitchRDPLater = value;
        }

        // ========== Public Methods (matching C# public methods) ==========

        /**
         * @brief Pause the remote console (stop rendering/input)
         * Equivalent to C#: public void Pause()
         */
        void pause();

        /**
         * @brief Unpause the remote console (resume rendering/input)
         * Equivalent to C#: public void Unpause()
         */
        void unpause();

        /**
         * @brief Start polling for VNC/RDP ports
         * Equivalent to C#: public void StartPolling()
         */
        void startPolling();

        /**
         * @brief Disconnect and cleanup all console connections
         * Equivalent to C#: called in Dispose()
         */
        void disconnectAndDispose();

        /**
         * @brief Send Ctrl+Alt+Del to the remote console
         * Equivalent to C#: public void SendCAD()
         */
        void sendCAD();

        /**
         * @brief Capture screenshot of console
         * Equivalent to C#: public Image Snapshot(...)
         */
        QImage snapshot();

        /**
         * @brief Set console scaling mode
         * Equivalent to C#: public bool Scaling { get; set; }
         */
        void setScaling(bool enabled);
        bool scaling() const;

        /**
         * @brief Check if must connect via remote desktop (GPU passthrough)
         * Equivalent to C#: public bool MustConnectRemoteDesktop()
         */
        bool mustConnectRemoteDesktop() const;

        /**
         * @brief Capture keyboard and mouse input to console
         * Equivalent to C#: internal void CaptureKeyboardAndMouse()
         * Reference: XSVNCScreen.cs lines 1288-1303
         */
        void captureKeyboardAndMouse();

        /**
         * @brief Release keyboard and mouse capture from console
         * Equivalent to C#: internal void UncaptureKeyboardAndMouse()
         * Reference: XSVNCScreen.cs lines 1276-1286
         */
        void uncaptureKeyboardAndMouse();

        // ========== Callback Delegates (matching C# MethodInvoker) ==========

        /**
         * @brief Callbacks for protocol detection (equivalent to C# MethodInvoker)
         */
        std::function<void()> onDetectRDP;
        std::function<void()> onDetectVNC;

    signals:
        // ========== Qt Signals (equivalent to C# events) ==========

        /**
         * @brief User cancelled authentication dialog
         * Equivalent to C#: public event EventHandler UserCancelledAuth
         */
        void userCancelledAuth();

        /**
         * @brief VNC connection attempt was cancelled
         * Equivalent to C#: public event EventHandler VncConnectionAttemptCancelled
         */
        void vncConnectionAttemptCancelled();

        /**
         * @brief GPU passthrough status changed
         * Equivalent to C#: public event Action<bool> GpuStatusChanged
         */
        void gpuStatusChanged(bool hasGpu);

        /**
         * @brief Connection name changed (e.g. VM renamed)
         * Equivalent to C#: public event Action<string> ConnectionNameChanged
         */
        void connectionNameChanged(const QString& name);

        /**
         * @brief Resize event for parent to handle
         * Equivalent to C#: internal EventHandler ResizeHandler
         */
        void resizeRequested();

    private slots:
        // ========== Qt Slots (equivalent to C# event handlers) ==========

        /**
         * @brief Handle VM property changes (power state, name, etc.)
         * Equivalent to C#: private void VM_PropertyChanged
         */
        void onVMPropertyChanged(const QString& vmRef, const QString& propertyName);

        /**
         * @brief Handle guest metrics changes (IP addresses)
         * Equivalent to C#: private void guestMetrics_PropertyChanged
         */
        void onGuestMetricsPropertyChanged(const QString& propertyName);

        /**
         * @brief Handle settings changes (RDP polling enabled/disabled)
         * Equivalent to C#: private void Default_PropertyChanged
         */
        void onSettingsPropertyChanged(const QString& propertyName);

        /**
         * @brief Poll RDP port timer callback
         * Equivalent to C#: private void PollRDPPort(object sender)
         */
        void pollRDPPort();

        /**
         * @brief Poll VNC port timer callback
         * Equivalent to C#: private void PollVNCPort(object sender)
         */
        void pollVNCPort();

        /**
         * @brief Handle VNC client successful connection
         * Equivalent to C#: vncClient.Connected += ...
         * Reference: XSVNCScreen.cs lines 242-250
         */
        void onVncClientConnected();

        /**
         * @brief Handle VNC client connection error
         * Equivalent to C#: vncClient.ErrorOccurred += ...
         * Reference: XSVNCScreen.cs lines 252-260
         */
        void onVncClientError(QObject* sender, const QString& error);

        /**
         * @brief Handle desktop resize event from VNC client
         * Equivalent to C#: vncClient.DesktopResized += ...
         * Reference: XSVNCScreen.cs lines 262-270
         */
        void onDesktopResized();

    public slots:
        /**
         * @brief Connect to a hosted console (via XenAPI Console object)
         * Made public slot so VNCTabView can trigger connection
         * Equivalent to C#: private void ConnectNewHostedConsole() (but called from constructor)
         */
        void connectNewHostedConsole();

    private:
        // ========== Private Methods (matching C# private methods exactly) ==========

        /**
         * @brief Initialize the console control (VNC or RDP client)
         * Equivalent to C#: private void InitSubControl()
         */
        void initSubControl();

        /**
         * @brief Register event listeners for VM/metrics changes
         * Equivalent to C#: called in constructor
         */
        void registerEventListeners();

        /**
         * @brief Unregister all event listeners
         * Equivalent to C#: private void UnregisterEventListeners()
         */
        void unregisterEventListeners();

        /**
         * @brief Route object data changes to specific handlers
         */
        void onObjectDataReceived(const QString& objectType, const QString& objectRef, const QVariantMap& data);

        /**
         * @brief Handle cache object changes from EventPoller (real-time updates)
         */
        void onCacheObjectChanged(XenConnection *connection, const QString& objectType, const QString& objectRef);

        /**
         * @brief Handle VM data changes (power state, metrics ref, etc.)
         */
        void onVMDataChanged(const QVariantMap& vmData);

        /**
         * @brief Handle guest metrics data changes (IP addresses)
         */
        void onGuestMetricsChanged(const QVariantMap& metricsData);

        /**
         * @brief Scan a specific port on all guest IP addresses
         * Equivalent to C#: public string PollPort(int port, bool vnc)
         * @param port Port number to scan (5900 for VNC, 3389 for RDP)
         * @param vnc Whether this is a VNC scan (affects connection reuse)
         * @return IP address if port is open, empty string otherwise
         */
        QString pollPort(int port, bool vnc);

        /**
         * @brief Connect to remote console (VNC or RDP)
         * Equivalent to C#: private void ConnectToRemoteConsole()
         */
        void connectToRemoteConsole();

        /**
         * @brief Connection succeeded handler
         * Equivalent to C#: private void ConnectionSuccess(object sender, EventArgs e)
         */
        void connectionSuccess();

        /**
         * @brief Main connection logic (runs on background thread)
         * Equivalent to C#: private void Connect(object o)
         */
        void connect();

        /**
         * @brief Connect hosted console with VNC client
         * Equivalent to C#: private void ConnectHostedConsole(VNCGraphicsClient v, Console console)
         */
        bool connectHostedConsole(VNCGraphicsClient* vncClient, const QString& consoleRef);

        /**
         * @brief Invoke connection with open stream
         * Equivalent to C#: private void InvokeConnection(VNCGraphicsClient v, Stream stream, Console console)
         */
        void invokeConnection(VNCGraphicsClient* vncClient, QTcpSocket* stream, const QString& consoleRef);

        /**
         * @brief Check if the source VM/host is running and should allow retry
         * Reference: XenAdmin/ConsoleView/XSVNCScreen.cs RetryConnection check
         */
        bool shouldRetryConnection() const;

        /**
         * @brief Retry connection after failure with exponential backoff
         * Equivalent to C#: private void RetryConnection(VNCGraphicsClient v, Exception exn)
         */
        void retryConnection(VNCGraphicsClient* vncClient, const QString& errorMessage);

        /**
         * @brief Sleep and retry connection (background thread)
         * Equivalent to C#: private void SleepAndRetryConnection(object o)
         */
        void sleepAndRetryConnection(IRemoteConsole* console);

        /**
         * @brief Set pending VNC connection stream (for connection reuse during polling)
         * Equivalent to C#: private void SetPendingVNCConnection(Stream s)
         */
        void setPendingVNCConnection(QTcpSocket* stream);

        /**
         * @brief Get pending VNC connection stream
         * Equivalent to C#: accessed under lock
         */
        QTcpSocket* getPendingVNCConnection();
        QTcpSocket* takePendingVNCConnection();

        QString currentConnectionSessionId();

        /**
         * @brief Check if source VM/Host has RDP capability
         * Equivalent to C#: private bool HasRDP
         */
        bool hasRDP() const;

        /**
         * @brief Compare two QMap dictionaries for equality
         * Equivalent to C#: private static bool EquateDictionary<T, TS>(...)
         */
        template <typename K, typename V>
        static bool equateDictionary(const QMap<K, V>& d1, const QMap<K, V>& d2);

        /**
         * @brief Auto-switch to RDP after delay
         * Equivalent to C#: private void AutoSwitchRDPLater()
         */
        void autoSwitchRDPLater();

        /**
         * @brief Trigger VNC connection attempt cancelled event
         * Equivalent to C#: private void OnVncConnectionAttemptCancelled()
         */
        void onVncConnectionAttemptCancelled();

        /**
         * @brief Set keyboard and mouse capture state
         * Equivalent to C#: private void SetKeyboardAndMouseCapture(bool value)
         * Reference: XSVNCScreen.cs lines 599-604
         */
        void setKeyboardAndMouseCapture(bool enabled);
        XenCache* cache() const;
        QSharedPointer<VM> resolveVM(const QString& vmRef) const;
        QSharedPointer<Host> resolveHost(const QString& hostRef) const;
        bool isControlDomainZero(const QString& vmRef, QString* outHostRef = nullptr) const;
        bool hasGPUPassthrough(const QString& vmRef) const;

        // ========== Private Fields (matching C# fields exactly) ==========

        // Core state
        QString _sourceRef;             // VM or Host ref (C#: VM Source)
        bool _sourceIsPv;               // Is source a PV VM? (C#: bool _sourceIsPv)
        XenConnection* _connection = nullptr;
        VNCTabView* _parentVNCTabView = nullptr;  // Parent view (C#: internal readonly VNCTabView)
        ConsoleKeyHandler* _keyHandler = nullptr; // Keyboard handler (C#: internal ConsoleKeyHandler)

        // Console clients
        VNCGraphicsClient* _vncClient = nullptr;  // VNC client instance (C#: volatile VNCGraphicsClient)
        RdpClient* _rdpClient = nullptr;          // RDP client instance (C#: RdpClient)
        IRemoteConsole* _remoteConsole = nullptr; // Active console (VNC or RDP) (C#: IRemoteConsole)

        // Connection state
        volatile bool _useVNC = true;               // Use VNC (true) or RDP (false) (C#: volatile bool)
        volatile bool _useSource = true;            // Use hosted/source console (text) vs graphical (C#: volatile bool)
        bool _autoSwitchRDPLater = false;            // Auto-switch to RDP later (C#: internal bool AutoSwitchRDPLater)
        int _connectionRetries = 0;              // Retry counter (C#: int _connectionRetries)
        bool _wasPaused = true;                     // Was paused before last operation (C#: bool _wasPaused)
        bool _haveTriedLoginWithoutPassword = false; // Tried passwordless login? (C#: bool)
        bool _ignoreNextError = false;               // Ignore next error (C#: bool)
        bool _userWantsToSwitchProtocol = false;     // User manually switching? (C#: bool)
        QString _lastPowerState;             // Track power state changes for proper reconnection

        // Credentials
        QString _elevatedUsername; // Elevated username (C#: internal string)
        QString _elevatedPassword; // Elevated password (C#: internal string)
        QByteArray _vncPassword;   // VNC password (C#: char[] _vncPassword)

        // Polling state
        QTimer* _connectionPoller = nullptr;            // Polling timer (C#: Timer _connectionPoller)
        QString _rdpIp;                       // Detected RDP IP (C#: public string RdpIp)
        QString _vncIp;                       // Detected VNC IP (C#: public string VncIp)
        bool _hostedConsoleConnectionPending = false; // Prevent concurrent hosted console attempts

        // Hosted consoles
        QMutex _hostedConsolesLock;     // Lock for console list (C#: object _hostedConsolesLock)
        QList<QString> _hostedConsoles; // Console refs (C#: List<XenRef<Console>>)

        // Active session (for persistent connection during polling)
        QMutex _activeSessionLock; // Lock for session (C#: object _activeSessionLock)
        QString _activeSessionRef; // Session ref (C#: Session _activeSession)

        // Pending VNC connection (reused between poll and actual connect)
        QMutex _pendingVNCConnectionLock;  // Lock for pending connection (C#: object)
        QTcpSocket* _pendingVNCConnection = nullptr; // Pending stream (C#: Stream _pendingVNCConnection)

        // Cached guest metrics
        QString _guestMetricsRef;               // Guest metrics ref for this VM
        QMap<QString, QString> _cachedNetworks; // Cached network info (C#: Dictionary<string, string>)

        // UI settings
        bool _autoCaptureKeyboardAndMouse = true; // Auto-capture input (C#: readonly bool)
        QColor _focusColor;                // Focus highlight color (C#: readonly Color)
};

#endif // XSVNCSCREEN_H
