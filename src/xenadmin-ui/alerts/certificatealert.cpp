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

#include "certificatealert.h"
#include <QXmlStreamReader>
#include <QDebug>

CertificateAlert::CertificateAlert(XenConnection* connection, const QVariantMap& messageData)
    : MessageAlert(connection, messageData),
      m_certType(CertificateType::Unknown),
      m_isExpired(false),
      m_daysUntilExpiry(0)
{
    this->parseCertificateMessage();
}

void CertificateAlert::parseCertificateMessage()
{
    // C# Reference: CertificateAlert.cs constructor line 44
    // Parse XML body: <date>2024-12-31T23:59:59Z</date>
    
    QString body = this->messageBody();
    QString msgType = this->messageType();
    
    // Determine certificate type
    if (msgType.startsWith("POOL_CA_CERTIFICATE"))
        this->m_certType = CertificateType::PoolCA;
    else if (msgType.startsWith("HOST_INTERNAL_CERTIFICATE"))
        this->m_certType = CertificateType::HostInternal;
    else if (msgType.startsWith("HOST_SERVER_CERTIFICATE"))
        this->m_certType = CertificateType::HostServer;
    
    // Determine if expired or expiring
    this->m_isExpired = msgType.contains("_EXPIRED");
    
    // Parse expiry date from XML
    QXmlStreamReader reader(body);
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("date"))
        {
            QString dateStr = reader.readElementText();
            this->m_expiryDate = QDateTime::fromString(dateStr, Qt::ISODate);
            
            if (this->m_expiryDate.isValid())
            {
                // Calculate days until expiry
                QDateTime now = QDateTime::currentDateTime();
                qint64 secondsUntilExpiry = now.secsTo(this->m_expiryDate);
                this->m_daysUntilExpiry = static_cast<int>(secondsUntilExpiry / 86400);
                
                qDebug() << "CertificateAlert: Expiry date" << this->m_expiryDate
                         << "Days until expiry:" << this->m_daysUntilExpiry;
            }
            break;
        }
    }
    
    if (reader.hasError())
    {
        qDebug() << "CertificateAlert: XML parse error:" << reader.errorString();
    }
}

QString CertificateAlert::title() const
{
    // C# Reference: CertificateAlert.cs Title property line 58
    QString objectName = this->appliesTo();
    if (objectName.isEmpty())
        objectName = tr("Unknown");
    
    QString timeStr = this->formatExpiryTime();
    
    switch (this->m_certType)
    {
        case CertificateType::PoolCA:
            if (this->m_isExpired)
                return tr("Pool CA certificate on %1 has expired").arg(objectName);
            else
                return tr("Pool CA certificate on %1 expires %2").arg(objectName, timeStr);
            
        case CertificateType::HostInternal:
            if (this->m_isExpired)
                return tr("Host internal certificate on %1 has expired").arg(objectName);
            else
                return tr("Host internal certificate on %1 expires %2").arg(objectName, timeStr);
            
        case CertificateType::HostServer:
            if (this->m_isExpired)
                return tr("Host server certificate on %1 has expired").arg(objectName);
            else
                return tr("Host server certificate on %1 expires %2").arg(objectName, timeStr);
            
        default:
            return tr("Certificate alert for %1").arg(objectName);
    }
}

QString CertificateAlert::description() const
{
    // C# Reference: CertificateAlert.cs Description property line 149
    QString objectName = this->appliesTo();
    if (objectName.isEmpty())
        objectName = tr("Unknown");
    
    QString timeStr = this->formatExpiryTime();
    
    switch (this->m_certType)
    {
        case CertificateType::PoolCA:
            if (this->m_isExpired)
                return tr("The pool CA certificate on %1 has expired. "
                         "Please install a new certificate to restore secure connections.")
                    .arg(objectName);
            else
                return tr("The pool CA certificate on %1 will expire %2. "
                         "Please install a new certificate before expiration.")
                    .arg(objectName, timeStr);
            
        case CertificateType::HostInternal:
            if (this->m_isExpired)
                return tr("The host internal certificate on %1 has expired. "
                         "The host may not function correctly until the certificate is renewed.")
                    .arg(objectName);
            else
                return tr("The host internal certificate on %1 will expire %2. "
                         "The certificate should be renewed before expiration.")
                    .arg(objectName, timeStr);
            
        case CertificateType::HostServer:
            if (this->m_isExpired)
                return tr("The host server certificate on %1 has expired. "
                         "Secure connections to this host may fail.")
                    .arg(objectName);
            else
                return tr("The host server certificate on %1 will expire %2. "
                         "Please renew the certificate before expiration.")
                    .arg(objectName, timeStr);
            
        default:
            return MessageAlert::description();
    }
}

QString CertificateAlert::formatExpiryTime() const
{
    // C# Reference: CertificateAlert.cs Title - time formatting logic line 73-93
    if (!this->m_expiryDate.isValid())
        return tr("soon");
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 secondsUntilExpiry = now.secsTo(this->m_expiryDate);
    
    if (secondsUntilExpiry < 0)
        return tr("recently");  // Already expired
    
    double hours = secondsUntilExpiry / 3600.0;
    double days = hours / 24.0;
    double minutes = secondsUntilExpiry / 60.0;
    
    if (days >= 1.0)
        return tr("in %1 day(s)").arg(qRound(days));
    
    if (hours >= 1.0)
        return tr("in %1 hour(s)").arg(qRound(hours));
    
    if (minutes >= 1.0)
        return tr("in %1 minute(s)").arg(qRound(minutes));
    
    return tr("very soon");
}
