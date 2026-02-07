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

#include "policyalert.h"
#include <QRegularExpression>
#include <QDebug>

using namespace XenLib;

PolicyAlert::PolicyAlert(XenConnection* connection, const QVariantMap& messageData) : MessageAlert(connection, messageData), m_policyType(PolicyAlertType::Info)
{
    this->parsePolicyMessage();
}

void PolicyAlert::parsePolicyMessage()
{
    // C# Reference: PolicyAlert.cs constructor line 48
    
    // Get policy name from message (would need XenAPI lookup of VMSS object)
    // For now, use a placeholder
    this->m_policyName = tr("Snapshot Schedule");
    
    // Determine alert type from priority
    this->m_policyType = this->policyTypeFromPriority(this->m_messageData.value("priority").toInt());
    
    QString msgType = this->GetMessageType();
    QString body = this->GetMessageBody();
    
    // For non-error types, use simple formatting
    if (this->m_policyType != PolicyAlertType::Error)
    {
        // Simple success/info messages
        if (msgType == "VMSS_SNAPSHOT_SUCCEEDED")
        {
            this->m_title = tr("Snapshot completed for policy '%1'").arg(this->m_policyName);
            this->m_description = this->m_title;
        } else if (msgType == "VMSS_SNAPSHOT_MISSED_EVENT")
        {
            this->m_title = tr("Snapshot missed for policy '%1'").arg(this->m_policyName);
            this->m_description = tr("The snapshot schedule '%1' missed its scheduled time.").arg(this->m_policyName);
        } else
        {
            this->m_title = tr("Policy event: %1").arg(msgType);
            this->m_description = body;
        }
        return;
    }
    
    // Parse error messages (C# Reference: line 66-103)
    // Format: VM:<name>UUID:<uuid>Error:['<error_code>',...]
    QRegularExpression vmErrorRegex("^VM:(.*)UUID:(.*)Error:\\[('.*',?)*\\],?$");
    QStringList lines = body.split('\n');
    QStringList vmFailures;
    
    for (const QString& line : lines)
    {
        QRegularExpressionMatch match = vmErrorRegex.match(line);
        if (!match.hasMatch())
            continue;
        
        QString vmName = match.captured(1).trimmed();
        QString errorPart = match.captured(3);
        
        // Extract first error code from ['error1', 'error2', ...]
        QRegularExpression errorCodeRegex("'([^']+)'");
        QRegularExpressionMatch errorMatch = errorCodeRegex.match(errorPart);
        
        if (errorMatch.hasMatch())
        {
            QString errorCode = errorMatch.captured(1);
            // TODO: Map error code to friendly message using XenAPI Failure class
            QString errorMessage = errorCode;  // For now, show error code directly
            vmFailures.append(tr("  - %1: %2").arg(vmName, errorMessage));
        }
    }
    
    if (!vmFailures.isEmpty())
    {
        this->m_title = tr("Snapshot failed for %1 VM(s)").arg(vmFailures.count());
        this->m_description = tr("Policy '%1' failed:\n%2").arg(this->m_policyName).arg(vmFailures.join("\n"));
    } else
    {
        // Generic failure message
        this->m_title = tr("Snapshot failed");
        this->m_description = tr("Policy '%1' failed: %2").arg(this->m_policyName, body);
    }
}

PolicyAlertType PolicyAlert::policyTypeFromPriority(int priority) const
{
    // C# Reference: PolicyAlert.cs FromPriority() line 126
    // CA-343763: Logic works for pre-Stockholm servers where info=4 and warn=1
    
    if (priority < 3)
        return PolicyAlertType::Error;
    
    if (priority == 3)
        return PolicyAlertType::Warn;
    
    return PolicyAlertType::Info;
}

QString PolicyAlert::GetTitle() const
{
    return this->m_title;
}

QString PolicyAlert::GetDescription() const
{
    return this->m_description;
}
