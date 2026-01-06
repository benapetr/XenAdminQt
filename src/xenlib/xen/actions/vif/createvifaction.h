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

#ifndef CREATEVIFACTION_H
#define CREATEVIFACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QVariantMap>

/**
 * @brief CreateVIFAction - Creates a virtual network interface
 *
 * Qt port of C# XenAdmin.Actions.CreateVIFAction.
 * Creates a new VIF (Virtual Interface) for a VM and optionally hot-plugs it.
 *
 * C# path: XenModel/Actions/VIF/CreateVIFAction.cs
 *
 * Features:
 * - Creates VIF using async_create task
 * - Attempts hot-plug if VM is running and operation is allowed
 * - Sets RebootRequired property if hot-plug not possible
 */
class CreateVIFAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param vmRef VM opaque reference
         * @param vifRecord VIF record (VM, network, device, MAC, MTU, etc.)
         * @param parent Parent QObject
         */
        CreateVIFAction(XenConnection* connection,
                        const QString& vmRef,
                        const QVariantMap& vifRecord,
                        QObject* parent = nullptr);

        /**
         * @brief Check if VM reboot is required
         * @return true if hot-plug was not possible and reboot is needed
         */
        bool rebootRequired() const
        {
            return m_rebootRequired;
        }

    protected:
        void run() override;

    private:
        QString m_vmRef;
        QString m_vmName;
        QVariantMap m_vifRecord;
        bool m_rebootRequired;
};

#endif // CREATEVIFACTION_H
