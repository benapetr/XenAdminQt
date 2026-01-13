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

#ifndef REBOOTHOSTACTION_H
#define REBOOTHOSTACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"
#include <functional>
#include <QSharedPointer>

class Pool;

/**
 * @brief RebootHostAction - Reboot a physical host
 *
 * Qt equivalent of C# XenAdmin.Actions.RebootHostAction
 *
 * Reboots a XenServer host:
 * 1. Checks HA configuration and may reduce ntol if needed
 * 2. Disables the host
 * 3. Shuts down all resident VMs (clean shutdown)
 * 4. Reboots the host
 * 5. Closes connection if host is coordinator
 *
 * Key features:
 * - HA ntol management (ensures pool can tolerate host loss)
 * - Graceful VM shutdown with fallback to hard shutdown
 * - Error recovery (re-enables host on failure)
 * - Connection interrupt if rebooting coordinator
 *
 * Usage:
 *   RebootHostAction* action = new RebootHostAction(connection, host);
 *   connect(action, &AsyncOperation::completed, [](bool success) {
 *       if (success) {
 *           // Host is rebooting
 *       }
 *   });
 *   action->runAsync();
 */
class XENLIB_EXPORT RebootHostAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param host Host to reboot
         * @param acceptNtolChanges Callback to confirm ntol reduction (return true to cancel)
         * @param parent Parent QObject
         *
         * C# equivalent: RebootHostAction(Host, Func<...>)
         */
        explicit RebootHostAction(QSharedPointer<Host> host,
                                  std::function<bool(QSharedPointer<Pool>, qint64, qint64)> acceptNtolChanges = nullptr,
                                  QObject* parent = nullptr);

    protected:
        /**
         * @brief Execute the reboot operation
         *
         * Steps:
         * 1. MaybeReduceNtolBeforeOp() - Adjust HA settings if needed
         * 2. ShutdownVMs(true) - Shutdown all resident VMs (for reboot)
         * 3. Host.async_reboot() - Reboot the host
         * 4. PollToCompletion() - Wait for reboot task
         * 5. Host.enable() on error - Re-enable if reboot fails
         * 6. Connection.Interrupt() - Close connection if coordinator
         *
         * C# equivalent: Run() override
         */
        void run() override;

    private:
        QSharedPointer<Host> m_host;
        bool m_wasEnabled;
        std::function<bool(QSharedPointer<Pool>, qint64, qint64)> m_acceptNtolChanges;

        /**
         * @brief Shutdown all VMs on the host
         * @param isForReboot true if shutting down for reboot, false for shutdown
         *
         * Disables host, then shuts down all non-control-domain VMs.
         * Uses clean_shutdown if allowed, otherwise hard_shutdown.
         *
         * C# equivalent: ShutdownVMs(bool)
         */
        void shutdownVMs(bool isForReboot);

        /**
         * @brief Check and reduce HA ntol if needed
         *
         * Ensures pool has at least 1 'slack' host failure tolerance.
         * May throw exception if ntol reduction would be required but user declines.
         *
         * C# equivalent: MaybeReduceNtolBeforeOp()
         * Note: Currently simplified - full HA logic not yet implemented
         */
        void maybeReduceNtolBeforeOp();
};

#endif // REBOOTHOSTACTION_H
