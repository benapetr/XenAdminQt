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

#ifndef VNCGRAPHICSCLIENT_H
#define VNCGRAPHICSCLIENT_H

#include <QWidget>
#include <QCursor>
#include <QTcpSocket>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QSet>
#include <QMutex>
#include <QClipboard>
#include "IRemoteConsole.h"

class ConsoleKeyHandler;

/**
 * @brief VNC Graphics Client implementation
 *
 * This class implements the VNC (RFB) protocol client matching the C# VNCGraphicsClient.
 * It provides framebuffer rendering, keyboard/mouse input, clipboard sync, and more.
 *
 * Key features:
 * - Double-buffered rendering (backBuffer + frontGraphics)
 * - Scaling with aspect ratio preservation
 * - Keyboard: scan codes and keysyms modes
 * - Mouse: coordinate translation and throttling
 * - Clipboard: bidirectional sync
 * - CAD injection: Ctrl+Alt+Delete
 *
 * Matches: xenadmin/XenAdmin/ConsoleView/VNCGraphicsClient.cs
 */
class VNCGraphicsClient : public QWidget, public IRemoteConsole
{
    Q_OBJECT

    public:
        static constexpr int BORDER_PADDING = 5;
        static constexpr qint32 RAW_ENCODING = 0;
        static constexpr qint32 COPY_RECTANGLE_ENCODING = 1;
        static constexpr qint32 RRE_ENCODING = 2;
        static constexpr qint32 CORRE_ENCODING = 4;
        static constexpr qint32 CURSOR_PSEUDO_ENCODING = -239;
        static constexpr qint32 DESKTOP_SIZE_PSEUDO_ENCODING = -223; // C# VNCStream.DESKTOP_SIZE_PSEUDO_ENCODING
        static constexpr qint32 XENCENTER_ENCODING = -254;
        static constexpr int BORDER_WIDTH = 1;
        static constexpr int MOUSE_EVENTS_BEFORE_UPDATE = 2;
        static constexpr int MOUSE_EVENTS_DROPPED = 5;

        explicit VNCGraphicsClient(QWidget* parent = nullptr);
        ~VNCGraphicsClient() override;

        // IRemoteConsole interface
        ConsoleKeyHandler* KeyHandler() const override;
        void SetKeyHandler(ConsoleKeyHandler* handler) override;
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
        bool IsScaling() const override;
        void SetScaling(bool value) override;
        void SetDisplayBorder(bool value) override;
        QSize DesktopSize() const override;
        void SetDesktopSize(const QSize& size) override;
        QRect ConsoleBounds() const override;

        // Connection management (matches C# Connect/Disconnect)
        void Connect(QTcpSocket* stream, const QString& password, const QByteArray& initialData = QByteArray());
        bool IsConnected() const
        {
            return m_connected;
        }
        bool IsTerminated() const
        {
            return m_terminated;
        }

        // Source mode (text console)
        void SetUseSource(bool value)
        {
            m_useSource = value;
        }

        bool GetUseSource() const
        {
            return m_useSource;
        }

        // QEMU extended key encoding
        void SetUseQemuExtKeyEncoding(bool value)
        {
            m_useQemuExtKeyEncoding = value;
        }

    signals:
        void errorOccurred(QObject* sender, const QString& error);
        void connectionSuccess();
        void desktopResized();

    protected:
        // Qt event handlers
        bool event(QEvent* event) override; // Intercept Tab key before focus navigation
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;

    private slots:
        void onSocketReadyRead();
        void onSocketDisconnected();
        void onSocketError(QAbstractSocket::SocketError error);
        void onClipboardChanged();
        void requestFramebufferUpdate();

    private:
        Qt::Key remapKey(Qt::Key input);

        // Protocol handling
        void handleProtocolVersion();
        void handleSecurityHandshake();
        void handleSecurityResult();
        void handleServerInit();
        bool handleFramebufferUpdate();
        //! C# VNCStream.DesktopSize / VNCGraphicsClient.ClientDesktopSize
        void applyDesktopSize(int width, int height);
        //! C# ReadCursorPseudoEncoding + ClientSetCursor: pixels plus 1-bit mask, hotspot x/y
        void applyRemoteCursor(int hotX, int hotY, int width, int height, const QByteArray& pixels, const QByteArray& mask);
        //! C# OnMouseMove cursor choice: remote/local cursor over the guest screen, else default
        void updateCursor(const QPoint& pos);
        bool handleSetColorMapEntries();
        bool handleBell();
        bool handleServerCutText();

        // Client messages
        void sendClientInit();
        void sendSetPixelFormat();
        void sendSetEncodings();
        void sendFramebufferUpdateRequest(bool incremental);
        void sendKeyEvent(quint32 key, bool down);
        void sendScanCodeEvent(quint32 scanCode, quint32 keysym, bool down);
        void sendPointerEvent(quint8 buttonMask, quint16 x, quint16 y);
        void sendClientCutText(const QString& text);
        int bytesPerPixel() const;
        QRgb decodePixel(const uchar* data) const;
        //! Pixel format is the RGB32 layout sendSetPixelFormat() requests
        bool isNativeRgb32() const;
        //! Write a rectangle of server pixels into the back buffer (caller holds m_backBufferMutex)
        void writePixels(int x, int y, int width, int height, const uchar* data);
        //! C# ClientFillRectangle / ClientCopyRectangle (caller holds m_backBufferMutex)
        void fillRect(int x, int y, int width, int height, QRgb color);
        void copyRect(int srcX, int srcY, int width, int height, int dstX, int dstY);
        //! Bytes a rectangle of this encoding needs after its header, -1 if more data must
        //! arrive first to know, -2 if the encoding is not supported
        int rectDataSize(qint32 encoding, int width, int height, int offset) const;

        // Rendering helpers (matches C# Damage, OnPaint, etc.)
        void damage(int x, int y, int width, int height);
        void updateScale();
        void renderDamage();
        void drawBorder(QPainter& painter, const QRect& consoleRect);
        void setupGraphicsOptions(QPainter& painter);

        // Input helpers
        QPoint translateMouseCoords(const QPoint& pos);
        quint32 qtKeyToKeysymWithModifiers(Qt::Key key, Qt::KeyboardModifiers modifiers, const QString& text);
        quint32 qtKeyToKeysym(Qt::Key key);
        quint32 qtKeyToScanCode(Qt::Key key);
        void sendScanCodes(Qt::Key key, bool down);

        // Clipboard helpers
        bool redirectingClipboard();
        void setConsoleClipboard();

        // Network helpers
        quint8 readU8();
        quint16 readU16();
        quint32 readU32();
        void writeU8(quint8 value);
        void writeU16(quint16 value);
        void writeU32(quint32 value);
        QByteArray readBytes(int count);

        // Network state
        QTcpSocket* m_vncStream = nullptr; // Matches C# _vncStream (but QTcpSocket instead of VNCStream)
        volatile bool m_connected = false;
        volatile bool m_terminated = false;
        enum State
        {
            Disconnected,
            ProtocolVersion,
            SecurityHandshake,
            SecurityResult,
            Initialization,
            Normal
        };
        State m_state = State::Disconnected;
        int m_protocolMinorVersion = 8; // RFB protocol minor version (3 for 3.3, 7 for 3.7, 8 for 3.8)
        QByteArray m_readBuffer;
        QString m_password;

        // Rendering state (matches C# fields with exact names)
        QImage m_backBuffer;          // Matches C# Bitmap _backBuffer
        QMutex m_backBufferMutex;     // Protects _backBuffer access
        bool m_backBufferInteresting = false; // Matches C# _backBufferInteresting
        QRect m_damage;               // Matches C# Rectangle _damage

        // Graphics (C# has _backGraphics and _frontGraphics, Qt uses QPainter)
        // We'll create QPainter instances on-demand instead of storing them

        // Scaling state (matches C# fields)
        bool m_scaling = true;
        float m_scale = 1.0f;
        float m_dx = 0.0f, m_dy = 0.0f; // Translation offsets
        int m_bump = 0; // For damage inflation

        // Input state (matches C# fields)
        bool m_sendScanCodes = true;
        bool m_useSource = false; // Text console mode
        bool m_displayBorder = true;
        bool m_useQemuExtKeyEncoding = false;
        // Next FramebufferUpdateRequest must be non-incremental (C# VNCStream._incremental = false)
        bool m_forceFullUpdate = false;

        // Mouse cursor (C# _remoteCursor / _localCursor / _cursorOver): the server's cursor
        // shape once it sends one, otherwise a small local cursor, while over the guest screen
        QCursor m_remoteCursor;
        bool m_hasRemoteCursor = false;
        bool m_cursorOver = false;
        QSet<Qt::Key> m_pressedKeys;
        QSet<int> m_pressedScans;
        int m_currentMouseState = 0;
        int m_mouseMoved = 0;
        int m_mouseNotMoved = 0;
        QMouseEvent* m_pending = nullptr;
        int m_pendingState = 0;
        int m_lastState = 0;
        bool m_modifierKeyPressedAlone = false;

        // Clipboard state (matches C# fields)
        QString m_clipboardStash;
        bool m_updateClipboardOnFocus = false;
        static bool m_handlingChange;

        // Keyboard handler
        ConsoleKeyHandler* m_keyHandler = nullptr;

        // Pause state
        bool m_helperIsPaused = true;

        // Update timer
        QTimer* m_updateTimer;

        // Framebuffer info
        int m_fbWidth = 640;
        int m_fbHeight = 480;
        QString m_desktopName;

        // Pixel format
        struct
        {
            quint8 bitsPerPixel;
            quint8 depth;
            quint8 bigEndian;
            quint8 trueColor;
            quint16 redMax;
            quint16 greenMax;
            quint16 blueMax;
            quint8 redShift;
            quint8 greenShift;
            quint8 blueShift;
        } m_pixelFormat;
};

#endif // VNCGRAPHICSCLIENT_H
