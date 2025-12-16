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

#ifndef CHANGEHOSTAUTOSTARTACTION_H
#define CHANGEHOSTAUTOSTARTACTION_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"
#include <QString>

class XenConnection;

/**
 * @brief Action to enable/disable VM autostart on host boot
 *
 * This action modifies the Pool's other_config["auto_poweron"] setting.
 * When enabled, VMs configured with auto_poweron will start automatically when the host boots.
 *
 * C# equivalent: XenAdmin.Actions.ChangeHostAutostartAction
 */
class XENLIB_EXPORT ChangeHostAutostartAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct action to change host autostart setting
     * @param connection XenServer connection
     * @param hostRef Host opaque reference
     * @param enable True to enable autostart, false to disable
     * @param suppressHistory True to suppress operation history (default: true)
     * @param parent Parent QObject
     */
    ChangeHostAutostartAction(XenConnection* connection,
                              const QString& hostRef,
                              bool enable,
                              bool suppressHistory = true,
                              QObject* parent = nullptr);

protected:
    void run() override;

private:
    QString m_hostRef;
    bool m_enableAutostart;
};

#endif // CHANGEHOSTAUTOSTARTACTION_H
