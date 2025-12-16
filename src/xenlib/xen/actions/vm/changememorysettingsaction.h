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

#ifndef CHANGEMEMORYSETTINGSACTION_H
#define CHANGEMEMORYSETTINGSACTION_H

#include "../../asyncoperation.h"
#include <QString>

/**
 * @brief Action to change VM memory settings with optional reboot
 *
 * Changes VM memory configuration (static and/or dynamic limits).
 * If static memory changes and VM is not halted, performs shutdown,
 * applies changes, then restarts the VM.
 *
 * Equivalent to C# XenAdmin ChangeMemorySettingsAction.
 */
class ChangeMemorySettingsAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct memory settings change action
     * @param connection XenServer connection
     * @param vmRef VM opaque reference
     * @param staticMin Minimum static memory (bytes)
     * @param dynamicMin Minimum dynamic memory (bytes)
     * @param dynamicMax Maximum dynamic memory (bytes)
     * @param staticMax Maximum static memory (bytes)
     * @param parent Parent QObject
     */
    explicit ChangeMemorySettingsAction(XenConnection* connection,
                                        const QString& vmRef,
                                        qint64 staticMin,
                                        qint64 dynamicMin,
                                        qint64 dynamicMax,
                                        qint64 staticMax,
                                        QObject* parent = nullptr);

protected:
    void run() override;

private:
    QString m_vmRef;
    qint64 m_staticMin;
    qint64 m_dynamicMin;
    qint64 m_dynamicMax;
    qint64 m_staticMax;
    bool m_staticChanged;
    bool m_needReboot;
    QString m_vmHost; // Host affinity for restart
};

#endif // CHANGEMEMORYSETTINGSACTION_H
