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

#include "sr.h"
#include "network/connection.h"
#include "host.h"
#include "../xenlib.h"
#include "../xencache.h"

SR::SR(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString SR::objectType() const
{
    return "sr";
}

QString SR::type() const
{
    return stringProperty("type");
}

bool SR::shared() const
{
    return boolProperty("shared", false);
}

qint64 SR::physicalSize() const
{
    return longProperty("physical_size", 0);
}

qint64 SR::physicalUtilisation() const
{
    return longProperty("physical_utilisation", 0);
}

qint64 SR::virtualAllocation() const
{
    return longProperty("virtual_allocation", 0);
}

qint64 SR::freeSpace() const
{
    return physicalSize() - physicalUtilisation();
}

QStringList SR::vdiRefs() const
{
    return stringListProperty("VDIs");
}

QStringList SR::pbdRefs() const
{
    return stringListProperty("PBDs");
}

QString SR::contentType() const
{
    return stringProperty("content_type", "user");
}

QVariantMap SR::otherConfig() const
{
    return property("other_config").toMap();
}

QVariantMap SR::smConfig() const
{
    return property("sm_config").toMap();
}

QStringList SR::tags() const
{
    return stringListProperty("tags");
}

QStringList SR::allowedOperations() const
{
    return stringListProperty("allowed_operations");
}

QVariantMap SR::currentOperations() const
{
    return property("current_operations").toMap();
}

bool SR::supportsTrim() const
{
    // Check sm_config for trim support
    QVariantMap sm = smConfig();
    return sm.value("supports_trim", false).toBool();
}

QString SR::homeRef() const
{
    // For local SRs, find the host via PBD connections
    // For shared SRs, could return any connected host or empty
    QStringList pbds = pbdRefs();
    if (pbds.isEmpty())
        return QString();

    // Get first PBD and extract host reference from it
    // TODO: Query cache for PBD.host
    // For now, return empty - will be implemented when PBD class is added
    return QString();
}

Host* SR::getFirstAttachedStorageHost() const
{
    QStringList pbds = pbdRefs();
    if (pbds.isEmpty())
        return nullptr;

    // Iterate through PBDs to find first currently_attached one
    XenCache* cache = connection()->getCache();
    if (!cache)
        return nullptr;

    for (const QString& pbdRef : pbds)
    {
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);
        if (pbdData.isEmpty())
            continue;

        bool currentlyAttached = pbdData.value("currently_attached", false).toBool();
        if (currentlyAttached)
        {
            QString hostRef = pbdData.value("host").toString();
            if (!hostRef.isEmpty())
            {
                return new Host(connection(), hostRef, nullptr);
            }
        }
    }

    return nullptr;
}
