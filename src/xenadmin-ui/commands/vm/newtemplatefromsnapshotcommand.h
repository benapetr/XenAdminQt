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

#ifndef NEWTEMPLATEFROMSNAPSHOTCOMMAND_H
#define NEWTEMPLATEFROMSNAPSHOTCOMMAND_H

#include "../command.h"

class MainWindow;
class XenConnection;

/**
 * @brief Command to create a new template from a VM snapshot
 *
 * This command creates a template by cloning a VM snapshot. It prompts
 * the user for a template name and then uses VMCloneAction to create
 * the template. The resulting template is marked as "instant" in
 * other_config to indicate it was created from a snapshot.
 *
 * Requirements:
 * - Single VM snapshot selected (is_a_snapshot == true)
 *
 * C# equivalent: XenAdmin.Commands.NewTemplateFromSnapshotCommand
 */
class NewTemplateFromSnapshotCommand : public Command
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new NewTemplateFromSnapshotCommand
         * @param mainWindow Main window reference
         * @param parent Parent QObject
         */
        explicit NewTemplateFromSnapshotCommand(MainWindow* mainWindow, QObject* parent = nullptr);
        explicit NewTemplateFromSnapshotCommand(const QString& snapshotRef, XenConnection* connection, MainWindow* mainWindow, QObject* parent = nullptr);

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
         * @brief Execute the create template command
         *
         * Prompts user for template name, then creates template by cloning
         * the snapshot. Sets other_config["instant"] = "true" to mark as
         * instant template.
         */
        void Run() override;

        /**
         * @brief Get menu text for this command
         * @return "Create Template from Snapshot..."
         */
        QString MenuText() const override;

    private:
        QString m_snapshotRef;
        XenConnection* m_connection = nullptr;

        /**
         * @brief Generate a unique name for the new template
         * @param snapshotName Name of the source snapshot
         * @return Unique template name (e.g., "Template from 'Snapshot 1'")
         */
        QString generateUniqueName(const QString& snapshotName) const;
};

#endif // NEWTEMPLATEFROMSNAPSHOTCOMMAND_H
