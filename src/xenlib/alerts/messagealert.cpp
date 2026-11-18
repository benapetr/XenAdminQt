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
#include "../xen/network/connection.h"
#include "../xen/xenobject.h"
#include "../xen/xenobjecttype.h"
#include "../xencache.h"
#include <QDateTime>

using namespace XenLib;

MessageAlert::MessageAlert(XenConnection* connection, const QVariantMap& messageData) : Alert(connection), m_messageData(messageData), m_priority(AlertPriority::Unknown)
{
    this->parseMessageData();
}

QString MessageAlert::GetTitle() const
{
    return this->m_title;
}

QString MessageAlert::GetDescription() const
{
    return this->m_description;
}

AlertPriority MessageAlert::GetPriority() const
{
    return this->m_priority;
}

QString MessageAlert::AppliesTo() const
{
    return this->m_appliesTo;
}

QString MessageAlert::GetName() const
{
    return this->m_name;
}

void MessageAlert::Dismiss()
{
    // C# MessageAlert.Dismiss() calls Message.destroy synchronously. Here the server-side
    // deletion is done by DismissAlertsAction (off the UI thread), which the UI runs for
    // message alerts; this only marks the alert locally.
    this->m_dismissing = true;
}

bool MessageAlert::IsAllowedToDismiss() const
{
    // C# Reference: MessageAlert.AllowedToDismiss()
    if (this->m_dismissing)
        return false;

    // this shouldn't happen for this type of alert, but check for safety
    if (!this->m_connection)
        return true;

    // if we are disconnected do not dismiss as the alert will disappear soon
    if (!this->m_connection->IsConnected())
        return false;

    // TODO: C# also requires the session to hold a role allowed to call Message.destroy
    // (unless it's the local superuser). Add once RBAC role checks are wired up.
    return true;
}

QString MessageAlert::GetMessageType() const
{
    return this->m_messageData.value("name").toString();
}

QString MessageAlert::GetMessageBody() const
{
    return this->m_messageData.value("body").toString();
}

QString MessageAlert::GetObjUUID() const
{
    return this->m_messageData.value("obj_uuid").toString();
}

QString MessageAlert::GetOpaqueRef() const
{
    return this->m_messageData.value("ref").toString();
}

QString MessageAlert::GetObjClass() const
{
    return this->m_messageData.value("cls").toString();
}

QSharedPointer<XenObject> MessageAlert::GetXenObject() const
{
    // C# Reference: Message.GetXenObject() - resolves cls + obj_uuid via the cache
    if (!this->m_connection)
        return QSharedPointer<XenObject>();

    XenCache* cache = this->m_connection->GetCache();
    const QString objUuid = this->GetObjUUID();
    if (!cache || objUuid.isEmpty())
        return QSharedPointer<XenObject>();

    const XenObjectType type = XenObjectTypeFromString(this->GetObjClass());
    if (type == XenObjectType::Null)
        return QSharedPointer<XenObject>();

    const QList<QSharedPointer<XenObject>> objects = cache->GetAll(type);
    for (const QSharedPointer<XenObject>& object : objects)
    {
        if (object && object->IsValid() && object->GetUUID() == objUuid)
            return object;
    }

    return QSharedPointer<XenObject>();
}

Alert* MessageAlert::ParseMessage(XenConnection* connection, const QVariantMap& messageData)
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

void MessageAlert::RemoveAlert(const QString& messageRef)
{
    // C# Reference: MessageAlert.cs line 447 - RemoveWithMessage()
    // Find and remove alert associated with this message opaque_ref
    
    Alert* alert = AlertManager::instance()->FindAlert([messageRef](Alert* a)
    {
        MessageAlert* msgAlert = dynamic_cast<MessageAlert*>(a);
        if (!msgAlert)
            return false;
        return msgAlert->GetOpaqueRef() == messageRef;
    });
    
    if (alert)
    {
        AlertManager::instance()->RemoveAlert(alert);
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
    QString msgType = this->GetMessageType();
    QString msgBody = this->GetMessageBody();
    
    // Set title to message type
    this->m_title = msgType;
    this->m_name = msgType;
    
    // Set description
    this->m_description = this->getFriendlyDescription();
    
    // Extract obj_uuid for appliesTo
    QString objUuid = this->GetObjUUID();
    if (!objUuid.isEmpty()) {
        this->m_appliesTo = objUuid;
    } else {
        this->m_appliesTo = QString();
    }
}

QString MessageAlert::getFriendlyDescription()
{
    QString msgType = this->GetMessageType();
    QString msgBody = this->GetMessageBody();
    
    // C# MessageAlert has detailed switch statement for each message type
    // TODO: Port full message type handling from MessageAlert.cs lines 132-300
    // For now, return basic message body
    
    if (!msgBody.isEmpty()) {
        return msgBody;
    }
    
    return msgType;
}
