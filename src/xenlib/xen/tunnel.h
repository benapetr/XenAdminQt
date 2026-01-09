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

#ifndef TUNNEL_H
#define TUNNEL_H

#include "xenobject.h"

/**
 * @brief Tunnel - A tunnel for network traffic
 *
 * Qt equivalent of C# XenAPI.Tunnel class. Represents a network tunnel.
 *
 * Key properties (from C# Tunnel class):
 * - uuid
 * - access_PIF (access PIF reference)
 * - transport_PIF (transport PIF reference)
 * - status (status map)
 * - other_config (additional configuration)
 * - protocol (tunnel protocol - gre/vxlan)
 *
 * First published in XenServer 5.6 FP1.
 */
class XENLIB_EXPORT Tunnel : public XenObject
{
    Q_OBJECT

    public:
        explicit Tunnel(XenConnection* connection,
                        const QString& opaqueRef,
                        QObject* parent = nullptr);
        ~Tunnel() override = default;

        /**
         * @brief Get access PIF reference
         * @return Opaque reference to access PIF
         */
        QString GetAccessPIFRef() const;

        /**
         * @brief Get transport PIF reference
         * @return Opaque reference to transport PIF
         */
        QString GetTransportPIFRef() const;

        /**
         * @brief Get status dictionary
         * @return GetStatus key-value pairs
         */
        QVariantMap GetStatus() const;

        /**
         * @brief Get protocol
         * @return Tunnel protocol (e.g., "gre", "vxlan")
         */
        QString GetProtocol() const;

        // XenObject interface
        QString GetObjectType() const override;
};

#endif // TUNNEL_H
