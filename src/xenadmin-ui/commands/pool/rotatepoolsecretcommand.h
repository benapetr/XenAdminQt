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

#ifndef ROTATEPOOLSECRETCOMMAND_H
#define ROTATEPOOLSECRETCOMMAND_H

#include "poolcommand.h"
#include <QString>
#include <QVariantMap>

class MainWindow;
class Pool;
class Host;

/**
 * @brief Command to rotate the pool secret (shared authentication secret)
 *
 * Qt equivalent of C# XenAdmin.Commands.RotatePoolSecretCommand
 *
 * This command rotates the pool's shared secret used for authentication
 * between pool members. After rotation, the secret is changed on all hosts
 * in the pool.
 *
 * Requirements:
 * - Pool must exist
 * - XenServer version must be Stockholm or greater (XenServer 8.0+)
 * - Pool must not have HA enabled
 * - Pool must not be in rolling upgrade mode
 * - No host in pool can have secret rotation restriction
 * - User must have Pool Admin role (RBAC check)
 *
 * The command:
 * 1. Checks RBAC permissions (Pool Admin required)
 * 2. Shows reminder dialog about changing passwords (first time only)
 * 3. Calls Pool.rotate_secret API via DelegatedAsyncAction
 *
 * C# Reference: XenAdmin/Commands/RotatePoolSecretCommand.cs
 */

class XenCache;

class RotatePoolSecretCommand : public PoolCommand
{
    Q_OBJECT

    public:
        explicit RotatePoolSecretCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;
        QString GetCantRunReason() const;

    private:
        /**
         * @brief Check if any host in pool has rotation restriction
         * @param poolRef Pool reference
         * @return true if any host is restricted
         */
        bool hasRotationRestriction(const QSharedPointer<Pool>& pool) const;

        /**
         * @brief Check if pool meets requirements for secret rotation
         * @param poolData Pool data record
         * @return true if rotation is allowed
         */
        bool canRotateSecret(const QSharedPointer<Pool>& pool) const;

        /**
         * @brief Check if XenServer version supports secret rotation
         * @return true if Stockholm or greater (8.0+)
         */
        bool isStockholmOrGreater(const QSharedPointer<Pool>& pool) const;
};

#endif // ROTATEPOOLSECRETCOMMAND_H
