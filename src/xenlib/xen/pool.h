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

#ifndef POOL_H
#define POOL_H

#include "xenobject.h"

class Host;
class SR;

/**
 * @brief Pool - Pool-wide information
 *
 * Qt equivalent of C# XenAPI.Pool class. Represents a XenServer resource pool.
 *
 * Key properties (from C# Pool class):
 * - name_label, name_description
 * - master (reference to master host)
 * - default_SR (reference to default storage repository)
 * - ha_enabled, ha_configuration
 * - other_config
 */
class XENLIB_EXPORT Pool : public XenObject
{
    Q_OBJECT
    Q_PROPERTY(QString master READ masterRef NOTIFY dataChanged)
    Q_PROPERTY(QString defaultSR READ defaultSRRef NOTIFY dataChanged)
    Q_PROPERTY(bool haEnabled READ haEnabled NOTIFY dataChanged)

public:
    explicit Pool(XenConnection* connection,
                  const QString& opaqueRef,
                  QObject* parent = nullptr);
    ~Pool() override = default;

    /**
     * @brief Get reference to pool master host
     * @return Host opaque reference
     */
    QString masterRef() const;

    /**
     * @brief Get reference to default SR
     * @return SR opaque reference
     */
    QString defaultSRRef() const;

    /**
     * @brief Check if HA is enabled
     * @return true if HA is enabled
     */
    bool haEnabled() const;

    /**
     * @brief Get HA configuration
     * @return Map of HA configuration keys/values
     */
    QVariantMap haConfiguration() const;

    /**
     * @brief Get other_config dictionary
     * @return Map of additional configuration
     */
    QVariantMap otherConfig() const;

    /**
     * @brief Get list of all host refs in this pool
     * @return List of host opaque references
     */
    QStringList hostRefs() const;

    /**
     * @brief Check if this is a pool-of-one (single host pool)
     * @return true if pool has only one host
     */
    bool isPoolOfOne() const;

    /**
     * @brief Get tags
     * @return List of tag strings
     */
    QStringList tags() const;

    /**
     * @brief Get WLB (Workload Balancing) enabled status
     * @return true if WLB is enabled
     */
    bool wlbEnabled() const;

    /**
     * @brief Get live patching disabled status
     * @return true if live patching is disabled
     */
    bool livePatchingDisabled() const;

protected:
    QString objectType() const override;
};

#endif // POOL_H
