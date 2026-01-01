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
    static constexpr int BORDER_WIDTH = 1;
    static constexpr int MOUSE_EVENTS_BEFORE_UPDATE = 2;
    static constexpr int MOUSE_EVENTS_DROPPED = 5;

    explicit VNCGraphicsClient(QWidget* parent = nullptr);
    ~VNCGraphicsClient() override;

    // IRemoteConsole interface
    ConsoleKeyHandler* keyHandler() const override;
    void setKeyHandler(ConsoleKeyHandler* handler) override;
    QWidget* consoleControl() override
    {
        return this;
    }
    void activate() override;
    void disconnectAndDispose() override;
    void pause() override;
    void unpause() override;
    void sendCAD() override;
    QImage snapshot() override;
    void setSendScanCodes(bool value) override;
    bool scaling() const override;
    void setScaling(bool value) override;
    void setDisplayBorder(bool value) override;
    QSize desktopSize() const override;
    void setDesktopSize(const QSize& size) override;
    QRect consoleBounds() const override;

    // Connection management (matches C# Connect/Disconnect)
    void connect(QTcpSocket* stream, const QString& password);
    bool connected() const
    {
        return _connected;
    }
    bool terminated() const
    {
        return _terminated;
    }

    // Source mode (text console)
    void setUseSource(bool value)
    {
        _useSource = value;
    }
    bool useSource() const
    {
        return _useSource;
    }

    // QEMU extended key encoding
    void setUseQemuExtKeyEncoding(bool value)
    {
        _useQemuExtKeyEncoding = value;
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
    // Protocol handling
    void handleProtocolVersion();
    void handleSecurityHandshake();
    void handleSecurityResult();
    void handleServerInit();
    bool handleFramebufferUpdate();
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
    QTcpSocket* _vncStream; // Matches C# _vncStream (but QTcpSocket instead of VNCStream)
    volatile bool _connected;
    volatile bool _terminated;
    enum State
    {
        Disconnected,
        ProtocolVersion,
        SecurityHandshake,
        SecurityResult,
        Initialization,
        Normal
    };
    State _state;
    int _protocolMinorVersion; // RFB protocol minor version (3 for 3.3, 7 for 3.7, 8 for 3.8)
    QByteArray _readBuffer;
    QString _password;

    // Rendering state (matches C# fields with exact names)
    QImage _backBuffer;          // Matches C# Bitmap _backBuffer
    QMutex _backBufferMutex;     // Protects _backBuffer access
    bool _backBufferInteresting; // Matches C# _backBufferInteresting
    QRect _damage;               // Matches C# Rectangle _damage

    // Graphics (C# has _backGraphics and _frontGraphics, Qt uses QPainter)
    // We'll create QPainter instances on-demand instead of storing them

    // Scaling state (matches C# fields)
    bool _scaling;
    float _scale;
    float _dx, _dy; // Translation offsets
    float _oldDx, _oldDy;
    int _bump; // For damage inflation

    // Input state (matches C# fields)
    bool _sendScanCodes;
    bool _useSource; // Text console mode
    bool _displayBorder;
    bool _useQemuExtKeyEncoding;
    QSet<Qt::Key> _pressedKeys;
    QSet<int> _pressedScans;
    int _currentMouseState;
    int _mouseMoved;
    int _mouseNotMoved;
    QMouseEvent* _pending;
    int _pendingState;
    int _lastState;
    bool _modifierKeyPressedAlone;

    // Clipboard state (matches C# fields)
    QString _clipboardStash;
    bool _updateClipboardOnFocus;
    static bool _handlingChange;

    // Keyboard handler
    ConsoleKeyHandler* _keyHandler;

    // Pause state
    bool _helperIsPaused;

    // Update timer
    QTimer* _updateTimer;

    // Framebuffer info
    int _fbWidth;
    int _fbHeight;
    QString _desktopName;

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
    } _pixelFormat;
};

#endif // VNCGRAPHICSCLIENT_H
