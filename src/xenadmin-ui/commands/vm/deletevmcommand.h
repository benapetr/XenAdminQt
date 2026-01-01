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

#ifndef DELETEVMCOMMAND_H
#define DELETEVMCOMMAND_H

#include "vmcommand.h"

/**
 * @class DeleteVMCommand
 * @brief Command to delete a halted VM with optional disk deletion.
 *
 * Displays a confirmation dialog that allows the user to choose whether to:
 * - Delete the VM metadata only (keeping virtual disks for potential reuse)
 * - Delete the VM AND all associated virtual disk files (default option)
 *
 * The VM must be in a halted state before it can be deleted.
 * Matches the C# XenAdmin DeleteVMCommand behavior with ConfirmVMDeleteDialog.
 *
 * @see UninstallVMCommand for unconditional VM+disk deletion without choice
 */
class DeleteVMCommand : public VMCommand
{
    Q_OBJECT

    public:
        explicit DeleteVMCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        // Inherited from Command
        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    protected:
        bool isVMDeletable() const;
};

#endif // DELETEVMCOMMAND_H
