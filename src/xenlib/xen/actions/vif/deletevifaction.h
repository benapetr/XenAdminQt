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

#ifndef DELETEVIFACTION_H
#define DELETEVIFACTION_H

#include "../../asyncoperation.h"
#include <QString>

/**
 * @brief DeleteVIFAction - Deletes a virtual network interface
 *
 * Qt port of C# XenAdmin.Actions.DeleteVIFAction.
 * Unplugs (if running) and destroys a VIF.
 *
 * C# path: XenModel/Actions/VIF/DeleteVIFAction.cs
 *
 * Features:
 * - Unplugs VIF if VM is running and operation is allowed
 * - Destroys VIF record
 * - Handles DEVICE_ALREADY_DETACHED gracefully
 */
class DeleteVIFAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection
         * @param vifRef VIF opaque reference
         * @param parent Parent QObject
         */
        DeleteVIFAction(XenConnection* connection,
                        const QString& vifRef,
                        QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vifRef;
        QString m_vmRef;
        QString m_vmName;
        QString m_networkName;
};

#endif // DELETEVIFACTION_H
