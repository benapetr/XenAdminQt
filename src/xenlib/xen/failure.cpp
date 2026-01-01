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

#include "failure.h"
#include "friendlyerrornames.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QDebug>

// Define error code constants (matches xenadmin/XenModel/XenAPI-Extensions/Failure.cs)
const char* const Failure::CANNOT_EVACUATE_HOST = "CANNOT_EVACUATE_HOST";
const char* const Failure::DEVICE_ALREADY_DETACHED = "DEVICE_ALREADY_DETACHED";
const char* const Failure::DYNAMIC_MEMORY_CONTROL_UNAVAILABLE = "DYNAMIC_MEMORY_CONTROL_UNAVAILABLE";
const char* const Failure::HANDLE_INVALID = "HANDLE_INVALID";
const char* const Failure::HA_NO_PLAN = "HA_NO_PLAN";
const char* const Failure::HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN = "HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN";
const char* const Failure::HOST_IS_SLAVE = "HOST_IS_SLAVE";
const char* const Failure::HOST_OFFLINE = "HOST_OFFLINE";
const char* const Failure::HOST_STILL_BOOTING = "HOST_STILL_BOOTING";
const char* const Failure::NO_HOSTS_AVAILABLE = "NO_HOSTS_AVAILABLE";
const char* const Failure::PATCH_ALREADY_EXISTS = "PATCH_ALREADY_EXISTS";
const char* const Failure::PATCH_APPLY_FAILED = "PATCH_APPLY_FAILED";
const char* const Failure::SESSION_AUTHENTICATION_FAILED = "SESSION_AUTHENTICATION_FAILED";
const char* const Failure::SESSION_INVALID = "SESSION_INVALID";
const char* const Failure::SR_HAS_NO_PBDS = "SR_HAS_NO_PBDS";
const char* const Failure::VM_BAD_POWER_STATE = "VM_BAD_POWER_STATE";
const char* const Failure::VM_REQUIRES_SR = "VM_REQUIRES_SR";
const char* const Failure::VM_REQUIRES_NETWORK = "VM_REQUIRES_NETWORK";
const char* const Failure::VM_REQUIRES_GPU = "VM_REQUIRES_GPU";
const char* const Failure::VM_MISSING_PV_DRIVERS = "VM_MISSING_PV_DRIVERS";
const char* const Failure::HOST_NOT_ENOUGH_FREE_MEMORY = "HOST_NOT_ENOUGH_FREE_MEMORY";
const char* const Failure::SR_BACKEND_FAILURE_72 = "SR_BACKEND_FAILURE_72";
const char* const Failure::SR_BACKEND_FAILURE_73 = "SR_BACKEND_FAILURE_73";
const char* const Failure::SR_BACKEND_FAILURE_107 = "SR_BACKEND_FAILURE_107";
const char* const Failure::SR_BACKEND_FAILURE_111 = "SR_BACKEND_FAILURE_111";
const char* const Failure::SR_BACKEND_FAILURE_112 = "SR_BACKEND_FAILURE_112";
const char* const Failure::SR_BACKEND_FAILURE_113 = "SR_BACKEND_FAILURE_113";
const char* const Failure::SR_BACKEND_FAILURE_114 = "SR_BACKEND_FAILURE_114";
const char* const Failure::SR_BACKEND_FAILURE_140 = "SR_BACKEND_FAILURE_140";
const char* const Failure::SR_BACKEND_FAILURE_222 = "SR_BACKEND_FAILURE_222";
const char* const Failure::SR_BACKEND_FAILURE_225 = "SR_BACKEND_FAILURE_225";
const char* const Failure::SR_BACKEND_FAILURE_454 = "SR_BACKEND_FAILURE_454";
const char* const Failure::SUBJECT_CANNOT_BE_RESOLVED = "SUBJECT_CANNOT_BE_RESOLVED";
const char* const Failure::OBJECT_NOLONGER_EXISTS = "OBJECT_NOLONGER_EXISTS";
const char* const Failure::PERMISSION_DENIED = "PERMISSION_DENIED";
const char* const Failure::RBAC_PERMISSION_DENIED_FRIENDLY = "RBAC_PERMISSION_DENIED_FRIENDLY";
const char* const Failure::RBAC_PERMISSION_DENIED = "RBAC_PERMISSION_DENIED";
const char* const Failure::LICENSE_CHECKOUT_ERROR = "LICENSE_CHECKOUT_ERROR";
const char* const Failure::VDI_IN_USE = "VDI_IN_USE";
const char* const Failure::AUTH_ENABLE_FAILED = "AUTH_ENABLE_FAILED";
const char* const Failure::POOL_AUTH_ENABLE_FAILED_WRONG_CREDENTIALS = "POOL_AUTH_ENABLE_FAILED_WRONG_CREDENTIALS";
const char* const Failure::HOST_UNKNOWN_TO_MASTER = "HOST_UNKNOWN_TO_MASTER";
const char* const Failure::VM_HAS_VGPU = "VM_HAS_VGPU";
const char* const Failure::VM_HAS_PCI_ATTACHED = "VM_HAS_PCI_ATTACHED";
const char* const Failure::OUT_OF_SPACE = "OUT_OF_SPACE";
const char* const Failure::PVS_SITE_CONTAINS_RUNNING_PROXIES = "PVS_SITE_CONTAINS_RUNNING_PROXIES";
const char* const Failure::VM_LACKS_FEATURE = "VM_LACKS_FEATURE";
const char* const Failure::VM_LACKS_FEATURE_SUSPEND = "VM_LACKS_FEATURE_SUSPEND";
const char* const Failure::VM_FAILED_SHUTDOWN_ACKNOWLEDGMENT = "VM_FAILED_SHUTDOWN_ACKNOWLEDGMENT";
const char* const Failure::OTHER_OPERATION_IN_PROGRESS = "OTHER_OPERATION_IN_PROGRESS";
const char* const Failure::PATCH_ALREADY_APPLIED = "PATCH_ALREADY_APPLIED";
const char* const Failure::UPDATE_ALREADY_APPLIED = "UPDATE_ALREADY_APPLIED";
const char* const Failure::UPDATE_ALREADY_EXISTS = "UPDATE_ALREADY_EXISTS";
const char* const Failure::UPDATES_REQUIRE_RECOMMENDED_GUIDANCE = "UPDATES_REQUIRE_RECOMMENDED_GUIDANCE";
const char* const Failure::MEMORY_CONSTRAINT_VIOLATION = "MEMORY_CONSTRAINT_VIOLATION";
const char* const Failure::VIF_NOT_IN_MAP = "VIF_NOT_IN_MAP";
const char* const Failure::INTERNAL_ERROR = "INTERNAL_ERROR";
const char* const Failure::MESSAGE_PARAMETER_COUNT_MISMATCH = "MESSAGE_PARAMETER_COUNT_MISMATCH";

// Constructors

Failure::Failure(const QStringList& errorDescription)
    : std::runtime_error("XenAPI Failure")
    , m_errorDescription(errorDescription)
{
    this->parseExceptionMessage();
}

Failure::Failure(const QString& errorCode)
    : Failure(QStringList() << errorCode)
{
}

Failure::Failure(const QString& errorCode, const QString& param1)
    : Failure(QStringList() << errorCode << param1)
{
}

Failure::Failure(const QString& errorCode, const QString& param1, const QString& param2)
    : Failure(QStringList() << errorCode << param1 << param2)
{
}

// Property accessors

QString Failure::errorCode() const
{
    return this->m_errorDescription.isEmpty() ? QString() : this->m_errorDescription.first();
}

// Parse error description and generate friendly error message
// Matches xenadmin/XenModel/XenAPI/Failure.cs ParseExceptionMessage()
void Failure::parseExceptionMessage()
{
    if (this->m_errorDescription.isEmpty())
    {
        this->m_errorText = "Unknown XenAPI error";
        this->m_shortError = this->m_errorText;
        return;
    }

    QString code = this->m_errorDescription.first();

    // Try to get friendly error format string from FriendlyErrorNames
    QString formatString = FriendlyErrorNames::getString(code);

    if (formatString.isEmpty())
    {
        // If we don't have a translation, just combine all the error results from the server
        // Only show non-empty bits of ErrorDescription, and trim them
        QStringList cleanBits;
        for (const QString& s : this->m_errorDescription)
        {
            QString trimmed = s.trimmed();
            if (!trimmed.isEmpty())
                cleanBits.append(trimmed);
        }

        this->m_errorText = cleanBits.join(" - ");
    }
    else
    {
        // We have a format string - fill in parameters
        // Parameters are errorDescription[1], errorDescription[2], etc.
        // Use QString::arg() to fill them in
        QString result = formatString;

        // Replace placeholders {0}, {1}, {2}, etc. with parameters
        for (int i = 1; i < this->m_errorDescription.count(); ++i)
        {
            QString placeholder = QString("{%1}").arg(i - 1);
            result.replace(placeholder, this->m_errorDescription[i]);
        }

        this->m_errorText = result;
    }

    // Try to get short error message (error code + "-SHORT" suffix)
    QString shortKey = code + "-SHORT";
    this->m_shortError = FriendlyErrorNames::getString(shortKey);
    if (this->m_shortError.isEmpty())
        this->m_shortError = this->m_errorText;

    this->parseSmapiV3Failures();
    this->parseCslgFailures();
}

void Failure::parseSmapiV3Failures()
{
    if (this->m_errorDescription.count() < 3)
        return;

    const QString code = this->m_errorDescription.first();
    if (code.isEmpty() || !code.startsWith("SR_BACKEND_FAILURE"))
        return;

    const QString jsonPayload = this->m_errorDescription[2];
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonPayload.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return;

    QJsonObject obj = doc.object();
    const QString errorText = obj.value(QStringLiteral("error")).toString();
    if (!errorText.isEmpty())
        this->m_errorText = errorText;
}

void Failure::parseCslgFailures()
{
    if (this->m_errorDescription.count() < 3)
        return;

    const QString code = this->m_errorDescription.first();
    if (code.isEmpty() || !code.startsWith("SR_BACKEND_FAILURE"))
        return;

    const QString payload = this->m_errorDescription[2];
    QRegularExpression re(QStringLiteral("<StorageLinkServiceError>.*</StorageLinkServiceError>"),
                          QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(payload);
    if (!match.hasMatch())
        return;

    const QString xml = match.captured(0);
    QXmlStreamReader reader(xml);
    QString faultText;
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("Fault"))
        {
            faultText = reader.readElementText();
            break;
        }
    }
    if (faultText.isEmpty())
        return;

    if (this->m_errorText.isEmpty())
        this->m_errorText = faultText;
    else
        this->m_errorText = QString("%1 (%2)").arg(this->m_errorText, faultText);
}
