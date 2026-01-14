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

#ifndef ALARMMESSAGEALERT_H
#define ALARMMESSAGEALERT_H

#include "messagealert.h"

/**
 * Alarm types for performance monitoring alerts
 * C# Reference: XenAdmin/Alerts/Types/AlarmMessageAlert.cs enum AlarmType
 */
enum class AlarmType
{
    None = 0,
    Cpu,
    Net,
    Disk,
    FileSystem,
    Memory,
    Dom0MemoryDemand,
    LogFileSystem,
    Storage,
    SrPhysicalUtilisation
};

/**
 * Specialized alert for performance alarm messages (ALARM message type).
 * Parses XML configuration from message body to extract:
 * - Current value (e.g., CPU usage percentage)
 * - Trigger level (threshold that was crossed)
 * - Trigger period (duration threshold was exceeded)
 * 
 * C# Reference: XenAdmin/Alerts/Types/AlarmMessageAlert.cs
 */
class AlarmMessageAlert : public MessageAlert
{
    Q_OBJECT

    public:
        explicit AlarmMessageAlert(XenConnection* connection, const QVariantMap& messageData);

        // Override alert properties for alarm-specific formatting
        AlertPriority GetPriority() const override;
        QString GetDescription() const override;

    private:
        void parseAlarmMessage();

        AlarmType m_alarmType;
        double m_currentValue;
        double m_triggerLevel;
        int m_triggerPeriod;
        QString m_srUuid;  // For storage alarms

        // Helper methods
        QString formatCpuDescription() const;
        QString formatNetDescription() const;
        QString formatDiskDescription() const;
        QString formatFileSystemDescription() const;
        QString formatMemoryDescription() const;
        QString formatDom0MemoryDescription() const;
        QString formatStorageDescription() const;
        QString formatSrPhysicalDescription() const;
        QString formatLogFileSystemDescription() const;

        // Formatting helpers
        QString percentageString(double value) const;
        QString timeString(int seconds) const;
        QString dataRateString(double bytesPerSecond) const;
        QString memorySizeString(double bytes) const;
};

#endif // ALARMMESSAGEALERT_H
