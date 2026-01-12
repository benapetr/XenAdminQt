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

#ifndef VMSTARTONACTION_H
#define VMSTARTONACTION_H

#include "vmstartabstractaction.h"

class VM;
class Host;

/**
 * @brief Start a VM on a specific host (VM.async_start_on)
 *
 * Qt port of C# VMStartOnAction. Starts a VM on a specified host.
 */
class XENLIB_EXPORT VMStartOnAction : public VMStartAbstractAction
{
    Q_OBJECT

    public:
        explicit VMStartOnAction(QSharedPointer<VM> vm,
                                 QSharedPointer<Host> hostToStart,
                                 WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                 StartDiagnosisForm startDiagnosisForm,
                                 QObject* parent = nullptr);

        bool IsStart() const override
        {
            return true;
        }
        VMStartAbstractAction* Clone() override;

    protected:
        void run() override;
        void doAction(int start, int end) override;

    private:
        QSharedPointer<Host> m_hostToStart;
};

#endif // VMSTARTONACTION_H
