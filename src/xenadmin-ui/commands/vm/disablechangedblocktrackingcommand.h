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

#ifndef DISABLECHANGEDBLOCKTRACKINGCOMMAND_H
#define DISABLECHANGEDBLOCKTRACKINGCOMMAND_H

#include "../command.h"

class MainWindow;

/**
 * @brief Command to disable Changed Block Tracking (CBT) for VM disks
 *
 * This command iterates through all VBDs of the selected VM(s), checks which
 * VDIs have CBT enabled, and launches VDIDisableCbtAction for each one.
 * If multiple VDIs need CBT disabled, uses ParallelAction for efficiency.
 *
 * CBT (Changed Block Tracking) allows incremental backups by tracking which
 * disk blocks have changed. This command disables that feature.
 *
 * Requirements:
 * - VM(s) selected (not templates)
 * - At least one VDI with CBT enabled
 * - CBT feature licensed (Host.RestrictChangedBlockTracking)
 *
 * C# equivalent: XenAdmin.Commands.DisableChangedBlockTrackingCommand
 */
class DisableChangedBlockTrackingCommand : public Command
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new DisableChangedBlockTrackingCommand
         * @param mainWindow Main window reference
         * @param parent Parent QObject
         */
        explicit DisableChangedBlockTrackingCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        /**
         * @brief Check if command can run with current selection
         *
         * Validates:
         * - Selection contains VM(s) (not templates)
         * - All VMs in same connection/pool
         * - At least one VM has a VDI with CBT enabled
         * - CBT feature is licensed (not restricted)
         *
         * @return true if command can execute
         */
        bool CanRun() const override;

        /**
         * @brief Execute the disable CBT command
         *
         * For each selected VM:
         * 1. Resolves all VBDs (virtual block devices)
         * 2. Resolves each VDI (virtual disk image)
         * 3. Checks if VDI has cbt_enabled == true
         * 4. Launches VDIDisableCbtAction for each enabled VDI
         *
         * Uses ParallelAction if multiple VDIs need processing.
         * Shows confirmation dialog before execution.
         */
        void Run() override;

        /**
         * @brief Get menu text for this command
         * @return "Disable Changed Block &Tracking"
         */
        QString MenuText() const override;

    private:
        /**
         * @brief Check if CBT feature is licensed for connection
         * @param connRef Connection reference
         * @return true if CBT is available (not restricted)
         */
        bool isCbtLicensed(const QString& connRef) const;

        /**
         * @brief Check if VM has at least one VDI with CBT enabled
         * @param vmRef VM reference
         * @return true if any VDI has cbt_enabled == true
         */
        bool hasVdiWithCbtEnabled(const QString& vmRef) const;
};

#endif // DISABLECHANGEDBLOCKTRACKINGCOMMAND_H
