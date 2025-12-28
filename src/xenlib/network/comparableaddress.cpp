/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * Copyright (c) Cloud Software Group, Inc.
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

#include "comparableaddress.h"
#include <QStringList>
#include <QRegularExpression>

ComparableAddress::ComparableAddress()
    : isPartialIP_(false)
{
}

ComparableAddress::ComparableAddress(const QHostAddress& ip)
    : addressIP_(ip)
    , isPartialIP_(false)
{
}

ComparableAddress::ComparableAddress(const QString& partialIP)
    : partialIPPattern_(partialIP)
    , isPartialIP_(true)
{
}

ComparableAddress::ComparableAddress(const QString& hostname, bool isName)
    : addressString_(hostname)
    , isPartialIP_(false)
{
    Q_UNUSED(isName);  // Just to differentiate constructor signature
}

bool ComparableAddress::TryParse(const QString& candidate, bool allowPartialIP, bool allowName, ComparableAddress& outAddress)
{
    if (candidate.isEmpty())
    {
        return false;
    }

    // Try parsing as IP address first
    QHostAddress ipAddress(candidate);
    if (!ipAddress.isNull())
    {
        outAddress = ComparableAddress(ipAddress);
        return true;
    }

    // Try parsing as partial IP (wildcards: *, x, _, n)
    if (allowPartialIP && isPartialIPPattern(candidate))
    {
        outAddress = ComparableAddress(candidate);
        return true;
    }

    // Allow as hostname
    if (allowName)
    {
        outAddress = ComparableAddress(candidate, true);
        return true;
    }

    return false;
}

QString ComparableAddress::toString() const
{
    if (isIP())
    {
        return this->addressIP_.toString();
    }
    else if (isPartialIP())
    {
        return this->partialIPPattern_;
    }
    else
    {
        return this->addressString_;
    }
}

bool ComparableAddress::operator==(const ComparableAddress& other) const
{
    if (other.isPartialIP() && this->isIP())
    {
        return other.partialIPEquals(this->addressIP_);
    }
    else if (other.isIP() && this->isPartialIP())
    {
        return this->partialIPEquals(other.addressIP_);
    }
    else
    {
        return this->compareTo(other) == 0;
    }
}

bool ComparableAddress::operator!=(const ComparableAddress& other) const
{
    return !(*this == other);
}

bool ComparableAddress::operator<(const ComparableAddress& other) const
{
    return this->compareTo(other) < 0;
}

bool ComparableAddress::equalsIP(const QHostAddress& ip) const
{
    if (this->isIP())
    {
        return this->addressIP_ == ip;
    }
    else if (this->isPartialIP())
    {
        return this->partialIPEquals(ip);
    }
    else
    {
        return false;
    }
}

uint ComparableAddress::qHash() const
{
    return ::qHash(this->toString());
}

int ComparableAddress::compareTo(const ComparableAddress& other) const
{
    // Comparison order: IP addresses < Partial IPs < Hostnames

    if (this->isIP())
    {
        if (other.isIP())
        {
            // Compare IP addresses byte-by-byte
            // IPv6 addresses come first (have more bytes)
            QAbstractSocket::NetworkLayerProtocol thisProtocol = this->addressIP_.protocol();
            QAbstractSocket::NetworkLayerProtocol otherProtocol = other.addressIP_.protocol();

            if (thisProtocol != otherProtocol)
            {
                // IPv6 < IPv4
                return (thisProtocol == QAbstractSocket::IPv6Protocol) ? -1 : 1;
            }

            // Same protocol - compare addresses
            QString thisStr = this->addressIP_.toString();
            QString otherStr = other.addressIP_.toString();
            return thisStr.compare(otherStr);
        }
        else
        {
            return -1;  // IP < Partial/Hostname
        }
    }
    else if (this->isPartialIP())
    {
        if (other.isIP())
        {
            return 1;  // Partial > IP
        }
        else if (other.isPartialIP())
        {
            // Natural string comparison
            return this->partialIPPattern_.compare(other.partialIPPattern_);
        }
        else
        {
            return -1;  // Partial < Hostname
        }
    }
    else  // Hostname
    {
        if (other.isIP() || other.isPartialIP())
        {
            return 1;  // Hostname > IP/Partial
        }
        else
        {
            return this->addressString_.compare(other.addressString_);
        }
    }
}

bool ComparableAddress::partialIPEquals(const QHostAddress& ip) const
{
    if (!this->isPartialIP())
    {
        return false;
    }

    // Convert IP to octets
    QString ipStr = ip.toString();
    QStringList ipParts = ipStr.split('.');

    if (ipParts.size() != 4)
    {
        return false;  // Only IPv4 partial matching supported
    }

    // Convert partial pattern to segments
    QStringList patternParts = this->partialIPPattern_.split('.');

    if (patternParts.size() != 4)
    {
        return false;
    }

    // Compare each segment
    for (int i = 0; i < 4; ++i)
    {
        QString pattern = patternParts[i].trimmed();

        // Wildcard characters: *, x, _, n, or empty
        if (pattern == "*" || pattern == "x" || pattern == "_" || pattern == "n" || pattern.isEmpty())
        {
            continue;  // Wildcard matches any value
        }

        // Must match exact number
        if (pattern != ipParts[i])
        {
            return false;
        }
    }

    return true;
}

bool ComparableAddress::isPartialIPPattern(const QString& str)
{
    if (!str.contains('.'))
    {
        return false;
    }

    QStringList parts = str.split('.');
    if (parts.size() != 4)
    {
        return false;
    }

    // Check if any segment is a wildcard or valid number
    static QRegularExpression wildcardRegex("^[*xn_]?$");
    static QRegularExpression numberRegex("^[0-9]{1,3}$");

    for (const QString& part : parts)
    {
        QString trimmed = part.trimmed();

        // Allow wildcards
        if (wildcardRegex.match(trimmed).hasMatch())
        {
            continue;
        }

        // Allow valid numbers (0-255)
        if (numberRegex.match(trimmed).hasMatch())
        {
            bool ok;
            int value = trimmed.toInt(&ok);
            if (ok && value >= 0 && value <= 255)
            {
                continue;
            }
        }

        return false;  // Invalid segment
    }

    return true;
}
