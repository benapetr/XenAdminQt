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

#ifndef CROSSPOOLMOVEVMCOMMAND_H
#define CROSSPOOLMOVEVMCOMMAND_H

#include "crosspoolmigratecommand.h"

/**
 * @brief Cross-pool move VM command
 *
 * Qt port of C# XenAdmin.Commands.CrossPoolMoveVMCommand
 *
 * This command moves a halted or suspended VM to a different pool.
 * It inherits from CrossPoolMigrateCommand but with different restrictions:
 * - Only works on halted or suspended VMs (not running)
 * - Uses WizardMode::Move
 * - Different menu text
 *
 * C# location: XenAdmin/Commands/CrossPoolMoveVMCommand.cs
 */
class CrossPoolMoveVMCommand : public CrossPoolMigrateCommand
{
    Q_OBJECT

    public:
        explicit CrossPoolMoveVMCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        // Inherited from Command
        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

        /**
         * @brief Check if a specific VM can be moved cross-pool
         * @param vm VM to check
         * @return true if VM can be moved
         */
        static bool CanRunOnVM(QSharedPointer<VM> vm);

        /**
         * @brief Get the appropriate wizard mode based on VM power state
         * @param vm VM to check
         * @return WizardMode::Migrate if suspended, WizardMode::Move otherwise
         */
        static CrossPoolMigrateWizard::WizardMode GetWizardMode(QSharedPointer<VM> vm);
};

#endif // CROSSPOOLMOVEVMCOMMAND_H
