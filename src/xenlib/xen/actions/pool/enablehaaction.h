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

#ifndef ENABLEHAACTION_H
#define ENABLEHAACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>

class XenConnection;
class VM;
class SR;

/**
 * @brief EnableHAAction enables High Availability on a pool.
 *
 * Matches C# XenModel/Actions/Pool/EnableHAAction.cs
 *
 * Note: This is a simplified version. The C# implementation includes:
 * - VM restart priority configuration (VMStartupOptions)
 * - Complex error handling for VDI_NOT_AVAILABLE
 * These features are partially implemented.
 */
class EnableHAAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for enabling HA
     * @param connection Connection to the pool
     * @param poolRef Pool opaque reference
     * @param heartbeatSRRefs List of SR refs to use for heartbeat
     * @param failuresToTolerate Number of host failures to tolerate (0-3 typically)
     * @param vmStartupOptions Optional map of VM ref -> startup options (priority, order, delay)
     * @param parent Parent QObject
     */
    EnableHAAction(XenConnection* connection,
                   const QString& poolRef,
                   const QStringList& heartbeatSRRefs,
                   qint64 failuresToTolerate,
                   const QMap<QString, QVariantMap>& vmStartupOptions = QMap<QString, QVariantMap>(),
                   QObject* parent = nullptr);

protected:
    void run() override;

private:
    QString m_poolRef;
    QStringList m_heartbeatSRRefs;
    qint64 m_failuresToTolerate;
    QMap<QString, QVariantMap> m_vmStartupOptions; // VM ref -> {priority, order, delay}
};

#endif // ENABLEHAACTION_H
