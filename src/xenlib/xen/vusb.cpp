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

#include "vusb.h"
#include "network/connection.h"
#include "../xencache.h"

VUSB::VUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString VUSB::GetObjectType() const
{
    return "vusb";
}

QString VUSB::Uuid() const
{
    return this->stringProperty("uuid");
}

QStringList VUSB::AllowedOperations() const
{
    return this->property("allowed_operations").toStringList();
}

QVariantMap VUSB::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

QString VUSB::VMRef() const
{
    return this->stringProperty("VM");
}

QString VUSB::USBGroupRef() const
{
    return this->stringProperty("USB_group");
}

QVariantMap VUSB::OtherConfig() const
{
    return this->property("other_config").toMap();
}

bool VUSB::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

// Helper methods
bool VUSB::IsAttached() const
{
    return this->CurrentlyAttached();
}
