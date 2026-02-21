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

#ifndef REVERTTOSNAPSHOTCOMMAND_H
#define REVERTTOSNAPSHOTCOMMAND_H

#include "../command.h"

class RevertToSnapshotCommand : public Command
{
    Q_OBJECT

    public:
        explicit RevertToSnapshotCommand(QObject* parent = nullptr);
        explicit RevertToSnapshotCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        explicit RevertToSnapshotCommand(const QString& snapshotUuid, MainWindow* mainWindow, QObject* parent = nullptr);

        void Run() override;
        bool CanRun() const override;
        QString MenuText() const override;

    private:
        QString m_snapshotUuid;
        QString effectiveSnapshotUuid() const;

        // Check if snapshot can be reverted to
        bool canRevertToSnapshot() const;
        void revertToSnapshot();
        bool showConfirmationDialog();
};

#endif // REVERTTOSNAPSHOTCOMMAND_H
