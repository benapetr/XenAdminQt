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

#ifndef SETHAPRIORITIESACTION_H
#define SETHAPRIORITIESACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QMap>
#include <QVariantMap>

class XenConnection;

/**
 * @brief SetHaPrioritiesAction - Sets HA restart priorities and NTOL when HA is already enabled.
 *
 * Matches C# XenModel/Actions/VM/SetHaPrioritiesAction.cs
 *
 * This action:
 * - First moves VMs from protected -> unprotected (to avoid overcommitment)
 * - Then sets the new NTOL
 * - Then moves VMs from unprotected -> protected
 * - Finally syncs the pool database to ensure settings propagate to all hosts
 */
class SetHaPrioritiesAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param pool Pool object
         * @param vmStartupOptions Map of VM ref -> {ha_restart_priority, order, start_delay}
         * @param ntol Number of host failures to tolerate
         * @param parent Parent QObject
         */
        SetHaPrioritiesAction(QSharedPointer<Pool> pool,
                              const QMap<QString, QVariantMap>& vmStartupOptions,
                              qint64 ntol,
                              QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<Pool> m_pool;
        QMap<QString, QVariantMap> m_vmStartupOptions;
        qint64 m_ntol;

        // Helper to check if a priority is a "restart" priority
        bool isRestartPriority(const QString& priority) const;
};

#endif // SETHAPRIORITIESACTION_H
