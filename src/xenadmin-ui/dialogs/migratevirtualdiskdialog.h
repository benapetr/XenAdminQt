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

#ifndef MIGRATEVIRTUALDISKDIALOG_H
#define MIGRATEVIRTUALDISKDIALOG_H

#include "movevirtualdiskdialog.h"

/**
 * @brief Dialog for migrating (live-moving) VDIs to a different SR
 *
 * This is a specialized version of MoveVirtualDiskDialog that uses
 * pool_migrate instead of copy+delete. This allows live migration
 * of VDIs that are attached to running VMs.
 *
 * Qt equivalent of C# XenAdmin.Dialogs.MigrateVirtualDiskDialog
 *
 * Key Differences from MoveVirtualDiskDialog:
 * - Uses VDI.async_pool_migrate API call (via MigrateVirtualDiskAction)
 * - Can migrate VDIs from running VMs (live migration)
 * - Filters SRs to show only those that support migration
 * - Shows different UI text ("Migrate" instead of "Move")
 *
 * Implementation Note:
 * This class is a thin wrapper that overrides the action creation
 * to use MigrateVirtualDiskAction instead of MoveVirtualDiskAction.
 * All UI logic is inherited from MoveVirtualDiskDialog.
 *
 * C# Reference: XenAdmin/Dialogs/MoveVirtualDiskDialog.cs (nested class)
 */
class MigrateVirtualDiskDialog : public MoveVirtualDiskDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for single VDI migration
     * @param xenLib XenLib instance
     * @param vdiRef VDI reference to migrate
     * @param parent Parent widget
     */
    explicit MigrateVirtualDiskDialog(XenLib* xenLib, const QString& vdiRef,
                                      QWidget* parent = nullptr);

    /**
     * @brief Constructor for multiple VDI migration
     * @param xenLib XenLib instance
     * @param vdiRefs List of VDI references to migrate
     * @param parent Parent widget
     */
    explicit MigrateVirtualDiskDialog(XenLib* xenLib, const QStringList& vdiRefs,
                                      QWidget* parent = nullptr);

protected:
    /**
     * @brief Create and execute migration actions
     *
     * Overridden to use MigrateVirtualDiskAction instead of MoveVirtualDiskAction.
     * This uses VDI.async_pool_migrate API which allows live migration.
     *
     * @param targetSRRef Target SR reference
     * @param targetSRName Target SR name (for display in progress messages)
     */
    void createAndRunActions(const QString& targetSRRef, const QString& targetSRName) override;
};

#endif // MIGRATEVIRTUALDISKDIALOG_H
