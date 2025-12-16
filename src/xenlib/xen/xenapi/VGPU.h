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

#ifndef VGPU_H
#define VGPU_H

#include <QString>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief VGPU - Virtual GPU management
     *
     * Static class providing XenAPI bindings for VGPU operations.
     * Manages virtual GPU assignments to VMs.
     */
    class VGPU
    {
    private:
        VGPU() = delete; // Static-only class

    public:
        /**
         * @brief Destroy a VGPU
         * @param session Active XenSession
         * @param vgpu VGPU opaque reference
         */
        static void destroy(XenSession* session, const QString& vgpu);

        /**
         * @brief Create a VGPU asynchronously
         * @param session Active XenSession
         * @param vm VM opaque reference
         * @param gpu_group GPU group opaque reference
         * @param device Device number (usually "0")
         * @param other_config Additional configuration
         * @return Task reference
         */
        static QString async_create(XenSession* session, const QString& vm,
                                    const QString& gpu_group, const QString& device,
                                    const QVariantMap& other_config);

        /**
         * @brief Create a VGPU asynchronously with type
         * @param session Active XenSession
         * @param vm VM opaque reference
         * @param gpu_group GPU group opaque reference
         * @param device Device number (usually "0")
         * @param other_config Additional configuration
         * @param vgpu_type VGPU type opaque reference
         * @return Task reference
         */
        static QString async_create(XenSession* session, const QString& vm,
                                    const QString& gpu_group, const QString& device,
                                    const QVariantMap& other_config, const QString& vgpu_type);
    };

} // namespace XenAPI

#endif // VGPU_H
