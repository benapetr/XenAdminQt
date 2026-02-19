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

#ifndef IREMOTECONSOLE_H
#define IREMOTECONSOLE_H

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QSize>

class ConsoleKeyHandler;

/**
 * @brief Interface for remote console implementations (VNC, RDP, etc.)
 *
 * This interface defines the contract that all remote console implementations
 * must follow. It matches the C# IRemoteConsole interface from XenAdmin.
 *
 * Implementations:
 * - VNCGraphicsClient: VNC protocol console
 * - RdpClient: RDP protocol console
 */
class IRemoteConsole
{
    public:
        virtual ~IRemoteConsole() = default;

        /**
         * @brief Get the console key handler for managing keyboard shortcuts
         */
        virtual ConsoleKeyHandler* KeyHandler() const = 0;
        virtual void SetKeyHandler(ConsoleKeyHandler* handler) = 0;

        /**
         * @brief Get the widget that displays the console
         */
        virtual QWidget* ConsoleControl() = 0;

        /**
         * @brief Activate the console (focus and bring to front)
         */
        virtual void Activate() = 0;

        /**
         * @brief Disconnect from the console and dispose resources
         */
        virtual void DisconnectAndDispose() = 0;

        /**
         * @brief Pause console updates (used when console is not visible)
         */
        virtual void Pause() = 0;

        /**
         * @brief Resume console updates
         */
        virtual void Unpause() = 0;

        /**
         * @brief Send Ctrl+Alt+Delete to the remote console
         */
        virtual void SendCAD() = 0;

        /**
         * @brief Send function-key combo with optional Ctrl/Alt modifiers
         * @param ctrl Include Ctrl modifier
         * @param alt Include Alt modifier
         * @param functionNumber Function key number (1-12)
         */
        virtual void SendFunctionKeyWithModifiers(bool ctrl, bool alt, int functionNumber) = 0;

        /**
         * @brief Take a Snapshot of the current console display
         * @return QImage containing the Snapshot
         */
        virtual QImage Snapshot() = 0;

        /**
         * @brief Enable/disable scan code sending for keyboard input
         * @param value true to send scan codes, false to send keysyms
         */
        virtual void SetSendScanCodes(bool value) = 0;

        /**
         * @brief Get/set IsScaling mode
         * @param value true to scale console to fit window, false for 1:1 pixels
         */
        virtual bool IsScaling() const = 0;
        virtual void SetScaling(bool value) = 0;

        /**
         * @brief Set whether to display border around console
         * @param value true to show border, false to hide
         */
        virtual void SetDisplayBorder(bool value) = 0;

        /**
         * @brief Get/set the desktop size
         * @return Size of the remote desktop in pixels
         */
        virtual QSize DesktopSize() const = 0;
        virtual void SetDesktopSize(const QSize& size) = 0;

        /**
         * @brief Get the bounds of the console display area
         * @return Rectangle containing console bounds
         */
        virtual QRect ConsoleBounds() const = 0;
};

#endif // IREMOTECONSOLE_H
