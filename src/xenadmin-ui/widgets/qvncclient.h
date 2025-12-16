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

#ifndef QVNCCLIENT_H
#define QVNCCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QImage>
#include <QTimer>

/**
 * @brief Simple VNC client widget for RFB protocol
 *
 * This widget provides basic VNC (Remote Frame Buffer) protocol support
 * for connecting to VM consoles. It handles:
 * - RFB protocol handshake and authentication
 * - Frame buffer updates and rendering
 * - Keyboard and mouse input forwarding
 *
 * Based on RFB Protocol 3.8 specification
 */
class QVncClient : public QWidget
{
    Q_OBJECT

public:
    explicit QVncClient(QWidget* parent = nullptr);
    ~QVncClient();

    // Connection methods
    void connectToHost(const QString& host, int port = 5900, const QString& password = QString());
    void connectWithSocket(QTcpSocket* socket, const QString& password = QString());
    void disconnectFromHost();
    bool isConnected() const
    {
        return m_connected;
    }

    // Display settings
    void setScaling(bool enabled);
    bool scaling() const
    {
        return m_scaling;
    }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void frameUpdated();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void requestFramebufferUpdate();

private:
    // Protocol handling
    void handleServerInit();
    void handleFramebufferUpdate();
    void handleSetColorMapEntries();
    void handleBell();
    void handleServerCutText();

    // Message sending
    void sendClientInit();
    void sendSetPixelFormat();
    void sendSetEncodings();
    void sendFramebufferUpdateRequest(bool incremental = false);
    void sendKeyEvent(quint32 key, bool down);
    void sendPointerEvent(quint8 buttonMask, quint16 x, quint16 y);

    // Helper methods
    quint8 readU8();
    quint16 readU16();
    quint32 readU32();
    void writeU8(quint8 value);
    void writeU16(quint16 value);
    void writeU32(quint32 value);
    QByteArray readBytes(int count);
    void waitForBytes(int count);

    // State
    QTcpSocket* m_socket;
    bool m_connected;
    bool m_scaling;

    // Protocol state
    enum State
    {
        Disconnected,
        ProtocolVersion,
        SecurityHandshake,
        Authentication,
        SecurityResult,
        Initialization,
        Normal
    };
    State m_state;

    // Frame buffer
    QImage m_framebuffer;
    int m_fbWidth;
    int m_fbHeight;
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

    // Input state
    quint8 m_mouseButtons;
    QPoint m_lastMousePos;

    // Update timer
    QTimer* m_updateTimer;

    // Password for authentication
    QString m_password;

    // Buffering
    QByteArray m_readBuffer;
};

#endif // QVNCCLIENT_H
