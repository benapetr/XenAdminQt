/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#include "messagealert.h"
#include "alertmanager.h"
#include "../../xenlib/xen/connection.h"
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
    
    // TODO: Implement specialized alert types
    // For now, all messages create basic MessageAlert
    // Future: Add AlarmMessageAlert, PolicyAlert, CertificateAlert, etc.
    
    /*
    if (msgType == "ALARM")
        return new AlarmMessageAlert(connection, messageData);
    else if (msgType.startsWith("VMSS_"))
        return new PolicyAlert(connection, messageData);
    else if (msgType.contains("CERTIFICATE"))
        return new CertificateAlert(connection, messageData);
    else if (msgType.contains("UPDATES_FEATURE_EXPIR"))
        return new CssExpiryAlert(connection, messageData);
    else if (msgType == "FAILED_LOGIN_ATTEMPTS")
        return new FailedLoginAttemptAlert(connection, messageData);
    else if (msgType.contains("LEAF_COALESCE"))
        return new LeafCoalesceAlert(connection, messageData);
    */
    
    // Default: basic MessageAlert
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
