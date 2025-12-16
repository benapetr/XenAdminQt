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

#include "vbd.h"

VBD::VBD(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VBD::vmRef() const
{
    return stringProperty("VM");
}

QString VBD::vdiRef() const
{
    QString vdi = stringProperty("VDI");
    // Return empty string if VDI is NULL reference (CD drive with no disc)
    if (vdi == "OpaqueRef:NULL")
    {
        return QString();
    }
    return vdi;
}

QString VBD::device() const
{
    return stringProperty("device");
}

QString VBD::userdevice() const
{
    return stringProperty("userdevice");
}

bool VBD::bootable() const
{
    return boolProperty("bootable");
}

QString VBD::mode() const
{
    return stringProperty("mode");
}

bool VBD::isReadOnly() const
{
    return mode() == "RO";
}

QString VBD::type() const
{
    return stringProperty("type");
}

bool VBD::isCD() const
{
    return type() == "CD";
}

bool VBD::unpluggable() const
{
    return boolProperty("unpluggable");
}

bool VBD::currentlyAttached() const
{
    return boolProperty("currently_attached");
}

bool VBD::empty() const
{
    return boolProperty("empty");
}

QStringList VBD::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

bool VBD::canPlug() const
{
    return allowedOperations().contains("plug");
}

bool VBD::canUnplug() const
{
    return allowedOperations().contains("unplug");
}

QString VBD::description() const
{
    QString typeStr = isCD() ? "CD Drive" : "Disk";
    QString deviceNum = userdevice();
    QString deviceName = device();

    if (deviceName.isEmpty())
    {
        return QString("%1 %2").arg(typeStr, deviceNum);
    } else
    {
        return QString("%1 %2 (%3)").arg(typeStr, deviceNum, deviceName);
    }
}

QString VBD::objectType() const
{
    return "VBD";
}
