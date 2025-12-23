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

#ifndef TAKESNAPSHOTCOMMAND_H
#define TAKESNAPSHOTCOMMAND_H

#include "../command.h"
#include "../../dialogs/vmsnapshotdialog.h"

class TakeSnapshotCommand : public Command
{
    Q_OBJECT

    public:
        explicit TakeSnapshotCommand(QObject* parent = nullptr);
        explicit TakeSnapshotCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        explicit TakeSnapshotCommand(const QString& vmUuid, MainWindow* mainWindow, QObject* parent = nullptr);

        void Run() override;
        bool CanRun() const override;
        QString MenuText() const override;

    signals:
        void snapshotStarted();
        void snapshotCompleted(bool success);

    private:
        QString m_vmUuid;

        // Check if VM supports snapshot operations
        bool canTakeSnapshot() const;
        void showSnapshotDialog();
        void executeSnapshotOperation(const QString& name, const QString& description, VmSnapshotDialog::SnapshotType type);
};

#endif // TAKESNAPSHOTCOMMAND_H
