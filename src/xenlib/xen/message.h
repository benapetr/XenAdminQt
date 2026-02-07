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

#ifndef MESSAGE_H
#define MESSAGE_H

#include "xenobject.h"
#include <QDateTime>

/**
 * @brief Message - A message for the attention of the administrator
 *
 * Qt equivalent of C# XenAPI.Message class. Represents an administrative message.
 * First published in XenServer 5.0.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - name: The name of the message
 * - priority: The message priority (0 being low priority)
 * - cls: The class of object this message is associated with
 * - obj_uuid: The uuid of the object this message is associated with
 * - timestamp: The time at which the message was created
 * - body: The body of the message
 */
class XENLIB_EXPORT Message : public XenObject
{
    Q_OBJECT

    public:
        explicit Message(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Message() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Message; }

        //! Get the name of the message
        QString Name() const;

        //! Get the message priority (0 being low priority)
        qint64 Priority() const;

        //! Get the class of object this message is associated with
        QString Cls() const;

        //! Get the uuid of the object this message is associated with
        QString ObjUuid() const;

        //! Get the time at which the message was created
        QDateTime Timestamp() const;

        //! Get message timestamp adjusted for server offset and converted to local time
        QDateTime TimestampLocal() const;

        //! Get the body of the message
        QString Body() const;

        //! Check whether this message type should be displayed on performance graphs
        bool ShowOnGraphs() const;

        //! Check whether this message type should be hidden from alerts/events views
        bool IsSquelched() const;
};

#endif // MESSAGE_H
