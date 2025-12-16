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

#ifndef SRPROBEACTION_H
#define SRPROBEACTION_H

#include "../../asyncoperation.h"
#include "../../host.h"
#include <QVariantMap>
#include <QVariantList>

/**
 * @brief SrProbeAction - Probe for existing SRs on storage target
 *
 * Qt equivalent of C# XenAdmin.Actions.SrProbeAction
 *
 * Probes storage backend to discover existing SRs that can be attached.
 * Uses SR.async_probe for most SR types, SR.probe_ext for gfs2.
 * Won't appear in program history (suppressHistory = true).
 *
 * Key features:
 * - Scans storage targets (NFS server, iSCSI target, HBA device)
 * - Returns list of discovered SRs with UUIDs and metadata
 * - Special handling for gfs2 (probe_ext) vs other types (async_probe)
 * - Ignores expected failures for gfs2 (DeviceNotFoundException, ISCSI auth)
 *
 * Usage:
 *   QVariantMap dconf;
 *   dconf["server"] = "192.168.1.10";
 *   dconf["serverpath"] = "/exports/sr1";
 *
 *   SrProbeAction* action = new SrProbeAction(connection, host, "nfs", dconf);
 *   connect(action, &AsyncOperation::completed, [action](bool success) {
 *       if (success) {
 *           QVariantList srs = action->discoveredSRs();
 *           // Display SRs to user for selection
 *       }
 *   });
 *   action->runAsync();
 */
class XENLIB_EXPORT SrProbeAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param connection XenConnection
     * @param host Host to probe from
     * @param srType SR type (e.g., "nfs", "lvmoiscsi", "gfs2")
     * @param deviceConfig Device configuration (server, target, device, etc.)
     * @param smConfig SM configuration (optional)
     * @param parent Parent QObject
     *
     * C# equivalent: SrProbeAction(IXenConnection, Host, SR.SRTypes, Dictionary<string,string>, Dictionary<string,string>)
     */
    SrProbeAction(XenConnection* connection,
                  Host* host,
                  const QString& srType,
                  const QVariantMap& deviceConfig,
                  const QVariantMap& smConfig = QVariantMap(),
                  QObject* parent = nullptr);

    /**
     * @brief Get discovered SRs
     * @return List of SR info records
     *
     * Each record contains:
     * - "uuid" - SR UUID
     * - "name_label" - SR name
     * - "name_description" - SR description
     * - "total_space" - Total space in bytes
     * - "free_space" - Free space in bytes
     *
     * C# equivalent: SRs property
     */
    QVariantList discoveredSRs() const
    {
        return m_discoveredSRs;
    }

    /**
     * @brief Get SR type being probed
     * @return SR type string
     *
     * C# equivalent: SrType property
     */
    QString srType() const
    {
        return m_srType;
    }

protected:
    /**
     * @brief Execute the probe operation
     *
     * For non-gfs2 types:
     * 1. Calls SR.async_probe()
     * 2. Polls task to completion
     * 3. Parses XML result into SR list
     *
     * For gfs2:
     * 1. Calls SR.probe_ext() (synchronous)
     * 2. Returns structured data directly
     * 3. Ignores expected failures (DeviceNotFoundException, ISCSI auth)
     *
     * C# equivalent: Run() override
     */
    void run() override;

private:
    Host* m_host;
    QString m_srType;
    QVariantMap m_deviceConfig;
    QVariantMap m_smConfig;
    QVariantList m_discoveredSRs;

    /**
     * @brief Parse SR probe XML result
     * @param xml XML string from SR.probe
     * @return List of SR info records
     *
     * C# equivalent: SR.ParseSRListXML()
     */
    QVariantList parseSRListXML(const QString& xml);
};

#endif // SRPROBEACTION_H
