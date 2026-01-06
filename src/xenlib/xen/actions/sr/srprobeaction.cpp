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

#include "srprobeaction.h"
#include "../../xenapi/xenapi_SR.h"
#include <QXmlStreamReader>

using namespace XenAPI;

SrProbeAction::SrProbeAction(XenConnection* connection,
                             Host* host,
                             const QString& srType,
                             const QVariantMap& deviceConfig,
                             const QVariantMap& smConfig,
                             QObject* parent)
    : AsyncOperation(connection,
                     QString("Scanning for %1 SRs").arg(srType),
                     QString("Scanning storage..."),
                     parent),
      m_host(host),
      m_srType(srType),
      m_deviceConfig(deviceConfig),
      m_smConfig(smConfig)
{
    // Build descriptive description based on SR type and target
    QString target;
    if (srType == "nfs")
    {
        target = deviceConfig.value("server").toString();
    } else if (srType == "lvmoiscsi")
    {
        target = deviceConfig.value("target").toString();
    } else if (srType == "lvmohba" || srType == "lvmofcoe")
    {
        target = deviceConfig.contains("device")
                     ? deviceConfig.value("device").toString()
                     : deviceConfig.value("SCSIid").toString();
    } else if (srType == "gfs2")
    {
        target = deviceConfig.contains("target")
                     ? deviceConfig.value("target").toString()
                     : deviceConfig.value("SCSIid").toString();
    } else
    {
        target = "server";
    }

    SetDescription(QString("Scanning %1 storage on %2").arg(srType).arg(target));

    // Won't appear in history (matches C# SuppressHistory = true)
    SetSuppressHistory(true);
}

void SrProbeAction::run()
{
    if (!m_host)
    {
        setError("No host specified for SR probe");
        return;
    }

    try
    {
        if (m_srType == "gfs2")
        {
            // GFS2 uses probe_ext (synchronous, returns structured data)
            try
            {
                m_discoveredSRs = XenAPI::SR::probe_ext(GetSession(),
                                                        m_host->OpaqueRef(),
                                                        m_deviceConfig,
                                                        m_srType,
                                                        m_smConfig);
            } catch (const std::exception& e)
            {
                QString error = e.what();

                // Ignore expected gfs2 failures (CA-335356, CA-337280)
                if (error.contains("DeviceNotFoundException") ||
                    (error.contains("ISCSILogin") &&
                     m_deviceConfig.contains("chapuser") &&
                     m_deviceConfig.contains("chappassword")))
                {
                    // Expected failure - return empty list
                    m_discoveredSRs = QVariantList();
                } else
                {
                    throw; // Re-throw unexpected errors
                }
            }
        } else
        {
            // Other SR types use async_probe (returns XML)
            QString taskRef = XenAPI::SR::async_probe(GetSession(),
                                                      m_host->OpaqueRef(),
                                                      m_deviceConfig,
                                                      m_srType,
                                                      m_smConfig);

            // Poll task to completion
            pollToCompletion(taskRef, 0, 100);

            // Parse XML GetResult
            QString xmlResult = GetResult();
            m_discoveredSRs = parseSRListXML(xmlResult);
        }

        SetDescription("SR scan successful");

    } catch (const std::exception& e)
    {
        setError(QString("Failed to probe for SRs: %1").arg(e.what()));
    }
}

QVariantList SrProbeAction::parseSRListXML(const QString& xml)
{
    /*
     * Parse SR probe XML format (from C# SR.ParseSRListXML):
     *
     * <SRlist>
     *   <SR>
     *     <UUID>12345678-1234-1234-1234-123456789abc</UUID>
     *     <NameLabel>My Storage</NameLabel>
     *     <NameDescription>Description</NameDescription>
     *     <TotalSpace>1099511627776</TotalSpace>
     *     <FreeSpace>549755813888</FreeSpace>
     *     <Aggregated>true</Aggregated>
     *     <PoolMetadataDetected>false</PoolMetadataDetected>
     *   </SR>
     *   ...
     * </SRlist>
     */

    QVariantList result;
    QXmlStreamReader reader(xml);
    QVariantMap currentSR;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement())
        {
            QString elementName = reader.name().toString();

            if (elementName == "SR")
            {
                // Start of new SR record
                currentSR.clear();
            } else if (elementName == "UUID")
            {
                currentSR["uuid"] = reader.readElementText();
            } else if (elementName == "NameLabel")
            {
                currentSR["name_label"] = reader.readElementText();
            } else if (elementName == "NameDescription")
            {
                currentSR["name_description"] = reader.readElementText();
            } else if (elementName == "TotalSpace")
            {
                currentSR["total_space"] = reader.readElementText().toLongLong();
            } else if (elementName == "FreeSpace")
            {
                currentSR["free_space"] = reader.readElementText().toLongLong();
            } else if (elementName == "Aggregated")
            {
                currentSR["aggregated"] = (reader.readElementText().toLower() == "true");
            } else if (elementName == "PoolMetadataDetected")
            {
                currentSR["pool_metadata_detected"] = (reader.readElementText().toLower() == "true");
            }
        } else if (reader.isEndElement() && reader.name().toString() == "SR")
        {
            // End of SR record - add to list if it has a UUID
            if (!currentSR.isEmpty() && currentSR.contains("uuid"))
            {
                result.append(currentSR);
            }
        }
    }

    if (reader.hasError())
    {
        throw std::runtime_error(QString("XML parse error: %1").arg(reader.errorString()).toStdString());
    }

    return result;
}
