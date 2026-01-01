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

#ifndef EXPORTSNAPSHOTASTEMPLATECOMMAND_H
#define EXPORTSNAPSHOTASTEMPLATECOMMAND_H

#include "../command.h"

class MainWindow;
class XenConnection;

/**
 * @brief Command to export a VM snapshot as a template
 *
 * This command allows exporting a VM snapshot in the same way as
 * exporting a regular VM. It reuses the ExportVMCommand internally
 * since snapshots can be exported just like VMs.
 *
 * Requirements:
 * - Single VM snapshot selected (is_a_snapshot == true)
 *
 * C# equivalent: XenAdmin.Commands.ExportSnapshotAsTemplateCommand
 */
class ExportSnapshotAsTemplateCommand : public Command
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new ExportSnapshotAsTemplateCommand
         * @param mainWindow Main window reference
         * @param parent Parent QObject
         */
        explicit ExportSnapshotAsTemplateCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        explicit ExportSnapshotAsTemplateCommand(const QString& snapshotRef,
                                                 XenConnection* connection,
                                                 MainWindow* mainWindow,
                                                 QObject* parent = nullptr);

        /**
         * @brief Check if command can run with current selection
         *
         * Validates:
         * - Exactly one VM selected
         * - VM is a snapshot (is_a_snapshot == true)
         *
         * @return true if command can execute
         */
        bool CanRun() const override;

        /**
         * @brief Execute the export snapshot command
         *
         * Creates and runs ExportVMCommand for the selected snapshot.
         * This reuses the same export wizard as regular VM export.
         */
        void Run() override;

        /**
         * @brief Get menu text for this command
         * @return "Export Snapshot as Template..."
         */
        QString MenuText() const override;

    private:
        QString m_snapshotRef;
        XenConnection* m_connection = nullptr;
};

#endif // EXPORTSNAPSHOTASTEMPLATECOMMAND_H
