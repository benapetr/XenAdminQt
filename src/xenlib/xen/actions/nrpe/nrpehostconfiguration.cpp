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

#include "nrpehostconfiguration.h"
#include <QStringList>

QString NRPEHostConfiguration::AllowHostsWithoutLocalAddress(const QString& allowHosts)
{
    QStringList kept;
    for (const QString& host : allowHosts.split(','))
    {
        const QString trimmed = host.trimmed();
        if (trimmed != "127.0.0.1" && trimmed != "::1" && !trimmed.isEmpty())
            kept << host;
    }
    return kept.join(',');
}

void NRPEHostConfiguration::ParseGeneralConfiguration(const QString& output)
{
    for (const QString& line : output.split('\n'))
    {
        const QString item = line.trimmed();
        if (item.startsWith("allowed_hosts:"))
            this->allowHosts = AllowHostsWithoutLocalAddress(item.mid(QString("allowed_hosts:").size()).trimmed());
        else if (item.startsWith("debug:"))
            this->debug = item.mid(QString("debug:").size()).trimmed() == DEBUG_ENABLE;
        else if (item.startsWith("ssl_logging:"))
            this->sslLogging = item.mid(QString("ssl_logging:").size()).trimmed() == SSL_LOGGING_ENABLE;
    }
}

void NRPEHostConfiguration::ParseThresholds(const QString& output)
{
    for (const QString& line : output.split('\n'))
    {
        // C# takes words 0, 4 and 8 of each line; lines too short for that are skipped here
        const QStringList words = line.split(' ');
        if (words.size() < 9)
            continue;
        this->checks.insert(words.at(0), Check{words.at(4), words.at(8)});
    }
}
