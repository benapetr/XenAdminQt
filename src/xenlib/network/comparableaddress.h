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

#ifndef COMPARABLEADDRESS_H
#define COMPARABLEADDRESS_H

#include <QString>
#include <QHostAddress>

/**
 * @brief Comparable IP address/hostname for search filtering
 *
 * Represents either an IP address (IPv4/IPv6), partial IP (with wildcards), or hostname.
 * Supports CIDR notation for IP range matching.
 *
 * C# equivalent: XenCenterLib/ComparableAddress.cs
 *
 * Examples:
 * - Full IP: "192.168.1.100", "2001:db8::1"
 * - Partial IP: "192.168.*.*, "10.0.1.*"
 * - Hostname: "server.example.com"
 * - CIDR: "192.168.1.0/24" (handled via partial IP conversion)
 */
class ComparableAddress
{
    public:
        // Static factory method (matches C# TryParse)
        static bool TryParse(const QString& candidate, bool allowPartialIP, bool allowName, ComparableAddress& outAddress);

        // Constructors (private, use TryParse)
        ComparableAddress();

        // Accessors
        bool isIP() const { return this->addressIP_.protocol() != QAbstractSocket::UnknownNetworkLayerProtocol; }
        bool isPartialIP() const { return this->isPartialIP_; }
        bool isHostname() const { return !isIP() && !isPartialIP(); }
        QHostAddress getAddressIP() const { return this->addressIP_; }
        QString toString() const;

        // Comparison operators
        bool operator==(const ComparableAddress& other) const;
        bool operator!=(const ComparableAddress& other) const;
        bool operator<(const ComparableAddress& other) const;
        int compareTo(const ComparableAddress& other) const;

        // Special: equals IPAddress
        bool equalsIP(const QHostAddress& ip) const;

        // Hash support
        uint qHash() const;

    private:
        // Private constructors
        explicit ComparableAddress(const QHostAddress& ip);
        explicit ComparableAddress(const QString& partialIP);
        ComparableAddress(const QString& hostname, bool isName);

        // Partial IP matching
        bool partialIPEquals(const QHostAddress& ip) const;
        static bool isPartialIPPattern(const QString& str);

        QHostAddress addressIP_;
        QString partialIPPattern_;  // e.g., "192.168.*.*"
        QString addressString_;     // Hostname
        bool isPartialIP_;
};

// Qt container support
inline uint qHash(const ComparableAddress& addr)
{
    return addr.qHash();
}

#endif // COMPARABLEADDRESS_H
