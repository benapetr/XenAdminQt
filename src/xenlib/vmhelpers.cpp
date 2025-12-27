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

#include "vmhelpers.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include <QtCore/QDebug>

/**
 * Get the "home" host for a VM
 * Matches: xenadmin/XenModel/XenAPI-Extensions/VM.cs line 457-481
 */
QString VMHelpers::getVMHome(XenConnection* conn, const QVariantMap& vmRecord)
{
    if (!conn || vmRecord.isEmpty())
    {
        return QString();
    }

    XenCache* cache = conn->GetCache();
    if (!cache)
    {
        return QString();
    }

    // 1. Check if this is a snapshot - snapshots have the same "home" as their VM
    bool isSnapshot = vmRecord.value("is_a_snapshot").toBool();
    if (isSnapshot)
    {
        QString snapshotOf = vmRecord.value("snapshot_of").toString();
        if (!snapshotOf.isEmpty() && snapshotOf != "OpaqueRef:NULL")
        {
            QVariantMap parentVM = cache->ResolveObjectData("vm", snapshotOf);
            if (!parentVM.isEmpty())
            {
                return getVMHome(conn, parentVM); // Recursive call
            }
        }
        return QString(); // Parent VM deleted
    }

    // 2. Templates (apart from snapshots) don't have a "home", even if their affinity is set (CA-36286)
    bool isTemplate = vmRecord.value("is_a_template").toBool();
    if (isTemplate)
    {
        return QString();
    }

    // 3. Running or Paused VMs: return resident_on host
    QString powerState = vmRecord.value("power_state").toString();
    if (powerState == "Running" || powerState == "Paused")
    {
        QString residentOn = vmRecord.value("resident_on").toString();
        if (!residentOn.isEmpty() && residentOn != "OpaqueRef:NULL")
        {
            return residentOn;
        }
    }

    // 4. Check for storage host (local storage)
    QString storageHost = getVMStorageHost(conn, vmRecord, false);
    if (!storageHost.isEmpty())
    {
        return storageHost;
    }

    // 5. Check affinity host (if set and live)
    QString affinity = vmRecord.value("affinity").toString();
    if (!affinity.isEmpty() && affinity != "OpaqueRef:NULL")
    {
        QVariantMap affinityHost = cache->ResolveObjectData("host", affinity);
        if (!affinityHost.isEmpty())
        {
            // Check if host is live (enabled field in host record)
            bool enabled = affinityHost.value("enabled").toBool();
            if (enabled)
            {
                return affinity;
            }
        }
    }

    // 6. No home found - this is an offline VM with no affinity
    return QString();
}

/**
 * Get the storage host for a VM
 * Matches: xenadmin/XenModel/XenAPI-Extensions/VM.cs line 517-534
 */
QString VMHelpers::getVMStorageHost(XenConnection *conn, const QVariantMap& vmRecord, bool ignoreCDs)
{
    if (!conn || vmRecord.isEmpty())
    {
        return QString();
    }

    XenCache* cache = conn->GetCache();
    if (!cache)
    {
        return QString();
    }

    // Get VBDs list for this VM
    QVariantList vbds = vmRecord.value("VBDs").toList();

    for (const QVariant& vbdRefVariant : vbds)
    {
        QString vbdRef = vbdRefVariant.toString();
        if (vbdRef.isEmpty() || vbdRef == "OpaqueRef:NULL")
        {
            continue;
        }

        // Resolve VBD
        QVariantMap vbd = cache->ResolveObjectData("vbd", vbdRef);
        if (vbd.isEmpty())
        {
            continue;
        }

        // Skip CDs if requested
        if (ignoreCDs)
        {
            QString vbdType = vbd.value("type").toString();
            if (vbdType == "CD")
            {
                continue;
            }
        }

        // Get VDI from VBD
        QString vdiRef = vbd.value("VDI").toString();
        if (vdiRef.isEmpty() || vdiRef == "OpaqueRef:NULL")
        {
            continue;
        }

        QVariantMap vdi = cache->ResolveObjectData("vdi", vdiRef);
        if (vdi.isEmpty())
        {
            continue;
        }

        // TODO: Check VDI.Show(true) - for now assume all VDIs are shown
        // In C#: if (!theVDI.Show(true)) continue;

        // Get SR from VDI
        QString srRef = vdi.value("SR").toString();
        if (srRef.isEmpty() || srRef == "OpaqueRef:NULL")
        {
            continue;
        }

        QVariantMap sr = cache->ResolveObjectData("sr", srRef);
        if (sr.isEmpty())
        {
            continue;
        }

        // Get storage host from SR
        // SR has a storage host only if it's NOT shared and has exactly 1 PBD
        bool shared = sr.value("shared").toBool();
        if (shared)
        {
            continue; // Shared SRs don't have a single storage host
        }

        QVariantList pbds = sr.value("PBDs").toList();
        if (pbds.count() != 1)
        {
            continue; // SR must have exactly 1 PBD for non-shared storage
        }

        QString pbdRef = pbds.first().toString();
        if (pbdRef.isEmpty() || pbdRef == "OpaqueRef:NULL")
        {
            continue;
        }

        QVariantMap pbd = cache->ResolveObjectData("pbd", pbdRef);
        if (pbd.isEmpty())
        {
            continue;
        }

        QString hostRef = pbd.value("host").toString();
        if (!hostRef.isEmpty() && hostRef != "OpaqueRef:NULL")
        {
            return hostRef; // Found storage host!
        }
    }

    return QString(); // No storage host found
}
