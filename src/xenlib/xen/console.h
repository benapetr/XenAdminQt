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

#ifndef CONSOLE_H
#define CONSOLE_H

#include "xenobject.h"
#include <QMap>

class VM;

/**
 * @brief Console - A console for accessing a VM
 *
 * Qt equivalent of C# XenAPI.Console class. Represents a VM console.
 * First published in XenServer 4.0.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - protocol: Console protocol (rfb, vt100, etc.)
 * - location: Connection location URL
 * - VM: Reference to the VM this console belongs to
 * - other_config: Additional configuration
 */
class XENLIB_EXPORT Console : public XenObject
{
    Q_OBJECT

    public:
        explicit Console(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Console() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Console; }

        // Property accessors (read from cache)
        QString GetProtocol() const;
        QString GetLocation() const;
        QString GetVMRef() const;

        //! Get VM that owns this console
        QSharedPointer<VM> GetVM() const;
};

#endif // CONSOLE_H
