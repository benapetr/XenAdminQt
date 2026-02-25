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

#ifndef MIGRATEVIRTUALDISKCOMMAND_H
#define MIGRATEVIRTUALDISKCOMMAND_H

#include "vdicommand.h"
#include <QString>

class SR;
class VDI;

/**
 * @brief Command to migrate (live-move) one or more VDIs to a different SR
 *
 * Qt equivalent of C# XenAdmin.Commands.MigrateVirtualDiskCommand
 *
 * Migration differs from moving in that it uses VDI.async_pool_migrate
 * which allows live migration of VDIs attached to running VMs. Regular
 * move requires VMs to be halted.
 *
 * The command:
 * 1. Validates that VDI(s) can be migrated (not snapshots, not locked, etc.)
 * 2. Opens MigrateVirtualDiskDialog to select destination SR
 * 3. Dialog creates MigrateVirtualDiskAction(s) to perform the migration
 *
 * Validation Rules (must ALL be true):
 * - VDI is not a snapshot
 * - VDI is not locked
 * - VDI is not HA type
 * - VDI does not have CBT enabled
 * - VDI has at least one VBD attached
 * - Source SR exists and is not HBA LUN-per-VDI
 * - Source SR supports storage migration
 *
 * C# Reference: XenAdmin/Commands/MigrateVirtualDiskCommand.cs
 */
class MigrateVirtualDiskCommand : public VDICommand
{
    Q_OBJECT

    public:
        explicit MigrateVirtualDiskCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        QList<QSharedPointer<VDI>> getSelectedVDIs() const;
        /**
         * @brief Check if VDI can be migrated
         * @param vdi VDI object
         * @return true if VDI can be migrated
         *
         * Checks:
         * - Not a snapshot (is_a_snapshot == false)
         * - Not locked (Locked == false)
         * - Not HA type (type != "ha_statefile" and type != "redo_log")
         * - CBT not enabled (cbt_enabled == false)
         * - Has at least one VBD
         * - SR exists and is valid
         * - SR is not HBA LUN-per-VDI
         * - SR supports storage migration
         */
        bool canBeMigrated(const QSharedPointer<VDI>& vdi, QString& reason) const;

        /**
         * @brief Check if VDI is HA type (statefile or redo log)
         * @param vdi VDI object
         * @return true if VDI is HA type
         */
        bool isHAType(const QSharedPointer<VDI> &vdi) const;

        /**
         * @brief Check if VDI is metadata for DR
         * @param vdi VDI object
         * @return true if VDI is DR metadata
         */
        bool isMetadataForDR(const QSharedPointer<VDI> &vdi) const;

        /**
         * @brief Check if SR is HBA LUN-per-VDI type
         * @param sr SR object
         * @return true if SR is HBA LUN-per-VDI
         */
        bool isHBALunPerVDI(const QSharedPointer<SR> &sr) const;

        /**
         * @brief Check if SR supports storage migration
         * @param sr SR object
         * @return true if SR supports migration
         */
        bool supportsStorageMigration(const QSharedPointer<SR> &sr) const;
};

#endif // MIGRATEVIRTUALDISKCOMMAND_H
