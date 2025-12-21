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

#ifndef XENAPI_PIF_H
#define XENAPI_PIF_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief PIF (Physical Interface) XenAPI bindings
     *
     * Static-only class providing XenServer PIF API calls.
     * Matches C# XenModel/XenAPI/PIF.cs structure.
     */
    class PIF
    {
        private:
            PIF() = delete; // Static-only class

        public:
            // PIF configuration
            static void reconfigure_ip(XenSession* session, const QString& pif,
                                       const QString& mode, const QString& ip,
                                       const QString& netmask, const QString& gateway,
                                       const QString& dns);
            static QString async_reconfigure_ip(XenSession* session, const QString& pif,
                                                const QString& mode, const QString& ip,
                                                const QString& netmask, const QString& gateway,
                                                const QString& dns);
            static void plug(XenSession* session, const QString& pif);
            static void unplug(XenSession* session, const QString& pif);

            static QString async_plug(XenSession* session, const QString& pif);
            static QString async_unplug(XenSession* session, const QString& pif);

            // PIF properties
            static void set_disallow_unplug(XenSession* session, const QString& pif, bool value);
            static void set_property(XenSession* session, const QString& pif, const QString& name, const QString& value);
            static void add_to_other_config(XenSession* session, const QString& pif, const QString& key, const QString& value);
            static void remove_from_other_config(XenSession* session, const QString& pif, const QString& key);

            // PIF queries
            static QVariantMap get_record(XenSession* session, const QString& pif);
            static QVariantList get_all(XenSession* session);
            static QString get_network(XenSession* session, const QString& pif);
            static QString get_host(XenSession* session, const QString& pif);

            // PIF operations
            /**
             * @brief Scan for new physical interfaces on a host
             * @param session XenSession
             * @param host Host opaque reference
             *
             * Matches C# PIF.scan()
             */
            static void scan(XenSession* session, const QString& host);
    };

} // namespace XenAPI

#endif // XENAPI_PIF_H
