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

#ifndef VDIDISABLECBTACTION_H
#define VDIDISABLECBTACTION_H

#include "../../asyncoperation.h"

class XenConnection;
class VM;

/**
 * @brief VDIDisableCbtAction - Disable Changed Block Tracking for a VDI
 *
 * Matches C# XenAdmin.Actions.VDIDisableCbtAction
 *
 * This action calls VDI.async_disable_cbt to disable CBT for a virtual disk.
 * Changed Block Tracking allows incremental backups by tracking which blocks
 * have changed since the last backup.
 *
 * C# location: XenModel/Actions/VDI/VDIDisableCbtAction.cs
 */
class VDIDisableCbtAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param vmName Name of the VM owning this VDI (for display)
         * @param vdiRef VDI reference
         * @param parent Parent QObject
         */
        explicit VDIDisableCbtAction(XenConnection* connection,
                                     const QString& vmName,
                                     const QString& vdiRef,
                                     QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vdiRef;
        QString m_vmName;
};

#endif // VDIDISABLECBTACTION_H
