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

#ifndef PUSB_H
#define PUSB_H

#include "xenobject.h"
#include <QString>
#include <QMap>
#include <QSharedPointer>

class Host;
class USBGroup;

/**
 * @brief PUSB - A physical USB device
 *
 * Qt equivalent of C# PUSB class.
 * First published in XenServer 7.3.
 */
class XENLIB_EXPORT PUSB : public XenObject
{
    Q_OBJECT

    public:
        explicit PUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~PUSB();

        QString GetObjectType() const override { return "PUSB"; }

        // Property accessors
        QString GetUSBGroupRef() const;
        QString GetHostRef() const;
        QString Path() const;
        QString VendorId() const;
        QString VendorDesc() const;
        QString ProductId() const;
        QString ProductDesc() const;
        QString Serial() const;
        QString Version() const;
        QString Description() const;
        bool PassthroughEnabled() const;
        double Speed() const;

        // Object resolution getters
        QSharedPointer<USBGroup> GetUSBGroup() const;
        QSharedPointer<Host> GetHost() const;
};

#endif // PUSB_H
