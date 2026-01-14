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

#ifndef POLICYALERT_H
#define POLICYALERT_H

#include "messagealert.h"

namespace XenLib
{
/**
 * Alert types for VMSS (VM Snapshot Schedule) policy messages
 * C# Reference: XenAdmin/Alerts/Types/PolicyAlert.cs enum PolicyAlertType
 */
enum class PolicyAlertType
{
    Error = 1,  // Snapshot failed
    Warn = 3,   // Warning conditions
    Info = 5    // Snapshot succeeded
};

/**
 * Specialized alert for VMSS snapshot schedule messages.
 * Handles:
 * - VMSS_SNAPSHOT_FAILED
 * - VMSS_SNAPSHOT_SUCCEEDED
 * - VMSS_SNAPSHOT_MISSED_EVENT
 * - VMSS_XAPI_LOGON_FAILURE
 * - VMSS_LICENSE_ERROR
 * - VMSS_SNAPSHOT_LOCK_FAILED
 * 
 * C# Reference: XenAdmin/Alerts/Types/PolicyAlert.cs
 */
class XENLIB_EXPORT PolicyAlert : public MessageAlert
{
    Q_OBJECT

    public:
        explicit PolicyAlert(XenConnection* connection, const QVariantMap& messageData);

        // Override alert properties
        QString GetTitle() const override;
        QString GetDescription() const override;

    private:
        void parsePolicyMessage();
        PolicyAlertType policyTypeFromPriority(int priority) const;

        PolicyAlertType m_policyType;
        QString m_title;
        QString m_description;
        QString m_policyName;
};
} // XenLib

#endif // POLICYALERT_H
