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

#ifndef NETWORK_H
#define NETWORK_H

#include "xenobject.h"

class VIF;
class PIF;

/**
 * @brief Network - A virtual network
 *
 * Qt equivalent of C# XenAPI.Network class. Represents a virtual network.
 *
 * Key properties (from C# Network class):
 * - name_label, name_description
 * - bridge (Linux bridge name)
 * - managed (whether bridge is managed by xapi)
 * - MTU (maximum transmission unit)
 * - VIFs (virtual network interfaces connected to this network)
 * - PIFs (physical network interfaces connected to this network)
 * - other_config, tags
 */
class XENLIB_EXPORT Network : public XenObject
{
    Q_OBJECT

    public:
        explicit Network(XenConnection* connection,
                         const QString& opaqueRef,
                         QObject* parent = nullptr);
        ~Network() override = default;

        /**
         * @brief Get Bridge name
         * @return Linux Bridge name (e.g., "xenbr0")
         */
        QString Bridge() const;

        /**
         * @brief Check if bridge is IsManaged by xapi
         * @return true if IsManaged by xapi, false if external bridge
         */
        bool IsManaged() const;

        /**
         * @brief Get MTU (Maximum Transmission Unit)
         * @return MTU in bytes
         */
        qint64 GetMTU() const;

        /**
         * @brief Get list of VIF references
         * @return List of VIF opaque references
         */
        QStringList GetVIFRefs() const;

        /**
         * @brief Get list of PIF references
         * @return List of PIF opaque references
         */
        QStringList GetPIFRefs() const;

        /**
         * @brief Get human-readable name
         * @return Network name label
         */
        QString NameLabel() const;

        /**
         * @brief Get human-readable description
         * @return Network description
         */
        QString Description() const;

        /**
         * @brief Get other_config dictionary
         * @return Other configuration key-value pairs
         */
        QVariantMap OtherConfig() const;

        /**
         * @brief Get allowed operations
         * @return List of allowed network operations
         */
        QStringList AllowedOperations() const;

        /**
         * @brief Get current operations
         * @return Map of task ID to operation type
         */
        QVariantMap CurrentOperations() const;

        /**
         * @brief Get binary blobs associated with this network
         * @return Map of blob name to blob reference
         */
        QVariantMap blobs() const;

        /**
         * @brief Get user-specified tags for categorization
         * @return List of tag strings
         */
        QStringList tags() const;

        /**
         * @brief Get default locking mode for VIFs
         * @return Default locking mode string ("locked", "unlocked", or "disabled")
         */
        QString defaultLockingMode() const;

        /**
         * @brief Get IP addresses assigned to VIFs on this network
         * @return Map of VIF reference to assigned IP address (for xapi-managed DHCP networks)
         */
        QVariantMap assignedIPs() const;

        /**
         * @brief Get purposes for which the server will use this network
         * @return List of network purpose strings ("nbd", "insecure_nbd", etc.)
         */
        QStringList purpose() const;

        // XenObject interface
        QString GetObjectType() const override { return "network"; }
};

#endif // NETWORK_H
