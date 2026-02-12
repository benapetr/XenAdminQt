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

#include "RdpClient.h"
#include "ConsoleKeyHandler.h"
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>

// FreeRDP support is optional - if HAVE_FREERDP is defined, include headers
#ifdef HAVE_FREERDP
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>
#include <freerdp/input.h>

/**
 * @brief Custom context structure for FreeRDP callbacks
 *
 * This structure extends rdpContext to include a pointer back to the
 * RdpClient instance, allowing FreeRDP callbacks to access Qt methods.
 */
struct RdpClientContext
{
    rdpContext context; // Must be first member
    RdpClient* client;  // Pointer to Qt RdpClient instance
};

// ========== FreeRDP Callback Functions ==========

/**
 * @brief FreeRDP callback: Connection pre-connect
 */
static BOOL rdp_pre_connect(freerdp* instance)
{
    RdpClientContext* ctx = (RdpClientContext*) instance->context;

    qDebug() << "RdpClient: Pre-connect callback";

    // Initialize GDI (Graphics Device Interface)
    if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
    {
        qWarning() << "RdpClient: Failed to initialize GDI";
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief FreeRDP callback: Connection post-connect
 */
static BOOL rdp_post_connect(freerdp* instance)
{
    RdpClientContext* ctx = (RdpClientContext*) instance->context;
    RdpClient* client = ctx->client;

    qDebug() << "RdpClient: Post-connect callback - connection established";

    // Notify Qt side about successful connection
    QMetaObject::invokeMethod(client, [client]() { emit client->connected(); }, Qt::QueuedConnection);

    return TRUE;
}

/**
 * @brief FreeRDP callback: Connection post-disconnect
 */
static void rdp_post_disconnect(freerdp* instance)
{
    RdpClientContext* ctx = (RdpClientContext*) instance->context;
    RdpClient* client = ctx->client;

    qDebug() << "RdpClient: Post-disconnect callback";

    // Notify Qt side about disconnection
    QMetaObject::invokeMethod(client, [client]() { emit client->disconnected(); }, Qt::QueuedConnection);
}

/**
 * @brief FreeRDP callback: Desktop size changed
 */
static BOOL rdp_desktop_resize(freerdp* instance)
{
    RdpClientContext* ctx = (RdpClientContext*) instance->context;
    RdpClient* client = ctx->client;

    rdpGdi* gdi = instance->context->gdi;
    qDebug() << "RdpClient: Desktop resize:" << gdi->width << "x" << gdi->height;

    // Notify Qt side about resize
    QSize newSize(gdi->width, gdi->height);
    QMetaObject::invokeMethod(client, [client, newSize]() { emit client->desktopResized(newSize); }, Qt::QueuedConnection);

    return TRUE;
}

#if !defined(FREERDP_VERSION_MAJOR) || (FREERDP_VERSION_MAJOR < 3)
// Wrapper for FreeRDP 2.x where DesktopResize expects rdpContext*
static BOOL rdp_desktop_resize_ctx(rdpContext* context)
{
    if (!context || !context->instance)
        return FALSE;
    return rdp_desktop_resize(context->instance);
}
#endif
#endif // HAVE_FREERDP

// ========== RdpClient Implementation ==========

RdpClient::RdpClient(QWidget* parent, const QSize& size)
    : QWidget(parent), _rdpInstance(nullptr), _rdpContext(nullptr), _desktopWidth(size.width()), _desktopHeight(size.height()), _connected(false), _connecting(false), _authWarningVisible(false), _terminated(false), _scaling(false), _paused(false), _keyHandler(nullptr), _modifierKeyPressedAlone(false), _rdpThread(nullptr), _disposalTimer(nullptr), _disposalAttempts(5)
{
    qDebug() << "RdpClient: Constructing with size" << size;

#ifndef HAVE_FREERDP
    qWarning() << "RdpClient: Built without FreeRDP support - RDP functionality disabled";
    qWarning() << "RdpClient: Install libfreerdp-dev and rebuild to enable RDP";
#endif

    this->setFocusPolicy(Qt::StrongFocus);
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    this->setMouseTracking(true);

    this->resize(size);

    // Initialize framebuffer (black screen initially)
    this->_framebuffer = QImage(this->_desktopWidth, this->_desktopHeight, QImage::Format_RGB32);
    this->_framebuffer.fill(Qt::black);
}

RdpClient::~RdpClient()
{
    qDebug() << "RdpClient: Destructor called";

    // Stop connection if running
    this->_terminated = true;

    // Cleanup FreeRDP
    cleanupFreeRDP();

    // Stop disposal timer if running
    if (_disposalTimer)
    {
        _disposalTimer->stop();
        delete _disposalTimer;
        _disposalTimer = nullptr;
    }
}

bool RdpClient::initializeFreeRDP()
{
    qDebug() << "RdpClient: Initializing FreeRDP";

#ifndef HAVE_FREERDP
    qCritical() << "RdpClient: Cannot initialize - built without FreeRDP support";
    return false;
#else
    QMutexLocker locker(&_connectionMutex);

    if (_rdpInstance)
    {
        qWarning() << "RdpClient: FreeRDP already initialized";
        return false;
    }

    // Create FreeRDP instance
    _rdpInstance = freerdp_new();
    if (!_rdpInstance)
    {
        qCritical() << "RdpClient: Failed to create FreeRDP instance";
        return false;
    }

    // Allocate custom context with RdpClient pointer
    _rdpInstance->ContextSize = sizeof(RdpClientContext);
    _rdpInstance->ContextNew = nullptr; // We'll manually initialize
    _rdpInstance->ContextFree = nullptr;

    if (!freerdp_context_new(_rdpInstance))
    {
        qCritical() << "RdpClient: Failed to create FreeRDP context";
        freerdp_free(_rdpInstance);
        _rdpInstance = nullptr;
        return false;
    }

    _rdpContext = _rdpInstance->context;

    // Set pointer to RdpClient in custom context
    RdpClientContext* ctx = (RdpClientContext*) _rdpContext;
    ctx->client = this;

    // Set FreeRDP callbacks
    _rdpInstance->PreConnect = rdp_pre_connect;
    _rdpInstance->PostConnect = rdp_post_connect;
    _rdpInstance->PostDisconnect = rdp_post_disconnect;

    // Set update callbacks
#if defined(FREERDP_VERSION_MAJOR) && (FREERDP_VERSION_MAJOR >= 3)
    _rdpContext->update->DesktopResize = rdp_desktop_resize;      // freerdp* version
#else
    _rdpContext->update->DesktopResize = rdp_desktop_resize_ctx;  // rdpContext* wrapper
#endif
    qDebug() << "RdpClient: FreeRDP initialized successfully";
    return true;
#endif // HAVE_FREERDP
}

void RdpClient::cleanupFreeRDP()
{
    qDebug() << "RdpClient: Cleaning up FreeRDP";

#ifdef HAVE_FREERDP
    QMutexLocker locker(&_connectionMutex);

    if (_rdpInstance)
    {
        if (_rdpContext)
        {
            freerdp_context_free(_rdpInstance);
            _rdpContext = nullptr;
        }

        freerdp_free(_rdpInstance);
        _rdpInstance = nullptr;
    }
#endif

    _connected = false;
    _connecting = false;
}

void RdpClient::configureRDPSettings()
{
#ifndef HAVE_FREERDP
    qWarning() << "RdpClient: Cannot configure settings - built without FreeRDP support";
    return;
#else
    if (!_rdpInstance || !_rdpInstance->context || !_rdpInstance->context->settings)
    {
        qWarning() << "RdpClient: Cannot configure settings - FreeRDP not initialized";
        return;
    }

    rdpSettings* settings = _rdpInstance->context->settings;

    // Basic connection settings
    freerdp_settings_set_string(settings, FreeRDP_ServerHostname, _serverAddress.toUtf8().constData());
    freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, _desktopWidth);
    freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, _desktopHeight);
    freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);

    // Authentication settings
    if (!_username.isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Username, _username.toUtf8().constData());
    if (!_password.isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Password, _password.toUtf8().constData());
    if (!_domain.isEmpty())
        freerdp_settings_set_string(settings, FreeRDP_Domain, _domain.toUtf8().constData());

    // Enable NLA (Network Level Authentication) - matching C# CA-103910
    freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE);
    freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE);

    // Security settings
    freerdp_settings_set_bool(settings, FreeRDP_IgnoreCertificate, TRUE); // Accept any certificate
    freerdp_settings_set_uint32(settings, FreeRDP_AuthenticationLevel, 2);

    // Performance settings
    freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
    freerdp_settings_set_bool(settings, FreeRDP_OffscreenSupportLevel, TRUE);

    qDebug() << "RdpClient: Settings configured for" << _serverAddress << _desktopWidth << "x" << _desktopHeight;
#endif // HAVE_FREERDP
}

void RdpClient::Connect(const QString& rdpIP, int width, int height)
{
    qDebug() << "RdpClient: Connecting to" << rdpIP << "at" << width << "x" << height;

#ifndef HAVE_FREERDP
    qCritical() << "RdpClient: Cannot connect - built without FreeRDP support";
    emit errorOccurred("RDP support not available (FreeRDP not installed)");
    return;
#else
    if (_connecting || _connected)
    {
        qWarning() << "RdpClient: Already connecting or connected";
        return;
    }

    _serverAddress = rdpIP;
    _desktopWidth = width;
    _desktopHeight = height;

    // Initialize FreeRDP
    if (!initializeFreeRDP())
    {
        emit errorOccurred("Failed to initialize RDP client");
        return;
    }

    // Configure settings
    configureRDPSettings();

    // Update framebuffer size
    {
        QMutexLocker locker(&_framebufferMutex);
        _framebuffer = QImage(_desktopWidth, _desktopHeight, QImage::Format_RGB32);
        _framebuffer.fill(Qt::black);
    }

    _connecting = true;
    _terminated = false;

    // Run connection in worker thread (FreeRDP blocks during connection)
    _rdpThread = QThread::create([this]() {
        runRDPConnection();
    });

    QObject::connect(_rdpThread, &QThread::finished, _rdpThread, &QThread::deleteLater);
    _rdpThread->start();
#endif // HAVE_FREERDP
}

void RdpClient::Connect(const QString& rdpIP)
{
    Connect(rdpIP, _desktopWidth, _desktopHeight);
}

void RdpClient::runRDPConnection()
{
#ifdef HAVE_FREERDP
    qDebug() << "RdpClient: Running RDP connection on worker thread";

    if (!_rdpInstance)
    {
        qCritical() << "RdpClient: RDP instance is null in runRDPConnection";
        _connecting = false;
        return;
    }

    // Connect to RDP server (blocks until connection established or fails)
    BOOL status = freerdp_connect(_rdpInstance);

    if (!status)
    {
        qWarning() << "RdpClient: Failed to connect to" << _serverAddress;
        _connecting = false;

        QString error = "Failed to connect to RDP server";
        QMetaObject::invokeMethod(this, [this, error]() { emit errorOccurred(error); }, Qt::QueuedConnection);

        return;
    }

    _connected = true;
    _connecting = false;

    qDebug() << "RdpClient: Connected successfully, entering message loop";

    // Main RDP message loop
    while (!_terminated && _connected)
    {
        // Check for RDP events (non-blocking with 100ms timeout)
        HANDLE handles[64];
        DWORD count = freerdp_get_event_handles(_rdpInstance->context, handles, 64);

        if (count == 0)
        {
            qWarning() << "RdpClient: Failed to get event handles";
            break;
        }

        // Wait for events with timeout
        DWORD status = WaitForMultipleObjects(count, handles, FALSE, 100);

        if (status == WAIT_FAILED)
        {
            qWarning() << "RdpClient: WaitForMultipleObjects failed";
            break;
        }

        // Process RDP events
        if (!freerdp_check_event_handles(_rdpInstance->context))
        {
            qWarning() << "RdpClient: Failed to check event handles";
            break;
        }
    }

    qDebug() << "RdpClient: Exiting message loop";

    // Disconnect if still connected
    if (_connected && _rdpInstance)
    {
        freerdp_disconnect(_rdpInstance);
        _connected = false;
    }
#endif // HAVE_FREERDP
}

void RdpClient::Disconnect()
{
    qDebug() << "RdpClient: Disconnecting";

    _terminated = true;
    _connected = false;
    _connecting = false;

    // Wait for RDP thread to finish
    if (_rdpThread && _rdpThread->isRunning())
    {
        _rdpThread->wait(5000); // Wait up to 5 seconds
    }

    // Cleanup FreeRDP
    cleanupFreeRDP();
}

void RdpClient::DisconnectAndDispose()
{
    qDebug() << "RdpClient: DisconnectAndDispose called";

    Disconnect();

    // Use deferred disposal like C# (to avoid crashes during callbacks)
    // Reference: C# RdpClient.Dispose() with Timer
    if (!_disposalTimer)
    {
        _disposalTimer = new QTimer(this);
        _disposalTimer->setInterval(100); // 100ms like C#
        _disposalAttempts = 5;

        QObject::connect(_disposalTimer, &QTimer::timeout, this, [this]() {
            qDebug() << "RdpClient: Disposal timer tick, attempts left:" << _disposalAttempts;

            // Try to cleanup
            try
            {
                cleanupFreeRDP();

                _disposalTimer->stop();
                _disposalTimer->deleteLater();
                _disposalTimer = nullptr;

                qDebug() << "RdpClient: Disposal successful";
            } catch (...)
            {
                if (_disposalAttempts > 0)
                {
                    _disposalAttempts--;
                    qDebug() << "RdpClient: Disposal failed, retrying...";
                    return;
                }

                qWarning() << "RdpClient: Disposal failed after all attempts";
                _disposalTimer->stop();
                _disposalTimer->deleteLater();
                _disposalTimer = nullptr;
            }
        });

        _disposalTimer->start();
    }
}

void RdpClient::SetCredentials(const QString& username, const QString& password, const QString& domain)
{
    _username = username;
    _password = password;
    _domain = domain;

    qDebug() << "RdpClient: Credentials set for user:" << username;
}

void RdpClient::UpdateDisplay(int width, int height, const QPoint& locationOffset)
{
    // Dynamic resolution update (if connected and FreeRDP supports it)
    // Reference: C# RdpClient.UpdateDisplay()

    if (!_connected || !_rdpInstance)
    {
        qDebug() << "RdpClient: Cannot update display - not connected";
        return;
    }

    qDebug() << "RdpClient: UpdateDisplay:" << width << "x" << height << "offset:" << locationOffset;

    _desktopWidth = width;
    _desktopHeight = height;
    _locationOffset = locationOffset;

    // FreeRDP display update channel (if available)
    // This would require DISP channel implementation for dynamic resolution
    // For now, just resize the widget
    resize(width, height);
    move(locationOffset);
}

void RdpClient::Activate()
{
    qDebug() << "RdpClient: Activate";

    if (!isVisible())
        show();

    setFocus();
    raise();
}

void RdpClient::Pause()
{
    qDebug() << "RdpClient: Pause";
    _paused = true;
}

void RdpClient::Unpause()
{
    qDebug() << "RdpClient: Unpause";
    _paused = false;
    update(); // Force repaint
}

void RdpClient::SendCAD()
{
    qDebug() << "RdpClient: Send Ctrl+Alt+Delete";

#ifndef HAVE_FREERDP
    qWarning() << "RdpClient: Cannot send CAD - built without FreeRDP support";
    return;
#else
    if (!_connected || !_rdpInstance || !_rdpContext)
    {
        qWarning() << "RdpClient: Cannot send CAD - not connected";
        return;
    }

    // Send Ctrl+Alt+Delete via RDP input
    // This is a special RDP sequence
    rdpInput* input = _rdpContext->input;
    if (input && input->KeyboardEvent)
    {
        // Press Ctrl
        input->KeyboardEvent(input, KBD_FLAGS_DOWN, 0x1D); // Ctrl scan code
        // Press Alt
        input->KeyboardEvent(input, KBD_FLAGS_DOWN, 0x38); // Alt scan code
        // Press Delete
        input->KeyboardEvent(input, KBD_FLAGS_DOWN | KBD_FLAGS_EXTENDED, 0x53); // Delete scan code

        // Release in reverse order
        input->KeyboardEvent(input, KBD_FLAGS_RELEASE | KBD_FLAGS_EXTENDED, 0x53);
        input->KeyboardEvent(input, KBD_FLAGS_RELEASE, 0x38);
        input->KeyboardEvent(input, KBD_FLAGS_RELEASE, 0x1D);
    }
#endif
}

QImage RdpClient::Snapshot()
{
    QMutexLocker locker(&_framebufferMutex);
    return _framebuffer.copy();
}

void RdpClient::SetSendScanCodes(bool value)
{
    // RDP always uses scan codes, so this is informational only
    Q_UNUSED(value);
}

void RdpClient::SetScaling(bool value)
{
    _scaling = value;
    update();
}

void RdpClient::SetDisplayBorder(bool value)
{
    // Not implemented for RDP
    Q_UNUSED(value);
}

QSize RdpClient::DesktopSize() const
{
#ifdef HAVE_FREERDP
    if (_rdpInstance && _rdpContext && _rdpContext->gdi)
    {
        return QSize(_rdpContext->gdi->width, _rdpContext->gdi->height);
    }
#endif
    return QSize(_desktopWidth, _desktopHeight);
}

void RdpClient::SetDesktopSize(const QSize& size)
{
    _desktopWidth = size.width();
    _desktopHeight = size.height();
}

QRect RdpClient::ConsoleBounds() const
{
    return QRect(pos(), DesktopSize());
}

// ========== Qt Event Handlers ==========

void RdpClient::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (_paused)
        return;

    QPainter painter(this);

#ifdef HAVE_FREERDP
    // Copy framebuffer from GDI buffer
    if (_rdpContext && _rdpContext->gdi && _rdpContext->gdi->primary_buffer)
    {
        QMutexLocker locker(&_framebufferMutex);

        rdpGdi* gdi = _rdpContext->gdi;

        // Create QImage wrapper around GDI buffer (no copy)
        QImage gdiImage(gdi->primary_buffer, gdi->width, gdi->height, gdi->stride, QImage::Format_RGB32);

        if (_scaling)
        {
            // Scale to fit widget
            painter.drawImage(rect(), gdiImage);
        } else
        {
            // Draw at 1:1 scale
            painter.drawImage(0, 0, gdiImage);
        }
    } else
#endif
    {
        // No GDI buffer or FreeRDP not available - draw "RDP not available" message
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
                         "RDP support not available\n\nInstall FreeRDP development libraries:\n"
                         "sudo apt-get install libfreerdp-dev libfreerdp-client2 libwinpr2-dev\n\n"
                         "Then rebuild the application");
    }
}

void RdpClient::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void RdpClient::keyPressEvent(QKeyEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::keyPressEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::keyPressEvent(event);
        return;
    }

    uint32_t scanCode = qtKeyToRDPScanCode(event->key(), event->modifiers());
    if (scanCode != 0)
    {
        rdpInput* input = _rdpContext->input;
        input->KeyboardEvent(input, KBD_FLAGS_DOWN, scanCode);
        _pressedScans.insert(scanCode);
    }
#endif
}

void RdpClient::keyReleaseEvent(QKeyEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::keyReleaseEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::keyReleaseEvent(event);
        return;
    }

    uint32_t scanCode = qtKeyToRDPScanCode(event->key(), event->modifiers());
    if (scanCode != 0 && _pressedScans.contains(scanCode))
    {
        rdpInput* input = _rdpContext->input;
        input->KeyboardEvent(input, KBD_FLAGS_RELEASE, scanCode);
        _pressedScans.remove(scanCode);
    }
#endif
}

void RdpClient::mousePressEvent(QMouseEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::mousePressEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    rdpInput* input = _rdpContext->input;
    uint16_t flags = PTR_FLAGS_DOWN;

    switch (event->button())
    {
    case Qt::LeftButton:
        flags |= PTR_FLAGS_BUTTON1;
        break;
    case Qt::RightButton:
        flags |= PTR_FLAGS_BUTTON2;
        break;
    case Qt::MiddleButton:
        flags |= PTR_FLAGS_BUTTON3;
        break;
    default:
        return;
    }

    input->MouseEvent(input, flags, event->pos().x(), event->pos().y());
#endif
}

void RdpClient::mouseReleaseEvent(QMouseEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::mouseReleaseEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    rdpInput* input = _rdpContext->input;
    uint16_t flags = 0;

    switch (event->button())
    {
    case Qt::LeftButton:
        flags |= PTR_FLAGS_BUTTON1;
        break;
    case Qt::RightButton:
        flags |= PTR_FLAGS_BUTTON2;
        break;
    case Qt::MiddleButton:
        flags |= PTR_FLAGS_BUTTON3;
        break;
    default:
        return;
    }

    input->MouseEvent(input, flags, event->pos().x(), event->pos().y());
#endif
}

void RdpClient::mouseMoveEvent(QMouseEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::mouseMoveEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    rdpInput* input = _rdpContext->input;
    input->MouseEvent(input, PTR_FLAGS_MOVE, event->pos().x(), event->pos().y());
#endif
}

void RdpClient::wheelEvent(QWheelEvent* event)
{
#ifndef HAVE_FREERDP
    QWidget::wheelEvent(event);
    return;
#else
    if (!_connected || !_rdpContext || !_rdpContext->input)
    {
        QWidget::wheelEvent(event);
        return;
    }

    rdpInput* input = _rdpContext->input;
    uint16_t flags = PTR_FLAGS_WHEEL;

    // RDP wheel delta is in units of 120
    int delta = event->angleDelta().y();
    if (delta != 0)
    {
        flags |= (delta > 0) ? 0x0078 : 0xFF88; // Positive or negative rotation
        input->MouseEvent(input, flags, event->position().x(), event->position().y());
    }
#endif
}

void RdpClient::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    qDebug() << "RdpClient: Focus gained";
    update();
}

void RdpClient::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    qDebug() << "RdpClient: Focus lost";

#ifdef HAVE_FREERDP
    // Release all pressed keys
    if (_rdpContext && _rdpContext->input)
    {
        rdpInput* input = _rdpContext->input;
        for (int scanCode : _pressedScans)
        {
            input->KeyboardEvent(input, KBD_FLAGS_RELEASE, scanCode);
        }
    }
#endif
    _pressedScans.clear();
}

// ========== Helper Functions ==========

uint32_t RdpClient::qtKeyToRDPScanCode(int key, Qt::KeyboardModifiers modifiers)
{
    // Simplified Qt key to RDP scan code mapping
    // Full implementation would need comprehensive key mapping table

    // Reference: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

    Q_UNUSED(modifiers);

    switch (key)
    {
    // Letters
    case Qt::Key_A:
        return 0x1E;
    case Qt::Key_B:
        return 0x30;
    case Qt::Key_C:
        return 0x2E;
    case Qt::Key_D:
        return 0x20;
    case Qt::Key_E:
        return 0x12;
    case Qt::Key_F:
        return 0x21;
    case Qt::Key_G:
        return 0x22;
    case Qt::Key_H:
        return 0x23;
    case Qt::Key_I:
        return 0x17;
    case Qt::Key_J:
        return 0x24;
    case Qt::Key_K:
        return 0x25;
    case Qt::Key_L:
        return 0x26;
    case Qt::Key_M:
        return 0x32;
    case Qt::Key_N:
        return 0x31;
    case Qt::Key_O:
        return 0x18;
    case Qt::Key_P:
        return 0x19;
    case Qt::Key_Q:
        return 0x10;
    case Qt::Key_R:
        return 0x13;
    case Qt::Key_S:
        return 0x1F;
    case Qt::Key_T:
        return 0x14;
    case Qt::Key_U:
        return 0x16;
    case Qt::Key_V:
        return 0x2F;
    case Qt::Key_W:
        return 0x11;
    case Qt::Key_X:
        return 0x2D;
    case Qt::Key_Y:
        return 0x15;
    case Qt::Key_Z:
        return 0x2C;

    // Numbers
    case Qt::Key_0:
        return 0x0B;
    case Qt::Key_1:
        return 0x02;
    case Qt::Key_2:
        return 0x03;
    case Qt::Key_3:
        return 0x04;
    case Qt::Key_4:
        return 0x05;
    case Qt::Key_5:
        return 0x06;
    case Qt::Key_6:
        return 0x07;
    case Qt::Key_7:
        return 0x08;
    case Qt::Key_8:
        return 0x09;
    case Qt::Key_9:
        return 0x0A;

    // Function keys
    case Qt::Key_F1:
        return 0x3B;
    case Qt::Key_F2:
        return 0x3C;
    case Qt::Key_F3:
        return 0x3D;
    case Qt::Key_F4:
        return 0x3E;
    case Qt::Key_F5:
        return 0x3F;
    case Qt::Key_F6:
        return 0x40;
    case Qt::Key_F7:
        return 0x41;
    case Qt::Key_F8:
        return 0x42;
    case Qt::Key_F9:
        return 0x43;
    case Qt::Key_F10:
        return 0x44;
    case Qt::Key_F11:
        return 0x57;
    case Qt::Key_F12:
        return 0x58;

    // Modifiers
    case Qt::Key_Control:
        return 0x1D;
    case Qt::Key_Shift:
        return 0x2A;
    case Qt::Key_Alt:
        return 0x38;

    // Special keys
    case Qt::Key_Escape:
        return 0x01;
    case Qt::Key_Tab:
        return 0x0F;
    case Qt::Key_Backspace:
        return 0x0E;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 0x1C;
    case Qt::Key_Space:
        return 0x39;

    // Punctuation
    case Qt::Key_Minus:
        return 0x0C;
    case Qt::Key_Equal:
        return 0x0D;
    case Qt::Key_BracketLeft:
        return 0x1A;
    case Qt::Key_BracketRight:
        return 0x1B;
    case Qt::Key_Backslash:
        return 0x2B;
    case Qt::Key_Semicolon:
        return 0x27;
    case Qt::Key_Apostrophe:
        return 0x28;
    case Qt::Key_Comma:
        return 0x33;
    case Qt::Key_Period:
        return 0x34;
    case Qt::Key_Slash:
        return 0x35;

    default:
        return 0;
    }
}
