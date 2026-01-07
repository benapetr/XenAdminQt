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

#ifndef VBDCREATEANDPLUGACTION_H
#define VBDCREATEANDPLUGACTION_H

#include "xen/asyncoperation.h"
#include <QString>
#include <QVariantMap>

class VM;
class XenConnection;

/**
 * @brief Creates a VBD and attempts to hot-plug it to a VM
 *
 * This action creates a VBD record in the XenServer database and then
 * attempts to hot-plug it to the VM if possible. For HVM VMs and non-empty
 * VBDs (disk drives), it checks if the plug operation is allowed and performs
 * it. For PV VMs with empty VBDs (CD drives), no plug is needed.
 *
 * C# reference: XenModel/Actions/VBD/VbdCreateAndPlugAction.cs
 */
class VbdCreateAndPlugAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a VBD create and plug action
         * @param vm The VM to attach the VBD to
         * @param vbdRecord The VBD record to create (should include VM, VDI, device, etc.)
         * @param vdiName Name of the VDI being attached (for display purposes)
         * @param suppress Suppress progress notifications
         * @param parent Parent QObject
         */
        VbdCreateAndPlugAction(QSharedPointer<VM> vm, const QVariantMap& vbdRecord,
                               const QString& vdiName, bool suppress = false,
                               QObject* parent = nullptr);

    signals:
        /**
         * @brief Emitted when user action is required (e.g., reboot VM)
         * @param instruction User instruction message
         */
        void showUserInstruction(const QString& instruction);

    protected:
        void run() override;

    private:
        QSharedPointer<VM> m_vm;
        QVariantMap m_vbdRecord;
        QString m_vdiName;
        bool m_suppress;

        bool isVMHVM() const;
        bool isVBDEmpty() const;
};

#endif // VBDCREATEANDPLUGACTION_H
