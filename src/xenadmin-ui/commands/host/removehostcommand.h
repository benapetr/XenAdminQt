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

#ifndef REMOVEHOSTCOMMAND_H
#define REMOVEHOSTCOMMAND_H

#include "hostcommand.h"

/**
 * @brief RemoveHostCommand - Remove host connection from server list
 *
 * Qt equivalent of C# XenAdmin.Commands.RemoveHostCommand
 *
 * Removes the selected host connection from the server list.
 * This command can only be run on:
 * - Disconnected hosts
 * - Coordinator/pool master hosts
 *
 * The command will:
 * - Disconnect the connection if still connected
 * - Remove the connection from the server list
 * - Save the updated server list
 */
class RemoveHostCommand : public HostCommand
{
    Q_OBJECT

    public:
        explicit RemoveHostCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        /**
         * @brief Check if host can be removed
         * @param host Host object
         * @return true if host can be removed
         */
        bool canHostBeRemoved(QSharedPointer<Host> host) const;

        /**
         * @brief Check if host is pool coordinator
         * @param host Host object
         * @return true if host is pool coordinator
         */
        bool isHostCoordinator(QSharedPointer<Host> host) const;
};

#endif // REMOVEHOSTCOMMAND_H
