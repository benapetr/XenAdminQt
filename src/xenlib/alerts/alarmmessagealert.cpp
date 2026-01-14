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

#include "alarmmessagealert.h"
#include <QXmlStreamReader>
#include <QDebug>

AlarmMessageAlert::AlarmMessageAlert(XenConnection* connection, const QVariantMap& messageData) : MessageAlert(connection, messageData),
      m_alarmType(AlarmType::None),
      m_currentValue(0.0),
      m_triggerLevel(0.0),
      m_triggerPeriod(0)
{
    this->parseAlarmMessage();
}

void AlarmMessageAlert::parseAlarmMessage()
{
    // C# Reference: AlarmMessageAlert.cs ParseAlarmMessage() line 57
    /*
     * message.body looks like:
     * value: 1234
     * config:
     * <variable>
     *  <name value="cpu_usage"/>
     *  <alarm_trigger_level value="0.9"/>
     *  <alarm_trigger_period value="60"/>
     * </variable>
     */
    
    QString body = this->GetMessageBody();
    QStringList lines = body.split('\n');
    
    if (lines.count() < 2)
    {
        qDebug() << "AlarmMessageAlert: Invalid message body format";
        return;
    }
    
    // Parse current value from first line
    QString valueLine = lines[0];
    if (valueLine.startsWith("value: "))
    {
        QString valueStr = valueLine.mid(7).trimmed();
        bool ok = false;
        this->m_currentValue = valueStr.toDouble(&ok);
        if (!ok)
            qDebug() << "AlarmMessageAlert: Failed to parse current value:" << valueStr;
    }
    
    // Parse XML from remaining lines
    QString xml = lines.mid(1).join("");
    xml.replace("config:", "");  // Remove config: prefix
    
    QXmlStreamReader reader(xml);
    QString variableName;
    
    while (!reader.atEnd())
    {
        reader.readNext();
        
        if (reader.isStartElement())
        {
            if (reader.name() == QLatin1String("name"))
            {
                variableName = reader.attributes().value("value").toString();
            }
            else if (reader.name() == QLatin1String("alarm_trigger_level"))
            {
                QString levelStr = reader.attributes().value("value").toString();
                bool ok = false;
                this->m_triggerLevel = levelStr.toDouble(&ok);
                if (!ok)
                    qDebug() << "AlarmMessageAlert: Failed to parse trigger level:" << levelStr;
            }
            else if (reader.name() == QLatin1String("alarm_trigger_period"))
            {
                QString periodStr = reader.attributes().value("value").toString();
                bool ok = false;
                this->m_triggerPeriod = periodStr.toInt(&ok);
                if (!ok)
                    qDebug() << "AlarmMessageAlert: Failed to parse trigger period:" << periodStr;
            }
        }
    }
    
    if (reader.hasError())
    {
        qDebug() << "AlarmMessageAlert: XML parse error:" << reader.errorString();
    }
    
    // Map variable name to alarm type (C# Reference: line 104-142)
    if (variableName == "cpu_usage")
        this->m_alarmType = AlarmType::Cpu;
    else if (variableName == "network_usage")
        this->m_alarmType = AlarmType::Net;
    else if (variableName == "disk_usage")
        this->m_alarmType = AlarmType::Disk;
    else if (variableName == "fs_usage")
        this->m_alarmType = AlarmType::FileSystem;
    else if (variableName == "mem_free_kib")
        this->m_alarmType = AlarmType::Memory;
    else if (variableName == "dom0_mem_usage")
        this->m_alarmType = AlarmType::Dom0MemoryDemand;
    else if (variableName == "log_fs_usage")
        this->m_alarmType = AlarmType::LogFileSystem;
    else if (variableName == "sr_physical_utilisation")
        this->m_alarmType = AlarmType::SrPhysicalUtilisation;
    else if (variableName.startsWith("sr_io_throughput_"))
    {
        // Storage-specific alarm: sr_io_throughput_<uuid>
        this->m_alarmType = AlarmType::Storage;
        this->m_srUuid = variableName.mid(17);  // Extract UUID after prefix
    }
    else
    {
        qDebug() << "AlarmMessageAlert: Unrecognized alarm type:" << variableName;
        this->m_alarmType = AlarmType::None;
    }
}

AlertPriority AlarmMessageAlert::GetPriority() const
{
    // C# Reference: AlarmMessageAlert.cs Priority property line 144
    switch (this->m_alarmType)
    {
        case AlarmType::FileSystem:
            return AlertPriority::Priority2;  // Service-loss imminent
        case AlarmType::LogFileSystem:
            return AlertPriority::Priority3;  // Service degraded
        default:
            return MessageAlert::GetPriority();  // Use base class priority from message
    }
}

QString AlarmMessageAlert::GetDescription() const
{
    // C# Reference: AlarmMessageAlert.cs Description property line 155
    switch (this->m_alarmType)
    {
        case AlarmType::Cpu:
            return this->formatCpuDescription();
        case AlarmType::Net:
            return this->formatNetDescription();
        case AlarmType::Disk:
            return this->formatDiskDescription();
        case AlarmType::FileSystem:
            return this->formatFileSystemDescription();
        case AlarmType::Memory:
            return this->formatMemoryDescription();
        case AlarmType::Dom0MemoryDemand:
            return this->formatDom0MemoryDescription();
        case AlarmType::LogFileSystem:
            return this->formatLogFileSystemDescription();
        case AlarmType::Storage:
            return this->formatStorageDescription();
        case AlarmType::SrPhysicalUtilisation:
            return this->formatSrPhysicalDescription();
        default:
            return MessageAlert::GetDescription();  // Fallback to base class
    }
}

// Formatting methods (C# Reference: line 162-223)

QString AlarmMessageAlert::formatCpuDescription() const
{
    return tr("CPU usage on %1 was %2 for %3 (trigger level %4)")
        .arg(this->AppliesTo())
        .arg(this->percentageString(this->m_currentValue))
        .arg(this->timeString(this->m_triggerPeriod))
        .arg(this->percentageString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatNetDescription() const
{
    return tr("Network usage on %1 was %2 for %3 (trigger level %4)")
        .arg(this->AppliesTo())
        .arg(this->dataRateString(this->m_currentValue))
        .arg(this->timeString(this->m_triggerPeriod))
        .arg(this->dataRateString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatDiskDescription() const
{
    return tr("Disk usage on %1 was %2 for %3 (trigger level %4)")
        .arg(this->AppliesTo())
        .arg(this->dataRateString(this->m_currentValue))
        .arg(this->timeString(this->m_triggerPeriod))
        .arg(this->dataRateString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatFileSystemDescription() const
{
    return tr("Filesystem usage on %1 was %2 (trigger level %3). This may cause XenServer to stop working.")
        .arg(this->AppliesTo())
        .arg(this->percentageString(this->m_currentValue))
        .arg(this->percentageString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatMemoryDescription() const
{
    // m_currentValue and m_triggerLevel are in KiB (C# comment line 173)
    return tr("Free memory on %1 was %2 for %3 (trigger level %4)")
        .arg(this->AppliesTo())
        .arg(this->memorySizeString(this->m_currentValue * 1024.0))  // KiB to bytes
        .arg(this->timeString(this->m_triggerPeriod))
        .arg(this->memorySizeString(this->m_triggerLevel * 1024.0));
}

QString AlarmMessageAlert::formatDom0MemoryDescription() const
{
    return tr("Dom0 memory demand on %1 was %2 (trigger level %3)")
        .arg(this->AppliesTo())
        .arg(this->percentageString(this->m_currentValue))
        .arg(this->percentageString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatLogFileSystemDescription() const
{
    return tr("Log filesystem usage on %1 was %2 (trigger level %3)")
        .arg(this->AppliesTo())
        .arg(this->percentageString(this->m_currentValue))
        .arg(this->percentageString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatStorageDescription() const
{
    // TODO: Look up SR name by UUID
    QString srName = this->m_srUuid.isEmpty() ? tr("Unknown SR") : this->m_srUuid;
    return tr("I/O throughput on storage %1 was %2 for %3 (trigger level %4)")
        .arg(srName)
        .arg(this->dataRateString(this->m_currentValue))
        .arg(this->timeString(this->m_triggerPeriod))
        .arg(this->dataRateString(this->m_triggerLevel));
}

QString AlarmMessageAlert::formatSrPhysicalDescription() const
{
    return tr("Physical utilization of SR on %1 was %2 (trigger level %3)")
        .arg(this->AppliesTo())
        .arg(this->percentageString(this->m_currentValue))
        .arg(this->percentageString(this->m_triggerLevel));
}

// Helper formatting functions

QString AlarmMessageAlert::percentageString(double value) const
{
    // Convert to percentage (value is typically 0.0-1.0)
    double percent = value * 100.0;
    return QString("%1%").arg(percent, 0, 'f', 1);
}

QString AlarmMessageAlert::timeString(int seconds) const
{
    if (seconds < 60)
        return tr("%1 second(s)").arg(seconds);
    
    int minutes = seconds / 60;
    int remainingSeconds = seconds % 60;
    
    if (minutes < 60)
    {
        if (remainingSeconds == 0)
            return tr("%1 minute(s)").arg(minutes);
        return tr("%1 minute(s) %2 second(s)").arg(minutes).arg(remainingSeconds);
    }
    
    int hours = minutes / 60;
    int remainingMinutes = minutes % 60;
    
    if (remainingMinutes == 0)
        return tr("%1 hour(s)").arg(hours);
    return tr("%1 hour(s) %2 minute(s)").arg(hours).arg(remainingMinutes);
}

QString AlarmMessageAlert::dataRateString(double bytesPerSecond) const
{
    // Convert bytes/second to human-readable format
    if (bytesPerSecond < 1024.0)
        return QString("%1 B/s").arg(bytesPerSecond, 0, 'f', 1);
    
    double kbps = bytesPerSecond / 1024.0;
    if (kbps < 1024.0)
        return QString("%1 KB/s").arg(kbps, 0, 'f', 1);
    
    double mbps = kbps / 1024.0;
    if (mbps < 1024.0)
        return QString("%1 MB/s").arg(mbps, 0, 'f', 1);
    
    double gbps = mbps / 1024.0;
    return QString("%1 GB/s").arg(gbps, 0, 'f', 1);
}

QString AlarmMessageAlert::memorySizeString(double bytes) const
{
    // Convert bytes to human-readable format
    if (bytes < 1024.0)
        return QString("%1 B").arg(bytes, 0, 'f', 0);
    
    double kb = bytes / 1024.0;
    if (kb < 1024.0)
        return QString("%1 KB").arg(kb, 0, 'f', 1);
    
    double mb = kb / 1024.0;
    if (mb < 1024.0)
        return QString("%1 MB").arg(mb, 0, 'f', 1);
    
    double gb = mb / 1024.0;
    return QString("%1 GB").arg(gb, 0, 'f', 2);
}
