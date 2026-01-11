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

#ifndef CHANGEVCPUSETTINGSACTION_H
#define CHANGEVCPUSETTINGSACTION_H

#include "../../asyncoperation.h"
#include <QString>

class VM;

/**
 * @brief Action to change VM VCPU configuration
 *
 * Changes VM virtual CPU count (VCPUs_max and VCPUs_at_startup).
 * For running VMs, can only increase VCPUs via hot-plug.
 * For halted VMs, sets both max and startup with proper ordering.
 *
 * Equivalent to C# XenAdmin ChangeVCPUSettingsAction.
 */
class ChangeVCPUSettingsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct VCPU settings change action
         * @param vm VM object to modify
         * @param vcpusMax Maximum number of VCPUs
         * @param vcpusAtStartup Number of VCPUs to enable at startup
         * @param parent Parent QObject
         */
        explicit ChangeVCPUSettingsAction(QSharedPointer<VM> vm,
                                          qint64 vcpusMax,
                                          qint64 vcpusAtStartup,
                                          QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<VM> m_vm;
        qint64 m_vcpusMax;
        qint64 m_vcpusAtStartup;
};

#endif // CHANGEVCPUSETTINGSACTION_H
