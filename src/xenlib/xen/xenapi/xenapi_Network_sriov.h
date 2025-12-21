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

#ifndef XENAPI_NETWORKSRIOV_H
#define XENAPI_NETWORKSRIOV_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief Network_sriov XenAPI bindings
     *
     * Static-only class providing XenServer Network_sriov API calls for SR-IOV configuration.
     * Matches C# XenModel/XenAPI/Network_sriov.cs structure.
     */
    class Network_sriov
    {
        private:
            Network_sriov() = delete; // Static-only class

        public:
            // SR-IOV operations
            static QString async_create(XenSession* session, const QString& pif, const QString& network);
            static QString async_destroy(XenSession* session, const QString& network_sriov);

            // SR-IOV queries
            static QVariantMap get_record(XenSession* session, const QString& network_sriov);
            static QVariantList get_all(XenSession* session);
    };

} // namespace XenAPI

#endif // XENAPI_NETWORKSRIOV_H
