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

#include "VNCGraphicsClient.h"
#include "ConsoleKeyHandler.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QtEndian>
#include <QtMath>
#include <QDebug>

// Static member initialization
bool VNCGraphicsClient::_handlingChange = false;

VNCGraphicsClient::VNCGraphicsClient(QWidget* parent)
    : QWidget(parent), _vncStream(nullptr), _connected(false), _terminated(false), _state(Disconnected), _protocolMinorVersion(8) // Default to RFB 3.8, will be set during handshake
      ,
      _backBufferInteresting(false), _scaling(true), _scale(1.0f), _dx(0.0f), _dy(0.0f), _oldDx(0.0f), _oldDy(0.0f), _bump(0), _sendScanCodes(true), _useSource(false), _displayBorder(true), _useQemuExtKeyEncoding(false), _currentMouseState(0), _mouseMoved(0), _mouseNotMoved(0), _pending(nullptr), _pendingState(0), _lastState(0), _modifierKeyPressedAlone(false), _updateClipboardOnFocus(false), _keyHandler(nullptr), _helperIsPaused(true), _fbWidth(640), _fbHeight(480)
{
    // NOTE: Don't enable mouse tracking or set focus policy here!
    // These will be enabled when we successfully connect (see connectToHost)
    // Setting them in constructor causes UI freeze when widget exists but isn't connected
    this->setAttribute(Qt::WA_OpaquePaintEvent);

    // Setup control styles (matches C# SetStyle calls)
    this->setAttribute(Qt::WA_NoSystemBackground, false); // Opaque = false in C#

    // Initialize back buffer (matches C# constructor)
    this->_backBuffer = QImage(640, 480, QImage::Format_RGB32);
    this->_backBuffer.fill(this->palette().color(QPalette::Window));

    // Periodic framebuffer update requests
    this->_updateTimer = new QTimer(this);
    this->_updateTimer->setInterval(40); // 25 FPS
    QObject::connect(this->_updateTimer, &QTimer::timeout, this, &VNCGraphicsClient::requestFramebufferUpdate);

    // Clipboard synchronization
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged,
                     this, &VNCGraphicsClient::onClipboardChanged);

    qDebug() << "VNCGraphicsClient: Initialized with 640x480 backbuffer";
}

VNCGraphicsClient::~VNCGraphicsClient()
{
    // Disconnect clipboard (matches C# Dispose)
    this->disconnect(QApplication::clipboard(), &QClipboard::dataChanged,
               this, &VNCGraphicsClient::onClipboardChanged);

    this->DisconnectAndDispose();

    // Cleanup back buffer (matches C# lock(_backBuffer) block in Dispose)
    QMutexLocker locker(&this->_backBufferMutex);
    // _backBuffer is automatically cleaned up by QImage destructor
}

//=============================================================================
// IRemoteConsole Interface Implementation
//=============================================================================

ConsoleKeyHandler* VNCGraphicsClient::KeyHandler() const
{
    return this->_keyHandler;
}

void VNCGraphicsClient::SetKeyHandler(ConsoleKeyHandler* handler)
{
    this->_keyHandler = handler;
}

void VNCGraphicsClient::Activate()
{
    // Only grab focus if we're actually connected
    if (this->_connected && this->_state == Normal)
    {
        this->setFocus();
        this->raise();
    }
}

void VNCGraphicsClient::DisconnectAndDispose()
{
    // Matches C# Disconnect() method
    this->_connected = false;
    this->_terminated = true;

    // Disable mouse tracking and focus when disconnecting
    this->setMouseTracking(false);
    this->setFocusPolicy(Qt::NoFocus);

    if (this->_vncStream)
    {
        disconnect(this->_vncStream, nullptr, this, nullptr);
        this->_vncStream->close();

        // Transfer ownership and schedule deletion
        QTcpSocket* stream = this->_vncStream;
        this->_vncStream = nullptr;
        stream->deleteLater();
    }

    this->_updateTimer->stop();

    {
        QMutexLocker locker(&this->_backBufferMutex);
        this->_backBuffer.fill(Qt::black);
        this->_backBufferInteresting = false;
        this->_damage = QRect();
    }

    update();
}

void VNCGraphicsClient::Pause()
{
    // Matches C# Pause() in IRemoteConsole
    this->_helperIsPaused = true;
    this->_updateTimer->stop();
}

void VNCGraphicsClient::Unpause()
{
    // Matches C# UnPause() in IRemoteConsole
    this->_helperIsPaused = false;
    if (this->_connected && this->_state == Normal)
    {
        this->_updateTimer->start();
    }
    update();
}

void VNCGraphicsClient::SendCAD()
{
    // Matches C# SendCAD() method
    if (!this->_connected)
        return;

    qDebug() << "VNCGraphicsClient: Sending Ctrl+Alt+Delete";

    // Send Ctrl down, Alt down, Delete down, Delete up, Alt up, Ctrl up
    // X11 Keysyms: XK_Control_L = 0xffe3, XK_Alt_L = 0xffe9, XK_Delete = 0xffff
    sendKeyEvent(0xFFE3, true);  // Left Control down
    sendKeyEvent(0xFFE9, true);  // Left Alt down
    sendKeyEvent(0xFFFF, true);  // Delete down
    sendKeyEvent(0xFFFF, false); // Delete up
    sendKeyEvent(0xFFE9, false); // Left Alt up
    sendKeyEvent(0xFFE3, false); // Left Control up
}

void VNCGraphicsClient::SendFunctionKeyWithModifiers(bool ctrl, bool alt, int functionNumber)
{
    if (!this->_connected || functionNumber < 1 || functionNumber > 12)
        return;

    // X11 keysyms: F1..F12 are contiguous from 0xFFBE.
    const quint32 functionKeysym = 0xFFBE + static_cast<quint32>(functionNumber - 1);

    if (ctrl)
        sendKeyEvent(0xFFE3, true); // Left Control down
    if (alt)
        sendKeyEvent(0xFFE9, true); // Left Alt down

    sendKeyEvent(functionKeysym, true);
    sendKeyEvent(functionKeysym, false);

    if (alt)
        sendKeyEvent(0xFFE9, false); // Left Alt up
    if (ctrl)
        sendKeyEvent(0xFFE3, false); // Left Control up
}

QImage VNCGraphicsClient::Snapshot()
{
    // Matches C# Snapshot() method
    QMutexLocker locker(&this->_backBufferMutex);
    return this->_backBuffer.copy();
}

void VNCGraphicsClient::SetSendScanCodes(bool value)
{
    this->_sendScanCodes = value;
}

bool VNCGraphicsClient::IsScaling() const
{
    return this->_scaling;
}

void VNCGraphicsClient::SetScaling(bool value)
{
    // Matches C# Scaling property setter
    if (this->_scaling != value)
    {
        this->_scaling = value;
        updateScale();
        update();
    }
}

void VNCGraphicsClient::SetDisplayBorder(bool value)
{
    this->_displayBorder = value;
    update();
}

QSize VNCGraphicsClient::DesktopSize() const
{
    // Matches C# DesktopSize property
    return QSize(this->_fbWidth, this->_fbHeight);
}

void VNCGraphicsClient::SetDesktopSize(const QSize& size)
{
    // Desktop size is set by server during initialization
    // This method exists for interface compatibility
    Q_UNUSED(size);
}

QRect VNCGraphicsClient::ConsoleBounds() const
{
    // Matches C# ConsoleBounds property
    return rect();
}

//=============================================================================
// Connection Management (matches C# Connect/Disconnect)
//=============================================================================

void VNCGraphicsClient::connect(QTcpSocket* stream, const QString& password)
{
    // Matches C# Connect(Stream stream, char[] password)
    qDebug() << "VNCGraphicsClient: Starting VNC connection";

    // C# checks: if (Connected || Terminated) close and reconnect
    // We should only refuse if already actively connected, not if terminated
    if (this->_connected && this->_vncStream)
    {
        qDebug() << "VNCGraphicsClient: Already connected, disconnecting first";
        DisconnectAndDispose();
    }

    // Reset all connection state (matches C# starting fresh connection)
    this->_terminated = false;
    this->_connected = false; // Will set to true after setup
    this->_vncStream = stream;
    this->_vncStream->setParent(this);
    this->_password = password;
    this->_state = ProtocolVersion;
    this->_readBuffer.clear();

    // Clear back buffer (prevents stale imagery from previous session)
    {
        QMutexLocker locker(&this->_backBufferMutex);
        this->_backBuffer.fill(Qt::black);
        this->_backBufferInteresting = false;
        this->_damage = QRect(); // Reset damage rect to null
    }

    // Force widget to repaint with cleared backbuffer (C# equivalently shows black before first frame)
    update();

    // Connect socket signals
    QObject::connect(this->_vncStream, &QTcpSocket::readyRead,
                     this, &VNCGraphicsClient::onSocketReadyRead);
    QObject::connect(this->_vncStream, &QTcpSocket::disconnected,
                     this, &VNCGraphicsClient::onSocketDisconnected);
    QObject::connect(this->_vncStream, &QTcpSocket::errorOccurred,
                     this, &VNCGraphicsClient::onSocketError);

    this->_connected = true;

    // If socket already has data, process it
    if (this->_vncStream->bytesAvailable() > 0)
    {
        onSocketReadyRead();
    }
}

void VNCGraphicsClient::onSocketDisconnected()
{
    qDebug() << "VNCGraphicsClient: Socket disconnected";
    this->_connected = false;
    this->_state = Disconnected;
    this->_updateTimer->stop();
    update();
}

void VNCGraphicsClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errorStr = this->_vncStream ? this->_vncStream->errorString() : "Unknown error";
    qWarning() << "VNCGraphicsClient: Socket error:" << errorStr;

    this->_connected = false;
    emit errorOccurred(this, errorStr);
}

//=============================================================================
// Protocol State Machine (matches C# VNCStream handling)
//=============================================================================

void VNCGraphicsClient::onSocketReadyRead()
{
    this->_readBuffer.append(this->_vncStream->readAll());

    // Process all available data through state machine
    while (!this->_readBuffer.isEmpty() && this->_connected)
    {
        switch (this->_state)
        {
        case Disconnected:
            return; // Nothing to process when disconnected

        case ProtocolVersion:
            handleProtocolVersion();
            break;
        case SecurityHandshake:
            handleSecurityHandshake();
            break;
        case SecurityResult:
            handleSecurityResult();
            break;
        case Initialization:
            handleServerInit();
            break;
        case Normal: {
            // Handle server messages
            if (this->_readBuffer.size() < 1)
                return;

            quint8 msgType = (quint8) this->_readBuffer.at(0);
            // qDebug() << "VNCGraphicsClient: Processing message type:" << msgType << "buffer size:" << this->_readBuffer.size();
            bool processed = false;
            switch (msgType)
            {
            case 0: // FramebufferUpdate
                processed = handleFramebufferUpdate();
                break;
            case 1: // SetColorMapEntries
                processed = handleSetColorMapEntries();
                break;
            case 2: // Bell
                processed = handleBell();
                break;
            case 3: // ServerCutText
                processed = handleServerCutText();
                break;
            default:
                qWarning() << "VNCGraphicsClient: Unknown message type:" << msgType;
                qWarning() << "VNCGraphicsClient: Buffer size:" << this->_readBuffer.size();
                qWarning() << "VNCGraphicsClient: Next few bytes:"
                           << ((this->_readBuffer.size() > 1) ? QString::number((quint8) this->_readBuffer.at(1), 16) : "N/A")
                           << ((this->_readBuffer.size() > 2) ? QString::number((quint8) this->_readBuffer.at(2), 16) : "N/A")
                           << ((this->_readBuffer.size() > 3) ? QString::number((quint8) this->_readBuffer.at(3), 16) : "N/A");

                // Unknown message - cannot determine length, so we must disconnect
                // to avoid protocol desynchronization
                emit errorOccurred(this, QString("Unknown VNC message type: %1").arg(msgType));
                DisconnectAndDispose();
                return;
            }

            if (!processed)
                return; // wait for more data
            break;
        }

        default:
            return;
        }
    }
}

void VNCGraphicsClient::handleProtocolVersion()
{
    // Wait for "RFB xxx.yyy\n" (12 bytes)
    if (this->_readBuffer.size() < 12)
        return;

    QString version = QString::fromLatin1(this->_readBuffer.left(12));
    qDebug() << "VNCGraphicsClient: Server version:" << version.trimmed();

    // Parse server version (e.g., "RFB 003.003" or "RFB 003.008")
    QString versionStr = version.trimmed();

    // Extract major and minor version numbers
    // Format: "RFB MMM.mmm" where MMM is major, mmm is minor
    int majorVersion = 3; // Default
    int minorVersion = 8; // Default to 3.8

    if (versionStr.startsWith("RFB "))
    {
        QStringList parts = versionStr.mid(4).split('.');
        if (parts.size() == 2)
        {
            majorVersion = parts[0].toInt();
            minorVersion = parts[1].toInt();
            qDebug() << "VNCGraphicsClient: Parsed server version:" << majorVersion << "." << minorVersion;
        }
    }

    // Respond with matching version (RFB protocol requires matching the server's version)
    // XenServer uses RFB 003.003, so we need to match that
    QString clientVersion;
    if (minorVersion <= 3)
    {
        // Server is using RFB 3.3 - respond with 3.3
        clientVersion = "RFB 003.003\n";
        this->_protocolMinorVersion = 3;
        qDebug() << "VNCGraphicsClient: Using RFB 3.3 protocol";
    } else if (minorVersion <= 7)
    {
        // Server supports up to 3.7 - use 3.7
        clientVersion = "RFB 003.007\n";
        this->_protocolMinorVersion = 7;
        qDebug() << "VNCGraphicsClient: Using RFB 3.7 protocol";
    } else
    {
        // Server supports 3.8 or higher - use 3.8
        clientVersion = "RFB 003.008\n";
        this->_protocolMinorVersion = 8;
        qDebug() << "VNCGraphicsClient: Using RFB 3.8 protocol";
    }

    this->_vncStream->write(clientVersion.toLatin1());
    this->_vncStream->flush();

    this->_readBuffer.remove(0, 12);
    this->_state = SecurityHandshake;
}

void VNCGraphicsClient::handleSecurityHandshake()
{
    // RFB 3.3 and 3.7+ use different security handshake formats
    if (this->_protocolMinorVersion <= 3)
    {
        // RFB 3.3: Server sends 32-bit security type directly
        if (this->_readBuffer.size() < 4)
            return;

        quint32 securityType = readU32();
        qDebug() << "VNCGraphicsClient: RFB 3.3 security type:" << securityType;

        // Security types: 0=Failed, 1=None, 2=VNC Authentication
        if (securityType == 0)
        {
            // Connection failed - read reason string
            qWarning() << "VNCGraphicsClient: Server rejected connection";
            emit errorOccurred(this, "Server rejected connection");
            DisconnectAndDispose();
            return;
        } else if (securityType == 1)
        {
            // No authentication - proceed directly to ClientInit
            qDebug() << "VNCGraphicsClient: No authentication required";
            this->_state = Initialization;
            sendClientInit();
        } else if (securityType == 2)
        {
            // VNC Authentication - wait for 16-byte challenge
            qDebug() << "VNCGraphicsClient: VNC authentication required";

            if (this->_readBuffer.size() < 16)
                return;

            // Read 16-byte challenge
            QByteArray challenge = this->_readBuffer.left(16);
            this->_readBuffer.remove(0, 16);

            // TODO: Implement proper DES encryption with password
            // For now, send back the challenge unmodified (will fail but allows testing)
            qWarning() << "VNCGraphicsClient: VNC authentication not fully implemented";
            this->_vncStream->write(challenge);
            this->_vncStream->flush();

            this->_state = SecurityResult;
        } else
        {
            qWarning() << "VNCGraphicsClient: Unknown security type:" << securityType;
            emit errorOccurred(this, QString("Unknown security type: %1").arg(securityType));
            DisconnectAndDispose();
        }
    } else
    {
        // RFB 3.7+: Server sends list of security types
        // Wait for security type count
        if (this->_readBuffer.size() < 1)
            return;

        quint8 securityTypeCount = (quint8) this->_readBuffer.at(0);

        if (securityTypeCount == 0)
        {
            // Connection failed - read reason string
            this->_readBuffer.remove(0, 1);
            if (this->_readBuffer.size() < 4)
                return;

            quint32 reasonLength = readU32();
            if (this->_readBuffer.size() < (int) reasonLength)
                return;

            QString reason = QString::fromUtf8(this->_readBuffer.left(reasonLength));
            this->_readBuffer.remove(0, reasonLength);

            qWarning() << "VNCGraphicsClient: Server rejected connection:" << reason;
            emit errorOccurred(this, "Server rejected: " + reason);
            DisconnectAndDispose();
            return;
        }

        if (this->_readBuffer.size() < 1 + securityTypeCount)
            return;

        qDebug() << "VNCGraphicsClient: Security types:" << securityTypeCount;

        // Look for security types (1 = None, 2 = VNC Authentication)
        bool foundNone = false;
        bool foundVNC = false;

        for (int i = 0; i < securityTypeCount; i++)
        {
            quint8 secType = (quint8) this->_readBuffer.at(1 + i);
            qDebug() << "VNCGraphicsClient: Security type:" << secType;
            if (secType == 1)
                foundNone = true;
            if (secType == 2)
                foundVNC = true;
        }

        this->_readBuffer.remove(0, 1 + securityTypeCount);

        // Choose security type (prefer VNC auth if password provided)
        if (!this->_password.isEmpty() && foundVNC)
        {
            qDebug() << "VNCGraphicsClient: Using VNC authentication";
            writeU8(2); // VNC Authentication
            this->_vncStream->flush();

            // Wait for challenge (handled in separate state)
            // For now, skip to SecurityResult (proper DES encryption needed for production)
            qWarning() << "VNCGraphicsClient: VNC authentication not fully implemented - using empty response";

            // Send dummy 16-byte response
            this->_vncStream->write(QByteArray(16, 0));
            this->_vncStream->flush();
            this->_state = SecurityResult;
        } else if (foundNone)
        {
            qDebug() << "VNCGraphicsClient: Using no authentication";
            writeU8(1); // None
            this->_vncStream->flush();
            this->_state = SecurityResult;
        } else
        {
            qWarning() << "VNCGraphicsClient: No compatible security type found";
            emit errorOccurred(this, "No compatible security type");
            DisconnectAndDispose();
        }
    }
}

void VNCGraphicsClient::handleSecurityResult()
{
    if (this->_readBuffer.size() < 4)
        return;

    quint32 result = readU32();
    qDebug() << "VNCGraphicsClient: Security result:" << result;

    if (result != 0)
    {
        // Authentication failed - read reason if available
        QString reason = "Authentication failed";
        if (this->_readBuffer.size() >= 4)
        {
            quint32 reasonLength = readU32();
            if (this->_readBuffer.size() >= (int) reasonLength)
            {
                reason = QString::fromLatin1(readBytes(reasonLength));
            }
        }
        emit errorOccurred(this, reason);
        DisconnectAndDispose();
        return;
    }

    // Authentication successful - send ClientInit
    sendClientInit();
    this->_state = Initialization;
}

void VNCGraphicsClient::sendClientInit()
{
    // Matches C# VNCStream initialization
    writeU8(1); // Shared flag (1 = shared desktop)
    this->_vncStream->flush();
    qDebug() << "VNCGraphicsClient: Sent ClientInit (shared=1)";
}

void VNCGraphicsClient::handleServerInit()
{
    // ServerInit structure:
    // width: U16, height: U16, pixel_format: 16 bytes, name_length: U32, name: string
    if (this->_readBuffer.size() < 24)
        return;

    this->_fbWidth = readU16();
    this->_fbHeight = readU16();

    // Pixel format (16 bytes)
    this->_pixelFormat.bitsPerPixel = readU8();
    this->_pixelFormat.depth = readU8();
    this->_pixelFormat.bigEndian = readU8();
    this->_pixelFormat.trueColor = readU8();
    this->_pixelFormat.redMax = readU16();
    this->_pixelFormat.greenMax = readU16();
    this->_pixelFormat.blueMax = readU16();
    this->_pixelFormat.redShift = readU8();
    this->_pixelFormat.greenShift = readU8();
    this->_pixelFormat.blueShift = readU8();
    readBytes(3); // padding

    quint32 nameLength = readU32();
    if (this->_readBuffer.size() < (int) nameLength)
        return;

    this->_desktopName = QString::fromUtf8(readBytes(nameLength));

    qDebug() << "VNCGraphicsClient: Framebuffer:" << this->_fbWidth << "x" << this->_fbHeight;
    qDebug() << "VNCGraphicsClient: Desktop name:" << this->_desktopName;
    qDebug() << "VNCGraphicsClient: Pixel format:" << this->_pixelFormat.bitsPerPixel << "bpp";

    // Create/resize framebuffer (matches C# OnDesktopSizeChanged)
    {
        QMutexLocker locker(&this->_backBufferMutex);
        this->_backBuffer = QImage(this->_fbWidth, this->_fbHeight, QImage::Format_RGB32);
        this->_backBuffer.fill(Qt::black);
        this->_backBufferInteresting = false;
    }

    // Decide whether to force RGB32 to match XenCenter behaviour.
    const bool serverTrueColor = this->_pixelFormat.trueColor != 0;
    const bool serverIsRgb32 =
        serverTrueColor &&
        this->_pixelFormat.bitsPerPixel == 32 &&
        this->_pixelFormat.depth >= 24 &&
        this->_pixelFormat.redMax == 255 &&
        this->_pixelFormat.greenMax == 255 &&
        this->_pixelFormat.blueMax == 255 &&
        this->_pixelFormat.redShift == 16 &&
        this->_pixelFormat.greenShift == 8 &&
        this->_pixelFormat.blueShift == 0 &&
        this->_pixelFormat.bigEndian == 0;

    if (!serverIsRgb32)
    {
        qDebug() << "VNCGraphicsClient: Requesting RGB32 pixel format";
        sendSetPixelFormat();
    } else
    {
        qDebug() << "VNCGraphicsClient: Server pixel format already RGB32";
    }

    // Send SetEncodings
    qDebug() << "VNCGraphicsClient: Sending SetEncodings";
    sendSetEncodings();

    // Request initial framebuffer update
    qDebug() << "VNCGraphicsClient: Requesting initial framebuffer update";
    sendFramebufferUpdateRequest(false);

    this->_state = Normal;
    qDebug() << "VNCGraphicsClient: Entered Normal state";
    updateScale();

    // NOW we can enable mouse tracking and focus - we're fully connected!
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);

    // Start update timer
    this->_updateTimer->start();

    emit connectionSuccess();
    emit desktopResized();
    update();
}

void VNCGraphicsClient::sendSetPixelFormat()
{
    // SetPixelFormat message (matches C# VNCStream)
    writeU8(0); // Message type
    writeU8(0); // Padding
    writeU8(0);
    writeU8(0);

    // Set pixel format to 32-bit RGB
    writeU8(32);   // bits per pixel
    writeU8(24);   // depth
    writeU8(0);    // big endian flag
    writeU8(1);    // true color flag
    writeU16(255); // red max
    writeU16(255); // green max
    writeU16(255); // blue max
    writeU8(16);   // red shift
    writeU8(8);    // green shift
    writeU8(0);    // blue shift
    writeU8(0);    // padding
    writeU8(0);
    writeU8(0);

    this->_vncStream->flush();

    // Update local pixel format to match what we requested
    this->_pixelFormat.bitsPerPixel = 32;
    this->_pixelFormat.depth = 24;
    this->_pixelFormat.bigEndian = 0;
    this->_pixelFormat.trueColor = 1;
    this->_pixelFormat.redMax = 255;
    this->_pixelFormat.greenMax = 255;
    this->_pixelFormat.blueMax = 255;
    this->_pixelFormat.redShift = 16;
    this->_pixelFormat.greenShift = 8;
    this->_pixelFormat.blueShift = 0;
}

void VNCGraphicsClient::sendSetEncodings()
{
    // SetEncodings message (matches C# VNCStream)
    writeU8(2);  // Message type
    writeU8(0);  // Padding
    writeU16(1); // Number of encodings

    // Encoding types (we only support Raw for now)
    writeU32(0); // Raw encoding

    this->_vncStream->flush();
}

void VNCGraphicsClient::sendFramebufferUpdateRequest(bool incremental)
{
    // FramebufferUpdateRequest message
    writeU8(3);                   // Message type
    writeU8(incremental ? 1 : 0); // Incremental flag
    writeU16(0);                  // x
    writeU16(0);                  // y
    writeU16(this->_fbWidth);           // width
    writeU16(this->_fbHeight);          // height

    this->_vncStream->flush();
}

void VNCGraphicsClient::requestFramebufferUpdate()
{
    if (this->_connected && this->_state == Normal && !this->_helperIsPaused)
    {
        sendFramebufferUpdateRequest(true);
    }
}

Qt::Key VNCGraphicsClient::remapKey(Qt::Key input)
{
    // macOS keyboard mapping fix: Qt maps Cmd→Control and Ctrl→Meta by default
    // We need to swap them so physical Ctrl sends VNC Control
    Qt::Key mappedKey = input;
#ifdef Q_OS_MACOS
    if (mappedKey == Qt::Key_Control)
        mappedKey = Qt::Key_Meta;
    else if (mappedKey == Qt::Key_Meta)
        mappedKey = Qt::Key_Control;
#endif
    return mappedKey;
}

bool VNCGraphicsClient::handleFramebufferUpdate()
{
    // FramebufferUpdate structure:
    // msg_type: U8, padding: U8, num_rects: U16
    // Then for each rect: x: U16, y: U16, width: U16, height: U16, encoding: I32, data...

    // We must parse the ENTIRE message before consuming ANY bytes from the buffer.
    // This prevents desynchronization when partial data arrives.

    int offset = 0;
    const int bpp = bytesPerPixel();

    // Check if header is available
    if (this->_readBuffer.size() < 4)
        return false;

    // Peek at header (don't consume yet)
    quint8 msgType = (quint8) this->_readBuffer.at(offset++); // Should be 0
    quint8 padding = (quint8) this->_readBuffer.at(offset++);
    quint16 numRects = ((quint8) this->_readBuffer.at(offset) << 8) | (quint8) this->_readBuffer.at(offset + 1);
    offset += 2;

    Q_UNUSED(msgType); // Already verified by caller
    Q_UNUSED(padding);

    // qDebug() << "VNCGraphicsClient: FramebufferUpdate with" << numRects << "rectangles";

    // Parse each rectangle to verify all data is present
    for (quint16 i = 0; i < numRects; i++)
    {
        // Check if rectangle header is available
        if (this->_readBuffer.size() - offset < 12)
        {
            // qDebug() << "VNCGraphicsClient: Waiting for rectangle header" << i << "of" << numRects;
            return false; // Wait for more data
        }

        // Peek at rectangle header
        quint16 peekX = ((quint8) this->_readBuffer.at(offset) << 8) | (quint8) this->_readBuffer.at(offset + 1);
        offset += 2;
        quint16 peekY = ((quint8) this->_readBuffer.at(offset) << 8) | (quint8) this->_readBuffer.at(offset + 1);
        offset += 2;
        quint16 width = ((quint8) this->_readBuffer.at(offset) << 8) | (quint8) this->_readBuffer.at(offset + 1);
        offset += 2;
        quint16 height = ((quint8) this->_readBuffer.at(offset) << 8) | (quint8) this->_readBuffer.at(offset + 1);
        offset += 2;

        // Encoding is signed 32-bit
        qint32 encoding = ((quint8) this->_readBuffer.at(offset) << 24) |
                          ((quint8) this->_readBuffer.at(offset + 1) << 16) |
                          ((quint8) this->_readBuffer.at(offset + 2) << 8) |
                          (quint8) this->_readBuffer.at(offset + 3);
        offset += 4;

        Q_UNUSED(peekX); // Only used for validation
        Q_UNUSED(peekY);

        if (encoding == 0)
        {
            // Raw encoding - check if pixel data is available
            int dataSize = width * height * bpp;

            if (this->_readBuffer.size() - offset < dataSize)
            {
                // qDebug() << "VNCGraphicsClient: Waiting for pixel data, need" << dataSize
                //          << "bytes, have" << (this->_readBuffer.size() - offset);
                return false; // Wait for more data
            }

            // Data is available, advance offset
            offset += dataSize;
        } else
        {
            qWarning() << "VNCGraphicsClient: Unsupported encoding:" << encoding;
            // For unsupported encodings, we can't determine size, so disconnect
            emit errorOccurred(this, QString("Unsupported encoding: %1").arg(encoding));
            DisconnectAndDispose();
            return false;
        }
    }

    // All data is available! Now consume and process atomically
    // qDebug() << "VNCGraphicsClient: All data available, consuming" << offset << "bytes";

    // Reset offset and consume header
    offset = 0;
    readU8(); // Message type
    readU8(); // Padding
    numRects = readU16();

    // Process each rectangle
    for (quint16 i = 0; i < numRects; i++)
    {
        quint16 x = readU16();
        quint16 y = readU16();
        quint16 width = readU16();
        quint16 height = readU16();
        qint32 encoding = (qint32) readU32();

        if (encoding == 0)
        {
            // Raw encoding
            const int bpp = bytesPerPixel();
            int dataSize = width * height * bpp;
            QByteArray pixelData = readBytes(dataSize);

            // Draw to back buffer (matches C# Damage/OnPaint)
            {
                QMutexLocker locker(&this->_backBufferMutex);

                for (int py = 0; py < height; py++)
                {
                    for (int px = 0; px < width; px++)
                    {
                        int pixelOffset = (py * width + px) * bpp;
                        if (x + px < (quint16) this->_fbWidth && y + py < (quint16) this->_fbHeight)
                        {
                            const uchar* pixelPtr = reinterpret_cast<const uchar*>(pixelData.constData() + pixelOffset);
                            this->_backBuffer.setPixel(x + px, y + py, decodePixel(pixelPtr));
                        }
                    }
                }

                this->_backBufferInteresting = true;
            }

            // Record damage (matches C# Damage method)
            damage(x, y, width, height);
        }
    }

    // Render damage to screen
    renderDamage();
    return true;
}

bool VNCGraphicsClient::handleSetColorMapEntries()
{
    // SetColorMapEntries: msg_type: U8, padding: U8, first_color: U16, num_colors: U16, colors...
    if (this->_readBuffer.size() < 6)
        return false;

    int offset = 0;
    offset += 2; // message type + padding
    quint16 firstColor = qFromBigEndian<quint16>((const uchar*) this->_readBuffer.constData() + offset);
    offset += 2;
    quint16 numColors = qFromBigEndian<quint16>((const uchar*) this->_readBuffer.constData() + offset);
    offset += 2;

    // Each color is 6 bytes (3 * U16)
    int colorDataSize = numColors * 6;
    if (this->_readBuffer.size() - offset < colorDataSize)
        return false;

    readU8();                 // Message type
    readU8();                 // Padding
    readU16();                // firstColor (discard)
    readU16();                // numColors (discard)
    readBytes(colorDataSize); // Skip color data (we use true color mode)
    qDebug() << "VNCGraphicsClient: SetColorMapEntries (ignored)" << numColors << "colors from" << firstColor;
    return true;
}

bool VNCGraphicsClient::handleBell()
{
    if (this->_readBuffer.size() < 1)
        return false;

    readU8(); // Message type
    qDebug() << "VNCGraphicsClient: Bell received";
    // Could emit a signal here for UI notification
    return true;
}

bool VNCGraphicsClient::handleServerCutText()
{
    // ServerCutText structure: msg_type: U8, padding: 3 bytes, length: U32, text: string
    if (this->_readBuffer.size() < 8)
        return false;

    quint32 length = qFromBigEndian<quint32>((const uchar*) this->_readBuffer.constData() + 4);
    if (this->_readBuffer.size() < 8 + (int) length)
        return false;

    readU8();     // Message type
    readBytes(3); // Padding
    length = readU32();

    QString text = QString::fromLatin1(readBytes(length));
    qDebug() << "VNCGraphicsClient: Server cut text:" << text.left(50);

    // Set clipboard (matches C# clipboard handling)
    if (redirectingClipboard())
    {
        this->_handlingChange = true;
        QApplication::clipboard()->setText(text);
        this->_handlingChange = false;
    }
    return true;
}

//=============================================================================
// Client Messages
//=============================================================================

// VNC message types (from C# VNCStream.cs)
static const quint8 KEY_EVENT = 4;          // Standard keysym-based key event
static const quint8 KEY_SCAN_EVENT = 254;   // XenServer/QEMU scan code event
static const quint8 QEMU_MSG = 255;         // QEMU extended message
static const quint8 QEMU_EXT_KEY_EVENT = 0; // QEMU extended key event subtype

void VNCGraphicsClient::sendKeyEvent(quint32 key, bool down)
{
    if (!this->_connected)
        return;

    writeU8(KEY_EVENT);    // Message type 4
    writeU8(down ? 1 : 0); // Down flag
    writeU16(0);           // Padding
    writeU32(key);         // Keysym

    this->_vncStream->flush();
}

void VNCGraphicsClient::sendScanCodeEvent(quint32 scanCode, quint32 keysym, bool down)
{
    if (!this->_connected)
        return;

    // Use QEMU extended key encoding if available (matches C# keyScanEvent)
    if (this->_useQemuExtKeyEncoding)
    {
        // QEMU extended key event format (from C# WriteQemuExtKey)
        writeU8(QEMU_MSG);           // Message type 255
        writeU8(QEMU_EXT_KEY_EVENT); // Subtype 0 = extended key
        writeU8(0);                  // Padding
        writeU8(down ? 1 : 0);       // Down flag
        writeU32(keysym);            // Keysym (-1 if not available)
        writeU32(scanCode);          // Scan code
    } else
    {
        // Legacy scan code event (from C# WriteKey with KEY_SCAN_EVENT)
        writeU8(KEY_SCAN_EVENT); // Message type 254
        writeU8(down ? 1 : 0);   // Down flag
        writeU16(0);             // Padding
        writeU32(scanCode);      // Scan code
    }

    this->_vncStream->flush();
}

void VNCGraphicsClient::sendPointerEvent(quint8 buttonMask, quint16 x, quint16 y)
{
    if (!this->_connected)
        return;

    writeU8(5);          // Message type
    writeU8(buttonMask); // Button mask
    writeU16(x);         // X position
    writeU16(y);         // Y position

    this->_vncStream->flush();
}

void VNCGraphicsClient::sendClientCutText(const QString& text)
{
    if (!this->_connected)
        return;

    QByteArray utf8Text = text.toUtf8();

    writeU8(6); // Message type
    writeU8(0); // Padding
    writeU8(0);
    writeU8(0);
    writeU32(utf8Text.length()); // Length

    this->_vncStream->write(utf8Text);
    this->_vncStream->flush();
}

//=============================================================================
// Rendering (matches C# Damage, OnPaint, etc.)
//=============================================================================

void VNCGraphicsClient::damage(int x, int y, int width, int height)
{
    // Matches C# Damage(int x, int y, int width, int height)
    QRect r(x, y, width, height);

    if (this->_scaling)
    {
        r.adjust(-this->_bump, -this->_bump, this->_bump, this->_bump); // Inflate for scaling fix
    }

    if (this->_damage.isEmpty())
    {
        this->_damage = r;
    } else
    {
        this->_damage = this->_damage.united(r);
    }
}

void VNCGraphicsClient::renderDamage()
{
    // Trigger repaint (matches C# Invalidate)
    if (!this->_damage.isEmpty())
    {
        update();
        this->_damage = QRect();
    }
}

bool VNCGraphicsClient::event(QEvent* event)
{
    // Intercept Tab and Shift+Tab before Qt uses them for focus navigation
    // This ensures Tab key is sent to the VNC server instead of changing focus
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab)
        {
            if (this->_connected)
            {
                // Handle it as a regular key event
                if (event->type() == QEvent::KeyPress)
                    keyPressEvent(keyEvent);
                else
                    keyReleaseEvent(keyEvent);
                return true; // Event handled, don't let Qt do focus navigation
            }
        }
    }

    return QWidget::event(event);
}

void VNCGraphicsClient::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    setupGraphicsOptions(painter);

    if (this->_backBuffer.isNull() || !this->_backBufferInteresting)
    {
        // No content yet - just draw black background
        // C#: base.OnPaintBackground(e) - does NOT draw "Connecting..." text
        painter.fillRect(rect(), Qt::black);
        return;
    }

    // Lock back buffer and render (matches C# lock(_backBuffer))
    QMutexLocker locker(&this->_backBufferMutex);

    if (this->_scaling)
    {
        // Scale to fit with aspect ratio preservation (matches C# scaling logic)
        // Calculate scaled size accounting for border padding if enabled
        int effectiveWidth = this->_displayBorder ? this->_fbWidth + BORDER_PADDING * 3 : this->_fbWidth;
        int effectiveHeight = this->_displayBorder ? this->_fbHeight + BORDER_PADDING * 3 : this->_fbHeight;

        float xScale = (float) width() / effectiveWidth;
        float yScale = (float) height() / effectiveHeight;
        float scale = qMin(xScale, yScale);
        scale = qMax(scale, 0.01f); // Prevent division by zero

        int scaledWidth = (int) (this->_fbWidth * scale);
        int scaledHeight = (int) (this->_fbHeight * scale);
        int offsetX = (width() - scaledWidth) / 2;
        int offsetY = (height() - scaledHeight) / 2;

        QRect targetRect(offsetX, offsetY, scaledWidth, scaledHeight);

        // Draw scaled image with smooth transform for better quality
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(targetRect, this->_backBuffer);

        // Draw surrounding black bars to avoid artifacts
        // Left bar
        if (offsetX > 0)
            painter.fillRect(0, 0, offsetX, height(), Qt::black);
        // Right bar
        int rightX = offsetX + scaledWidth;
        if (rightX < width())
            painter.fillRect(rightX, 0, width() - rightX, height(), Qt::black);
        // Top bar
        if (offsetY > 0)
            painter.fillRect(0, 0, width(), offsetY, Qt::black);
        // Bottom bar
        int bottomY = offsetY + scaledHeight;
        if (bottomY < height())
            painter.fillRect(0, bottomY, width(), height() - bottomY, Qt::black);

        // Draw border if enabled
        if (this->_displayBorder)
        {
            drawBorder(painter, targetRect);
        }
    } else
    {
        // 1:1 pixel mapping - but still CENTER the image (matches C# behavior)
        // C#: _dx = ((float)Size.Width - DesktopSize.Width) / 2;
        //     _dy = ((float)Size.Height - DesktopSize.Height) / 2;
        int offsetX = qMax(0, (width() - this->_fbWidth) / 2);
        int offsetY = qMax(0, (height() - this->_fbHeight) / 2);

        // Draw black background for areas not covered by console
        if (offsetX > 0 || offsetY > 0 ||
            this->_fbWidth < width() || this->_fbHeight < height())
        {
            painter.fillRect(rect(), Qt::black);
        }

        painter.drawImage(offsetX, offsetY, this->_backBuffer);

        // Draw border if enabled
        if (this->_displayBorder)
        {
            QRect consoleRect(offsetX, offsetY, this->_fbWidth, this->_fbHeight);
            drawBorder(painter, consoleRect);
        }
    }
}

void VNCGraphicsClient::drawBorder(QPainter& painter, const QRect& consoleRect)
{
    // Matches C# border rendering - draw around the console area
    // In scaled mode, this highlights the actual console area
    // The border is drawn AROUND the console rect
    QRect borderRect = consoleRect.adjusted(-BORDER_PADDING, -BORDER_PADDING,
                                            BORDER_PADDING, BORDER_PADDING);

    // Use highlight color when focused, gray otherwise
    QColor borderColor = hasFocus() ? QApplication::palette().color(QPalette::Highlight) : Qt::gray;
    painter.setPen(QPen(borderColor, BORDER_WIDTH));
    painter.drawRect(borderRect);
}

void VNCGraphicsClient::setupGraphicsOptions(QPainter& painter)
{
    // Matches C# SetupGraphicsOptions
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void VNCGraphicsClient::updateScale()
{
    // Matches C# SetupScaling calculation
    if (this->_backBuffer.isNull())
        return;

    if (this->_scaling)
    {
        // Calculate scale accounting for border padding if enabled
        int effectiveWidth = this->_displayBorder ? this->_fbWidth + BORDER_PADDING * 3 : this->_fbWidth;
        int effectiveHeight = this->_displayBorder ? this->_fbHeight + BORDER_PADDING * 3 : this->_fbHeight;

        float xScale = (float) width() / effectiveWidth;
        float yScale = (float) height() / effectiveHeight;
        this->_scale = qMin(xScale, yScale);
        this->_scale = qMax(this->_scale, 0.01f); // Prevent division by zero

        this->_dx = (width() - this->_fbWidth * this->_scale) / 2.0f;
        this->_dy = (height() - this->_fbHeight * this->_scale) / 2.0f;

        this->_bump = (int) qCeil(1.0f / this->_scale); // For damage inflation
    } else
    {
        this->_scale = 1.0f;
        this->_bump = 0;

        // Even in non-scaling mode, center the image if it's smaller than the widget
        // This matches C# behavior
        if (width() >= this->_fbWidth)
        {
            this->_dx = (width() - this->_fbWidth) / 2.0f;
        } else
        {
            this->_dx = this->_displayBorder ? BORDER_PADDING : 0;
        }

        if (height() >= this->_fbHeight)
        {
            this->_dy = (height() - this->_fbHeight) / 2.0f;
        } else
        {
            this->_dy = this->_displayBorder ? BORDER_PADDING : 0;
        }
    }
}

void VNCGraphicsClient::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    updateScale();
    update();
}

//=============================================================================
// Input Handling (matches C# mouse/keyboard events)
//=============================================================================

QPoint VNCGraphicsClient::translateMouseCoords(const QPoint& pos)
{
    if (this->_backBuffer.isNull())
        return QPoint(0, 0);

    if (this->_scaling)
    {
        int x = (int) ((pos.x() - this->_dx) / this->_scale);
        int y = (int) ((pos.y() - this->_dy) / this->_scale);
        return QPoint(qBound(0, x, this->_fbWidth - 1),
                      qBound(0, y, this->_fbHeight - 1));
    } else
    {
        // Even in non-scaling mode, account for centering offset
        int x = pos.x() - (int) this->_dx;
        int y = pos.y() - (int) this->_dy;
        return QPoint(qBound(0, x, this->_fbWidth - 1),
                      qBound(0, y, this->_fbHeight - 1));
    }
}

void VNCGraphicsClient::mousePressEvent(QMouseEvent* event)
{
    if (!this->_connected)
        return;

    int buttons = 0;
    if (event->buttons() & Qt::LeftButton)
        buttons |= 0x01;
    if (event->buttons() & Qt::MiddleButton)
        buttons |= 0x02;
    if (event->buttons() & Qt::RightButton)
        buttons |= 0x04;

    QPoint fbPos = translateMouseCoords(event->pos());
    sendPointerEvent(buttons, fbPos.x(), fbPos.y());
    this->_currentMouseState = buttons;
}

void VNCGraphicsClient::mouseReleaseEvent(QMouseEvent* event)
{
    if (!this->_connected)
        return;

    int buttons = 0;
    if (event->buttons() & Qt::LeftButton)
        buttons |= 0x01;
    if (event->buttons() & Qt::MiddleButton)
        buttons |= 0x02;
    if (event->buttons() & Qt::RightButton)
        buttons |= 0x04;

    QPoint fbPos = translateMouseCoords(event->pos());
    sendPointerEvent(buttons, fbPos.x(), fbPos.y());
    this->_currentMouseState = buttons;
}

void VNCGraphicsClient::mouseMoveEvent(QMouseEvent* event)
{
    if (!this->_connected)
        return;

    // Implement mouse throttling (matches C# MOUSE_EVENTS_BEFORE_UPDATE logic)
    this->_mouseMoved++;
    if (this->_mouseMoved > MOUSE_EVENTS_BEFORE_UPDATE && this->_mouseNotMoved < MOUSE_EVENTS_DROPPED)
    {
        this->_mouseNotMoved++;
        return; // Drop this event
    }

    this->_mouseMoved = 0;
    this->_mouseNotMoved = 0;

    QPoint fbPos = translateMouseCoords(event->pos());
    sendPointerEvent(this->_currentMouseState, fbPos.x(), fbPos.y());
}

void VNCGraphicsClient::keyPressEvent(QKeyEvent* event)
{
    if (!this->_connected)
        return;

    bool isRepeat = event->isAutoRepeat();
    Qt::Key mappedKey = this->remapKey((Qt::Key) event->key());

    // Let key handler process shortcuts first (matches C# ConsoleKeyHandler integration)
    if (!isRepeat && this->_keyHandler && this->_keyHandler->handleKeyEvent(mappedKey, true))
        return;

    if (this->_sendScanCodes)
    {
        sendScanCodes(mappedKey, true);
    } else
    {
        // Get the keysym by trying text first, then fallback to key mapping
        // This matches C# KeyMap.translateKey() behavior
        quint32 keysym = qtKeyToKeysymWithModifiers(mappedKey, event->modifiers(), event->text());

        if (keysym > 0)
        {
            sendKeyEvent(keysym, true);
        }
    }

    if (!isRepeat)
        this->_pressedKeys.insert(mappedKey);
}

void VNCGraphicsClient::keyReleaseEvent(QKeyEvent* event)
{
    if (!this->_connected || event->isAutoRepeat())
        return;

    Qt::Key mappedKey = this->remapKey((Qt::Key) event->key());

    if (this->_keyHandler && this->_keyHandler->handleKeyEvent(mappedKey, false))
        return;

    if (this->_sendScanCodes)
    {
        sendScanCodes(mappedKey, false);
    } else
    {
        // Get the keysym by trying text first, then fallback to key mapping
        // This matches C# KeyMap.translateKey() behavior
        quint32 keysym = qtKeyToKeysymWithModifiers(mappedKey, event->modifiers(), event->text());

        if (keysym > 0)
        {
            sendKeyEvent(keysym, false);
        }
    }

    this->_pressedKeys.remove(mappedKey);
}

void VNCGraphicsClient::focusInEvent(QFocusEvent* event)
{
    Q_UNUSED(event);

    // Update clipboard if needed (matches C# focus handling)
    if (this->_updateClipboardOnFocus && redirectingClipboard())
    {
        setConsoleClipboard();
    }
}

void VNCGraphicsClient::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    // Could release all keys here to prevent stuck keys
}

//=============================================================================
// Clipboard (matches C# clipboard handling)
//=============================================================================

bool VNCGraphicsClient::redirectingClipboard()
{
    // TODO: Get from settings (matches C# Properties.Settings.Default.ClipboardAndPrinterRedirection)
    return true;
}

void VNCGraphicsClient::onClipboardChanged()
{
    // Matches C# ClipboardChanged event handler
    if (!redirectingClipboard() || !this->_connected)
        return;

    try
    {
        if (!this->_handlingChange)
        {
            if (hasFocus())
            {
                setConsoleClipboard();
            } else
            {
                this->_updateClipboardOnFocus = true;
            }
        }
    } catch (...)
    {
        qWarning() << "VNCGraphicsClient: Clipboard error";
    }
}

void VNCGraphicsClient::setConsoleClipboard()
{
    try
    {
        this->_handlingChange = true;

        QString text = QApplication::clipboard()->text();

        // Convert line endings for text console (matches C# TalkingToVNCTerm logic)
        if (this->_useSource && !this->_sendScanCodes)
        {
            text = text.replace("\r\n", "\n");
        }

        sendClientCutText(text);
        this->_updateClipboardOnFocus = false;
    } catch (...)
    {
        qWarning() << "VNCGraphicsClient: Failed to set console clipboard";
    }

    this->_handlingChange = false;
}

//=============================================================================
// Key Translation (matches C# key mapping)
//=============================================================================

/**
 * @brief Convert Qt key to X11 keysym with proper modifier handling
 * This matches C# KeyMap.translateKey() behavior which uses Win32.ToUnicode
 * to respect keyboard state (Shift, Caps Lock, etc.)
 *
 * @param key Qt::Key code
 * @param modifiers Keyboard modifiers (Shift, Control, Alt, etc.)
 * @param text Text generated by the key event (respects keyboard layout)
 * @return X11 keysym value
 */
quint32 VNCGraphicsClient::qtKeyToKeysymWithModifiers(Qt::Key key, Qt::KeyboardModifiers modifiers, const QString& text)
{
    Q_UNUSED(modifiers); // Reserved for future use (e.g., detecting Alt+key combinations)

    // First, try to use the text if it's a printable character
    // This handles keyboard layout, Shift, and Caps Lock automatically
    if (!text.isEmpty())
    {
        QChar c = text.at(0);
        ushort unicode = c.unicode();

        // Handle printable ASCII characters (including space)
        if (unicode >= 0x20 && unicode <= 0x7E)
        {
            return unicode;
        }

        // Handle special control characters FIRST (before letter conversion)
        // These have their own keysyms and should not be converted to letters
        if (unicode == '\r' || unicode == '\n') // 0x0D, 0x0A
            return 0xFF0D;                      // Return
        if (unicode == '\t')                    // 0x09
            return 0xFF09;                      // Tab
        if (unicode == '\b' || unicode == 0x7F) // 0x08 or DEL
            return 0xFF08;                      // Backspace
        if (unicode == 0x1B)                    // Escape
            return 0xFF1B;                      // Escape

        // Handle Ctrl+letter combinations (Ctrl+A through Ctrl+Z produce ASCII 0x01-0x1A)
        // Convert back to lowercase letters for VNC keysym (matches C# UnicodeOfKey behavior)
        // VNC expects the letter keysym - the server combines it with the Ctrl modifier
        // Note: We already handled Tab (0x09), Backspace (0x08), Return (0x0D) above
        if (unicode >= 0x01 && unicode <= 0x1A)
        {
            // 0x01 (Ctrl+A) -> 0x61 ('a'), 0x03 (Ctrl+C) -> 0x63 ('c'), etc.
            return unicode + 0x60; // Convert to lowercase letter keysym
        }

        // Handle other special control characters
        if (unicode == 0x1C) // Ctrl+Backslash
            return '\\';
        if (unicode == 0x1D) // Ctrl+]
            return ']';
        if (unicode == 0x1E) // Ctrl+^
            return '^';
        if (unicode == 0x1F) // Ctrl+_
            return '_';
        if (unicode == 0x00) // Ctrl+Space or Ctrl+@
            return ' ';

        // For other Unicode characters, return the unicode value
        if (unicode > 0)
            return unicode;
    }

    // For keys without text (function keys, arrows, etc.), use key code mapping
    return qtKeyToKeysym(key);
}

quint32 VNCGraphicsClient::qtKeyToKeysym(Qt::Key key)
{
    // Basic X11 keysym mapping (matches C# keysym conversion)
    // Full mapping would be much larger - this is a subset

    switch (key)
    {
    // Function keys
    case Qt::Key_F1:
        return 0xFFBE;
    case Qt::Key_F2:
        return 0xFFBF;
    case Qt::Key_F3:
        return 0xFFC0;
    case Qt::Key_F4:
        return 0xFFC1;
    case Qt::Key_F5:
        return 0xFFC2;
    case Qt::Key_F6:
        return 0xFFC3;
    case Qt::Key_F7:
        return 0xFFC4;
    case Qt::Key_F8:
        return 0xFFC5;
    case Qt::Key_F9:
        return 0xFFC6;
    case Qt::Key_F10:
        return 0xFFC7;
    case Qt::Key_F11:
        return 0xFFC8;
    case Qt::Key_F12:
        return 0xFFC9;

    // Modifier keys
    case Qt::Key_Shift:
        return 0xFFE1;
    case Qt::Key_Control:
        return 0xFFE3;
    case Qt::Key_Alt:
        return 0xFFE9;
    case Qt::Key_Meta:
        return 0xFFEB;

    // Special keys
    case Qt::Key_Escape:
        return 0xFF1B;
    case Qt::Key_Tab:
        return 0xFF09;
    case Qt::Key_Backtab:
        return 0xFE20;
    case Qt::Key_Backspace:
        return 0xFF08;
    case Qt::Key_Return:
        return 0xFF0D;
    case Qt::Key_Enter:
        return 0xFF8D;
    case Qt::Key_Insert:
        return 0xFF63;
    case Qt::Key_Delete:
        return 0xFFFF;
    case Qt::Key_Pause:
        return 0xFF13;
    case Qt::Key_Print:
        return 0xFF61;
    case Qt::Key_Home:
        return 0xFF50;
    case Qt::Key_End:
        return 0xFF57;
    case Qt::Key_Left:
        return 0xFF51;
    case Qt::Key_Up:
        return 0xFF52;
    case Qt::Key_Right:
        return 0xFF53;
    case Qt::Key_Down:
        return 0xFF54;
    case Qt::Key_PageUp:
        return 0xFF55;
    case Qt::Key_PageDown:
        return 0xFF56;

    default:
        // For ASCII characters, keysym == ASCII code
        if (key >= 0x20 && key <= 0x7E)
            return key;

        // For other keys, use the Qt key code (may not be correct)
        return key;
    }
}

quint32 VNCGraphicsClient::qtKeyToScanCode(Qt::Key key)
{
    // AT keyboard scan codes (Set 1) - matches Windows virtual key scan codes
    // Reference: https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_1
    // These map to the scan codes sent by Windows InterceptKeys hook in C# XenAdmin

    switch (key)
    {
    // Modifier keys
    case Qt::Key_Control:
        return ConsoleKeyHandler::CTRL_SCAN; // 29
    case Qt::Key_Alt:
        return ConsoleKeyHandler::ALT_SCAN; // 56
    case Qt::Key_Shift:
        return ConsoleKeyHandler::L_SHIFT_SCAN; // 42
    case Qt::Key_Meta:
        return 91 + 128; // Windows/Super key (extended)

    // Row 1: Function keys
    case Qt::Key_Escape:
        return 1;
    case Qt::Key_F1:
        return 59;
    case Qt::Key_F2:
        return 60;
    case Qt::Key_F3:
        return 61;
    case Qt::Key_F4:
        return 62;
    case Qt::Key_F5:
        return 63;
    case Qt::Key_F6:
        return 64;
    case Qt::Key_F7:
        return 65;
    case Qt::Key_F8:
        return 66;
    case Qt::Key_F9:
        return 67;
    case Qt::Key_F10:
        return 68;
    case Qt::Key_F11:
        return ConsoleKeyHandler::F11_SCAN; // 87
    case Qt::Key_F12:
        return ConsoleKeyHandler::F12_SCAN; // 88

    // Row 2: Number row
    case Qt::Key_QuoteLeft:
        return 41; // ` and ~
    case Qt::Key_1:
        return 2;
    case Qt::Key_2:
        return 3;
    case Qt::Key_3:
        return 4;
    case Qt::Key_4:
        return 5;
    case Qt::Key_5:
        return 6;
    case Qt::Key_6:
        return 7;
    case Qt::Key_7:
        return 8;
    case Qt::Key_8:
        return 9;
    case Qt::Key_9:
        return 10;
    case Qt::Key_0:
        return 11;
    case Qt::Key_Minus:
        return 12;
    case Qt::Key_Equal:
        return 13;
    case Qt::Key_Backspace:
        return 14;

    // Row 3: QWERTY row
    case Qt::Key_Tab:
        return 15;
    case Qt::Key_Q:
        return 16;
    case Qt::Key_W:
        return 17;
    case Qt::Key_E:
        return 18;
    case Qt::Key_R:
        return 19;
    case Qt::Key_T:
        return 20;
    case Qt::Key_Y:
        return 21;
    case Qt::Key_U:
        return ConsoleKeyHandler::U_SCAN; // 22
    case Qt::Key_I:
        return 23;
    case Qt::Key_O:
        return 24;
    case Qt::Key_P:
        return 25;
    case Qt::Key_BracketLeft:
        return 26;
    case Qt::Key_BracketRight:
        return 27;
    case Qt::Key_Backslash:
        return 43;

    // Row 4: ASDF row
    case Qt::Key_CapsLock:
        return 58;
    case Qt::Key_A:
        return 30;
    case Qt::Key_S:
        return 31;
    case Qt::Key_D:
        return 32;
    case Qt::Key_F:
        return ConsoleKeyHandler::F_SCAN; // 33
    case Qt::Key_G:
        return 34;
    case Qt::Key_H:
        return 35;
    case Qt::Key_J:
        return 36;
    case Qt::Key_K:
        return 37;
    case Qt::Key_L:
        return 38;
    case Qt::Key_Semicolon:
        return 39;
    case Qt::Key_Apostrophe:
        return 40;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return ConsoleKeyHandler::ENTER_SCAN; // 28

    // Row 5: ZXCV row
    case Qt::Key_Z:
        return 44;
    case Qt::Key_X:
        return 45;
    case Qt::Key_C:
        return 46;
    case Qt::Key_V:
        return 47;
    case Qt::Key_B:
        return 48;
    case Qt::Key_N:
        return 49;
    case Qt::Key_M:
        return 50;
    case Qt::Key_Comma:
        return 51;
    case Qt::Key_Period:
        return 52;
    case Qt::Key_Slash:
        return 53;

    // Row 6: Bottom row
    case Qt::Key_Space:
        return 57;

    // Navigation cluster (extended keys - add 128)
    case Qt::Key_Insert:
        return ConsoleKeyHandler::INS_SCAN; // 82 + 128 = 210
    case Qt::Key_Delete:
        return ConsoleKeyHandler::DEL_SCAN; // 83 + 128 = 211
    case Qt::Key_Home:
        return 71 + 128;
    case Qt::Key_End:
        return 79 + 128;
    case Qt::Key_PageUp:
        return 73 + 128;
    case Qt::Key_PageDown:
        return 81 + 128;

    // Arrow keys (extended)
    case Qt::Key_Up:
        return 72 + 128;
    case Qt::Key_Down:
        return 80 + 128;
    case Qt::Key_Left:
        return 75 + 128;
    case Qt::Key_Right:
        return 77 + 128;

    // Other special keys
    case Qt::Key_Print:
        return 55 + 128;
    case Qt::Key_ScrollLock:
        return 70;
    case Qt::Key_Pause:
        return 69; // NumLock shares this, but Pause is not extended
    case Qt::Key_NumLock:
        return 69;

    default:
        // For unmapped keys, return 0 to fall back to keysym mode
        return 0;
    }
}

void VNCGraphicsClient::sendScanCodes(Qt::Key key, bool down)
{
    // Matches C# SendScanCodes behavior - uses scan codes for low-level key events
    quint32 scanCode = qtKeyToScanCode(key);

    if (scanCode != 0)
    {
        if (down)
        {
            this->_pressedScans.insert(scanCode);
        } else
        {
            this->_pressedScans.remove(scanCode);
        }

        // Get keysym for the key as well (used in QEMU extended encoding)
        quint32 keysym = qtKeyToKeysym(key);

        // Send using scan code event (message type 254 or QEMU extended)
        sendScanCodeEvent(scanCode, keysym, down);
    } else
    {
        // Fall back to regular keysym for unmapped keys
        quint32 keysym = qtKeyToKeysym(key);
        if (keysym > 0)
        {
            sendKeyEvent(keysym, down);
        }
    }
}

int VNCGraphicsClient::bytesPerPixel() const
{
    int bpp = this->_pixelFormat.bitsPerPixel / 8;
    if (bpp <= 0)
        bpp = 1;
    return bpp;
}

QRgb VNCGraphicsClient::decodePixel(const uchar* data) const
{
    const int bpp = bytesPerPixel();
    quint32 value = 0;

    switch (bpp)
    {
    case 1:
        value = data[0];
        break;
    case 2:
        value = this->_pixelFormat.bigEndian
                    ? qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data))
                    : qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data));
        break;
    case 3:
        if (this->_pixelFormat.bigEndian)
            value = (quint32(data[0]) << 16) | (quint32(data[1]) << 8) | quint32(data[2]);
        else
            value = quint32(data[0]) | (quint32(data[1]) << 8) | (quint32(data[2]) << 16);
        break;
    default:
        value = this->_pixelFormat.bigEndian
                    ? qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(data))
                    : qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data));
        break;
    }

    auto scaleComponent = [](quint32 component, quint32 maxVal) -> quint8 {
        if (maxVal == 0)
            return 0;
        if (maxVal == 255)
            return static_cast<quint8>(component);
        return static_cast<quint8>((component * 255) / maxVal);
    };

    if (this->_pixelFormat.trueColor)
    {
        quint32 r = (value >> this->_pixelFormat.redShift) & this->_pixelFormat.redMax;
        quint32 g = (value >> this->_pixelFormat.greenShift) & this->_pixelFormat.greenMax;
        quint32 b = (value >> this->_pixelFormat.blueShift) & this->_pixelFormat.blueMax;
        return qRgb(scaleComponent(r, this->_pixelFormat.redMax),
                    scaleComponent(g, this->_pixelFormat.greenMax),
                    scaleComponent(b, this->_pixelFormat.blueMax));
    }

    // Fallback for non true-color formats (approximate as grayscale)
    quint8 gray = static_cast<quint8>(value & 0xFF);
    return qRgb(gray, gray, gray);
}

//=============================================================================
// Network Helpers
//=============================================================================

quint8 VNCGraphicsClient::readU8()
{
    quint8 value = (quint8) this->_readBuffer.at(0);
    this->_readBuffer.remove(0, 1);
    return value;
}

quint16 VNCGraphicsClient::readU16()
{
    quint16 value = qFromBigEndian<quint16>((const uchar*) this->_readBuffer.constData());
    this->_readBuffer.remove(0, 2);
    return value;
}

quint32 VNCGraphicsClient::readU32()
{
    quint32 value = qFromBigEndian<quint32>((const uchar*) this->_readBuffer.constData());
    this->_readBuffer.remove(0, 4);
    return value;
}

void VNCGraphicsClient::writeU8(quint8 value)
{
    this->_vncStream->write((const char*) &value, 1);
}

void VNCGraphicsClient::writeU16(quint16 value)
{
    quint16 bigEndian = qToBigEndian(value);
    this->_vncStream->write((const char*) &bigEndian, 2);
}

void VNCGraphicsClient::writeU32(quint32 value)
{
    quint32 bigEndian = qToBigEndian(value);
    this->_vncStream->write((const char*) &bigEndian, 4);
}

QByteArray VNCGraphicsClient::readBytes(int count)
{
    QByteArray data = this->_readBuffer.left(count);
    this->_readBuffer.remove(0, count);
    return data;
}
