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
    Q_PROPERTY(QString bridge READ bridge NOTIFY dataChanged)
    Q_PROPERTY(bool managed READ managed NOTIFY dataChanged)
    Q_PROPERTY(qint64 mtu READ mtu NOTIFY dataChanged)

public:
    explicit Network(XenConnection* connection,
                     const QString& opaqueRef,
                     QObject* parent = nullptr);
    ~Network() override = default;

    /**
     * @brief Get bridge name
     * @return Linux bridge name (e.g., "xenbr0")
     */
    QString bridge() const;

    /**
     * @brief Check if bridge is managed by xapi
     * @return true if managed by xapi, false if external bridge
     */
    bool managed() const;

    /**
     * @brief Get MTU (Maximum Transmission Unit)
     * @return MTU in bytes
     */
    qint64 mtu() const;

    /**
     * @brief Get list of VIF references
     * @return List of VIF opaque references
     */
    QStringList vifRefs() const;

    /**
     * @brief Get list of PIF references
     * @return List of PIF opaque references
     */
    QStringList pifRefs() const;

    /**
     * @brief Get other_config dictionary
     * @return Other configuration key-value pairs
     */
    QVariantMap otherConfig() const;

    // XenObject interface
    QString objectType() const override { return "network"; }
};

#endif // NETWORK_H
