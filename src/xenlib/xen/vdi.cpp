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

#include "vdi.h"
#include <QLocale>

VDI::VDI(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

qint64 VDI::virtualSize() const
{
    return longProperty("virtual_size");
}

qint64 VDI::physicalUtilisation() const
{
    return longProperty("physical_utilisation");
}

QString VDI::type() const
{
    return stringProperty("type");
}

bool VDI::sharable() const
{
    return boolProperty("sharable");
}

bool VDI::readOnly() const
{
    return boolProperty("read_only");
}

QString VDI::srRef() const
{
    return stringProperty("SR");
}

QStringList VDI::vbdRefs() const
{
    return stringListProperty("VBDs");
}

bool VDI::isInUse() const
{
    return !vbdRefs().isEmpty();
}

QString VDI::sizeString() const
{
    qint64 size = virtualSize();

    if (size < 0)
    {
        return "Unknown";
    }

    // Format as human-readable size
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (size >= TB)
    {
        return QString("%1 TB").arg(QLocale().toString(size / (double) TB, 'f', 2));
    } else if (size >= GB)
    {
        return QString("%1 GB").arg(QLocale().toString(size / (double) GB, 'f', 2));
    } else if (size >= MB)
    {
        return QString("%1 MB").arg(QLocale().toString(size / (double) MB, 'f', 2));
    } else if (size >= KB)
    {
        return QString("%1 KB").arg(QLocale().toString(size / (double) KB, 'f', 2));
    } else
    {
        return QString("%1 bytes").arg(size);
    }
}

QString VDI::snapshotOfRef() const
{
    return stringProperty("snapshot_of");
}

bool VDI::isSnapshot() const
{
    QString snapshotOf = snapshotOfRef();
    return !snapshotOf.isEmpty() && snapshotOf != "OpaqueRef:NULL";
}

QString VDI::objectType() const
{
    return "vdi";
}
