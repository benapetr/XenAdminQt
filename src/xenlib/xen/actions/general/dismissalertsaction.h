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

#ifndef DISMISSALERTSACTION_H
#define DISMISSALERTSACTION_H

#include "../../asyncoperation.h"
#include <QStringList>

/**
 * @brief Destroys the XenAPI messages behind dismissed alerts on one connection.
 *
 * C# Reference: XenAdmin/Actions/GUIActions/DismissAlertsAction.cs
 *
 * Unlike the C# action this works on message opaque refs instead of Alert objects,
 * because alerts are owned and deleted by AlertManager on the UI thread. The caller
 * removes the alerts from AlertManager when the action completes (the event poller
 * also removes them when the server reports the messages as deleted).
 */
class XENLIB_EXPORT DismissAlertsAction : public AsyncOperation
{
    Q_OBJECT

    public:
        DismissAlertsAction(XenConnection* connection, const QStringList& messageRefs, QObject* parent = nullptr);

        QStringList GetMessageRefs() const { return this->m_messageRefs; }

    protected:
        void run() override;

    private:
        static QString actionTitle(XenConnection* connection, int alertCount);

        QStringList m_messageRefs;
        bool m_canDestroyMany = false;
};

#endif // DISMISSALERTSACTION_H
