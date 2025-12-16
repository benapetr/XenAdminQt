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

#ifndef XENAPI_VIF_H
#define XENAPI_VIF_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief VIF (Virtual Network Interface) XenAPI bindings
     *
     * Static-only class providing XenServer VIF API calls.
     * Matches C# XenModel/XenAPI/VIF.cs structure.
     */
    class VIF
    {
    private:
        VIF() = delete; // Static-only class

    public:
        /**
         * @brief Create a new VIF asynchronously
         * @param session XenSession
         * @param vifRecord VIF record with VM, network, device, MAC, MTU, etc.
         * @return Task reference
         *
         * Matches C# VIF.async_create()
         */
        static QString async_create(XenSession* session, const QVariantMap& vifRecord);

        /**
         * @brief Destroy a VIF
         * @param session XenSession
         * @param vif VIF opaque reference
         *
         * Matches C# VIF.destroy()
         */
        static void destroy(XenSession* session, const QString& vif);

        /**
         * @brief Plug (hot-plug) a VIF
         * @param session XenSession
         * @param vif VIF opaque reference
         *
         * Matches C# VIF.plug()
         */
        static void plug(XenSession* session, const QString& vif);

        /**
         * @brief Unplug a VIF
         * @param session XenSession
         * @param vif VIF opaque reference
         *
         * Matches C# VIF.unplug()
         */
        static void unplug(XenSession* session, const QString& vif);

        /**
         * @brief Get allowed operations for a VIF
         * @param session XenSession
         * @param vif VIF opaque reference
         * @return List of allowed vif_operations (e.g., "plug", "unplug")
         *
         * Matches C# VIF.get_allowed_operations()
         */
        static QStringList get_allowed_operations(XenSession* session, const QString& vif);

        /**
         * @brief Get VIF record
         * @param session XenSession
         * @param vif VIF opaque reference
         * @return VIF record as QVariantMap
         *
         * Matches C# VIF.get_record()
         */
        static QVariantMap get_record(XenSession* session, const QString& vif);

        /**
         * @brief Get all VIF references
         * @param session XenSession
         * @return List of VIF references
         *
         * Matches C# VIF.get_all()
         */
        static QVariantList get_all(XenSession* session);
    };

} // namespace XenAPI

#endif // XENAPI_VIF_H
