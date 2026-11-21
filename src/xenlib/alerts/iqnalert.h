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

#ifndef IQNALERT_H
#define IQNALERT_H

#include "alert.h"
#include <QSharedPointer>

class Host;

namespace XenLib
{
    /**
     * Client-side alert about a server's iSCSI IQN: missing, or the same as another server's.
     *
     * C# Reference: XenAdmin/Alerts/Types/IqnAlert.cs, MissingIqnAlert.cs, DuplicateIqnAlert.cs
     * and HelpersGUI.PerformIQNCheck. The UI gives it an "Edit IQN" fix link.
     */
    class XENLIB_EXPORT IqnAlert : public Alert
    {
        Q_OBJECT

        public:
            QSharedPointer<Host> GetHost() const { return this->m_host; }

            AlertPriority GetPriority() const override { return AlertPriority::Priority3; }
            QString AppliesTo() const override;
            QString GetFixLinkText() const override { return tr("Edit IQN"); }
            void Dismiss() override;

            /**
             * Adds alerts for live servers without an IQN and for servers sharing an IQN, on
             * every connection, unless an equal alert is already shown.
             *
             * C# HelpersGUI.PerformIQNCheck, which runs it when iSCSI is used (iSCSIisUsed)
             * after connecting, and when the new SR wizard's iSCSI page is shown.
             */
            static void PerformCheck(const QList<XenConnection*>& connections);

            //! C# HelpersGUI.iSCSIisUsed: an lvmoiscsi SR exists on any connection
            static bool IsIscsiUsed(const QList<XenConnection*>& connections);

        protected:
            explicit IqnAlert(const QSharedPointer<Host>& host);

            QSharedPointer<Host> m_host;
    };

    class XENLIB_EXPORT MissingIqnAlert : public IqnAlert
    {
        Q_OBJECT

        public:
            explicit MissingIqnAlert(const QSharedPointer<Host>& host) : IqnAlert(host) {}

            QString GetTitle() const override;
            QString GetDescription() const override;
    };

    class XENLIB_EXPORT DuplicateIqnAlert : public IqnAlert
    {
        Q_OBJECT

        public:
            //! \a other is the server that already has this server's IQN
            DuplicateIqnAlert(const QSharedPointer<Host>& host, const QSharedPointer<Host>& other);

            QString GetTitle() const override;
            QString GetDescription() const override;

        private:
            QSharedPointer<Host> m_other;
    };
}

#endif // IQNALERT_H
