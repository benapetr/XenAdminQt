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

#ifndef DELETEVIRTUALDISKCOMMAND_H
#define DELETEVIRTUALDISKCOMMAND_H

#include "../command.h"

/**
 * @brief DeleteVirtualDiskCommand - Delete virtual disk (VDI)
 *
 * Qt equivalent of C# XenAdmin.Commands.DeleteVirtualDiskCommand
 *
 * Deletes a virtual disk (VDI). The command will:
 * - Check if VDI is in use (attached to VMs)
 * - Show appropriate warning based on VDI type (snapshot, ISO, system disk, regular)
 * - Detach from VMs if necessary
 * - Delete the VDI permanently
 *
 * Uses DestroyDiskAction which handles detach + destroy.
 */
class DeleteVirtualDiskCommand : public Command
{
    Q_OBJECT

    public:
        explicit DeleteVirtualDiskCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        /**
         * @brief Get selected VDI reference
         * @return VDI opaque reference or empty string
         */
        QString getSelectedVDIRef() const;

        /**
         * @brief Get selected VDI name
         * @return VDI name_label or empty string
         */
        QString getSelectedVDIName() const;

        /**
         * @brief Get VDI data from cache
         * @return VDI data map
         */
        QVariantMap getSelectedVDIData() const;

        /**
         * @brief Check if VDI can be deleted
         * @param vdiData VDI data from cache
         * @return true if VDI can be deleted
         */
        bool canVDIBeDeleted(const QVariantMap& vdiData) const;

        /**
         * @brief Get VDI type for display
         * @param vdiData VDI data from cache
         * @return VDI type string ("Snapshot", "ISO", "System Disk", "Virtual Disk")
         */
        QString getVDIType(const QVariantMap& vdiData) const;

        /**
         * @brief Get confirmation dialog text based on VDI type
         * @param vdiType Type of VDI
         * @param vdiName Name of VDI
         * @return Confirmation text
         */
        QString getConfirmationText(const QString& vdiType, const QString& vdiName) const;

        /**
         * @brief Get confirmation dialog title based on VDI type
         * @param vdiType Type of VDI
         * @return Dialog title
         */
        QString getConfirmationTitle(const QString& vdiType) const;
};

#endif // DELETEVIRTUALDISKCOMMAND_H
