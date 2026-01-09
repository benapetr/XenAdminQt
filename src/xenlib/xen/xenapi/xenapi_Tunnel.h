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

#ifndef XENAPI_TUNNEL_H
#define XENAPI_TUNNEL_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace XenAPI
{
    class Session;

    /**
     * @brief Tunnel XenAPI bindings
     *
     * Static-only class providing XenServer Tunnel API calls.
     * Matches C# XenModel/XenAPI/Tunnel.cs structure.
     *
     * First published in XenServer 5.6 FP1.
     */
    class Tunnel
    {
        private:
            Tunnel() = delete; // Static-only class

        public:
            /**
             * @brief Destroy a tunnel
             * @param session Active XenSession
             * @param tunnel Tunnel opaque reference
             *
             * First published in XenServer 5.6 FP1.
             */
            static void destroy(Session* session, const QString& tunnel);

            /**
             * @brief Destroy a tunnel (async)
             * @param session Active XenSession
             * @param tunnel Tunnel opaque reference
             * @return Task reference
             *
             * First published in XenServer 5.6 FP1.
             */
            static QString async_destroy(Session* session, const QString& tunnel);

            /**
             * @brief Get all tunnel references
             * @param session Active XenSession
             * @return List of tunnel references
             */
            static QVariantList get_all(Session* session);

            /**
             * @brief Get all tunnel records
             * @param session Active XenSession
             * @return Map of tunnel refs to tunnel records
             */
            static QVariantMap get_all_records(Session* session);
    };

} // namespace XenAPI

#endif // XENAPI_TUNNEL_H
