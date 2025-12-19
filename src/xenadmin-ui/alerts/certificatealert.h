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

#ifndef CERTIFICATEALERT_H
#define CERTIFICATEALERT_H

#include "messagealert.h"
#include <QDateTime>

/**
 * Certificate types for alert messages
 */
enum class CertificateType
{
    Unknown,
    PoolCA,           // Pool CA certificate
    HostInternal,     // Host internal certificate
    HostServer        // Host server certificate
};

/**
 * Specialized alert for certificate expiry warnings.
 * Handles:
 * - POOL_CA_CERTIFICATE_EXPIRED / EXPIRING_07/14/30
 * - HOST_INTERNAL_CERTIFICATE_EXPIRED / EXPIRING_07/14/30
 * - HOST_SERVER_CERTIFICATE_EXPIRED / EXPIRING_07/14/30
 * 
 * Parses XML body to extract certificate expiry date.
 * 
 * C# Reference: XenAdmin/Alerts/Types/CertificateAlert.cs
 */
class CertificateAlert : public MessageAlert
{
    Q_OBJECT

public:
    explicit CertificateAlert(XenConnection* connection, const QVariantMap& messageData);

    // Override alert properties
    QString title() const override;
    QString description() const override;

private:
    void parseCertificateMessage();
    QString formatExpiryTime() const;
    
    CertificateType m_certType;
    QDateTime m_expiryDate;
    bool m_isExpired;
    int m_daysUntilExpiry;
};

#endif // CERTIFICATEALERT_H
