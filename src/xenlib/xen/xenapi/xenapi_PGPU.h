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

#ifndef XENAPI_PGPU_H
#define XENAPI_PGPU_H

#include <QString>
#include <QStringList>

namespace XenAPI
{
    class Session;
}

namespace XenAPI
{

/**
 * @brief Static XenAPI bindings for PGPU (Physical GPU) operations
 *
 * Provides static methods for PGPU management matching the C# XenAPI.PGPU class.
 * All methods are synchronous and block until the API call completes.
 *
 * Physical GPUs (PGPUs) represent GPU devices on XenServer hosts. They can be
 * configured to support different VGPU types, allowing VMs to use virtual GPUs.
 *
 * C# equivalent: XenAPI.PGPU
 */
class PGPU
{
    private:
        PGPU() = delete; // Static-only class

    public:
        /**
         * @brief Set the enabled VGPU types for a physical GPU
         *
         * Configures which VGPU types can be created on this PGPU. Only enabled
         * VGPU types can be used by VMs.
         *
         * First published in XenServer 6.2 SP1 Tech-Preview.
         *
         * @param session XenServer session
         * @param pgpu PGPU opaque reference
         * @param value List of VGPU_type opaque references to enable
         * @throws std::runtime_error if the API call fails
         *
         * C# equivalent: PGPU.set_enabled_VGPU_types()
         */
        static void set_enabled_VGPU_types(Session* session, const QString& pgpu, const QStringList& value);

        /**
         * @brief Asynchronously set the enabled VGPU types for a physical GPU
         *
         * Returns immediately with a task reference. Use Task polling to track completion.
         *
         * First published in XenServer 6.2 SP1 Tech-Preview.
         *
         * @param session XenServer session
         * @param pgpu PGPU opaque reference
         * @param value List of VGPU_type opaque references to enable
         * @return Task opaque reference
         * @throws std::runtime_error if the API call fails
         *
         * C# equivalent: PGPU.async_set_enabled_VGPU_types()
         */
        static QString async_set_enabled_VGPU_types(Session* session, const QString& pgpu, const QStringList& value);
};

} // namespace XenAPI

#endif // XENAPI_PGPU_H
