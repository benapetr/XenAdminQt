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
#include "../settingsmanager.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QtEndian>
#include <QtMath>
#include <QDebug>
#include <algorithm>
#include <climits>

// Static member initialization
bool VNCGraphicsClient::m_handlingChange = false;

VNCGraphicsClient::VNCGraphicsClient(QWidget* parent) : QWidget(parent)
{
    // NOTE: Don't enable mouse tracking or set focus policy here!
    // These will be enabled when we successfully connect (see connectToHost)
    // Setting them in constructor causes UI freeze when widget exists but isn't connected
    this->setAttribute(Qt::WA_OpaquePaintEvent);

    // Setup control styles (matches C# SetStyle calls)
    this->setAttribute(Qt::WA_NoSystemBackground, false); // Opaque = false in C#

    // Initialize back buffer (matches C# constructor)
    this->m_backBuffer = QImage(640, 480, QImage::Format_RGB32);
    this->m_backBuffer.fill(this->palette().color(QPalette::Window));

    // Periodic framebuffer update requests
    this->m_updateTimer = new QTimer(this);
    this->m_updateTimer->setInterval(40); // 25 FPS
    QObject::connect(this->m_updateTimer, &QTimer::timeout, this, &VNCGraphicsClient::requestFramebufferUpdate);

    // Clipboard synchronization
    QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &VNCGraphicsClient::onClipboardChanged);

    qDebug() << "VNCGraphicsClient: Initialized with 640x480 backbuffer";
}

VNCGraphicsClient::~VNCGraphicsClient()
{
    // Disconnect clipboard (matches C# Dispose)
    this->disconnect(QApplication::clipboard(), &QClipboard::dataChanged, this, &VNCGraphicsClient::onClipboardChanged);

    this->DisconnectAndDispose();

    // Cleanup back buffer (matches C# lock(_backBuffer) block in Dispose)
    QMutexLocker locker(&this->m_backBufferMutex);
    // _backBuffer is automatically cleaned up by QImage destructor
}

//=============================================================================
// IRemoteConsole Interface Implementation
//=============================================================================

ConsoleKeyHandler* VNCGraphicsClient::KeyHandler() const
{
    return this->m_keyHandler;
}

void VNCGraphicsClient::SetKeyHandler(ConsoleKeyHandler* handler)
{
    this->m_keyHandler = handler;
}

void VNCGraphicsClient::Activate()
{
    // Only grab focus if we're actually connected
    if (this->m_connected && this->m_state == Normal)
    {
        this->setFocus();
        this->raise();
    }
}

void VNCGraphicsClient::DisconnectAndDispose()
{
    // Matches C# Disconnect() method
    this->m_connected = false;

    // C# disposes _remoteCursor here: a new connection starts with the local cursor
    this->m_hasRemoteCursor = false;
    if (this->m_cursorOver)
        this->unsetCursor();
    this->m_cursorOver = false;
    this->m_terminated = true;

    // Disable mouse tracking and focus when disconnecting
    this->setMouseTracking(false);
    this->setFocusPolicy(Qt::NoFocus);

    if (this->m_vncStream)
    {
        disconnect(this->m_vncStream, nullptr, this, nullptr);
        this->m_vncStream->close();

        // Transfer ownership and schedule deletion
        QTcpSocket* stream = this->m_vncStream;
        this->m_vncStream = nullptr;
        stream->deleteLater();
    }

    this->m_updateTimer->stop();

    {
        QMutexLocker locker(&this->m_backBufferMutex);
        this->m_backBuffer.fill(Qt::black);
        this->m_backBufferInteresting = false;
        this->m_damage = QRect();
    }

    this->update();
}

void VNCGraphicsClient::Pause()
{
    // Matches C# Pause() in IRemoteConsole
    this->m_helperIsPaused = true;
    this->m_updateTimer->stop();
}

void VNCGraphicsClient::Unpause()
{
    // Matches C# UnPause() in IRemoteConsole
    this->m_helperIsPaused = false;
    if (this->m_connected && this->m_state == Normal)
    {
        this->m_updateTimer->start();
    }
    this->update();
}

void VNCGraphicsClient::SendCAD()
{
    // Matches C# SendCAD() method
    if (!this->m_connected)
        return;

    // qDebug() << "VNCGraphicsClient: Sending Ctrl+Alt+Delete";

    // Send Ctrl down, Alt down, Delete down, Delete up, Alt up, Ctrl up
    // X11 Keysyms: XK_Control_L = 0xffe3, XK_Alt_L = 0xffe9, XK_Delete = 0xffff
    this->sendKeyEvent(0xFFE3, true);  // Left Control down
    this->sendKeyEvent(0xFFE9, true);  // Left Alt down
    this->sendKeyEvent(0xFFFF, true);  // Delete down
    this->sendKeyEvent(0xFFFF, false); // Delete up
    this->sendKeyEvent(0xFFE9, false); // Left Alt up
    this->sendKeyEvent(0xFFE3, false); // Left Control up
}

void VNCGraphicsClient::SendFunctionKeyWithModifiers(bool ctrl, bool alt, int functionNumber)
{
    if (!this->m_connected || functionNumber < 1 || functionNumber > 12)
        return;

    // X11 keysyms: F1..F12 are contiguous from 0xFFBE.
    const quint32 functionKeysym = 0xFFBE + static_cast<quint32>(functionNumber - 1);

    if (ctrl)
        this->sendKeyEvent(0xFFE3, true); // Left Control down
    if (alt)
        this->sendKeyEvent(0xFFE9, true); // Left Alt down

    this->sendKeyEvent(functionKeysym, true);
    this->sendKeyEvent(functionKeysym, false);

    if (alt)
        this->sendKeyEvent(0xFFE9, false); // Left Alt up
    if (ctrl)
        this->sendKeyEvent(0xFFE3, false); // Left Control up
}

QImage VNCGraphicsClient::Snapshot()
{
    // Matches C# Snapshot() method
    QMutexLocker locker(&this->m_backBufferMutex);
    return this->m_backBuffer.copy();
}

void VNCGraphicsClient::SetSendScanCodes(bool value)
{
    this->m_sendScanCodes = value;
}

bool VNCGraphicsClient::IsScaling() const
{
    return this->m_scaling;
}

void VNCGraphicsClient::SetScaling(bool value)
{
    // Matches C# Scaling property setter
    if (this->m_scaling != value)
    {
        this->m_scaling = value;
        updateScale();
        update();
    }
}

void VNCGraphicsClient::SetDisplayBorder(bool value)
{
    this->m_displayBorder = value;
    update();
}

QSize VNCGraphicsClient::DesktopSize() const
{
    // Matches C# DesktopSize property
    return QSize(this->m_fbWidth, this->m_fbHeight);
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

void VNCGraphicsClient::Connect(QTcpSocket* stream, const QString& password, const QByteArray& initialData)
{
    // Matches C# Connect(Stream stream, char[] password)
    qDebug() << "VNCGraphicsClient: Starting VNC connection";

    // C# checks: if (Connected || Terminated) close and reconnect
    // We should only refuse if already actively connected, not if terminated
    if (this->m_connected && this->m_vncStream)
    {
        qDebug() << "VNCGraphicsClient: Already connected, disconnecting first";
        this->DisconnectAndDispose();
    }

    // Reset all connection state (matches C# starting fresh connection)
    this->m_terminated = false;
    this->m_connected = false; // Will set to true after setup
    this->m_vncStream = stream;
    this->m_vncStream->setParent(this);
    this->m_password = password;
    this->m_state = ProtocolVersion;
    this->m_readBuffer.clear();
    if (!initialData.isEmpty())
        this->m_readBuffer.append(initialData);

    // Clear back buffer (prevents stale imagery from previous session)
    {
        QMutexLocker locker(&this->m_backBufferMutex);
        this->m_backBuffer.fill(Qt::black);
        this->m_backBufferInteresting = false;
        this->m_damage = QRect(); // Reset damage rect to null
    }

    // Force widget to repaint with cleared backbuffer (C# equivalently shows black before first frame)
    this->update();

    // Connect socket signals
    QObject::connect(this->m_vncStream, &QTcpSocket::readyRead, this, &VNCGraphicsClient::onSocketReadyRead);
    QObject::connect(this->m_vncStream, &QTcpSocket::disconnected, this, &VNCGraphicsClient::onSocketDisconnected);
    QObject::connect(this->m_vncStream, &QTcpSocket::errorOccurred, this, &VNCGraphicsClient::onSocketError);

    this->m_connected = true;

    // Process immediately if caller provided pre-read bytes (e.g. bytes read by HTTP CONNECT)
    // or if socket already has new data available.
    if (!this->m_readBuffer.isEmpty() || this->m_vncStream->bytesAvailable() > 0)
    {
        this->onSocketReadyRead();
    }
}

void VNCGraphicsClient::onSocketDisconnected()
{
    qDebug() << "VNCGraphicsClient: Socket disconnected";
    const bool unexpectedDisconnect = !this->m_terminated;
    this->m_connected = false;
    this->m_state = Disconnected;
    this->m_updateTimer->stop();
    this->update();

    if (unexpectedDisconnect)
        emit errorOccurred(this, "Socket disconnected");
}

void VNCGraphicsClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString errorStr = this->m_vncStream ? this->m_vncStream->errorString() : "Unknown error";
    qWarning() << "VNCGraphicsClient: Socket error:" << errorStr;

    this->m_connected = false;
    emit errorOccurred(this, errorStr);
}

//=============================================================================
// Protocol State Machine (matches C# VNCStream handling)
//=============================================================================

void VNCGraphicsClient::onSocketReadyRead()
{
    this->m_readBuffer.append(this->m_vncStream->readAll());

    // Process all available data through state machine
    while (!this->m_readBuffer.isEmpty() && this->m_connected)
    {
        switch (this->m_state)
        {
            case Disconnected:
                return; // Nothing to process when disconnected

            case ProtocolVersion:
                this->handleProtocolVersion();
                break;
            case SecurityHandshake:
                this->handleSecurityHandshake();
                break;
            case SecurityResult:
                this->handleSecurityResult();
                break;
            case Initialization:
                this->handleServerInit();
                break;
            case Normal: {
                // Handle server messages
                if (this->m_readBuffer.size() < 1)
                    return;

                quint8 msgType = (quint8) this->m_readBuffer.at(0);
                // qDebug() << "VNCGraphicsClient: Processing message type:" << msgType << "buffer size:" << this->_readBuffer.size();
                bool processed = false;
                switch (msgType)
                {
                    case 0: // FramebufferUpdate
                        processed = this->handleFramebufferUpdate();
                        break;
                    case 1: // SetColorMapEntries
                        processed = this->handleSetColorMapEntries();
                        break;
                    case 2: // Bell
                        processed = this->handleBell();
                        break;
                    case 3: // ServerCutText
                        processed = this->handleServerCutText();
                        break;
                    default:
                        qWarning() << "VNCGraphicsClient: Unknown message type:" << msgType;
                        qWarning() << "VNCGraphicsClient: Buffer size:" << this->m_readBuffer.size();
                        qWarning() << "VNCGraphicsClient: Next few bytes:"
                                   << ((this->m_readBuffer.size() > 1) ? QString::number((quint8) this->m_readBuffer.at(1), 16) : "N/A")
                                   << ((this->m_readBuffer.size() > 2) ? QString::number((quint8) this->m_readBuffer.at(2), 16) : "N/A")
                                   << ((this->m_readBuffer.size() > 3) ? QString::number((quint8) this->m_readBuffer.at(3), 16) : "N/A");

                        // Unknown message - cannot determine length, so we must disconnect
                        // to avoid protocol desynchronization
                        emit errorOccurred(this, QString("Unknown VNC message type: %1").arg(msgType));
                        this->DisconnectAndDispose();
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
    if (this->m_readBuffer.size() < 12)
        return;

    QString version = QString::fromLatin1(this->m_readBuffer.left(12)).trimmed();

    // Parse server version (e.g., "RFB 003.003" or "RFB 003.008")
    qDebug() << "VNCGraphicsClient: Server version:" << version;

    // Extract major and minor version numbers
    // Format: "RFB MMM.mmm" where MMM is major, mmm is minor
    int majorVersion = 3; // Default
    int minorVersion = 8; // Default to 3.8

    if (version.startsWith("RFB "))
    {
        QStringList parts = version.mid(4).split('.');
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
        this->m_protocolMinorVersion = 3;
        qDebug() << "VNCGraphicsClient: Using RFB 3.3 protocol";
    } else if (minorVersion <= 7)
    {
        // Server supports up to 3.7 - use 3.7
        clientVersion = "RFB 003.007\n";
        this->m_protocolMinorVersion = 7;
        qDebug() << "VNCGraphicsClient: Using RFB 3.7 protocol";
    } else
    {
        // Server supports 3.8 or higher - use 3.8
        clientVersion = "RFB 003.008\n";
        this->m_protocolMinorVersion = 8;
        qDebug() << "VNCGraphicsClient: Using RFB 3.8 protocol";
    }

    this->m_vncStream->write(clientVersion.toLatin1());
    this->m_vncStream->flush();

    this->m_readBuffer.remove(0, 12);
    this->m_state = SecurityHandshake;
}

void VNCGraphicsClient::handleSecurityHandshake()
{
    // RFB 3.3 and 3.7+ use different security handshake formats
    if (this->m_protocolMinorVersion <= 3)
    {
        // RFB 3.3: Server sends 32-bit security type directly
        if (this->m_readBuffer.size() < 4)
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
            this->m_state = Initialization;
            sendClientInit();
        } else if (securityType == 2)
        {
            // VNC Authentication - wait for 16-byte challenge
            qDebug() << "VNCGraphicsClient: VNC authentication required";

            if (this->m_readBuffer.size() < 16)
                return;

            // Read 16-byte challenge
            QByteArray challenge = this->m_readBuffer.left(16);
            this->m_readBuffer.remove(0, 16);

            // TODO: Implement proper DES encryption with password
            // For now, send back the challenge unmodified (will fail but allows testing)
            qWarning() << "VNCGraphicsClient: VNC authentication not fully implemented";
            this->m_vncStream->write(challenge);
            this->m_vncStream->flush();

            this->m_state = SecurityResult;
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
        if (this->m_readBuffer.size() < 1)
            return;

        quint8 securityTypeCount = (quint8) this->m_readBuffer.at(0);

        if (securityTypeCount == 0)
        {
            // Connection failed - read reason string
            this->m_readBuffer.remove(0, 1);
            if (this->m_readBuffer.size() < 4)
                return;

            quint32 reasonLength = readU32();
            if (this->m_readBuffer.size() < (int) reasonLength)
                return;

            QString reason = QString::fromUtf8(this->m_readBuffer.left(reasonLength));
            this->m_readBuffer.remove(0, reasonLength);

            qWarning() << "VNCGraphicsClient: Server rejected connection:" << reason;
            emit errorOccurred(this, "Server rejected: " + reason);
            DisconnectAndDispose();
            return;
        }

        if (this->m_readBuffer.size() < 1 + securityTypeCount)
            return;

        qDebug() << "VNCGraphicsClient: Security types:" << securityTypeCount;

        // Look for security types (1 = None, 2 = VNC Authentication)
        bool foundNone = false;
        bool foundVNC = false;

        for (int i = 0; i < securityTypeCount; i++)
        {
            quint8 secType = (quint8) this->m_readBuffer.at(1 + i);
            qDebug() << "VNCGraphicsClient: Security type:" << secType;
            if (secType == 1)
                foundNone = true;
            if (secType == 2)
                foundVNC = true;
        }

        this->m_readBuffer.remove(0, 1 + securityTypeCount);

        // Choose security type (prefer VNC auth if password provided)
        if (!this->m_password.isEmpty() && foundVNC)
        {
            qDebug() << "VNCGraphicsClient: Using VNC authentication";
            writeU8(2); // VNC Authentication
            this->m_vncStream->flush();

            // Wait for challenge (handled in separate state)
            // For now, skip to SecurityResult (proper DES encryption needed for production)
            qWarning() << "VNCGraphicsClient: VNC authentication not fully implemented - using empty response";

            // Send dummy 16-byte response
            this->m_vncStream->write(QByteArray(16, 0));
            this->m_vncStream->flush();
            this->m_state = SecurityResult;
        } else if (foundNone)
        {
            qDebug() << "VNCGraphicsClient: Using no authentication";
            writeU8(1); // None
            this->m_vncStream->flush();
            this->m_state = SecurityResult;
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
    if (this->m_readBuffer.size() < 4)
        return;

    quint32 result = readU32();
    qDebug() << "VNCGraphicsClient: Security result:" << result;

    if (result != 0)
    {
        // Authentication failed - read reason if available
        QString reason = "Authentication failed";
        if (this->m_readBuffer.size() >= 4)
        {
            quint32 reasonLength = readU32();
            if (this->m_readBuffer.size() >= (int) reasonLength)
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
    this->m_state = Initialization;
}

void VNCGraphicsClient::sendClientInit()
{
    // Matches C# VNCStream initialization
    writeU8(1); // Shared flag (1 = shared desktop)
    this->m_vncStream->flush();
    qDebug() << "VNCGraphicsClient: Sent ClientInit (shared=1)";
}

void VNCGraphicsClient::handleServerInit()
{
    // ServerInit structure:
    // width: U16, height: U16, pixel_format: 16 bytes, name_length: U32, name: string
    if (this->m_readBuffer.size() < 24)
        return;

    this->m_fbWidth = readU16();
    this->m_fbHeight = readU16();

    // Pixel format (16 bytes)
    this->m_pixelFormat.bitsPerPixel = readU8();
    this->m_pixelFormat.depth = readU8();
    this->m_pixelFormat.bigEndian = readU8();
    this->m_pixelFormat.trueColor = readU8();
    this->m_pixelFormat.redMax = readU16();
    this->m_pixelFormat.greenMax = readU16();
    this->m_pixelFormat.blueMax = readU16();
    this->m_pixelFormat.redShift = readU8();
    this->m_pixelFormat.greenShift = readU8();
    this->m_pixelFormat.blueShift = readU8();
    readBytes(3); // padding

    quint32 nameLength = readU32();
    if (this->m_readBuffer.size() < (int) nameLength)
        return;

    this->m_desktopName = QString::fromUtf8(readBytes(nameLength));

    qDebug() << "VNCGraphicsClient: Framebuffer:" << this->m_fbWidth << "x" << this->m_fbHeight;
    qDebug() << "VNCGraphicsClient: Desktop name:" << this->m_desktopName;
    qDebug() << "VNCGraphicsClient: Pixel format:" << this->m_pixelFormat.bitsPerPixel << "bpp";

    // Create/resize framebuffer (matches C# OnDesktopSizeChanged)
    {
        QMutexLocker locker(&this->m_backBufferMutex);
        this->m_backBuffer = QImage(this->m_fbWidth, this->m_fbHeight, QImage::Format_RGB32);
        this->m_backBuffer.fill(Qt::black);
        this->m_backBufferInteresting = false;
    }

    // Decide whether to force RGB32 to match XenCenter behaviour.
    const bool serverTrueColor = this->m_pixelFormat.trueColor != 0;
    const bool serverIsRgb32 =
        serverTrueColor &&
        this->m_pixelFormat.bitsPerPixel == 32 &&
        this->m_pixelFormat.depth >= 24 &&
        this->m_pixelFormat.redMax == 255 &&
        this->m_pixelFormat.greenMax == 255 &&
        this->m_pixelFormat.blueMax == 255 &&
        this->m_pixelFormat.redShift == 16 &&
        this->m_pixelFormat.greenShift == 8 &&
        this->m_pixelFormat.blueShift == 0 &&
        this->m_pixelFormat.bigEndian == 0;

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

    this->m_state = Normal;
    qDebug() << "VNCGraphicsClient: Entered Normal state";
    updateScale();

    // NOW we can enable mouse tracking and focus - we're fully connected!
    this->setMouseTracking(true);
    this->setFocusPolicy(Qt::StrongFocus);

    // Start update timer
    this->m_updateTimer->start();

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

    this->m_vncStream->flush();

    // Update local pixel format to match what we requested
    this->m_pixelFormat.bitsPerPixel = 32;
    this->m_pixelFormat.depth = 24;
    this->m_pixelFormat.bigEndian = 0;
    this->m_pixelFormat.trueColor = 1;
    this->m_pixelFormat.redMax = 255;
    this->m_pixelFormat.greenMax = 255;
    this->m_pixelFormat.blueMax = 255;
    this->m_pixelFormat.redShift = 16;
    this->m_pixelFormat.greenShift = 8;
    this->m_pixelFormat.blueShift = 0;
}

void VNCGraphicsClient::sendSetEncodings()
{
    // SetEncodings message (matches C# VNCStream)
    // Same order as C# VNCStream (preferred first): CoRRE, RRE, CopyRect, Raw, Cursor,
    // DesktopSize, XenCenter, QEMU extended key.
    const qint32 encodings[] = {
        CORRE_ENCODING,
        RRE_ENCODING,
        COPY_RECTANGLE_ENCODING,
        RAW_ENCODING,
        // Server sends the cursor shape and stops drawing it into the picture
        CURSOR_PSEUDO_ENCODING,
        // Lets the server announce resolution changes (e.g. while the guest boots)
        // instead of the console keeping the old size and showing only part of the screen
        DESKTOP_SIZE_PSEUDO_ENCODING,
        // Identifies the client as XenCenter to the server; C# advertises it but never
        // receives rectangles of this type either
        XENCENTER_ENCODING,
    };

    writeU8(2);  // Message type
    writeU8(0);  // Padding
    writeU16(sizeof(encodings) / sizeof(encodings[0])); // Number of encodings
    for (qint32 encoding : encodings)
        writeU32(static_cast<quint32>(encoding));

    this->m_vncStream->flush();
}

void VNCGraphicsClient::sendFramebufferUpdateRequest(bool incremental)
{
    // FramebufferUpdateRequest message
    writeU8(3);                   // Message type
    writeU8(incremental ? 1 : 0); // Incremental flag
    writeU16(0);                  // x
    writeU16(0);                  // y
    writeU16(this->m_fbWidth);           // width
    writeU16(this->m_fbHeight);          // height

    this->m_vncStream->flush();
}

void VNCGraphicsClient::requestFramebufferUpdate()
{
    if (this->m_connected && this->m_state == Normal && !this->m_helperIsPaused)
    {
        // After a desktop size change the whole new screen is needed, not just changes
        const bool incremental = !this->m_forceFullUpdate;
        this->m_forceFullUpdate = false;
        sendFramebufferUpdateRequest(incremental);
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

    // Check if header is available
    if (this->m_readBuffer.size() < 4)
        return false;

    // Peek at header (don't consume yet)
    quint8 msgType = (quint8) this->m_readBuffer.at(offset++); // Should be 0
    quint8 padding = (quint8) this->m_readBuffer.at(offset++);
    quint16 numRects = ((quint8) this->m_readBuffer.at(offset) << 8) | (quint8) this->m_readBuffer.at(offset + 1);
    offset += 2;

    Q_UNUSED(msgType); // Already verified by caller
    Q_UNUSED(padding);

    // qDebug() << "VNCGraphicsClient: FramebufferUpdate with" << numRects << "rectangles";

    // Parse each rectangle to verify all data is present
    for (quint16 i = 0; i < numRects; i++)
    {
        // Check if rectangle header is available
        if (this->m_readBuffer.size() - offset < 12)
        {
            // qDebug() << "VNCGraphicsClient: Waiting for rectangle header" << i << "of" << numRects;
            return false; // Wait for more data
        }

        // Peek at rectangle header
        quint16 peekX = ((quint8) this->m_readBuffer.at(offset) << 8) | (quint8) this->m_readBuffer.at(offset + 1);
        offset += 2;
        quint16 peekY = ((quint8) this->m_readBuffer.at(offset) << 8) | (quint8) this->m_readBuffer.at(offset + 1);
        offset += 2;
        quint16 width = ((quint8) this->m_readBuffer.at(offset) << 8) | (quint8) this->m_readBuffer.at(offset + 1);
        offset += 2;
        quint16 height = ((quint8) this->m_readBuffer.at(offset) << 8) | (quint8) this->m_readBuffer.at(offset + 1);
        offset += 2;

        // Encoding is signed 32-bit
        qint32 encoding = ((quint8) this->m_readBuffer.at(offset) << 24) |
                          ((quint8) this->m_readBuffer.at(offset + 1) << 16) |
                          ((quint8) this->m_readBuffer.at(offset + 2) << 8) |
                          (quint8) this->m_readBuffer.at(offset + 3);
        offset += 4;

        Q_UNUSED(peekX); // Only used for validation
        Q_UNUSED(peekY);

        const int dataSize = rectDataSize(encoding, width, height, offset);
        if (dataSize == -1)
            return false; // Wait for more data
        if (dataSize >= 0)
        {
            if (this->m_readBuffer.size() - offset < dataSize)
                return false; // Wait for more data
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
                QMutexLocker locker(&this->m_backBufferMutex);
                writePixels(x, y, width, height, reinterpret_cast<const uchar*>(pixelData.constData()));
                this->m_backBufferInteresting = true;
            }

            // Record damage (matches C# Damage method)
            damage(x, y, width, height);
        } else if (encoding == COPY_RECTANGLE_ENCODING)
        {
            // C# ReadCopyRectangleEncoding: source position, then copy within the framebuffer
            const quint16 srcX = readU16();
            const quint16 srcY = readU16();
            {
                QMutexLocker locker(&this->m_backBufferMutex);
                copyRect(srcX, srcY, width, height, x, y);
                this->m_backBufferInteresting = true;
            }
            damage(x, y, width, height);
        } else if (encoding == RRE_ENCODING || encoding == CORRE_ENCODING)
        {
            // C# ReadRREEncoding / ReadCoRREEncoding: background, then solid subrectangles
            // (CoRRE uses 8-bit subrectangle coordinates)
            const int bpp = bytesPerPixel();
            const quint32 count = readU32();
            const QByteArray background = readBytes(bpp);
            QMutexLocker locker(&this->m_backBufferMutex);
            fillRect(x, y, width, height, decodePixel(reinterpret_cast<const uchar*>(background.constData())));
            for (quint32 i = 0; i < count; ++i)
            {
                const QByteArray color = readBytes(bpp);
                int sx, sy, sw, sh;
                if (encoding == RRE_ENCODING)
                {
                    sx = readU16();
                    sy = readU16();
                    sw = readU16();
                    sh = readU16();
                } else
                {
                    sx = readU8();
                    sy = readU8();
                    sw = readU8();
                    sh = readU8();
                }
                fillRect(x + sx, y + sy, sw, sh, decodePixel(reinterpret_cast<const uchar*>(color.constData())));
            }
            this->m_backBufferInteresting = true;
            locker.unlock();
            damage(x, y, width, height);
        } else if (encoding == CURSOR_PSEUDO_ENCODING)
        {
            // C# ReadCursorPseudoEncoding: x/y are the hotspot
            const QByteArray pixels = readBytes(width * height * bytesPerPixel());
            const QByteArray mask = readBytes(((width + 7) / 8) * height);
            applyRemoteCursor(x, y, width, height, pixels, mask);
        } else if (encoding == DESKTOP_SIZE_PSEUDO_ENCODING)
        {
            // C# VNCStream: DesktopSize(width, height); _incremental = false;
            applyDesktopSize(width, height);
            this->m_forceFullUpdate = true;
        }
    }

    // Render damage to screen
    renderDamage();
    return true;
}

void VNCGraphicsClient::applyDesktopSize(int width, int height)
{
    // C# Reference: VNCGraphicsClient.ClientDesktopSize
    width = qMax(width, 1);
    height = qMax(height, 1);
    if (width == this->m_fbWidth && height == this->m_fbHeight)
        return;

    qDebug() << "VNCGraphicsClient: Desktop size changed from" << this->m_fbWidth << "x" << this->m_fbHeight
             << "to" << width << "x" << height;

    {
        QMutexLocker locker(&this->m_backBufferMutex);
        this->m_fbWidth = width;
        this->m_fbHeight = height;
        this->m_backBuffer = QImage(width, height, QImage::Format_RGB32);
        this->m_backBuffer.fill(Qt::black);
        this->m_backBufferInteresting = false;
    }

    updateScale();
    emit desktopResized();
    update();
}

void VNCGraphicsClient::applyRemoteCursor(int hotX, int hotY, int width, int height,
                                          const QByteArray& pixels, const QByteArray& mask)
{
    // C# Reference: VNCStream.CreateImage(cursor=true) and VNCGraphicsClient.ClientSetCursor
    if (width <= 0 || height <= 0)
    {
        // An empty cursor: the guest hides its pointer
        this->m_remoteCursor = QCursor(Qt::BlankCursor);
    } else
    {
        const int bpp = bytesPerPixel();
        const int maskStride = (width + 7) / 8;
        QImage image(width, height, QImage::Format_ARGB32);
        for (int row = 0; row < height; ++row)
        {
            QRgb* dst = reinterpret_cast<QRgb*>(image.scanLine(row));
            const uchar* src = reinterpret_cast<const uchar*>(pixels.constData()) + row * width * bpp;
            const uchar* maskRow = reinterpret_cast<const uchar*>(mask.constData()) + row * maskStride;
            for (int col = 0; col < width; ++col, src += bpp)
            {
                // Mask bit set = opaque, most significant bit first
                const bool opaque = (maskRow[col >> 3] >> (7 - (col & 7))) & 1;
                dst[col] = opaque ? (decodePixel(src) | 0xff000000u) : 0u;
            }
        }
        this->m_remoteCursor = QCursor(QPixmap::fromImage(image), qBound(0, hotX, width - 1), qBound(0, hotY, height - 1));
    }
    this->m_hasRemoteCursor = true;

    if (this->m_cursorOver)
        this->setCursor(this->m_remoteCursor);
}

void VNCGraphicsClient::updateCursor(const QPoint& pos)
{
    // C# Reference: VNCGraphicsClient.OnMouseMove - only replace the pointer over the
    // guest screen itself, not over the borders around a scaled or smaller screen
    const QRectF screen(this->m_dx, this->m_dy, this->m_fbWidth * this->m_scale, this->m_fbHeight * this->m_scale);
    const bool over = this->hasFocus() && this->m_connected && screen.contains(pos);
    if (over)
    {
        static const QCursor localCursor(QPixmap(":/icons/vnc_local_cursor.png"), 2, 2);
        this->setCursor(this->m_hasRemoteCursor ? this->m_remoteCursor : localCursor);
    } else if (this->m_cursorOver)
    {
        this->unsetCursor();
    }
    this->m_cursorOver = over;
}

bool VNCGraphicsClient::handleSetColorMapEntries()
{
    // SetColorMapEntries: msg_type: U8, padding: U8, first_color: U16, num_colors: U16, colors...
    if (this->m_readBuffer.size() < 6)
        return false;

    int offset = 0;
    offset += 2; // message type + padding
    quint16 firstColor = qFromBigEndian<quint16>((const uchar*) this->m_readBuffer.constData() + offset);
    offset += 2;
    quint16 numColors = qFromBigEndian<quint16>((const uchar*) this->m_readBuffer.constData() + offset);
    offset += 2;

    // Each color is 6 bytes (3 * U16)
    int colorDataSize = numColors * 6;
    if (this->m_readBuffer.size() - offset < colorDataSize)
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
    if (this->m_readBuffer.size() < 1)
        return false;

    readU8(); // Message type
    qDebug() << "VNCGraphicsClient: Bell received";
    // Could emit a signal here for UI notification
    return true;
}

bool VNCGraphicsClient::handleServerCutText()
{
    // ServerCutText structure: msg_type: U8, padding: 3 bytes, length: U32, text: string
    if (this->m_readBuffer.size() < 8)
        return false;

    quint32 length = qFromBigEndian<quint32>((const uchar*) this->m_readBuffer.constData() + 4);
    if (this->m_readBuffer.size() < 8 + (int) length)
        return false;

    readU8();     // Message type
    readBytes(3); // Padding
    length = readU32();

    QString text = QString::fromLatin1(readBytes(length));
    // Don't log the text itself: it is the guest's clipboard contents
    qDebug() << "VNCGraphicsClient: Server cut text," << text.length() << "characters";

    // Set clipboard (matches C# clipboard handling)
    if (redirectingClipboard())
    {
        this->m_handlingChange = true;
        QApplication::clipboard()->setText(text);
        this->m_handlingChange = false;
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
    if (!this->m_connected)
        return;

    writeU8(KEY_EVENT);    // Message type 4
    writeU8(down ? 1 : 0); // Down flag
    writeU16(0);           // Padding
    writeU32(key);         // Keysym

    this->m_vncStream->flush();
}

void VNCGraphicsClient::sendScanCodeEvent(quint32 scanCode, quint32 keysym, bool down)
{
    if (!this->m_connected)
        return;

    // Use QEMU extended key encoding if available (matches C# keyScanEvent)
    if (this->m_useQemuExtKeyEncoding)
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

    this->m_vncStream->flush();
}

void VNCGraphicsClient::sendPointerEvent(quint8 buttonMask, quint16 x, quint16 y)
{
    if (!this->m_connected)
        return;

    writeU8(5);          // Message type
    writeU8(buttonMask); // Button mask
    writeU16(x);         // X position
    writeU16(y);         // Y position

    this->m_vncStream->flush();
}

void VNCGraphicsClient::sendClientCutText(const QString& text)
{
    if (!this->m_connected)
        return;

    QByteArray utf8Text = text.toUtf8();

    writeU8(6); // Message type
    writeU8(0); // Padding
    writeU8(0);
    writeU8(0);
    writeU32(utf8Text.length()); // Length

    this->m_vncStream->write(utf8Text);
    this->m_vncStream->flush();
}

//=============================================================================
// Rendering (matches C# Damage, OnPaint, etc.)
//=============================================================================

void VNCGraphicsClient::damage(int x, int y, int width, int height)
{
    // Matches C# Damage(int x, int y, int width, int height)
    QRect r(x, y, width, height);

    if (this->m_scaling)
    {
        r.adjust(-this->m_bump, -this->m_bump, this->m_bump, this->m_bump); // Inflate for scaling fix
    }

    if (this->m_damage.isEmpty())
    {
        this->m_damage = r;
    } else
    {
        this->m_damage = this->m_damage.united(r);
    }
}

void VNCGraphicsClient::renderDamage()
{
    // Trigger repaint (matches C# Invalidate)
    if (!this->m_damage.isEmpty())
    {
        update();
        this->m_damage = QRect();
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
            if (this->m_connected)
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

    if (this->m_backBuffer.isNull() || !this->m_backBufferInteresting)
    {
        // No content yet - just draw black background
        // C#: base.OnPaintBackground(e) - does NOT draw "Connecting..." text
        painter.fillRect(rect(), Qt::black);
        return;
    }

    // Lock back buffer and render (matches C# lock(_backBuffer))
    QMutexLocker locker(&this->m_backBufferMutex);

    if (this->m_scaling)
    {
        // Scale to fit with aspect ratio preservation (matches C# scaling logic)
        // Calculate scaled size accounting for border padding if enabled
        int effectiveWidth = this->m_displayBorder ? this->m_fbWidth + BORDER_PADDING * 3 : this->m_fbWidth;
        int effectiveHeight = this->m_displayBorder ? this->m_fbHeight + BORDER_PADDING * 3 : this->m_fbHeight;

        float xScale = (float) width() / effectiveWidth;
        float yScale = (float) height() / effectiveHeight;
        float scale = qMin(xScale, yScale);
        scale = qMax(scale, 0.01f); // Prevent division by zero

        int scaledWidth = (int) (this->m_fbWidth * scale);
        int scaledHeight = (int) (this->m_fbHeight * scale);
        int offsetX = (width() - scaledWidth) / 2;
        int offsetY = (height() - scaledHeight) / 2;

        QRect targetRect(offsetX, offsetY, scaledWidth, scaledHeight);

        // Draw scaled image with smooth transform for better quality
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(targetRect, this->m_backBuffer);

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
        if (this->m_displayBorder)
        {
            drawBorder(painter, targetRect);
        }
    } else
    {
        // 1:1 pixel mapping - but still CENTER the image (matches C# behavior)
        // C#: _dx = ((float)Size.Width - DesktopSize.Width) / 2;
        //     _dy = ((float)Size.Height - DesktopSize.Height) / 2;
        int offsetX = qMax(0, (width() - this->m_fbWidth) / 2);
        int offsetY = qMax(0, (height() - this->m_fbHeight) / 2);

        // Draw black background for areas not covered by console
        if (offsetX > 0 || offsetY > 0 ||
            this->m_fbWidth < width() || this->m_fbHeight < height())
        {
            painter.fillRect(rect(), Qt::black);
        }

        painter.drawImage(offsetX, offsetY, this->m_backBuffer);

        // Draw border if enabled
        if (this->m_displayBorder)
        {
            QRect consoleRect(offsetX, offsetY, this->m_fbWidth, this->m_fbHeight);
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
    if (this->m_backBuffer.isNull())
        return;

    if (this->m_scaling)
    {
        // Calculate scale accounting for border padding if enabled
        int effectiveWidth = this->m_displayBorder ? this->m_fbWidth + BORDER_PADDING * 3 : this->m_fbWidth;
        int effectiveHeight = this->m_displayBorder ? this->m_fbHeight + BORDER_PADDING * 3 : this->m_fbHeight;

        float xScale = (float) width() / effectiveWidth;
        float yScale = (float) height() / effectiveHeight;
        this->m_scale = qMin(xScale, yScale);
        this->m_scale = qMax(this->m_scale, 0.01f); // Prevent division by zero

        this->m_dx = (width() - this->m_fbWidth * this->m_scale) / 2.0f;
        this->m_dy = (height() - this->m_fbHeight * this->m_scale) / 2.0f;

        this->m_bump = (int) qCeil(1.0f / this->m_scale); // For damage inflation
    } else
    {
        this->m_scale = 1.0f;
        this->m_bump = 0;

        // Even in non-scaling mode, center the image if it's smaller than the widget
        // This matches C# behavior
        if (width() >= this->m_fbWidth)
        {
            this->m_dx = (width() - this->m_fbWidth) / 2.0f;
        } else
        {
            this->m_dx = this->m_displayBorder ? BORDER_PADDING : 0;
        }

        if (height() >= this->m_fbHeight)
        {
            this->m_dy = (height() - this->m_fbHeight) / 2.0f;
        } else
        {
            this->m_dy = this->m_displayBorder ? BORDER_PADDING : 0;
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
    if (this->m_backBuffer.isNull())
        return QPoint(0, 0);

    if (this->m_scaling)
    {
        int x = (int) ((pos.x() - this->m_dx) / this->m_scale);
        int y = (int) ((pos.y() - this->m_dy) / this->m_scale);
        return QPoint(qBound(0, x, this->m_fbWidth - 1),
                      qBound(0, y, this->m_fbHeight - 1));
    } else
    {
        // Even in non-scaling mode, account for centering offset
        int x = pos.x() - (int) this->m_dx;
        int y = pos.y() - (int) this->m_dy;
        return QPoint(qBound(0, x, this->m_fbWidth - 1),
                      qBound(0, y, this->m_fbHeight - 1));
    }
}

void VNCGraphicsClient::mousePressEvent(QMouseEvent* event)
{
    if (!this->m_connected)
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
    this->m_currentMouseState = buttons;
}

void VNCGraphicsClient::mouseReleaseEvent(QMouseEvent* event)
{
    if (!this->m_connected)
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
    this->m_currentMouseState = buttons;
}

void VNCGraphicsClient::mouseMoveEvent(QMouseEvent* event)
{
    if (!this->m_connected)
        return;

    this->updateCursor(event->pos());

    // Implement mouse throttling (matches C# MOUSE_EVENTS_BEFORE_UPDATE logic)
    this->m_mouseMoved++;
    if (this->m_mouseMoved > MOUSE_EVENTS_BEFORE_UPDATE && this->m_mouseNotMoved < MOUSE_EVENTS_DROPPED)
    {
        this->m_mouseNotMoved++;
        return; // Drop this event
    }

    this->m_mouseMoved = 0;
    this->m_mouseNotMoved = 0;

    QPoint fbPos = translateMouseCoords(event->pos());
    sendPointerEvent(this->m_currentMouseState, fbPos.x(), fbPos.y());
}

void VNCGraphicsClient::keyPressEvent(QKeyEvent* event)
{
    if (!this->m_connected)
        return;

    bool isRepeat = event->isAutoRepeat();
    Qt::Key mappedKey = this->remapKey((Qt::Key) event->key());

    // Let key handler process shortcuts first (matches C# ConsoleKeyHandler integration)
    if (!isRepeat && this->m_keyHandler && this->m_keyHandler->handleKeyEvent(mappedKey, true))
        return;

    if (this->m_sendScanCodes)
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
        this->m_pressedKeys.insert(mappedKey);
}

void VNCGraphicsClient::keyReleaseEvent(QKeyEvent* event)
{
    if (!this->m_connected || event->isAutoRepeat())
        return;

    Qt::Key mappedKey = this->remapKey((Qt::Key) event->key());

    if (this->m_keyHandler && this->m_keyHandler->handleKeyEvent(mappedKey, false))
        return;

    if (this->m_sendScanCodes)
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

    this->m_pressedKeys.remove(mappedKey);
}

void VNCGraphicsClient::focusInEvent(QFocusEvent* event)
{
    Q_UNUSED(event);

    // Update clipboard if needed (matches C# focus handling)
    if (this->m_updateClipboardOnFocus && redirectingClipboard())
    {
        setConsoleClipboard();
    }
}

void VNCGraphicsClient::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    // C# OnLostFocus: back to the normal pointer
    this->m_cursorOver = false;
    this->unsetCursor();
    // Could release all keys here to prevent stuck keys
}

//=============================================================================
// Clipboard (matches C# clipboard handling)
//=============================================================================

bool VNCGraphicsClient::redirectingClipboard()
{
    // C# Reference: VNCGraphicsClient.RedirectingClipboard() -
    // Properties.Settings.Default.ClipboardAndPrinterRedirection (Options > Consoles)
    return SettingsManager::instance().GetConsoleClipboardAndPrinterRedirection();
}

void VNCGraphicsClient::onClipboardChanged()
{
    // Matches C# ClipboardChanged event handler
    if (!redirectingClipboard() || !this->m_connected)
        return;

    try
    {
        if (!this->m_handlingChange)
        {
            if (hasFocus())
            {
                setConsoleClipboard();
            } else
            {
                this->m_updateClipboardOnFocus = true;
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
        this->m_handlingChange = true;

        QString text = QApplication::clipboard()->text();

        // Convert line endings for text console (matches C# TalkingToVNCTerm logic)
        if (this->m_useSource && !this->m_sendScanCodes)
        {
            text = text.replace("\r\n", "\n");
        }

        sendClientCutText(text);
        this->m_updateClipboardOnFocus = false;
    } catch (...)
    {
        qWarning() << "VNCGraphicsClient: Failed to set console clipboard";
    }

    this->m_handlingChange = false;
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
            this->m_pressedScans.insert(scanCode);
        } else
        {
            this->m_pressedScans.remove(scanCode);
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

int VNCGraphicsClient::rectDataSize(qint32 encoding, int width, int height, int offset) const
{
    const int bpp = bytesPerPixel();
    const int available = this->m_readBuffer.size() - offset;
    switch (encoding)
    {
        case RAW_ENCODING:
            return width * height * bpp;
        case COPY_RECTANGLE_ENCODING:
            return 4; // src-x, src-y
        case RRE_ENCODING:
        case CORRE_ENCODING:
        {
            // number-of-subrectangles (U32), background pixel, then per subrectangle a pixel
            // and x, y, w, h (U16 each for RRE, U8 each for CoRRE)
            if (available < 4)
                return -1;
            const quint32 count = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(this->m_readBuffer.constData() + offset));
            const qint64 perSubrect = bpp + (encoding == RRE_ENCODING ? 8 : 4);
            const qint64 size = 4 + bpp + qint64(count) * perSubrect;
            return size > INT_MAX ? -2 : int(size);
        }
        case CURSOR_PSEUDO_ENCODING:
            // Pixels, then a 1-bit transparency mask with rows padded to whole bytes
            return width * height * bpp + ((width + 7) / 8) * height;
        case DESKTOP_SIZE_PSEUDO_ENCODING:
            return 0; // width/height are the new desktop size
        default:
            return -2;
    }
}

void VNCGraphicsClient::fillRect(int x, int y, int width, int height, QRgb color)
{
    const QRect r = QRect(x, y, width, height).intersected(this->m_backBuffer.rect());
    if (r.isEmpty())
        return;
    for (int row = r.top(); row <= r.bottom(); ++row)
    {
        QRgb* dst = reinterpret_cast<QRgb*>(this->m_backBuffer.scanLine(row)) + r.left();
        std::fill(dst, dst + r.width(), color);
    }
}

void VNCGraphicsClient::copyRect(int srcX, int srcY, int width, int height, int dstX, int dstY)
{
    // Source and destination may overlap, so copy the source out first (C# ClientCopyRectangle
    // draws the back buffer onto itself the same way)
    const QRect src = QRect(srcX, srcY, width, height).intersected(this->m_backBuffer.rect());
    if (src.isEmpty())
        return;
    const QImage piece = this->m_backBuffer.copy(src);
    const QRect dst = QRect(dstX + (src.left() - srcX), dstY + (src.top() - srcY), src.width(), src.height())
                          .intersected(this->m_backBuffer.rect());
    for (int row = dst.top(); row <= dst.bottom(); ++row)
    {
        const QRgb* from = reinterpret_cast<const QRgb*>(piece.constScanLine(row - (dstY + (src.top() - srcY))))
                           + (dst.left() - (dstX + (src.left() - srcX)));
        QRgb* to = reinterpret_cast<QRgb*>(this->m_backBuffer.scanLine(row)) + dst.left();
        std::copy(from, from + dst.width(), to);
    }
}

bool VNCGraphicsClient::isNativeRgb32() const
{
    // The format sendSetPixelFormat() requests: 32 bpp little-endian 0x00RRGGBB, which is
    // QImage::Format_RGB32 apart from the unused top byte
    const auto& f = this->m_pixelFormat;
    return f.bitsPerPixel == 32 && f.trueColor && !f.bigEndian &&
           f.redMax == 255 && f.greenMax == 255 && f.blueMax == 255 &&
           f.redShift == 16 && f.greenShift == 8 && f.blueShift == 0;
}

void VNCGraphicsClient::writePixels(int x, int y, int width, int height, const uchar* data)
{
    // Caller holds m_backBufferMutex. Writes rows directly into the back buffer instead
    // of one QImage::setPixel() call per pixel; anything outside the framebuffer is clipped.
    const int bpp = bytesPerPixel();
    const int x0 = qMax(x, 0);
    const int x1 = qMin(x + width, this->m_backBuffer.width());
    const int y1 = qMin(y + height, this->m_backBuffer.height());
    if (x0 >= x1)
        return;

    const bool native = isNativeRgb32();
    for (int row = qMax(y, 0); row < y1; ++row)
    {
        const uchar* src = data + (qint64(row - y) * width + (x0 - x)) * bpp;
        QRgb* dst = reinterpret_cast<QRgb*>(this->m_backBuffer.scanLine(row)) + x0;
        if (native)
        {
            for (int col = x0; col < x1; ++col, src += 4)
                *dst++ = qFromLittleEndian<quint32>(src) | 0xff000000u;
        } else
        {
            for (int col = x0; col < x1; ++col, src += bpp)
                *dst++ = decodePixel(src);
        }
    }
}

int VNCGraphicsClient::bytesPerPixel() const
{
    int bpp = this->m_pixelFormat.bitsPerPixel / 8;
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
        value = this->m_pixelFormat.bigEndian
                    ? qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(data))
                    : qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data));
        break;
    case 3:
        if (this->m_pixelFormat.bigEndian)
            value = (quint32(data[0]) << 16) | (quint32(data[1]) << 8) | quint32(data[2]);
        else
            value = quint32(data[0]) | (quint32(data[1]) << 8) | (quint32(data[2]) << 16);
        break;
    default:
        value = this->m_pixelFormat.bigEndian
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

    if (this->m_pixelFormat.trueColor)
    {
        quint32 r = (value >> this->m_pixelFormat.redShift) & this->m_pixelFormat.redMax;
        quint32 g = (value >> this->m_pixelFormat.greenShift) & this->m_pixelFormat.greenMax;
        quint32 b = (value >> this->m_pixelFormat.blueShift) & this->m_pixelFormat.blueMax;
        return qRgb(scaleComponent(r, this->m_pixelFormat.redMax),
                    scaleComponent(g, this->m_pixelFormat.greenMax),
                    scaleComponent(b, this->m_pixelFormat.blueMax));
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
    quint8 value = (quint8) this->m_readBuffer.at(0);
    this->m_readBuffer.remove(0, 1);
    return value;
}

quint16 VNCGraphicsClient::readU16()
{
    quint16 value = qFromBigEndian<quint16>((const uchar*) this->m_readBuffer.constData());
    this->m_readBuffer.remove(0, 2);
    return value;
}

quint32 VNCGraphicsClient::readU32()
{
    quint32 value = qFromBigEndian<quint32>((const uchar*) this->m_readBuffer.constData());
    this->m_readBuffer.remove(0, 4);
    return value;
}

void VNCGraphicsClient::writeU8(quint8 value)
{
    this->m_vncStream->write((const char*) &value, 1);
}

void VNCGraphicsClient::writeU16(quint16 value)
{
    quint16 bigEndian = qToBigEndian(value);
    this->m_vncStream->write((const char*) &bigEndian, 2);
}

void VNCGraphicsClient::writeU32(quint32 value)
{
    quint32 bigEndian = qToBigEndian(value);
    this->m_vncStream->write((const char*) &bigEndian, 4);
}

QByteArray VNCGraphicsClient::readBytes(int count)
{
    QByteArray data = this->m_readBuffer.left(count);
    this->m_readBuffer.remove(0, count);
    return data;
}
