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

#ifndef USBGROUP_H
#define USBGROUP_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSharedPointer>

class PUSB;
class VUSB;

/**
 * @brief USBGroup - A group of compatible USBs across the resource pool
 *
 * Qt equivalent of C# XenAPI.USB_group class.
 * First published in XenServer 7.3.
 */
class XENLIB_EXPORT USBGroup : public XenObject
{
    Q_OBJECT

    public:
        explicit USBGroup(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~USBGroup();

        QString GetObjectType() const override { return "USB_group"; }

        // Property accessors
        QString GetUuid() const;
        QString GetNameLabel() const;
        QString GetNameDescription() const;
        QStringList GetPUSBRefs() const;
        QStringList GetVUSBRefs() const;

        // Object resolution getters
        QList<QSharedPointer<PUSB>> GetPUSBs() const;
        QList<QSharedPointer<VUSB>> GetVUSBs() const;
};

#endif // USBGROUP_H
