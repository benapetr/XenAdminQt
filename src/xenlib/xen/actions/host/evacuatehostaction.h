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

#ifndef EVACUATEHOSTACTION_H
#define EVACUATEHOSTACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"

/**
 * @brief EvacuateHostAction - Evacuates all VMs from a host
 *
 * Matches C# XenAdmin.Actions.EvacuateHostAction
 *
 * This action:
 * 1. Disables the host (via maybeReduceNtolBeforeOp + Host.async_disable)
 * 2. Evacuates all VMs (via Host.async_evacuate or WLB recommendations)
 * 3. Optionally designates new pool coordinator if evacuating current coordinator
 * 4. On error, re-enables the host
 *
 * C# location: XenModel/Actions/Host/EvacuateHostAction.cs
 */
class EvacuateHostAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection to use
     * @param host Host object to evacuate
     * @param newCoordinator Optional new coordinator if evacuating current coordinator
     * @param parent Parent QObject
     */
    explicit EvacuateHostAction(XenConnection* connection,
                                Host* host,
                                Host* newCoordinator = nullptr,
                                QObject* parent = nullptr);

protected:
    void run() override;

private:
    /**
     * @brief Disable the host
     * @param start Starting progress percentage
     * @param finish Ending progress percentage
     *
     * Calls maybeReduceNtolBeforeOp, then Host.async_disable, then sets MAINTENANCE_MODE
     */
    void disable(int start, int finish);

    /**
     * @brief Enable the host (for error recovery)
     * @param start Starting progress percentage
     * @param finish Ending progress percentage
     * @param queryNtolIncrease Unused in error recovery path
     */
    void enable(int start, int finish, bool queryNtolIncrease);

    /**
     * @brief Check if host is pool coordinator
     * @return True if host is coordinator
     */
    bool isCoordinator() const;

    Host* m_host;
    Host* m_newCoordinator;
};

#endif // EVACUATEHOSTACTION_H
