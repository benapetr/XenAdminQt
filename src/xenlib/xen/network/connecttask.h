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

#ifndef XEN_CONNECTTASK_H
#define XEN_CONNECTTASK_H

#include "../../xenlib_global.h"
#include <QtCore/QString>

namespace XenAPI
{
    class Session;
}

/**
 * @brief Lightweight connection attempt state
 *
 * Mirrors the C# ConnectTask: tracks a single in-flight connection attempt,
 * its cancellation flag, and the session produced by a successful login.
 * This is scaffolding only and is not yet wired into runtime flow.
 */
class XENLIB_EXPORT ConnectTask
{
    public:
        /**
         * @brief Construct a connect task for a target host/port.
         */
        explicit ConnectTask(const QString& hostname, int port)
            : Hostname(hostname), Port(port)
        {
        }

        /// Target hostname for this connection attempt.
        QString Hostname;
        /// Target port for this connection attempt.
        int Port = 443;
        /// Set by connection logic when user cancels.
        bool Cancelled = false;
        /// True when the connection attempt has successfully completed.
        bool Connected = false;
        /// Session created by the connection attempt (not owned here).
        XenAPI::Session* Session = nullptr;

        /**
         * @brief Convenience for cancellation callbacks.
         */
        bool GetCancelled() const
        {
            return Cancelled;
        }
};

#endif // XEN_CONNECTTASK_H
