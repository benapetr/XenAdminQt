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

#ifndef ENABLEHOSTACTION_H
#define ENABLEHOSTACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"

/**
 * @brief EnableHostAction - Exits maintenance mode and enables a host
 *
 * Matches C# XenAdmin.Actions.EnableHostAction
 *
 * This action:
 * 1. Removes MAINTENANCE_MODE from host's other_config
 * 2. Calls Host.async_enable
 * 3. Optionally resumes evacuated VMs (migrated, halted, or suspended)
 * 4. May offer to increase HA ntol if appropriate
 *
 * C# location: XenModel/Actions/Host/EnableHostAction.cs
 */
class EnableHostAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection to use
     * @param host Host object to enable
     * @param resumeVMs If true, resume evacuated VMs after enabling
     * @param parent Parent QObject
     */
    explicit EnableHostAction(XenConnection* connection,
                              Host* host,
                              bool resumeVMs = false,
                              QObject* parent = nullptr);

protected:
    void run() override;

private:
    /**
     * @brief Enable the host
     * @param start Starting progress percentage
     * @param finish Ending progress percentage
     * @param queryNtolIncrease If true, ask user about increasing HA ntol
     *
     * Matches C# HostAbstractAction.Enable()
     */
    void enable(int start, int finish, bool queryNtolIncrease);

    Host* m_host;
    bool m_resumeVMs;
};

#endif // ENABLEHOSTACTION_H
