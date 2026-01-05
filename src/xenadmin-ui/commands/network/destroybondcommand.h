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

#ifndef DESTROYBONDCOMMAND_H
#define DESTROYBONDCOMMAND_H

#include "../command.h"

/**
 * @brief DestroyBondCommand - Delete a network bond
 *
 * Qt equivalent of C# XenAdmin.Commands.DestroyBondCommand
 *
 * Destroys a network bond, unbonding the NICs and removing the bond
 * configuration from all hosts in the pool.
 *
 * The command will:
 * - Check if bond affects management interface
 * - Show appropriate warnings (HA, primary management, secondary management)
 * - Get user confirmation
 * - Destroy the bond using DestroyBondAction
 */
class DestroyBondCommand : public Command
{
    Q_OBJECT

    public:
        explicit DestroyBondCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        /**
         * @brief Get selected network reference
         * @return Network opaque reference or empty string
         */
        QString getSelectedNetworkRef() const;

        /**
         * @brief Get selected network data
         * @return Network data from cache
         */
        QVariantMap getSelectedNetworkData() const;

        /**
         * @brief Check if network is a bond
         * @param networkData Network data from cache
         * @return true if network has a bond
         */
        bool isNetworkABond(const QVariantMap& networkData) const;

        /**
         * @brief Get bond reference from network
         * @param networkData Network data from cache
         * @return Bond opaque reference or empty string
         */
        QString getBondRefFromNetwork(const QVariantMap& networkData) const;

        /**
         * @brief Check if bond affects management interfaces
         * @param networkData Network data from cache
         * @param affectsPrimary Output: true if affects primary management
         * @param affectsSecondary Output: true if affects secondary management
         */
        void checkManagementImpact(const QVariantMap& networkData,
                                   bool& affectsPrimary,
                                   bool& affectsSecondary) const;

        /**
         * @brief Check if pool has HA enabled
         * @return true if HA is enabled
         */
        bool isHAEnabled() const;

        /**
         * @brief Get bond name for display
         * @param networkData Network data from cache
         * @return Bond name (typically PIF device name)
         */
        QString getBondName(const QVariantMap& networkData) const;
};

#endif // DESTROYBONDCOMMAND_H
