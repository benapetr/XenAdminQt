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

#ifndef VMCOPYACTION_H
#define VMCOPYACTION_H

#include "../../asyncoperation.h"
#include <QString>

class XenConnection;
class VM;
class SR;
class Host;

/**
 * @brief VMCopyAction copies a VM to a specified SR with a new name.
 *
 * This creates a full copy of the VM on the target storage.
 * Matches C# XenModel/Actions/VM/VMCopyAction.cs
 */
class VMCopyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for VM copy action
         * @param connection XenServer connection
         * @param vm VM to copy
         * @param host Host to place the copy on (can be nullptr)
         * @param sr SR to copy VM to
         * @param nameLabel Name for the copied VM
         * @param description Description for the copied VM
         */
        VMCopyAction(QSharedPointer<VM> vm,
                     QSharedPointer<Host> host,
                     QSharedPointer<SR> sr,
                     const QString& nameLabel,
                     const QString& description,
                     QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<VM> m_vm;
        QSharedPointer<Host> m_host;
        QSharedPointer<SR> m_sr;
        QString m_nameLabel;
        QString m_description;
};

#endif // VMCOPYACTION_H
