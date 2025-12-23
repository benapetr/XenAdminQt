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

#ifndef REATTACHSRCOMMAND_H
#define REATTACHSRCOMMAND_H

#include "../command.h"

/**
 * @brief ReattachSRCommand - Reattach a detached storage repository
 *
 * Qt equivalent of C# XenAdmin.Commands.ReattachSRCommand
 *
 * Opens the NewSR wizard in reattach mode to allow the user to
 * reattach a storage repository that has been detached from all hosts.
 *
 * The SR must have PBDs (not forgotten) but all PBDs must be unplugged.
 */
class ReattachSRCommand : public Command
{
    Q_OBJECT

    public:
        explicit ReattachSRCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        // Inherited from Command
        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        /**
         * @brief Get selected SR reference
         * @return SR opaque reference or empty string
         */
        QString getSelectedSRRef() const;

        /**
         * @brief Check if SR can be reattached
         * @param srData SR data from cache
         * @return true if SR can be reattached
         */
        bool canSRBeReattached(const QVariantMap& srData) const;
};

#endif // REATTACHSRCOMMAND_H
