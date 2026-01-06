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

#ifndef SETSSLLEGACYACTION_H
#define SETSSLLEGACYACTION_H

#include "../../asyncoperation.h"

class XenConnection;

/**
 * @brief Action to set SSL legacy mode on pool
 *
 * This action enables or disables SSL legacy protocol mode for a pool.
 * When enabled, the pool uses less secure SSL protocols for compatibility.
 * When disabled, only modern TLS protocols are used.
 *
 * C# Reference: xenadmin/XenAdmin/Actions/SetSslLegacyAction.cs
 */
class SetSslLegacyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct SSL legacy mode update action
         * @param connection Connection to the pool
         * @param poolRef Pool opaque reference
         * @param enableSslLegacy True to enable SSL legacy mode, false for TLS only
         * @param parent Parent QObject
         */
        SetSslLegacyAction(XenConnection* connection,
                           const QString& poolRef,
                           bool enableSslLegacy,
                           QObject* parent = nullptr);

        ~SetSslLegacyAction() override = default;

    protected:
        void run() override;

    private:
        QString m_poolRef;
        bool m_enableSslLegacy;
};

#endif // SETSSLLEGACYACTION_H
