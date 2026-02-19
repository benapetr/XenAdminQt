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

#ifndef RDPCLIENT_H
#define RDPCLIENT_H

#include "IRemoteConsole.h"
#include <QWidget>
#include <QTimer>
#include <QMutex>
#include <QImage>
#include <QThread>
#include <QSet>

// Forward declarations for FreeRDP types (to avoid including headers here)
typedef struct rdp_context rdpContext;
typedef struct rdp_freerdp freerdp;

class ConsoleKeyHandler;

/**
 * @brief RDP client implementation using FreeRDP library
 *
 * This class provides RDP (Remote Desktop Protocol) connectivity to Windows VMs
 * and Linux VMs with xrdp. It wraps the FreeRDP library and implements the
 * IRemoteConsole interface for integration with XenAdmin's console system.
 *
 * Reference: XenAdmin/ConsoleView/RdpClient.cs
 *
 * Key differences from C# implementation:
 * - Uses FreeRDP instead of Microsoft's RDP ActiveX control
 * - Runs FreeRDP in a separate thread to avoid blocking the UI
 * - Renders RDP framebuffer to QImage for display in QWidget
 * - Handles keyboard/mouse input translation to RDP scan codes
 */
class RdpClient : public QWidget, public IRemoteConsole
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param parent Parent widget (container for RDP display)
         * @param size Initial size of RDP display
         */
        explicit RdpClient(QWidget* parent, const QSize& size);

        /**
         * @brief Destructor
         */
        ~RdpClient() override;

        // ========== IRemoteConsole Interface Implementation ==========

        ConsoleKeyHandler* KeyHandler() const override
        {
            return _keyHandler;
        }
        void SetKeyHandler(ConsoleKeyHandler* handler) override
        {
            _keyHandler = handler;
        }
        QWidget* ConsoleControl() override
        {
            return this;
        }
        void Activate() override;
        void DisconnectAndDispose() override;
        void Pause() override;
        void Unpause() override;
        void SendCAD() override;
        void SendFunctionKeyWithModifiers(bool ctrl, bool alt, int functionNumber) override;
        QImage Snapshot() override;
        void SetSendScanCodes(bool value) override;
        bool IsScaling() const override
        {
            return _scaling;
        }
        void SetScaling(bool value) override;
        void SetDisplayBorder(bool value) override;
        QSize DesktopSize() const override;
        void SetDesktopSize(const QSize& size) override;
        QRect ConsoleBounds() const override;

        // ========== RDP-Specific Public Methods ==========

        /**
         * @brief Connect to RDP server
         * @param rdpIP IP address or hostname of RDP server
         * @param width Desired desktop width
         * @param height Desired desktop height
         *
         * Reference: C# RdpClient.RDPConnect()
         */
        void Connect(const QString& rdpIP, int width, int height);

        /**
         * @brief Connect using default size
         * @param rdpIP IP address or hostname
         *
         * Reference: C# RdpClient.Connect()
         */
        void Connect(const QString& rdpIP);

        /**
         * @brief Disconnect from RDP server
         *
         * Reference: C# RdpClient.Disconnect()
         */
        void Disconnect();

        /**
         * @brief Update display size (dynamic resolution)
         * @param width New desktop width
         * @param height New desktop height
         * @param locationOffset Display offset for positioning
         *
         * Reference: C# RdpClient.UpdateDisplay()
         */
        void UpdateDisplay(int width, int height, const QPoint& locationOffset);

        /**
         * @brief Check if RDP is currently connected
         * @return true if connected, false otherwise
         */
        bool IsConnected() const
        {
            return _connected;
        }

        /**
         * @brief Check if connection is in progress
         * @return true if connecting or showing auth warning
         *
         * Reference: C# RdpClient.IsAttemptingConnection
         */
        bool IsAttemptingConnection() const
        {
            return _connecting || _authWarningVisible;
        }

        /**
         * @brief Set credentials for RDP authentication
         * @param username Username
         * @param password Password
         * @param domain Domain (optional)
         */
        void SetCredentials(const QString& username, const QString& password, const QString& domain = QString());

    signals:
        /**
         * @brief Emitted when RDP disconnects
         *
         * Reference: C# RdpClient.OnDisconnected event
         */
        void disconnected();

        /**
         * @brief Emitted when connection succeeds
         */
        void connected();

        /**
         * @brief Emitted when desktop size changes
         * @param newSize New desktop size
         */
        void desktopResized(QSize newSize);

        /**
         * @brief Emitted when an error occurs
         * @param error Error message
         */
        void errorOccurred(const QString& error);

    protected:
        // Qt event handlers
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;

    private:
        /**
         * @brief Initialize FreeRDP context
         * @return true if successful, false otherwise
         */
        bool initializeFreeRDP();

        /**
         * @brief Cleanup FreeRDP resources
         */
        void cleanupFreeRDP();

        /**
         * @brief Configure RDP settings (NLA, audio, clipboard, etc.)
         *
         * Reference: C# RdpClient.RDPSetSettings()
         */
        void configureRDPSettings();

        /**
         * @brief Run FreeRDP connection (on worker thread)
         */
        void runRDPConnection();

        /**
         * @brief Update framebuffer image from RDP context
         * @param x X coordinate of updated region
         * @param y Y coordinate of updated region
         * @param width Width of updated region
         * @param height Height of updated region
         */
        void updateFramebuffer(int x, int y, int width, int height);

        /**
         * @brief Translate Qt key to RDP scan code
         * @param key Qt key code
         * @param modifiers Qt key modifiers
         * @return RDP scan code
         */
        uint32_t qtKeyToRDPScanCode(int key, Qt::KeyboardModifiers modifiers);

        // FreeRDP context and instance
        freerdp* _rdpInstance;
        rdpContext* _rdpContext;

        // Connection state
        QString _serverAddress;
        int _desktopWidth;
        int _desktopHeight;
        volatile bool _connected;
        volatile bool _connecting;
        volatile bool _authWarningVisible;
        volatile bool _terminated;

        // Credentials
        QString _username;
        QString _password;
        QString _domain;

        // Display state
        QImage _framebuffer;
        QMutex _framebufferMutex;
        bool _scaling;
        QPoint _locationOffset;
        bool _paused;

        // Keyboard/mouse state
        ConsoleKeyHandler* _keyHandler;
        QSet<int> _pressedScans;
        bool _modifierKeyPressedAlone;

        // Worker thread for FreeRDP
        QThread* _rdpThread;

        // Disposal timer (deferred cleanup like C#)
        QTimer* _disposalTimer;
        int _disposalAttempts;

        // Thread synchronization
        QMutex _connectionMutex;
};

#endif // RDPCLIENT_H
