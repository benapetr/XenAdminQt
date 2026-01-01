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

#ifndef UNINSTALLVMCOMMAND_H
#define UNINSTALLVMCOMMAND_H

#include "vmcommand.h"

/**
 * @class UninstallVMCommand
 * @brief Command to completely remove a VM and ALL its virtual disks.
 *
 * This is a destructive operation that ALWAYS deletes both the VM metadata
 * and all associated virtual disk files. Unlike DeleteVMCommand, this does NOT
 * offer an option to keep the disks - they are permanently deleted.
 *
 * Shows a warning dialog emphasizing the permanent nature of the deletion.
 * The VM cannot be running or have any active operations.
 *
 * Use cases:
 * - Complete removal of unwanted VMs and their storage
 * - Cleanup when disk space recovery is needed
 *
 * @see DeleteVMCommand for deletion with disk preservation option
 */
class UninstallVMCommand : public VMCommand
{
    Q_OBJECT

    public:
        explicit UninstallVMCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        // Inherited from Command
        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        bool canVMBeUninstalled() const;
};

#endif // UNINSTALLVMCOMMAND_H
