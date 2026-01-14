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
        QString GetTitle() const override;
        QString GetDescription() const override;
        AlertPriority GetPriority() const override;
        QString AppliesTo() const override;
        QString GetName() const override;
        void Dismiss() override;

        // Access to underlying message data
        QVariantMap GetMessageData() const { return this->m_messageData; }
        QString GetMessageType() const;
        QString GetMessageBody() const;
        QString GetObjUUID() const;
        QString GetOpaqueRef() const;

        // Factory method to create appropriate alert type based on message type
        // C# Reference: MessageAlert.cs line 462 - ParseMessage()
        static Alert* ParseMessage(XenConnection* connection, const QVariantMap& messageData);

        // Remove alert when message is destroyed
        static void RemoveAlert(const QString& messageRef);

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
