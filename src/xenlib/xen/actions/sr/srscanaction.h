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

#ifndef SRSCANACTION_H
#define SRSCANACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>
#include <QVariantList>

/**
 * @brief NetAppAggregate - NetApp storage aggregate information
 *
 * Qt equivalent of C# NetAppAggregate struct
 * Represents a NetApp filer aggregate with deduplication capabilities
 */
struct NetAppAggregate
{
    QString name;
    qint64 size;
    int disks;
    QString raidType;
    bool asisCapable; // A-SIS deduplication capable

    NetAppAggregate()
        : size(0), disks(0), asisCapable(false)
    {}

    NetAppAggregate(const QString& n, qint64 s, int d, const QString& r, bool a)
        : name(n), size(s), disks(d), raidType(r), asisCapable(a)
    {}

    bool operator<(const NetAppAggregate& other) const
    {
        return name < other.name;
    }
};

/**
 * @brief DellStoragePool - Dell EqualLogic storage pool information
 *
 * Qt equivalent of C# DellStoragePool struct
 * Represents a Dell storage pool with capacity and member information
 */
struct DellStoragePool
{
    QString name;
    bool isDefault;
    int members;
    int volumes;
    qint64 capacity;
    qint64 freespace;

    DellStoragePool()
        : isDefault(false), members(0), volumes(0), capacity(0), freespace(0)
    {}

    DellStoragePool(const QString& n, bool d, int m, int v, qint64 c, qint64 f)
        : name(n), isDefault(d), members(m), volumes(v), capacity(c), freespace(f)
    {}

    bool operator<(const DellStoragePool& other) const
    {
        return name < other.name;
    }
};

/**
 * @brief SrScanAction - Scan for SRs and aggregates on NetApp/Dell storage
 *
 * Qt equivalent of C# XenAdmin.Actions.SrScanAction
 *
 * Scans storage backend for existing SRs and available aggregates/pools:
 * 1. Probes for existing SRs using SR.async_probe
 * 2. Attempts SR.async_create (which is expected to fail)
 * 3. Parses failure error details to extract aggregate/pool information
 *
 * Key features:
 * - Discovers NetApp aggregates (for netapp SR type)
 * - Discovers Dell EqualLogic storage pools (for equal SR type)
 * - Lists existing SRs on the storage target
 * - Expects specific failure codes (123 for NetApp, 163 for Dell)
 * - Won't appear in program history (suppressHistory = true)
 *
 * Usage:
 *   SrScanAction* action = new SrScanAction(connection, "netapp",
 *                                            "192.168.1.10", "admin", "password");
 *   connect(action, &AsyncOperation::completed, [action](bool success) {
 *       if (success) {
 *           QList<NetAppAggregate> aggregates = action->aggregates();
 *           QVariantList srs = action->srs();
 *       }
 *   });
 *   action->runAsync();
 */
class XENLIB_EXPORT SrScanAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection
     * @param type SR type (e.g., "netapp", "equal")
     * @param hostname Storage filer hostname/IP
     * @param username Storage filer username
     * @param password Storage filer password
     * @param parent Parent QObject
     *
     * C# equivalent: SrScanAction(IXenConnection, string, string, string, SR.SRTypes)
     */
    SrScanAction(XenConnection* connection,
                 const QString& type,
                 const QString& hostname,
                 const QString& username,
                 const QString& password,
                 QObject* parent = nullptr);

    /**
     * @brief Get discovered SRs
     * @return List of SR info records (may be empty)
     *
     * C# equivalent: SRs property
     */
    QVariantList srs() const
    {
        return m_srs;
    }

    /**
     * @brief Get discovered NetApp aggregates
     * @return List of NetApp aggregates (may be empty)
     *
     * C# equivalent: Aggregates property
     */
    QList<NetAppAggregate> aggregates() const
    {
        return m_aggregates;
    }

    /**
     * @brief Get discovered Dell storage pools
     * @return List of Dell storage pools (may be empty)
     *
     * C# equivalent: StoragePools property
     */
    QList<DellStoragePool> storagePools() const
    {
        return m_storagePools;
    }

protected:
    /**
     * @brief Execute the scan operation
     *
     * Steps:
     * 1. SR.async_probe() - Find existing SRs
     * 2. SR.async_create() - Attempt create (expected to fail)
     * 3. Parse failure error details for aggregates/pools
     * 4. Throw exception if nothing found
     *
     * Expected failure codes:
     * - SR_BACKEND_FAILURE_123: NetApp aggregates in error[3]
     * - SR_BACKEND_FAILURE_163: Dell storage pools in error[3]
     *
     * C# equivalent: Run() override
     */
    void run() override;

private:
    QString m_type;
    QString m_hostname;
    QString m_username;
    QString m_password;

    QVariantList m_srs;
    QList<NetAppAggregate> m_aggregates;
    QList<DellStoragePool> m_storagePools;

    /**
     * @brief Parse NetApp aggregate XML
     * @param xml XML string from error response
     * @return List of aggregates
     *
     * C# equivalent: ParseAggregateXML()
     */
    QList<NetAppAggregate> parseAggregateXML(const QString& xml);

    /**
     * @brief Parse Dell storage pool XML
     * @param xml XML string from error response
     * @return List of storage pools
     *
     * C# equivalent: ParseDellStoragePoolsXML()
     */
    QList<DellStoragePool> parseDellStoragePoolsXML(const QString& xml);

    /**
     * @brief Parse SR list XML from probe result
     * @param xml XML string from SR.probe
     * @return List of SR info records
     *
     * Reuses logic from SrProbeAction
     */
    QVariantList parseSRListXML(const QString& xml);
};

#endif // SRSCANACTION_H
