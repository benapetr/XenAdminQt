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

#include "messagealert.h"
#include "alarmmessagealert.h"
#include "policyalert.h"
#include "certificatealert.h"
#include "alertmanager.h"
#include "../../xenlib/xen/network/connection.h"
#include <QDateTime>

MessageAlert::MessageAlert(XenConnection* connection, const QVariantMap& messageData) : Alert(connection), m_messageData(messageData), m_priority(AlertPriority::Unknown)
{
    this->parseMessageData();
}

QString MessageAlert::title() const
{
    return this->m_title;
}

QString MessageAlert::description() const
{
    return this->m_description;
}

AlertPriority MessageAlert::priority() const
{
    return this->m_priority;
}

QString MessageAlert::appliesTo() const
{
    return this->m_appliesTo;
}

QString MessageAlert::name() const
{
    return this->m_name;
}

void MessageAlert::dismiss()
{
    // TODO: Call XenAPI Message.destroy to remove the message
    // For now, just mark as dismissing
    this->m_dismissing = true;
}

QString MessageAlert::messageType() const
{
    return this->m_messageData.value("name").toString();
}

QString MessageAlert::messageBody() const
{
    return this->m_messageData.value("body").toString();
}

QString MessageAlert::objUuid() const
{
    return this->m_messageData.value("obj_uuid").toString();
}

QString MessageAlert::opaqueRef() const
{
    return this->m_messageData.value("ref").toString();
}

Alert* MessageAlert::parseMessage(XenConnection* connection, const QVariantMap& messageData)
{
    // C# Reference: MessageAlert.cs line 462 - ParseMessage()
    // Factory method that creates the appropriate alert type based on message type
    
    QString msgType = messageData.value("name").toString();
    
    // Create specialized alert types based on message type (C# Reference: line 467-504)
    
    // Performance alarms
    if (msgType == "ALARM")
        return new AlarmMessageAlert(connection, messageData);
    
    // VMSS (VM Snapshot Schedule) alerts
    if (msgType == "VMSS_SNAPSHOT_MISSED_EVENT" ||
        msgType == "VMSS_XAPI_LOGON_FAILURE" ||
        msgType == "VMSS_LICENSE_ERROR" ||
        msgType == "VMSS_SNAPSHOT_FAILED" ||
        msgType == "VMSS_SNAPSHOT_SUCCEEDED" ||
        msgType == "VMSS_SNAPSHOT_LOCK_FAILED")
    {
        return new PolicyAlert(connection, messageData);
    }
    
    // Certificate expiry warnings
    if (msgType == "POOL_CA_CERTIFICATE_EXPIRED" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_07" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_14" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_30" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRED" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_07" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_14" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_30" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRED" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_07" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_14" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_30")
    {
        return new CertificateAlert(connection, messageData);
    }
    
    // TODO: Add more specialized alert types:
    // - CssExpiryAlert (UPDATES_FEATURE_EXPIR*)
    // - FailedLoginAttemptAlert (FAILED_LOGIN_ATTEMPTS)
    // - LeafCoalesceAlert (LEAF_COALESCE_*)
    
    // Default: basic MessageAlert for unrecognized types
    return new MessageAlert(connection, messageData);
}

void MessageAlert::removeAlert(const QString& messageRef)
{
    // C# Reference: MessageAlert.cs line 447 - RemoveWithMessage()
    // Find and remove alert associated with this message opaque_ref
    
    Alert* alert = AlertManager::instance()->findAlert([messageRef](Alert* a)
    {
        MessageAlert* msgAlert = dynamic_cast<MessageAlert*>(a);
        if (!msgAlert)
            return false;
        return msgAlert->opaqueRef() == messageRef;
    });
    
    if (alert)
    {
        AlertManager::instance()->removeAlert(alert);
    }
}

void MessageAlert::parseMessageData()
{
    // Extract timestamp
    qint64 timestamp = this->m_messageData.value("timestamp").toLongLong();
    this->m_timestamp = QDateTime::fromSecsSinceEpoch(timestamp);
    
    // Extract priority (1-5, 1=highest)
    int priorityValue = this->m_messageData.value("priority").toInt();
    switch (priorityValue) {
        case 1: this->m_priority = AlertPriority::Priority1; break;
        case 2: this->m_priority = AlertPriority::Priority2; break;
        case 3: this->m_priority = AlertPriority::Priority3; break;
        case 4: this->m_priority = AlertPriority::Priority4; break;
        case 5: this->m_priority = AlertPriority::Priority5; break;
        default: this->m_priority = AlertPriority::Unknown; break;
    }
    
    // Extract name and body
    QString msgType = this->messageType();
    QString msgBody = this->messageBody();
    
    // Set title to message type
    this->m_title = msgType;
    this->m_name = msgType;
    
    // Set description
    this->m_description = this->getFriendlyDescription();
    
    // Extract obj_uuid for appliesTo
    QString objUuid = this->objUuid();
    if (!objUuid.isEmpty()) {
        this->m_appliesTo = objUuid;
    } else {
        this->m_appliesTo = QString();
    }
}

QString MessageAlert::getFriendlyDescription()
{
    QString msgType = this->messageType();
    QString msgBody = this->messageBody();
    
    // C# MessageAlert has detailed switch statement for each message type
    // TODO: Port full message type handling from MessageAlert.cs lines 132-300
    // For now, return basic message body
    
    if (!msgBody.isEmpty()) {
        return msgBody;
    }
    
    return msgType;
}
