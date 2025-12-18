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

#ifndef DESIGNATENEWMASTERACTION_H
#define DESIGNATENEWMASTERACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;

/**
 * @brief DesignateNewMasterAction performs an orderly handover of coordinator role.
 *
 * Matches C# usage of Pool.async_designate_new_master (used inline in EvacuateHostAction)
 *
 * This operation transfers the pool coordinator role to a new host in an orderly fashion.
 * Unlike emergency_transition_to_master, this is used during planned operations like
 * host maintenance or load balancing.
 *
 * The operation is asynchronous and returns a task to poll.
 */

class DesignateNewMasterAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for designating new coordinator
         * @param connection Connection to the current pool coordinator
         * @param newMasterRef Host reference of the new coordinator
         * @param parent Parent QObject
         */
        DesignateNewMasterAction(XenConnection* connection,
                                const QString& newMasterRef,
                                QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_newMasterRef;
};

#endif // DESIGNATENEWMASTERACTION_H
