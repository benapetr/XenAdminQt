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

#ifndef HOSTMETRICS_H
#define HOSTMETRICS_H

#include "xenobject.h"
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Host metrics XenObject wrapper
 *
 * Provides typed access to host_metrics fields in XenCache.
 * Qt equivalent of C# XenAPI.Host_metrics class.
 *
 * Key properties (from C# Host_metrics class):
 * - uuid - Unique identifier
 * - memory_total - Total host memory in bytes
 * - memory_free - Free host memory in bytes
 * - live - Pool master thinks this host is live
 * - last_updated - Time at which this information was last updated
 * - other_config - Additional configuration
 */
class XENLIB_EXPORT HostMetrics : public XenObject
{
    Q_OBJECT

public:
    explicit HostMetrics(XenConnection* connection,
                        const QString& opaqueRef,
                        QObject* parent = nullptr);
    ~HostMetrics() override = default;

    /**
     * @brief Check if pool master thinks this host is live
     * @return true if host is considered live by pool master
     */
    bool live() const;

    /**
     * @brief Get unique identifier
     * @return UUID string
     */
    QString uuid() const;

    /**
     * @brief Get total host memory
     * @return Total memory in bytes
     */
    qint64 memoryTotal() const;

    /**
     * @brief Get free host memory
     * @return Free memory in bytes
     */
    qint64 memoryFree() const;

    /**
     * @brief Get time at which this information was last updated
     * @return Timestamp of last update
     */
    QDateTime lastUpdated() const;

    /**
     * @brief Get additional configuration
     * @return Map of additional configuration key-value pairs
     */
    QVariantMap otherConfig() const;

protected:
    QString objectType() const override;
};

#endif // HOSTMETRICS_H
