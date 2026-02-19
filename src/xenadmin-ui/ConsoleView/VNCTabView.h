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

/**
 * @file VNCTabView.h
 * @brief UI wrapper for XSVNCScreen with controls for VNC/RDP switching, scaling, fullscreen
 *
 * Reference: xenadmin/XenAdmin/ConsoleView/VNCTabView.cs
 * This is the main UI component that users interact with for VM console access.
 * It provides:
 * - VNC/RDP protocol toggle button
 * - Scaling checkbox
 * - Fullscreen mode
 * - Dock/undock functionality
 * - CD/DVD ISO management
 * - Power state display
 * - GPU passthrough warnings
 */

#ifndef VNCTABVIEW_H
#define VNCTABVIEW_H

#include <QWidget>
#include <QSize>
#include <QString>
#include <QImage>
#include <QTimer>
#include <QMenu>
#include "ConsoleKeyHandler.h"

// Forward declarations
class XSVNCScreen;
class VNCView;
class XenConnection;
class XenCache;
class VM;

namespace Ui
{
    class VNCTabView;
}

/**
 * @class VNCTabView
 * @brief Qt widget providing UI controls for remote console access
 *
 * Reference: XenAdmin/ConsoleView/VNCTabView.cs (1593 lines)
 *
 * Layout hierarchy:
 * - Top gradient panel: HostLabel, ISO toolbar, SSH button, toggle console button
 * - Warning panel: Power state messages, GPU warnings
 * - Content panel: XSVNCScreen (VNC/RDP display)
 * - Bottom toolbar: Send CAD, scale checkbox, dock/undock, fullscreen
 *
 * Key responsibilities:
 * - Owns XSVNCScreen instance
 * - Handles protocol switching (VNC ↔ RDP)
 * - Manages scaling and fullscreen modes
 * - Registers keyboard shortcuts
 * - Displays power state and warnings
 * - Integrates with VNCView parent for docking
 */
class VNCTabView : public QWidget
{
    Q_OBJECT

    public:
        /**
         * @brief Timeout for INS key detection (milliseconds)
         * C#: public const int INS_KEY_TIMEOUT = 500;
         */
        static constexpr int INS_KEY_TIMEOUT = 500;

        /**
         * @brief Constructor
         * Reference: VNCTabView.cs lines 94-228
         *
         * @param parent Parent VNCView (for docking callbacks)
         * @param vm VM OpaqueRef
         * @param elevatedUsername Optional elevated credentials username
         * @param elevatedPassword Optional elevated credentials password
         * @param conn XenConnection instance for XenAPI access
         * @param parentWidget Qt parent widget
         */
        explicit VNCTabView(VNCView* parent, QSharedPointer<VM> vm, const QString& elevatedUsername, const QString& elevatedPassword, QWidget* parentWidget = nullptr);

        /**
         * @brief Destructor - cleanup resources, unregister listeners
         * Reference: VNCTabView.Designer.cs lines 14-37
         */
        ~VNCTabView();

        /**
         * @brief Get/set scaled mode
         * Reference: VNCTabView.cs lines 230-234
         */
        bool IsScaled() const;
        void SetScaled(bool scaled);

        /**
         * @brief Check if RDP control is enabled (for XSVNCScreen)
         * Reference: VNCTabView.cs line 92
         */
        bool IsRDPControlEnabled() const;

        /**
         * @brief Pause console updates (when tab hidden)
         * Reference: VNCTabView.cs lines 772-776
         */
        void Pause();

        /**
         * @brief Resume console updates (when tab visible)
         * Reference: VNCTabView.cs lines 778-782
         */
        void Unpause();

        /**
         * @brief Disable VNC toggle button (called by XSVNCScreen during default console detection)
         * Reference: VNCTabView.cs lines 1072-1076
         */
        void DisableToggleVNCButton();

        /**
         * @brief Enable VNC toggle button
         * Reference: VNCTabView.cs lines 1078-1082
         */
        void EnableToggleVNCButton();

        /**
         * @brief Update dock button icon/text based on docked state
         * Reference: VNCTabView.cs lines 478-490
         */
        void UpdateDockButton();

        /**
         * @brief Update fullscreen button enabled state
         * Reference: VNCTabView.cs lines 492-509
         */
        void UpdateFullScreenButton();

        /**
         * @brief Setup CD/DVD ISO toolbar
         * Reference: VNCTabView.cs lines 618-621
         */
        void SetupCD();

        /**
         * @brief Send Ctrl+Alt+Delete to the VM
         * Reference: VNCTabView.cs lines 1269-1273
         */
        void SendCAD();
        QImage Snapshot() const;

        /**
         * @brief Update parent VNCView minimum size based on content
         * Reference: VNCTabView.cs lines 350-358
         */
        void UpdateParentMinimumSize();

        /**
         * @brief Scale console to fit window (if scaling enabled)
         * Reference: VNCTabView.cs lines 846-868
         */
        void MaybeScale();

        /**
         * @brief Calculate required size to show desktop at 1:1 scale
         * Reference: VNCTabView.cs lines 883-910
         * @return Size needed for unscaled display
         */
        QSize GrowToFit();

        /**
         * @brief Access to keyboard shortcut handler
         * C#: internal readonly ConsoleKeyHandler KeyHandler = new ConsoleKeyHandler();
         */
        ConsoleKeyHandler* GetKeyHandler()
        {
            return &m_keyHandler;
        }

    public slots:
        /**
         * @brief Handle tab opened event - may auto-switch to RDP
         * Reference: VNCTabView.cs lines 1275-1300
         */
        void onTabOpened();

    signals:
        /**
         * @brief Emitted when toggle dock/undock requested
         * Reference: VNCTabView.cs line 995
         */
        void toggleDockRequested();

        /**
         * @brief Emitted when fullscreen mode requested
         * Reference: VNCTabView.cs line 1008
         */
        void toggleFullscreenRequested();

        /**
         * @brief Emitted when VM start requested (from power state label click)
         */
        void vmStartRequested(const QString& vmRef);

        /**
         * @brief Emitted when VM resume requested (from power state label click)
         */
        void vmResumeRequested(const QString& vmRef);

        /**
         * @brief Emitted when console size changes (for VNCView resize)
         * Reference: VNCTabView.cs line 826
         */
        void consoleResized();

    private slots:
        /**
         * @brief Handle VM property changes
         * Reference: VNCTabView.cs lines 511-563
         */
        void onVMPropertyChanged(const QString& propertyName);

        /**
         * @brief Handle guest metrics property changes
         * Reference: VNCTabView.cs lines 565-590
         */
        void onGuestMetricsPropertyChanged(const QString& propertyName);

        /**
         * @brief Handle settings changes
         * Reference: VNCTabView.cs lines 340-348
         */
        void onSettingsPropertyChanged(const QString& settingName);

        /**
         * @brief Handle send CAD button click
         * Reference: VNCTabView.cs lines 930-934
         */
        void onSendCADClicked();
        void onSendSpecialAltFn(int functionNumber);
        void onSendSpecialCtrlAltFn(int functionNumber);

        /**
         * @brief Handle scale checkbox toggled
         * Reference: VNCTabView.cs lines 912-928
         */
        void onScaleCheckBoxChanged(bool checked);

        /**
         * @brief Handle dock button click
         * Reference: VNCTabView.cs lines 936-941
         */
        void onDockButtonClicked();

        /**
         * @brief Handle fullscreen button click
         * Reference: VNCTabView.cs lines 943-946
         */
        void onFullscreenButtonClicked();

        /**
         * @brief Handle toggle console button (VNC ↔ RDP) click
         * Reference: VNCTabView.cs lines 1143-1241
         */
        void onToggleConsoleButtonClicked();

        /**
         * @brief Handle SSH console button click
         * Reference: VNCTabView.cs lines 1536-1550
         */
        void onSSHButtonClicked();

        /**
         * @brief Handle power state label click (show lifecycle menu)
         * Reference: VNCTabView.cs lines 738-770
         */
        void onPowerStateLabelClicked();

        /**
         * @brief Handle VNC/RDP resize events
         * Reference: VNCTabView.cs lines 826-834
         */
        void onRDPorVNCResizeHandler();

        /**
         * @brief RDP detected callback from XSVNCScreen
         * Reference: VNCTabView.cs lines 1084-1113
         */
        void onDetectRDP();

        /**
         * @brief VNC detected callback from XSVNCScreen
         * Reference: VNCTabView.cs lines 1115-1141
         */
        void onDetectVNC();

        /**
         * @brief User cancelled auth (password prompt)
         * Reference: VNCTabView.cs lines 1252-1273
         */
        void onUserCancelledAuth();

        /**
         * @brief VNC connection attempt cancelled
         * Reference: VNCTabView.cs lines 1243-1250
         */
        void onVncConnectionAttemptCancelled();

        /**
         * @brief INS key timer timeout (for Ctrl+Alt+Ins → Ctrl+Alt+Del)
         * Reference: VNCTabView.cs lines 969-983
         */
        void onInsKeyTimeout();


    private:
        /**
         * @brief Register keyboard shortcuts
         * Reference: VNCTabView.cs lines 360-421
         */
        void registerShortcutKeys();

        /**
         * @brief Deregister keyboard shortcuts
         * Reference: VNCTabView.cs lines 423-476
         */
        void deregisterShortcutKeys();

        /**
         * @brief Register event listeners (VM, guest metrics, hosts, settings)
         * Reference: VNCTabView.cs lines 94-228 (constructor registration)
         */
        void registerEventListeners();

        /**
         * @brief Unregister all event listeners
         * Reference: VNCTabView.cs lines 290-338
         */
        void unregisterEventListeners();

        /**
         * @brief Update power state display
         * Reference: VNCTabView.cs lines 623-660
         */
        void updatePowerState();

        /**
         * @brief Maybe enable toggle console button based on VM state
         * Reference: VNCTabView.cs lines 662-668
         */
        void maybeEnableButton();

        /**
         * @brief Show power state label with message
         * Reference: VNCTabView.cs lines 670-675
         */
        void enablePowerStateLabel(const QString& label);

        /**
         * @brief Hide power state label with message
         * Reference: VNCTabView.cs lines 677-682
         */
        void disablePowerStateLabel(const QString& label);

        /**
         * @brief Hide top bar controls
         * Reference: VNCTabView.cs lines 684-728
         */
        void hideTopBarContents();

        /**
         * @brief Show top bar controls
         * Reference: VNCTabView.cs lines 730-736
         */
        void showTopBarContents();

        /**
         * @brief Disable console controls when VM is powered off
         * Reference: VNCTabView.cs lines 1290-1302
         */
        void vmPowerOff();

        /**
         * @brief Enable console controls when VM is powered on
         * Reference: VNCTabView.cs lines 1304-1308
         */
        void vmPowerOn();

        /**
         * @brief Check if RDP can be enabled
         * Reference: VNCTabView.cs lines 784-788
         */
        bool canEnableRDP() const;

        /**
         * @brief Enable RDP if VM capabilities allow
         * Reference: VNCTabView.cs lines 592-598
         */
        void enableRDPIfCapable();

        /**
         * @brief Update button states and labels based on protocol
         * Reference: VNCTabView.cs lines 1194-1219
         */
        void updateButtons();

        /**
         * @brief Guess native console label (RDP/VNC/X Console)
         * Reference: VNCTabView.cs lines 790-824
         */
        QString guessNativeConsoleLabel() const;

        /**
         * @brief VNC resize handler (update scaling, parent size)
         * Reference: VNCTabView.cs lines 836-844
         */
        void vncResizeHandler();

        /**
         * @brief Check if desktop size has changed
         * Reference: VNCTabView.cs lines 870-881
         */
        bool desktopSizeHasChanged();

        /**
         * @brief Wait for INS key (Ctrl+Alt+Ins handling)
         * Reference: VNCTabView.cs lines 950-956
         */
        void waitForInsKey();

        /**
         * @brief Cancel wait for INS key and send CAD
         * Reference: VNCTabView.cs lines 958-967
         */
        void cancelWaitForInsKeyAndSendCAD();

        /**
         * @brief Update tooltip of toggle console button
         * Reference: VNCTabView.cs lines 1302-1332
         */
        void updateTooltipOfToggleButton();

        /**
         * @brief Update SSH console button state
         * Reference: VNCTabView.cs lines 1334-1365
         */
        void updateOpenSSHConsoleButtonState();
        void setupSpecialKeysMenu();
        void sendSpecialFunctionKey(bool ctrl, bool alt, int functionNumber);

        /**
         * @brief Show/hide RDP version warning
         * Reference: VNCTabView.cs lines 220-223
         */
        void showOrHideRdpVersionWarning();


        /**
         * @brief Show GPU warning if required
         * Reference: VNCTabView.cs lines 1367-1379
         */
        void showGpuWarningIfRequired(bool mustConnectRemoteDesktop);

        /**
         * @brief Toggle dock/undock (internal)
         * Reference: VNCTabView.cs lines 995-1005
         */
        void toggleDockUnDock();

        /**
         * @brief Toggle fullscreen (internal)
         * Reference: VNCTabView.cs lines 1008-1066
         */
        void toggleFullscreen();

        /**
         * @brief Toggle console focus (capture/release keyboard and mouse)
         * Reference: VNCTabView.cs lines 1436-1456
         */
        void toggleConsoleFocus();

    private:
        QVariantMap getCachedVmData() const;
        QString getCachedVmPowerState() const;
        XenCache* cache() const;
        QVariantMap getCachedObjectData(const QString& type, const QString& ref) const;
        bool isSRDriverDomain(const QString& vmRef, QString* outSRRef = nullptr) const;
        bool hasRDP(QSharedPointer<VM> vm) const;
        bool rdpControlEnabledForVm(QSharedPointer<VM> vm) const;
        bool canEnableRDPForVm() const;
        bool isVMWindows(const QString& vmRef) const;
        QString getVMIPAddressForSSH(const QString& vmRef) const;

        Ui::VNCTabView* ui; ///< Qt Designer UI

        XSVNCScreen* m_vncScreen = nullptr; ///< Console controller
        VNCView* m_parentVNCView = nullptr; ///< Parent docking manager
        XenConnection *m_connection = nullptr;

        QSharedPointer<VM> m_vm;
        QString _vmRef;           ///< VM OpaqueRef
        QString _guestMetricsRef; ///< Guest metrics OpaqueRef
        QString _targetHostRef;   ///< Target host OpaqueRef (for non-control-domain VMs)

        QSize m_lastDesktopSize;     ///< Last known desktop size
        bool m_switchOnTabOpened = false;    ///< Auto-switch to RDP on tab open
        bool m_ignoringResizes = false;      ///< Ignore resize events during scaling changes
        bool m_ignoreScaleChange = false;    ///< Ignore scale checkbox change events
        bool m_inToggleDockUnDock = false;   ///< Guard for dock/undock toggle
        bool m_inToggleFullscreen = false;   ///< Guard for fullscreen toggle
        bool m_inToggleConsoleFocus = false; ///< Guard for console focus toggle
        bool m_oldScaleValue = false;        ///< Previous scale checkbox value (C#: bool oldScaleValue)
        bool m_tryToConnectRDP = false;      ///< Flag to attempt RDP connection (C#: bool tryToConnectRDP)

        ConsoleKeyHandler m_keyHandler; ///< Keyboard shortcut handler

        QTimer* m_insKeyTimer = nullptr; ///< Timer for Ctrl+Alt+Ins detection
        QMenu* m_specialKeysMenu = nullptr;

        // RDP/VNC toggle state
        static constexpr bool RDP = true;   ///< C#: private const bool RDP = true;
        static constexpr bool XVNC = false; ///< C#: private const bool XVNC = false;
        bool m_toggleToXVNCorRDP;            ///< Next protocol to switch to

        // CD/DVD drive state

        // Fullscreen support (forward declare, will implement with VNCView)
        // FullScreenForm* _fullscreenForm;
        // FullScreenHint* _fullscreenHint;
};

#endif // VNCTABVIEW_H
