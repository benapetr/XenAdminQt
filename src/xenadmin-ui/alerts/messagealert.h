/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#ifndef MESSAGEALERT_H
#define MESSAGEALERT_H

#include "alert.h"
#include <QVariantMap>

class XenConnection;

/**
 * Alert based on XenAPI Message object.
 * C# Reference: XenAdmin/Alerts/Types/MessageAlert.cs
 * 
 * This is the primary alert type representing XenServer/XCP-ng
 * messages (notifications, warnings, errors) from the XenAPI.
 */
class MessageAlert : public Alert
{
public:
    explicit MessageAlert(XenConnection* connection, const QVariantMap& messageData);
    ~MessageAlert() override = default;

    // Alert interface implementation
    QString title() const override;
    QString description() const override;
    AlertPriority priority() const override;
    QString appliesTo() const override;
    QString name() const override;
    void dismiss() override;
    
    // Access to underlying message data
    QVariantMap messageData() const { return this->m_messageData; }
    QString messageType() const;
    QString messageBody() const;
    QString objUuid() const;
    QString opaqueRef() const;
    
    // Factory method to create appropriate alert type based on message type
    // C# Reference: MessageAlert.cs line 462 - ParseMessage()
    static Alert* parseMessage(XenConnection* connection, const QVariantMap& messageData);
    
    // Remove alert when message is destroyed
    static void removeAlert(const QString& messageRef);

protected:
    QVariantMap m_messageData;
    QString m_title;
    QString m_description;
    AlertPriority m_priority;
    QString m_appliesTo;
    QString m_name;
    
    virtual void parseMessageData();
    virtual QString getFriendlyDescription();
};

#endif // MESSAGEALERT_H
