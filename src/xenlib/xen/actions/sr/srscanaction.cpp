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

#include "srscanaction.h"
#include "../../network/connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_SR.h"
#include "../../xenapi/xenapi_Host.h"
#include <QXmlStreamReader>
#include <QDebug>

SrScanAction::SrScanAction(XenConnection* connection,
                           const QString& type,
                           const QString& hostname,
                           const QString& username,
                           const QString& password,
                           QObject* parent)
    : AsyncOperation(connection,
                     QString("Scanning %1 filer %2").arg(type).arg(hostname),
                     QString("Scanning for %1 storage on %2").arg(type).arg(hostname),
                     true,
                     parent),
      m_type(type),
      m_hostname(hostname),
      m_username(username),
      m_password(password)
{
}

void SrScanAction::run()
{
    try
    {
        // Build device config
        QVariantMap dconf;
        dconf["target"] = this->m_hostname;
        dconf["username"] = this->m_username;
        dconf["password"] = this->m_password;

        qDebug() << "SrScanAction: Attempting to find SRs on" << this->m_type << "filer" << this->m_hostname;

        // Step 1: Probe for existing SRs
        // Get coordinator host
        QVariantList hosts = XenAPI::Host::get_all(this->GetSession());
        if (hosts.isEmpty())
        {
            this->setError("No hosts available for scanning");
            return;
        }
        QString hostRef = hosts.first().toString();

        QString probeTaskRef = XenAPI::SR::async_probe(this->GetSession(), hostRef, dconf, this->m_type, QVariantMap());
        this->pollToCompletion(probeTaskRef, 0, 50);

        QString xmlResult = this->GetResult();
        this->m_srs = this->parseSRListXML(xmlResult);

        qDebug() << "SrScanAction: Attempting to find aggregates on" << this->m_type << "filer" << this->m_hostname;

        // Step 2: Attempt to create SR (expected to fail with aggregate/pool info)
        try
        {
            QString createTaskRef = XenAPI::SR::async_create(
                this->GetSession(),
                hostRef,
                dconf,
                0,                    // physical_size
                "TEMP_OBJECT_PREFIX", // Helpers.GuiTempObjectPrefix
                "",                   // description
                this->m_type,
                "",           // content_type
                true,         // shared
                QVariantMap() // sm_config
            );

            this->pollToCompletion(createTaskRef, 50, 100);

            // If we got here without exception, that's unexpected
            this->setError(QString("Unexpected response from %1").arg(this->m_hostname));

        } catch (const std::exception& e)
        {
            QString error = e.what();

            // We expect specific failure codes containing aggregate/pool XML
            if (error.contains("SR_BACKEND_FAILURE_123"))
            {
                // NetApp aggregates - extract XML from error
                // Error format: SR_BACKEND_FAILURE_123: <param1> <param2> <param3> <aggregateXML>
                // We need to extract the XML from the exception
                qDebug() << "SrScanAction: Found NetApp aggregate failure (123)";

                // For now, we'll parse what we can from the error string
                // TODO: Proper exception parsing with error parameters
                int xmlStart = error.indexOf('<');
                if (xmlStart >= 0)
                {
                    QString xml = error.mid(xmlStart);
                    this->m_aggregates = this->parseAggregateXML(xml);
                }

            } else if (error.contains("SR_BACKEND_FAILURE_163"))
            {
                // Dell storage pools
                qDebug() << "SrScanAction: Found Dell storage pool failure (163)";

                int xmlStart = error.indexOf('<');
                if (xmlStart >= 0)
                {
                    QString xml = error.mid(xmlStart);
                    this->m_storagePools = this->parseDellStoragePoolsXML(xml);
                }

            } else
            {
                // Unexpected error - re-throw
                throw;
            }
        }

        // Check if we found anything
        if (this->m_srs.isEmpty() && this->m_aggregates.isEmpty() && this->m_storagePools.isEmpty())
        {
            this->setError(QString("No existing SRs found and nowhere to create new storage on %1").arg(this->m_hostname));
            return;
        }

        this->SetDescription(QString("Scan of %1 completed").arg(this->m_hostname));

    } catch (const std::exception& e)
    {
        this->setError(QString("Failed to scan storage: %1").arg(e.what()));
    }
}

QList<NetAppAggregate> SrScanAction::parseAggregateXML(const QString& xml)
{
    /*
     * NetApp aggregate XML format:
     * <Aggr>
     *   <Name>aggr1</Name> or <Aggregate>aggr1</Aggregate>
     *   <Size>1099511627776</Size>
     *   <Disks>12</Disks>
     *   <RaidType>raid_dp</RaidType>
     *   <ASIS_Dedup>true</ASIS_Dedup>
     * </Aggr>
     */

    QList<NetAppAggregate> results;
    QXmlStreamReader reader(xml);

    QString currentName;
    qint64 currentSize = -1;
    int currentDisks = -1;
    QString currentRaidType;
    bool currentAsisCapable = false;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement())
        {
            QString elementName = reader.name().toString().toLower();

            if (elementName == "aggr")
            {
                // Start of new aggregate
                currentName.clear();
                currentSize = -1;
                currentDisks = -1;
                currentRaidType.clear();
                currentAsisCapable = false;
            } else if (elementName == "name" || elementName == "aggregate")
            {
                currentName = reader.readElementText().trimmed();
            } else if (elementName == "size")
            {
                currentSize = reader.readElementText().trimmed().toLongLong();
            } else if (elementName == "disks")
            {
                currentDisks = reader.readElementText().trimmed().toInt();
            } else if (elementName == "raidtype")
            {
                currentRaidType = reader.readElementText().trimmed();
            } else if (elementName == "asis_dedup")
            {
                QString value = reader.readElementText().trimmed().toLower();
                currentAsisCapable = (value == "true");
            }
        } else if (reader.isEndElement() && reader.name().toString().toLower() == "aggr")
        {
            // End of aggregate - add to list
            if (!currentName.isEmpty())
            {
                results.append(NetAppAggregate(currentName, currentSize, currentDisks,
                                               currentRaidType, currentAsisCapable));
            }
        }
    }

    if (reader.hasError())
    {
        qWarning() << "SrScanAction: XML parse error:" << reader.errorString();
        throw std::runtime_error(QString("Failed to parse aggregate XML from %1")
                                     .arg(this->m_hostname)
                                     .toStdString());
    }

    std::sort(results.begin(), results.end());
    return results;
}

QList<DellStoragePool> SrScanAction::parseDellStoragePoolsXML(const QString& xml)
{
    /*
     * Dell storage pool XML format:
     * <StoragePool>
     *   <Name>default</Name>
     *   <Default>true</Default>
     *   <Members>4</Members>
     *   <Volumes>8</Volumes>
     *   <Capacity>1099511627776</Capacity>
     *   <Freespace>549755813888</Freespace>
     * </StoragePool>
     */

    QList<DellStoragePool> results;
    QXmlStreamReader reader(xml);

    QString currentName;
    bool currentDefault = false;
    int currentMembers = 0;
    int currentVolumes = 0;
    qint64 currentCapacity = 0;
    qint64 currentFreespace = 0;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement())
        {
            QString elementName = reader.name().toString().toLower();

            if (elementName == "storagepool")
            {
                // Start of new storage pool
                currentName.clear();
                currentDefault = false;
                currentMembers = 0;
                currentVolumes = 0;
                currentCapacity = 0;
                currentFreespace = 0;
            } else if (elementName == "name")
            {
                currentName = reader.readElementText().trimmed();
            } else if (elementName == "default")
            {
                QString value = reader.readElementText().trimmed().toLower();
                currentDefault = (value == "true");
            } else if (elementName == "members")
            {
                currentMembers = reader.readElementText().trimmed().toInt();
            } else if (elementName == "volumes")
            {
                currentVolumes = reader.readElementText().trimmed().toInt();
            } else if (elementName == "capacity")
            {
                currentCapacity = reader.readElementText().trimmed().toLongLong();
            } else if (elementName == "freespace")
            {
                currentFreespace = reader.readElementText().trimmed().toLongLong();
            }
        } else if (reader.isEndElement() && reader.name().toString().toLower() == "storagepool")
        {
            // End of storage pool - add to list
            if (!currentName.isEmpty())
            {
                results.append(DellStoragePool(currentName, currentDefault, currentMembers,
                                               currentVolumes, currentCapacity, currentFreespace));
            }
        }
    }

    if (reader.hasError())
    {
        qWarning() << "SrScanAction: XML parse error:" << reader.errorString();
        throw std::runtime_error(QString("Failed to parse storage pool XML from %1")
                                     .arg(this->m_hostname)
                                     .toStdString());
    }

    std::sort(results.begin(), results.end());
    return results;
}

QVariantList SrScanAction::parseSRListXML(const QString& xml)
{
    /*
     * SR list XML format (same as SrProbeAction):
     * <SRlist>
     *   <SR>
     *     <UUID>...</UUID>
     *     <NameLabel>...</NameLabel>
     *     <NameDescription>...</NameDescription>
     *     ...
     *   </SR>
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
            }
        } else if (reader.isEndElement() && reader.name().toString() == "SR")
        {
            if (!currentSR.isEmpty() && currentSR.contains("uuid"))
            {
                result.append(currentSR);
            }
        }
    }

    if (reader.hasError())
    {
        qWarning() << "SrScanAction: SR list XML parse error:" << reader.errorString();
    }

    return result;
}
