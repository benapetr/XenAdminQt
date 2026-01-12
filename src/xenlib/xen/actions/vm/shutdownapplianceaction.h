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

#ifndef SHUTDOWNAPPLIANCEACTION_H
#define SHUTDOWNAPPLIANCEACTION_H

#include "../../asyncoperation.h"
#include <QString>

/**
 * @brief Action to shutdown a VM appliance (vApp)
 *
 * Port of C# ShutDownApplianceAction from:
 * xenadmin/XenModel/Actions/VMAppliances/ShutDownApplianceAction.cs
 *
 * Shuts down all VMs in the appliance. Try clean shutdown first, fall back to hard shutdown.
 */
class ShutDownApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct ShutDownApplianceAction
         * @param connection XenConnection
         * @param applianceRef OpaqueRef of VM_appliance to shut down
         * @param parent Parent QObject
         */
        explicit ShutDownApplianceAction(XenConnection* connection, const QString& applianceRef, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_applianceRef;
};

/**
 * @brief Action to perform clean shutdown of VM appliance
 *
 * Gracefully shuts down all VMs via guest OS.
 */
class CleanShutDownApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit CleanShutDownApplianceAction(XenConnection* connection, const QString& applianceRef,
                                              QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_applianceRef;
};

/**
 * @brief Action to perform hard shutdown of VM appliance
 *
 * Immediately powers off all VMs (equivalent to pulling power cable).
 */
class HardShutDownApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit HardShutDownApplianceAction(XenConnection* connection, const QString& applianceRef,
                                             QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_applianceRef;
};

#endif // SHUTDOWNAPPLIANCEACTION_H
