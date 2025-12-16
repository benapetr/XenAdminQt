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

#ifndef EMERGENCYTRANSITIONTOMASTERACTION_H
#define EMERGENCYTRANSITIONTOMASTERACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;

/// <summary>
/// EmergencyTransitionToMasterAction promotes a slave to master in emergency situations.
/// Matches C# usage of Pool.emergency_transition_to_master (no dedicated action class in C#)
///
/// This operation is used when the current pool coordinator is unavailable and a slave
/// needs to be promoted. It's a synchronous operation (no task polling).
///
/// IMPORTANT: This must be executed from a slave host's connection, not the pool coordinator.
/// </summary>
class EmergencyTransitionToMasterAction : public AsyncOperation
{
    Q_OBJECT

public:
    /// <summary>
    /// Constructor for emergency coordinator transition
    /// </summary>
    /// <param name="slaveConnection">Connection to the slave host being promoted</param>
    /// <param name="parent">Parent QObject</param>
    EmergencyTransitionToMasterAction(XenConnection* slaveConnection,
                                      QObject* parent = nullptr);

protected:
    void run() override;
};

#endif // EMERGENCYTRANSITIONTOMASTERACTION_H
