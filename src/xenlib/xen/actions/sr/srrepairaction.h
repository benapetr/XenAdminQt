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

#ifndef SRREPAIRACTION_H
#define SRREPAIRACTION_H

#include "../../asyncoperation.h"

class SR;

/**
 * @brief SrRepairAction - Repair a Storage Repository by recreating/plugging PBDs
 *
 * Qt equivalent of C# SrRepairAction (XenModel/Actions/SR/SrRepairAction.cs).
 * Repairs an SR by ensuring all relevant hosts have attached PBDs. For each host:
 * - Creates missing PBDs (using existing PBD as template for device config)
 * - Plugs existing PBDs that are unplugged
 *
 * From C# implementation:
 * - Processes coordinator host first (CA-176935, CA-173497)
 * - For shared SRs, creates/plugs PBDs on all hosts
 * - For non-shared SRs, only processes the storage host
 * - Uses first existing PBD as template for creating new PBDs
 * - Continues on failure, reports last failure at end
 *
 * Use cases:
 * - Fixing broken SR connections after network issues
 * - Repairing SR after host failures/restarts
 * - Sharing a previously unshared SR with all hosts
 */
class XENLIB_EXPORT SrRepairAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Repair a Storage Repository
         * @param sr SR to repair
         * @param isSharedAction true if explicitly sharing SR, false if just repairing
         * @param parent Parent object
         */
        explicit SrRepairAction(SR* sr,
                                bool isSharedAction = false,
                                QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        SR* m_sr;
        bool m_isSharedAction;
};

#endif // SRREPAIRACTION_H
