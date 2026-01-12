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

#ifndef HVMBOOTACTION_H
#define HVMBOOTACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QVariantMap>

class VM;

namespace XenAPI
{
    class Session;
}

/**
 * @brief Action to boot a VM in recovery mode with temporary boot settings
 *
 * This action:
 * 1. Saves current HVM boot policy and boot order
 * 2. Sets temporary boot policy to "BIOS order" and boot order to "DN" (DVD then Network)
 * 3. Starts the VM
 * 4. Restores original boot policy and boot order
 *
 * This allows booting from a recovery CD/ISO without permanently changing VM settings.
 *
 * C# equivalent: XenAdmin.Actions.HVMBootAction
 */
class HVMBootAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a new HVMBootAction
         * @param vm VM object to boot in recovery mode
         * @param parent Parent QObject
         */
        explicit HVMBootAction(QSharedPointer<VM> vm, QObject* parent = nullptr);

    protected:
        /**
         * @brief Execute the recovery boot sequence
         *
         * Steps:
         * 1. Get current HVM boot policy and boot order
         * 2. Set policy to "BIOS order" and order to "DN"
         * 3. Start VM
         * 4. Restore original boot policy and order
         *
         * @throws std::runtime_error if any step fails
         */
        void run() override;

    private:
        QSharedPointer<VM> m_vm;  ///< VM object
        QString m_oldBootPolicy; ///< Original boot policy to restore
        QString m_oldBootOrder;  ///< Original boot order to restore

        /**
         * @brief Extract "order" value from HVM_boot_params map
         * @param bootParams HVM_boot_params dictionary
         * @return Boot order string (e.g., "cdn", "DN") or empty if not set
         */
        QString getBootOrder(const QVariantMap& bootParams) const;

        /**
         * @brief Set "order" value in HVM_boot_params map
         * @param bootParams HVM_boot_params dictionary (modified in place)
         * @param order New boot order string
         */
        void setBootOrder(QVariantMap& bootParams, const QString& order);

        /**
         * @brief Restore original boot settings after recovery boot
         * @param session XenServer session
         *
         * Called after VM start (success or failure) to restore the original
         * boot policy and boot order. Errors are logged but not propagated.
         */
        void restoreBootSettings(XenAPI::Session* session);
};

#endif // HVMBOOTACTION_H
