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

#ifndef VMRECOVERYMODECOMMAND_H
#define VMRECOVERYMODECOMMAND_H

#include "../command.h"

class MainWindow;

/**
 * @brief Command to boot a VM in recovery mode
 *
 * This command temporarily changes the VM's boot settings to "BIOS order"
 * with boot order "DN" (DVD drive, then Network), starts the VM, and then
 * restores the original boot settings. This allows booting from a recovery
 * CD/ISO without permanently changing the VM configuration.
 *
 * Requirements:
 * - Single VM selected
 * - VM is halted (not running)
 * - VM is HVM (has HVM_boot_policy)
 * - User has permissions to start VM and modify boot settings
 *
 * C# equivalent: XenAdmin.Commands.VMRecoveryModeCommand
 */
class VMRecoveryModeCommand : public Command
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new VMRecoveryModeCommand
     * @param mainWindow Main window reference
     * @param parent Parent QObject
     */
    explicit VMRecoveryModeCommand(MainWindow* mainWindow, QObject* parent = nullptr);

    /**
     * @brief Check if command can run with current selection
     *
     * Validates:
     * - Exactly one VM selected
     * - VM is halted (power state is "Halted")
     * - VM supports HVM boot (has HVM_boot_policy)
     *
     * @return true if command can execute
     */
    bool canRun() const override;

    /**
     * @brief Execute the recovery boot command
     *
     * Creates and starts HVMBootAction to:
     * 1. Save current boot settings
     * 2. Set temporary recovery boot mode
     * 3. Start VM
     * 4. Restore original boot settings
     */
    void run() override;

    /**
     * @brief Get menu text for this command
     * @return "Boot in Recovery Mode"
     */
    QString menuText() const override;
};

#endif // VMRECOVERYMODECOMMAND_H
