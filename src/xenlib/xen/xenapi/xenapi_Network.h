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

#ifndef XENAPI_NETWORK_H
#define XENAPI_NETWORK_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief Network XenAPI bindings
     *
     * Static-only class providing XenServer Network API calls.
     * Matches C# XenModel/XenAPI/Network.cs structure.
     */
    class Network
    {
        private:
            Network() = delete; // Static-only class

        public:
            // Network creation and destruction
            static QString async_create(XenSession* session, const QVariantMap& record);
            static void destroy(XenSession* session, const QString& network);

            // Network configuration
            static void set_name_label(XenSession* session, const QString& network, const QString& label);
            static void set_name_description(XenSession* session, const QString& network, const QString& description);
            static void set_MTU(XenSession* session, const QString& network, qint64 mtu);

            // other_config management
            static void add_to_other_config(XenSession* session, const QString& network, const QString& key, const QString& value);
            static void remove_from_other_config(XenSession* session, const QString& network, const QString& key);

            // Network queries
            static QVariantMap get_record(XenSession* session, const QString& network);
            static QVariantList get_all(XenSession* session);
            static QVariantList get_PIFs(XenSession* session, const QString& network);
    };

} // namespace XenAPI

#endif // XENAPI_NETWORK_H
