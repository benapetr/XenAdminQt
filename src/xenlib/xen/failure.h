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

#ifndef FAILURE_H
#define FAILURE_H

#include "../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <stdexcept>

/*!
 * \brief XenAPI Failure exception class
 *
 * Represents a XenAPI failure response with error code and description.
 * Matches xenadmin/XenModel/XenAPI/Failure.cs structure.
 *
 * XenAPI failures have format: ["ERROR_CODE", "param1", "param2", ...]
 * The first element is the error code constant, rest are parameters for the error message.
 */
class XENLIB_EXPORT Failure : public std::runtime_error
{
    public:
        // Common XenAPI error code constants (from xenadmin/XenModel/XenAPI-Extensions/Failure.cs)
        static const char* const CANNOT_EVACUATE_HOST;
        static const char* const DEVICE_ALREADY_DETACHED;
        static const char* const DYNAMIC_MEMORY_CONTROL_UNAVAILABLE;
        static const char* const HANDLE_INVALID;
        static const char* const HA_NO_PLAN;
        static const char* const HA_OPERATION_WOULD_BREAK_FAILOVER_PLAN;
        static const char* const HOST_IS_SLAVE;
        static const char* const HOST_OFFLINE;
        static const char* const HOST_STILL_BOOTING;
        static const char* const NO_HOSTS_AVAILABLE;
        static const char* const PATCH_ALREADY_EXISTS;
        static const char* const PATCH_APPLY_FAILED;
        static const char* const SESSION_AUTHENTICATION_FAILED;
        static const char* const SESSION_INVALID;
        static const char* const SR_HAS_NO_PBDS;
        static const char* const VM_BAD_POWER_STATE;
        static const char* const VM_REQUIRES_SR;
        static const char* const VM_REQUIRES_NETWORK;
        static const char* const VM_REQUIRES_GPU;
        static const char* const VM_MISSING_PV_DRIVERS;
        static const char* const HOST_NOT_ENOUGH_FREE_MEMORY;
        static const char* const SR_BACKEND_FAILURE_72;
        static const char* const SR_BACKEND_FAILURE_73;
        static const char* const SR_BACKEND_FAILURE_107;
        static const char* const SR_BACKEND_FAILURE_111;
        static const char* const SR_BACKEND_FAILURE_112;
        static const char* const SR_BACKEND_FAILURE_113;
        static const char* const SR_BACKEND_FAILURE_114;
        static const char* const SR_BACKEND_FAILURE_140;
        static const char* const SR_BACKEND_FAILURE_222;
        static const char* const SR_BACKEND_FAILURE_225;
        static const char* const SR_BACKEND_FAILURE_454;
        static const char* const SUBJECT_CANNOT_BE_RESOLVED;
        static const char* const OBJECT_NOLONGER_EXISTS;
        static const char* const PERMISSION_DENIED;
        static const char* const RBAC_PERMISSION_DENIED_FRIENDLY;
        static const char* const RBAC_PERMISSION_DENIED;
        static const char* const LICENSE_CHECKOUT_ERROR;
        static const char* const VDI_IN_USE;
        static const char* const AUTH_ENABLE_FAILED;
        static const char* const POOL_AUTH_ENABLE_FAILED_WRONG_CREDENTIALS;
        static const char* const HOST_UNKNOWN_TO_MASTER;
        static const char* const VM_HAS_VGPU;
        static const char* const VM_HAS_PCI_ATTACHED;
        static const char* const OUT_OF_SPACE;
        static const char* const PVS_SITE_CONTAINS_RUNNING_PROXIES;
        static const char* const VM_LACKS_FEATURE;
        static const char* const VM_LACKS_FEATURE_SUSPEND;
        static const char* const VM_FAILED_SHUTDOWN_ACKNOWLEDGMENT;
        static const char* const OTHER_OPERATION_IN_PROGRESS;
        static const char* const PATCH_ALREADY_APPLIED;
        static const char* const UPDATE_ALREADY_APPLIED;
        static const char* const UPDATE_ALREADY_EXISTS;
        static const char* const UPDATES_REQUIRE_RECOMMENDED_GUIDANCE;
        static const char* const MEMORY_CONSTRAINT_VIOLATION;
        static const char* const VIF_NOT_IN_MAP;
        static const char* const INTERNAL_ERROR;
        static const char* const MESSAGE_PARAMETER_COUNT_MISMATCH;

        // Constructors
        explicit Failure(const QStringList& errorDescription);
        explicit Failure(const QString& errorCode);
        Failure(const QString& errorCode, const QString& param1);
        Failure(const QString& errorCode, const QString& param1, const QString& param2);

        // Properties matching C# Failure class
        const QStringList& errorDescription() const { return this->m_errorDescription; }
        QString message() const { return this->m_errorText; }
        QString shortMessage() const { return this->m_shortError; }

        // std::exception interface
        const char* what() const noexcept override { return this->m_errorText.toUtf8().constData(); }

        // Error code (first element of errorDescription)
        QString errorCode() const;

    private:
        QStringList m_errorDescription;
        QString m_errorText;        // Friendly error message with parameters filled in
        QString m_shortError;       // Short version of error (if available)

        // Parse error description and generate friendly messages
        void parseExceptionMessage();
        void parseSmapiV3Failures();
        void parseCslgFailures();
};

#endif // FAILURE_H
