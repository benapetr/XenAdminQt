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

#ifndef PROPERTYACCESSORHELPER_H
#define PROPERTYACCESSORHELPER_H

#include <QString>

// Forward declarations
class VM;
class Host;
class VDI;
class Pool;
class SR;

namespace XenSearch
{
    /**
     * PropertyAccessorHelper - Provides metric-based property accessors
     *
     * C# Equivalent: PropertyAccessorHelper.cs
     *
     * Provides utility methods for:
     * - CPU usage strings and values (VM/Host)
     * - Memory usage strings and values (VM/Host/VDI)
     * - Network usage strings (VM/Host)
     * - Disk usage strings (VM)
     * - HA status strings (VM/Pool/SR)
     *
     * NOTE: Currently returns placeholder values since MetricUpdater is not yet ported.
     *       In the future, this will integrate with the real-time metrics system.
     */
    class PropertyAccessorHelper
    {
        public:
            // VM CPU usage
            static QString vmCpuUsageString(VM* vm);
            static int vmCpuUsageRank(VM* vm);

            // VM Memory usage
            static QString vmMemoryUsageString(VM* vm);
            static int vmMemoryUsageRank(VM* vm);
            static double vmMemoryUsageValue(VM* vm);

            // VM Network usage
            static QString vmNetworkUsageString(VM* vm);

            // VM Disk usage
            static QString vmDiskUsageString(VM* vm);

            // Host CPU usage
            static QString hostCpuUsageString(Host* host);
            static int hostCpuUsageRank(Host* host);

            // Host Memory usage
            static QString hostMemoryUsageString(Host* host);
            static int hostMemoryUsageRank(Host* host);
            static double hostMemoryUsageValue(Host* host);

            // Host Network usage
            static QString hostNetworkUsageString(Host* host);

            // VDI Memory usage
            static QString vdiMemoryUsageString(VDI* vdi);

            // HA Status
            static QString GetPoolHAStatus(Pool* pool);
            static QString GetSRHAStatus(SR* sr);
            static QString GetVMHAStatus(VM* vm);
    };
} // namespace XenSearch

#endif // PROPERTYACCESSORHELPER_H
