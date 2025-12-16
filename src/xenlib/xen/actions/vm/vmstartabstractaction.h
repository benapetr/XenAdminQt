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

#ifndef VMSTARTABSTRACTACTION_H
#define VMSTARTABSTRACTACTION_H

#include "../../asyncoperation.h"
#include <functional>

class VM;
class Host;
class Pool;

/**
 * @brief Abstract base class for VM start/resume actions
 *
 * Qt port of C# VMStartAbstractAction. Handles HA agility checks,
 * warning dialogs for invalid HA config, and start diagnosis forms
 * for failure analysis.
 */
class XENLIB_EXPORT VMStartAbstractAction : public AsyncOperation
{
    Q_OBJECT

public:
    // Callback types matching C# delegates
    using WarningDialogHAInvalidConfig = std::function<void(VM*, bool)>;
    using StartDiagnosisForm = std::function<void(VMStartAbstractAction*, const QString&)>;

    virtual ~VMStartAbstractAction();

    /**
     * @brief Check if this is a "start" action (vs "resume")
     */
    virtual bool isStart() const = 0;

    /**
     * @brief Clone this action for retry purposes
     */
    virtual VMStartAbstractAction* clone() = 0;

protected:
    explicit VMStartAbstractAction(VM* vm,
                                   const QString& title,
                                   WarningDialogHAInvalidConfig warningDialogHAInvalidConfig,
                                   StartDiagnosisForm startDiagnosisForm,
                                   QObject* parent = nullptr);

    /**
     * @brief Perform the actual start/resume operation
     * @param start Progress start percentage (0-100)
     * @param end Progress end percentage (0-100)
     */
    virtual void doAction(int start, int end) = 0;

    /**
     * @brief Start or resume VM with HA protection checks
     *
     * Matches C# StartOrResumeVmWithHa. Checks VM agility if HA is enabled
     * and VM is protected, warns user if inconsistent state detected.
     */
    void startOrResumeVmWithHa(int start, int end);

    /**
     * @brief Add common API methods to role check list
     */
    void addCommonAPIMethodsToRoleCheck();

protected:
    WarningDialogHAInvalidConfig m_warningDialogHAInvalidConfig;
    StartDiagnosisForm m_startDiagnosisForm;
};

#endif // VMSTARTABSTRACTACTION_H
