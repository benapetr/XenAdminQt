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

#include "qvncclient.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QtEndian>

QVncClient::QVncClient(QWidget* parent)
    : QWidget(parent), m_socket(nullptr), m_connected(false), m_scaling(true), m_state(Disconnected), m_fbWidth(0), m_fbHeight(0), m_mouseButtons(0), m_updateTimer(new QTimer(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);

    // Request frame updates periodically
    connect(m_updateTimer, &QTimer::timeout, this, &QVncClient::requestFramebufferUpdate);
    m_updateTimer->setInterval(33); // ~30 FPS
}

QVncClient::~QVncClient()
{
    disconnectFromHost();
}

void QVncClient::connectToHost(const QString& host, int port, const QString& password)
{
    if (m_connected || m_state != Disconnected)
    {
        qDebug() << "QVncClient: Already connected or connecting";
        return;
    }

    // Create a new socket for direct connection
    if (m_socket)
    {
        delete m_socket;
    }

    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &QVncClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &QVncClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &QVncClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &QVncClient::onSocketError);

    m_password = password;
    m_state = ProtocolVersion;
    m_readBuffer.clear();

    qDebug() << "QVncClient: Connecting to" << host << ":" << port;
    m_socket->connectToHost(host, port);
}

void QVncClient::connectWithSocket(QTcpSocket* socket, const QString& password)
{
    if (m_connected || m_state != Disconnected)
    {
        qDebug() << "QVncClient: Already connected or connecting";
        return;
    }

    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug() << "QVncClient: Invalid or unconnected socket provided";
        emit connectionError("Invalid socket provided");
        return;
    }

    // Take ownership of the provided socket
    if (m_socket)
    {
        delete m_socket;
    }

    m_socket = socket;
    m_socket->setParent(this);

    // Connect signals
    connect(m_socket, &QTcpSocket::disconnected, this, &QVncClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &QVncClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &QVncClient::onSocketError);

    m_password = password;
    m_state = ProtocolVersion;
    m_readBuffer.clear();

    qDebug() << "QVncClient: Using provided socket, starting VNC handshake";

    // Manually trigger the connected state since socket is already connected
    onSocketConnected();
}

void QVncClient::disconnectFromHost()
{
    m_updateTimer->stop();
    m_state = Disconnected;
    m_connected = false;

    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_socket->disconnectFromHost();
    }
}

void QVncClient::setScaling(bool enabled)
{
    if (m_scaling != enabled)
    {
        m_scaling = enabled;
        update();
    }
}

void QVncClient::onSocketConnected()
{
    qDebug() << "QVncClient: Socket connected";
}

void QVncClient::onSocketDisconnected()
{
    qDebug() << "QVncClient: Socket disconnected";
    m_connected = false;
    m_state = Disconnected;
    m_updateTimer->stop();
    emit disconnected();
    update();
}

void QVncClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString error = m_socket->errorString();
    qDebug() << "QVncClient: Socket error:" << error;
    emit connectionError(error);
    m_state = Disconnected;
}

void QVncClient::onSocketReadyRead()
{
    m_readBuffer.append(m_socket->readAll());

    while (!m_readBuffer.isEmpty())
    {
        int consumed = 0;

        switch (m_state)
        {
        case ProtocolVersion: {
            if (m_readBuffer.size() < 12)
                return;

            QString version = QString::fromLatin1(m_readBuffer.left(12));
            qDebug() << "QVncClient: Server version:" << version;

            // Send our protocol version (RFB 003.008)
            m_socket->write("RFB 003.008\n");
            m_socket->flush();

            m_readBuffer.remove(0, 12);
            m_state = SecurityHandshake;
            break;
        }

        case SecurityHandshake: {
            if (m_readBuffer.size() < 1)
                return;

            quint8 securityTypeCount = (quint8) m_readBuffer.at(0);
            if (m_readBuffer.size() < 1 + securityTypeCount)
                return;

            qDebug() << "QVncClient: Security types:" << securityTypeCount;

            // Look for security types (1 = None, 2 = VNC Authentication)
            bool foundNone = false;
            bool foundVNC = false;

            for (int i = 0; i < securityTypeCount; i++)
            {
                quint8 secType = (quint8) m_readBuffer.at(1 + i);
                qDebug() << "QVncClient: Security type:" << secType;
                if (secType == 1)
                    foundNone = true;
                if (secType == 2)
                    foundVNC = true;
            }

            m_readBuffer.remove(0, 1 + securityTypeCount);

            // Choose security type
            if (!m_password.isEmpty() && foundVNC)
            {
                qDebug() << "QVncClient: Using VNC authentication";
                writeU8(2); // VNC Authentication
                m_socket->flush();
                m_state = Authentication;
            } else if (foundNone)
            {
                qDebug() << "QVncClient: Using no authentication";
                writeU8(1); // None
                m_socket->flush();
                m_state = SecurityResult;
            } else
            {
                emit connectionError("No supported security type found");
                disconnectFromHost();
                return;
            }
            break;
        }

        case Authentication: {
            // VNC Authentication with DES (simplified - real implementation needs proper DES)
            if (m_readBuffer.size() < 16)
                return;

            QByteArray challenge = m_readBuffer.left(16);
            m_readBuffer.remove(0, 16);

            // For now, just send the password padded/truncated to 8 bytes
            // Real VNC auth requires DES encryption
            QByteArray response(16, 0);
            QByteArray pwd = m_password.toLatin1();
            pwd.resize(8);

            // This is a placeholder - proper DES encryption needed for production
            for (int i = 0; i < 16; i++)
            {
                response[i] = challenge[i] ^ pwd[i % 8];
            }

            m_socket->write(response);
            m_socket->flush();
            m_state = SecurityResult;
            break;
        }

        case SecurityResult: {
            if (m_readBuffer.size() < 4)
                return;

            quint32 result = readU32();
            qDebug() << "QVncClient: Security result:" << result;

            if (result != 0)
            {
                // Authentication failed
                if (m_readBuffer.size() < 4)
                    return;
                quint32 reasonLength = readU32();
                if (m_readBuffer.size() < (int) reasonLength)
                    return;
                QString reason = QString::fromLatin1(m_readBuffer.left(reasonLength));
                m_readBuffer.remove(0, reasonLength);
                emit connectionError("Authentication failed: " + reason);
                disconnectFromHost();
                return;
            }

            // Send ClientInit
            sendClientInit();
            m_state = Initialization;
            break;
        }

        case Initialization: {
            handleServerInit();
            break;
        }

        case Normal: {
            // Handle server messages
            if (m_readBuffer.size() < 1)
                return;

            quint8 msgType = (quint8) m_readBuffer.at(0);

            switch (msgType)
            {
            case 0: // FramebufferUpdate
                handleFramebufferUpdate();
                break;
            case 1: // SetColorMapEntries
                handleSetColorMapEntries();
                break;
            case 2: // Bell
                handleBell();
                break;
            case 3: // ServerCutText
                handleServerCutText();
                break;
            default:
                qDebug() << "QVncClient: Unknown message type:" << msgType;
                m_readBuffer.remove(0, 1);
                break;
            }
            break;
        }

        default:
            return;
        }
    }
}

void QVncClient::handleServerInit()
{
    if (m_readBuffer.size() < 24)
        return;

    m_fbWidth = readU16();
    m_fbHeight = readU16();

    // Pixel format (16 bytes)
    m_pixelFormat.bitsPerPixel = readU8();
    m_pixelFormat.depth = readU8();
    m_pixelFormat.bigEndian = readU8();
    m_pixelFormat.trueColor = readU8();
    m_pixelFormat.redMax = readU16();
    m_pixelFormat.greenMax = readU16();
    m_pixelFormat.blueMax = readU16();
    m_pixelFormat.redShift = readU8();
    m_pixelFormat.greenShift = readU8();
    m_pixelFormat.blueShift = readU8();
    readBytes(3); // padding

    quint32 nameLength = readU32();
    if (m_readBuffer.size() < (int) nameLength)
        return;

    m_desktopName = QString::fromUtf8(readBytes(nameLength));

    qDebug() << "QVncClient: Framebuffer size:" << m_fbWidth << "x" << m_fbHeight;
    qDebug() << "QVncClient: Desktop name:" << m_desktopName;
    qDebug() << "QVncClient: Pixel format:" << m_pixelFormat.bitsPerPixel << "bpp";

    // Create framebuffer
    m_framebuffer = QImage(m_fbWidth, m_fbHeight, QImage::Format_RGB32);
    m_framebuffer.fill(Qt::black);

    // Send SetPixelFormat to request RGB32
    sendSetPixelFormat();

    // Send SetEncodings
    sendSetEncodings();

    // Request initial framebuffer update
    sendFramebufferUpdateRequest(false);

    m_state = Normal;
    m_connected = true;

    // Start update timer
    m_updateTimer->start();

    emit connected();
    update();
}

void QVncClient::handleFramebufferUpdate()
{
    if (m_readBuffer.size() < 4)
        return;

    m_readBuffer.remove(0, 1); // Remove message type
    readU8();                  // padding
    quint16 numRects = readU16();

    for (quint16 i = 0; i < numRects; i++)
    {
        if (m_readBuffer.size() < 12)
            return;

        quint16 x = readU16();
        quint16 y = readU16();
        quint16 width = readU16();
        quint16 height = readU16();
        qint32 encoding = (qint32) readU32();

        if (encoding == 0)
        { // Raw encoding
            int bytesPerPixel = m_pixelFormat.bitsPerPixel / 8;
            int dataSize = width * height * bytesPerPixel;

            if (m_readBuffer.size() < dataSize)
                return;

            QByteArray pixelData = readBytes(dataSize);

            // Convert pixel data to QImage format
            for (int py = 0; py < height; py++)
            {
                for (int px = 0; px < width; px++)
                {
                    int offset = (py * width + px) * bytesPerPixel;

                    quint32 pixel;
                    if (bytesPerPixel == 4)
                    {
                        pixel = qFromBigEndian<quint32>((const uchar*) pixelData.constData() + offset);
                    } else if (bytesPerPixel == 2)
                    {
                        pixel = qFromBigEndian<quint16>((const uchar*) pixelData.constData() + offset);
                    } else
                    {
                        pixel = (quint8) pixelData[offset];
                    }

                    // Extract RGB components
                    quint8 r = ((pixel >> m_pixelFormat.redShift) & m_pixelFormat.redMax) * 255 / m_pixelFormat.redMax;
                    quint8 g = ((pixel >> m_pixelFormat.greenShift) & m_pixelFormat.greenMax) * 255 / m_pixelFormat.greenMax;
                    quint8 b = ((pixel >> m_pixelFormat.blueShift) & m_pixelFormat.blueMax) * 255 / m_pixelFormat.blueMax;

                    if (x + px < (quint16) m_fbWidth && y + py < (quint16) m_fbHeight)
                    {
                        m_framebuffer.setPixel(x + px, y + py, qRgb(r, g, b));
                    }
                }
            }
        } else
        {
            qDebug() << "QVncClient: Unsupported encoding:" << encoding;
            // Skip unsupported encodings (would need to know size)
        }
    }

    emit frameUpdated();
    update();
}

void QVncClient::handleSetColorMapEntries()
{
    // Not implemented - skip message
    m_readBuffer.remove(0, 1);
}

void QVncClient::handleBell()
{
    qDebug() << "QVncClient: Bell received";
    m_readBuffer.remove(0, 1);
}

void QVncClient::handleServerCutText()
{
    if (m_readBuffer.size() < 8)
        return;

    m_readBuffer.remove(0, 1); // message type
    readBytes(3);              // padding
    quint32 length = readU32();

    if (m_readBuffer.size() < (int) length)
        return;

    QString text = QString::fromLatin1(readBytes(length));
    qDebug() << "QVncClient: Server cut text:" << text;
}

void QVncClient::sendClientInit()
{
    writeU8(1); // Shared flag (1 = shared)
    m_socket->flush();
}

void QVncClient::sendSetPixelFormat()
{
    writeU8(0); // Message type
    writeU8(0);
    writeU8(0);
    writeU8(0); // padding

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
    writeU8(0);
    writeU8(0);
    writeU8(0); // padding

    m_socket->flush();
}

void QVncClient::sendSetEncodings()
{
    writeU8(2);  // Message type
    writeU8(0);  // padding
    writeU16(1); // number of encodings

    // Encoding types (we only support Raw for now)
    writeU32(0); // Raw encoding

    m_socket->flush();
}

void QVncClient::sendFramebufferUpdateRequest(bool incremental)
{
    writeU8(3);                   // Message type
    writeU8(incremental ? 1 : 0); // incremental flag
    writeU16(0);                  // x
    writeU16(0);                  // y
    writeU16(m_fbWidth);          // width
    writeU16(m_fbHeight);         // height

    m_socket->flush();
}

void QVncClient::requestFramebufferUpdate()
{
    if (m_connected && m_state == Normal)
    {
        sendFramebufferUpdateRequest(true);
    }
}

void QVncClient::sendKeyEvent(quint32 key, bool down)
{
    if (!m_connected)
        return;

    writeU8(4);            // Message type
    writeU8(down ? 1 : 0); // down flag
    writeU16(0);           // padding
    writeU32(key);         // key

    m_socket->flush();
}

void QVncClient::sendPointerEvent(quint8 buttonMask, quint16 x, quint16 y)
{
    if (!m_connected)
        return;

    writeU8(5);          // Message type
    writeU8(buttonMask); // button mask
    writeU16(x);         // x position
    writeU16(y);         // y position

    m_socket->flush();
}

void QVncClient::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    if (m_framebuffer.isNull())
    {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
                         m_connected ? "Connecting..." : "Not connected");
        return;
    }

    if (m_scaling)
    {
        // Scale to fit widget while maintaining aspect ratio
        QSize scaled = m_framebuffer.size().scaled(size(), Qt::KeepAspectRatio);
        QRect targetRect((width() - scaled.width()) / 2,
                         (height() - scaled.height()) / 2,
                         scaled.width(), scaled.height());
        painter.drawImage(targetRect, m_framebuffer);
    } else
    {
        // Draw at original size
        painter.drawImage(0, 0, m_framebuffer);
    }
}

void QVncClient::mousePressEvent(QMouseEvent* event)
{
    if (!m_connected || m_framebuffer.isNull())
        return;

    // Update button mask
    if (event->button() == Qt::LeftButton)
        m_mouseButtons |= 0x01;
    else if (event->button() == Qt::MiddleButton)
        m_mouseButtons |= 0x02;
    else if (event->button() == Qt::RightButton)
        m_mouseButtons |= 0x04;

    // Convert to framebuffer coordinates
    QPoint fbPos = event->pos();
    if (m_scaling)
    {
        QSize scaled = m_framebuffer.size().scaled(size(), Qt::KeepAspectRatio);
        int offsetX = (width() - scaled.width()) / 2;
        int offsetY = (height() - scaled.height()) / 2;
        fbPos = QPoint((event->pos().x() - offsetX) * m_fbWidth / scaled.width(),
                       (event->pos().y() - offsetY) * m_fbHeight / scaled.height());
    }

    sendPointerEvent(m_mouseButtons, fbPos.x(), fbPos.y());
}

void QVncClient::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_connected || m_framebuffer.isNull())
        return;

    // Update button mask
    if (event->button() == Qt::LeftButton)
        m_mouseButtons &= ~0x01;
    else if (event->button() == Qt::MiddleButton)
        m_mouseButtons &= ~0x02;
    else if (event->button() == Qt::RightButton)
        m_mouseButtons &= ~0x04;

    // Convert to framebuffer coordinates
    QPoint fbPos = event->pos();
    if (m_scaling)
    {
        QSize scaled = m_framebuffer.size().scaled(size(), Qt::KeepAspectRatio);
        int offsetX = (width() - scaled.width()) / 2;
        int offsetY = (height() - scaled.height()) / 2;
        fbPos = QPoint((event->pos().x() - offsetX) * m_fbWidth / scaled.width(),
                       (event->pos().y() - offsetY) * m_fbHeight / scaled.height());
    }

    sendPointerEvent(m_mouseButtons, fbPos.x(), fbPos.y());
}

void QVncClient::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_connected || m_framebuffer.isNull())
        return;

    // Convert to framebuffer coordinates
    QPoint fbPos = event->pos();
    if (m_scaling)
    {
        QSize scaled = m_framebuffer.size().scaled(size(), Qt::KeepAspectRatio);
        int offsetX = (width() - scaled.width()) / 2;
        int offsetY = (height() - scaled.height()) / 2;
        fbPos = QPoint((event->pos().x() - offsetX) * m_fbWidth / scaled.width(),
                       (event->pos().y() - offsetY) * m_fbHeight / scaled.height());
    }

    sendPointerEvent(m_mouseButtons, fbPos.x(), fbPos.y());
}

void QVncClient::keyPressEvent(QKeyEvent* event)
{
    if (!m_connected)
        return;

    // Map Qt key to X11 keysym (simplified mapping)
    quint32 keysym = event->key();
    sendKeyEvent(keysym, true);
}

void QVncClient::keyReleaseEvent(QKeyEvent* event)
{
    if (!m_connected)
        return;

    quint32 keysym = event->key();
    sendKeyEvent(keysym, false);
}

void QVncClient::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    update();
}

// Helper methods for reading/writing protocol data
quint8 QVncClient::readU8()
{
    quint8 value = (quint8) m_readBuffer.at(0);
    m_readBuffer.remove(0, 1);
    return value;
}

quint16 QVncClient::readU16()
{
    quint16 value = qFromBigEndian<quint16>((const uchar*) m_readBuffer.constData());
    m_readBuffer.remove(0, 2);
    return value;
}

quint32 QVncClient::readU32()
{
    quint32 value = qFromBigEndian<quint32>((const uchar*) m_readBuffer.constData());
    m_readBuffer.remove(0, 4);
    return value;
}

void QVncClient::writeU8(quint8 value)
{
    m_socket->write((const char*) &value, 1);
}

void QVncClient::writeU16(quint16 value)
{
    quint16 bigEndian = qToBigEndian(value);
    m_socket->write((const char*) &bigEndian, 2);
}

void QVncClient::writeU32(quint32 value)
{
    quint32 bigEndian = qToBigEndian(value);
    m_socket->write((const char*) &bigEndian, 4);
}

QByteArray QVncClient::readBytes(int count)
{
    QByteArray data = m_readBuffer.left(count);
    m_readBuffer.remove(0, count);
    return data;
}
