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

#ifndef XENAPI_SECRET_H
#define XENAPI_SECRET_H

#include "../session.h"
#include <QString>

namespace XenAPI
{

    /**
     * @brief Secret - XenAPI Secret bindings
     *
     * Static-only class providing XenAPI Secret method bindings.
     * Secrets are used to store sensitive data like passwords.
     */
    class Secret
    {
        private:
            Secret() = delete; // Static-only class

        public:
            /**
             * @brief Create a new secret
             * @param session Active XenSession
             * @param value Secret value (e.g., password)
             * @return Secret UUID
             */
            static QString create(XenSession* session, const QString& value);

            /**
             * @brief Get secret by UUID
             * @param session Active XenSession
             * @param uuid Secret UUID
             * @return Secret opaque reference
             */
            static QString get_by_uuid(XenSession* session, const QString& uuid);

            /**
             * @brief Destroy a secret
             * @param session Active XenSession
             * @param secret Secret opaque reference
             */
            static void destroy(XenSession* session, const QString& secret);
    };

} // namespace XenAPI

#endif // XENAPI_SECRET_H
