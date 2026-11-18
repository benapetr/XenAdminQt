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

#ifndef FAILEDLOGINATTEMPTALERT_H
#define FAILEDLOGINATTEMPTALERT_H

#include "messagealert.h"
#include <QDateTime>
#include <QList>

namespace XenLib
{
/**
 * Alert for FAILED_LOGIN_ATTEMPTS messages.
 *
 * The message body is XML with optional <total> and <unknown> counts and
 * zero or more <known> entries describing the most common offenders
 * (number, date and optionally ip, originator, username, useragent).
 *
 * C# Reference: XenAdmin/Alerts/Types/FailedLoginAttemptAlert.cs
 */
class XENLIB_EXPORT FailedLoginAttemptAlert : public MessageAlert
{
    Q_OBJECT

    public:
        explicit FailedLoginAttemptAlert(XenConnection* connection, const QVariantMap& messageData);

        QString GetTitle() const override;
        QString GetDescription() const override;

    private:
        struct Offender
        {
            int number = 0;       // Number of failed login attempts from this offender
            QDateTime date;       // Last failed login attempt (UTC), invalid if unknown
            QString ip;
            QString originator;
            QString username;
            QString useragent;

            QString ToString() const;
        };

        void parseBody();

        int m_total = 0;
        int m_unknown = 0;
        QList<Offender> m_topOffenders;
};
} // XenLib

#endif // FAILEDLOGINATTEMPTALERT_H
