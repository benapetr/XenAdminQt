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

#ifndef VIF_H
#define VIF_H

#include "xenobject.h"

class Network;
class VM;

/**
 * @brief VIF - A virtual network interface
 *
 * Qt equivalent of C# XenAPI.VIF class. Represents a virtual network interface.
 *
 * Key properties (from C# VIF class):
 * - device (order in which VIF backends are created)
 * - network (virtual network this VIF is connected to)
 * - VM (virtual machine this VIF is connected to)
 * - MAC (ethernet MAC address)
 * - MTU (maximum transmission unit)
 * - currently_attached (whether device is currently attached)
 * - locking_mode (network locking mode)
 * - ipv4_allowed, ipv6_allowed (IP filtering)
 * - qos_algorithm_type, qos_algorithm_params (QoS settings)
 *
 * First published in XenServer 4.0.
 */
class XENLIB_EXPORT VIF : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(QString device READ Device NOTIFY dataChanged)
    Q_PROPERTY(QString MAC READ GetMAC NOTIFY dataChanged)
    Q_PROPERTY(qint64 MTU READ GetMTU NOTIFY dataChanged)
    Q_PROPERTY(bool currentlyAttached READ CurrentlyAttached NOTIFY dataChanged)

public:
    explicit VIF(XenConnection* connection,
                 const QString& opaqueRef,
                 QObject* parent = nullptr);
    ~VIF() override = default;

    /**
     * @brief Get allowed operations
     * @return List of allowed VIF operations
     *
     * First published in XenServer 4.0.
     */
    QStringList AllowedOperations() const;

    /**
     * @brief Get current operations
     * @return Map of task ID to operation type
     *
     * First published in XenServer 4.0.
     */
    QVariantMap CurrentOperations() const;

    /**
     * @brief Get device order
     * @return Order in which VIF backends are created by xapi (e.g., "0", "1", "2")
     *
     * First published in XenServer 4.0.
     */
    QString Device() const;

    /**
     * @brief Get network reference
     * @return Opaque reference to network this VIF is connected to
     *
     * First published in XenServer 4.0.
     */
    QString NetworkRef() const;

    /**
     * @brief Get VM reference
     * @return Opaque reference to VM this VIF is connected to
     *
     * First published in XenServer 4.0.
     */
    QString VMRef() const;

    /**
     * @brief Get MAC address
     * @return Ethernet MAC address of virtual interface, as exposed to guest
     *
     * First published in XenServer 4.0.
     */
    QString GetMAC() const;

    /**
     * @brief Get MTU (Maximum Transmission Unit)
     * @return MTU in octets
     *
     * First published in XenServer 4.0.
     */
    qint64 GetMTU() const;

    /**
     * @brief Get other_config dictionary
     * @return Additional configuration key-value pairs
     *
     * First published in XenServer 4.0.
     */
    QVariantMap OtherConfig() const;

    /**
     * @brief Check if device is currently attached
     * @return true if device is currently attached (erased on reboot)
     *
     * First published in XenServer 4.0.
     */
    bool CurrentlyAttached() const;

    /**
     * @brief Get status code
     * @return Error/success code associated with last attach-operation (erased on reboot)
     *
     * First published in XenServer 4.0.
     */
    qint64 StatusCode() const;

    /**
     * @brief Get status detail
     * @return Error/success information associated with last attach-operation status
     *
     * First published in XenServer 4.0.
     */
    QString StatusDetail() const;

    /**
     * @brief Get runtime properties
     * @return Device runtime properties
     *
     * First published in XenServer 4.0.
     */
    QVariantMap RuntimeProperties() const;

    /**
     * @brief Get QoS algorithm type
     * @return QoS algorithm to use
     *
     * First published in XenServer 4.0.
     */
    QString QosAlgorithmType() const;

    /**
     * @brief Get QoS algorithm parameters
     * @return Parameters for chosen QoS algorithm
     *
     * First published in XenServer 4.0.
     */
    QVariantMap QosAlgorithmParams() const;

    /**
     * @brief Get supported QoS algorithms
     * @return List of supported QoS algorithms for this VIF
     *
     * First published in XenServer 4.0.
     */
    QStringList QosSupportedAlgorithms() const;

    /**
     * @brief Get VIF metrics reference
     * @return Opaque reference to VIF_metrics object
     *
     * First published in XenServer 4.0.
     * Deprecated since XenServer 6.1.
     */
    QString MetricsRef() const;

    /**
     * @brief Check if MAC was autogenerated
     * @return true if MAC was autogenerated; false if set manually
     *
     * First published in XenServer 5.5.
     */
    bool MACAutogenerated() const;

    /**
     * @brief Get locking mode
     * @return Current locking mode of the VIF ("network_default", "locked", "unlocked", "disabled")
     *
     * First published in XenServer 6.1.
     */
    QString LockingMode() const;

    /**
     * @brief Get IPv4 allowed addresses
     * @return List of IPv4 addresses which can be used to filter traffic
     *
     * First published in XenServer 6.1.
     */
    QStringList Ipv4Allowed() const;

    /**
     * @brief Get IPv6 allowed addresses
     * @return List of IPv6 addresses which can be used to filter traffic
     *
     * First published in XenServer 6.1.
     */
    QStringList Ipv6Allowed() const;

    /**
     * @brief Get IPv4 configuration mode
     * @return Determines whether IPv4 addresses are configured on the VIF ("None", "Static")
     *
     * First published in XenServer 7.0.
     */
    QString Ipv4ConfigurationMode() const;

    /**
     * @brief Get IPv4 addresses
     * @return List of IPv4 addresses in CIDR format
     *
     * First published in XenServer 7.0.
     */
    QStringList Ipv4Addresses() const;

    /**
     * @brief Get IPv4 gateway
     * @return IPv4 gateway (empty string means no gateway is set)
     *
     * First published in XenServer 7.0.
     */
    QString Ipv4Gateway() const;

    /**
     * @brief Get IPv6 configuration mode
     * @return Determines whether IPv6 addresses are configured on the VIF ("None", "Static")
     *
     * First published in XenServer 7.0.
     */
    QString Ipv6ConfigurationMode() const;

    /**
     * @brief Get IPv6 addresses
     * @return List of IPv6 addresses in CIDR format
     *
     * First published in XenServer 7.0.
     */
    QStringList Ipv6Addresses() const;

    /**
     * @brief Get IPv6 gateway
     * @return IPv6 gateway (empty string means no gateway is set)
     *
     * First published in XenServer 7.0.
     */
    QString Ipv6Gateway() const;

    // XenObject interface
    QString GetObjectType() const override { return "vif"; }
};

#endif // VIF_H
