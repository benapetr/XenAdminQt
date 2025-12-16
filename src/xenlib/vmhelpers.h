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

#ifndef VMHELPERS_H
#define VMHELPERS_H

#include "xenlib_global.h"
#include <QtCore/QString>
#include <QtCore/QVariantMap>

class XenLib;

/**
 * @brief Helper functions for VM operations that require cache/connection access
 *
 * These are extension methods that can't be static in XenAPI::VM because they
 * need access to the connection and cache for resolving references.
 * Matches: xenadmin/XenModel/XenAPI-Extensions/VM.cs
 */
class XENLIB_EXPORT VMHelpers
{
public:
    /**
     * @brief Get the "home" host for a VM
     *
     * A VM's "home" is determined by:
     * 1. If it's a snapshot: the home of its snapshot_of VM
     * 2. If it's a template (non-snapshot): null (templates don't have homes)
     * 3. If Running or Paused: the resident_on host
     * 4. If it has local storage: the storage host
     * 5. If it has affinity set: the affinity host (if live)
     * 6. Otherwise: null (offline VM with no affinity)
     *
     * This matches the C# VM.Home() method in VM.cs line 457-481
     *
     * @param xenLib XenLib instance for cache access
     * @param vmRecord VM record containing all VM fields
     * @return Host reference string, or empty string if no home
     */
    static QString getVMHome(XenLib* xenLib, const QVariantMap& vmRecord);

    /**
     * @brief Get the storage host for a VM
     *
     * Returns the host where the VM's storage is located (for non-shared storage).
     * This is determined by finding the first VBD's VDI's SR's storage host.
     *
     * @param xenLib XenLib instance for cache access
     * @param vmRecord VM record containing VBDs list
     * @param ignoreCDs If true, skip CD/DVD drives
     * @return Host reference string, or empty string if no storage host
     */
    static QString getVMStorageHost(XenLib* xenLib, const QVariantMap& vmRecord, bool ignoreCDs = false);

private:
    VMHelpers() = delete; // Static class, no instances
    ~VMHelpers() = delete;
};

#endif // VMHELPERS_H
