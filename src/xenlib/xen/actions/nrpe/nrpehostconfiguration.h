/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef NRPEHOSTCONFIGURATION_H
#define NRPEHOSTCONFIGURATION_H

#include "../../../xenlib_global.h"
#include <QMap>
#include <QString>

/*!
 * \brief A host's NRPE settings (C# XenModel/Actions/NRPE/NRPEHostConfiguration.cs)
 *
 * allowHosts holds the monitoring servers without the local addresses the plugin always adds;
 * C# stores its text-box placeholder there when there are none, this port stores an empty string.
 */
struct XENLIB_EXPORT NRPEHostConfiguration
{
    static constexpr const char* PLUGIN_NAME = "nrpe";
    static constexpr const char* DEBUG_ENABLE = "1";
    static constexpr const char* DEBUG_DISABLE = "0";
    static constexpr const char* SSL_LOGGING_ENABLE = "0x2f";
    static constexpr const char* SSL_LOGGING_DISABLE = "0x00";

    struct Check
    {
        QString warningThreshold;
        QString criticalThreshold;

        bool operator==(const Check& other) const
        {
            return this->warningThreshold == other.warningThreshold && this->criticalThreshold == other.criticalThreshold;
        }
        bool operator!=(const Check& other) const { return !(*this == other); }
    };

    bool enableNRPE = false;
    QString allowHosts;
    bool debug = false;
    bool sslLogging = false;
    QMap<QString, Check> checks;  //!< by check name, e.g. "check_cpu"

    //! C# Equals: checks only count where both sides have them
    bool operator==(const NRPEHostConfiguration& other) const
    {
        if (this->enableNRPE != other.enableNRPE || this->allowHosts != other.allowHosts
            || this->debug != other.debug || this->sslLogging != other.sslLogging)
            return false;
        for (auto it = this->checks.constBegin(); it != this->checks.constEnd(); ++it)
        {
            auto original = other.checks.constFind(it.key());
            if (original != other.checks.constEnd() && *original != it.value())
                return false;
        }
        return true;
    }
    bool operator!=(const NRPEHostConfiguration& other) const { return !(*this == other); }

    //! Drops 127.0.0.1 and ::1, which the plugin always allows (C# AllowHostsWithoutLocalAddress)
    static QString AllowHostsWithoutLocalAddress(const QString& allowHosts);

    //! Parses get-config output ("allowed_hosts: ...", "debug: ...", "ssl_logging: ..." lines)
    void ParseGeneralConfiguration(const QString& output);

    //! Parses get-threshold output, one "check_cpu warning threshold - 50 critical threshold - 80" per line
    void ParseThresholds(const QString& output);
};

#endif // NRPEHOSTCONFIGURATION_H
