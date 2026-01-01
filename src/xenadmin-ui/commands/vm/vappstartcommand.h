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

#ifndef VAPPSTARTCOMMAND_H
#define VAPPSTARTCOMMAND_H

#include "../command.h"

class MainWindow;

/**
 * @brief Command to start a VM appliance (vApp)
 *
 * A VM appliance is a group of VMs that are managed together. This command
 * starts all VMs in the appliance according to their configured startup order.
 *
 * Supports two selection modes:
 * 1. VM_appliance object directly selected
 * 2. VM(s) belonging to same appliance selected
 *
 * C# equivalent: XenAdmin.Commands.VappStartCommand
 */
class VappStartCommand : public Command
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new VappStartCommand
         * @param mainWindow Main window reference
         * @param parent Parent QObject
         */
        explicit VappStartCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        /**
         * @brief Check if command can run with current selection
         *
         * Validates:
         * - Selection is VM_appliance(s) OR VM(s) from same appliance
         * - At least one appliance has "start" in allowed_operations
         *
         * @return true if command can execute
         */
        bool CanRun() const override;

        /**
         * @brief Execute the vApp start command
         *
         * Creates StartApplianceAction for each startable appliance.
         * If VMs selected, finds their common appliance and starts it.
         */
        void Run() override;

        /**
         * @brief Get menu text for this command
         * @return "Start vApp"
         */
        QString MenuText() const override;

    private:
        /**
         * @brief Check if appliance can be started
         * @param applianceData VM_appliance record data
         * @return true if "start" in allowed_operations
         */
        bool canStartAppliance(const QVariantMap& applianceData) const;

        /**
         * @brief Get appliance ref from selected VM
         * @param vmRef VM opaque reference
         * @return Appliance ref or empty string if not in appliance
         */
        QString getApplianceRefFromVM(const QString& vmRef) const;
};

#endif // VAPPSTARTCOMMAND_H
