/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef XENAPI_MESSAGE_H
#define XENAPI_MESSAGE_H

#include <QString>
#include <QStringList>
#include "xenlib_global.h"

namespace XenAPI
{
    class Session;

    /**
     * @brief Message - XenAPI message bindings
     *
     * Static-only class providing XenAPI message method bindings.
     */
    class XENLIB_EXPORT Message
    {
        private:
            Message() = delete; // Static-only class

        public:
            /**
             * @brief Destroy a message
             * @param session Active XenSession
             * @param message Message opaque reference
             */
            static void destroy(Session* session, const QString& message);

            /**
             * @brief Destroy several messages in one call (xapi 22.19.0 and newer)
             * @param session Active XenSession
             * @param messages Message opaque references
             * @return Task reference
             */
            static QString async_destroy_many(Session* session, const QStringList& messages);
    };

} // namespace XenAPI

#endif // XENAPI_MESSAGE_H
