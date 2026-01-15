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

#ifndef MOVEVIRTUALDISKCOMMAND_H
#define MOVEVIRTUALDISKCOMMAND_H

#include "vdicommand.h"

/**
 * @brief Command to move a virtual disk (VDI) to a different storage repository
 *
 * Qt equivalent of C# XenAdmin.Commands.MoveVirtualDiskCommand
 *
 * This command opens the MoveVirtualDiskDialog which allows the user to:
 * - Select a destination SR from compatible SRs
 * - Move the VDI to the new location
 *
 * The command can only run when:
 * - A VDI is selected
 * - VDI is not a snapshot
 * - VDI is not locked (in use)
 * - VDI is not HA metadata
 * - VDI does not have CBT enabled
 * - VDI's SR is not HBA LUN-per-VDI type
 * - All VMs using the VDI are halted
 *
 * C# Reference: XenAdmin/Commands/MoveVirtualDiskCommand.cs
 */
class MoveVirtualDiskCommand : public VDICommand
{
    Q_OBJECT

    public:
        explicit MoveVirtualDiskCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        /**
         * @brief Check if a VDI can be moved
         * @param vdi VDI object
         * @return true if VDI can be moved
         */
        bool canBeMoved(QSharedPointer<VDI> vdi) const;

        /**
         * @brief Check if any VM using this VDI is running
         * @param vdi VDI object
         * @return true if any attached VM is not halted
         */
        bool isVDIInUseByRunningVM(QSharedPointer<VDI> vdi) const;

        /**
         * @brief Check if VDI is HA metadata disk
         * @param vdi VDI object
         * @return true if VDI type is "ha"
         */
        bool isHAType(QSharedPointer<VDI> vdi) const;

        /**
         * @brief Check if VDI is DR metadata
         * @param vdi VDI object
         * @return true if VDI is metadata for disaster recovery
         */
        bool isMetadataForDR(QSharedPointer<VDI> vdi) const;
};

#endif // MOVEVIRTUALDISKCOMMAND_H
