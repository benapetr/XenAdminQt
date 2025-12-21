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

#ifndef XENAPI_PBD_H
#define XENAPI_PBD_H

#include "../session.h"
#include <QString>
#include <QVariantMap>

namespace XenAPI
{

    /**
     * @brief PBD - XenAPI Physical Block Device bindings
     *
     * Static-only class providing XenAPI PBD method bindings.
     * PBDs represent the connection between a host and an SR.
     */
    class PBD
    {
        private:
            PBD() = delete; // Static-only class

        public:
            /**
             * @brief Get PBD record
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             * @return PBD record (all fields)
             */
            static QVariantMap get_record(XenSession* session, const QString& pbd);

            /**
             * @brief Check if PBD is currently attached
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             * @return true if attached
             */
            static bool get_currently_attached(XenSession* session, const QString& pbd);

            /**
             * @brief Create a new PBD (async)
             * @param session Active XenSession
             * @param record PBD record (SR, host, device_config, currently_attached)
             * @return Task reference
             */
            static QString async_create(XenSession* session, const QVariantMap& record);

            /**
             * @brief Plug a PBD (async)
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             * @return Task reference
             */
            static QString async_plug(XenSession* session, const QString& pbd);

            /**
             * @brief Plug a PBD (sync)
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             */
            static void plug(XenSession* session, const QString& pbd);

            /**
             * @brief Unplug a PBD (sync)
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             */
            static void unplug(XenSession* session, const QString& pbd);

            /**
             * @brief Unplug a PBD (async)
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             * @return Task reference
             */
            static QString async_unplug(XenSession* session, const QString& pbd);

            /**
             * @brief Destroy a PBD (async)
             * @param session Active XenSession
             * @param pbd PBD opaque reference
             * @return Task reference
             */
            static QString async_destroy(XenSession* session, const QString& pbd);
    };

} // namespace XenAPI

#endif // XENAPI_PBD_H
