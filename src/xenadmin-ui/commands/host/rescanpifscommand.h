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

#ifndef RESCANPIFSCOMMAND_H
#define RESCANPIFSCOMMAND_H

#include "../command.h"
#include <QString>

class MainWindow;

/**
 * @brief Command to rescan PIFs (Physical Interfaces) on a host
 *
 * Qt equivalent of C# XenAdmin.Commands.RescanPIFsCommand
 *
 * This command rescans the network interfaces on a host to detect
 * any hardware changes (new NICs, removed NICs, etc.). It's useful
 * after hardware changes or to refresh the interface list.
 *
 * The command:
 * 1. Validates that a single host is selected
 * 2. Creates RescanPIFsAction for the host
 * 3. Launches the action to perform the rescan
 *
 * Requirements:
 * - Exactly one host must be selected
 * - Host must be connected
 *
 * C# Reference: XenAdmin/Commands/RescanPIFsCommand.cs
 */
class RescanPIFsCommand : public Command
{
    Q_OBJECT

    public:
        explicit RescanPIFsCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool canRun() const override;
        void run() override;
        QString menuText() const override;

    private:
        bool isHostSelected() const;
};

#endif // RESCANPIFSCOMMAND_H
