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

#ifndef EVENT_H
#define EVENT_H

#include "xenobject.h"
#include <QVariant>

/**
 * @brief Event - XenServer event notification
 *
 * Qt equivalent of C# XenAPI.Event class. Represents an event from the XenServer event system.
 *
 * Key properties:
 * - id: Event identifier
 * - timestamp: When the event occurred
 * - class_: Object class that generated the event (vm, host, etc.)
 * - operation: Operation type (add, del, mod)
 * - opaqueRef: Reference to the object that changed
 * - snapshot: Snapshot of object state after the event
 */
class XENLIB_EXPORT Event : public XenObject
{
    Q_OBJECT

    public:
        explicit Event(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Event() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Event; }

        // Property accessors (read from cache)
        qint64 EventId() const;
        QString Timestamp() const;
        QString Class() const;
        QString Operation() const;
        QString ObjectRef() const;
        QVariant Snapshot() const;
};

#endif // EVENT_H
