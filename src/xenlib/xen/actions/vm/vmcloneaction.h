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

#ifndef VMCLONEACTION_H
#define VMCLONEACTION_H

#include "../../asyncoperation.h"
#include "../../vm.h"

/**
 * @brief VMCloneAction - Clones a VM
 *
 * Matches C# XenAdmin.Actions.VMActions.VMCloneAction
 *
 * This action:
 * 1. Clones the VM using VM.async_clone
 * 2. Sets the name and description of the new VM
 * 3. Returns the ref of the cloned VM in result()
 *
 * C# location: XenModel/Actions/VM/VMCloneAction.cs
 */
class VMCloneAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param vm VM object to clone
         * @param name Name for the cloned VM
         * @param description Description for the cloned VM
         * @param parent Parent QObject
         */
        explicit VMCloneAction(XenConnection* connection,
                               QSharedPointer<VM> vm,
                               const QString& name,
                               const QString& description,
                               QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<VM> m_vm;
        QString m_cloneName;
        QString m_cloneDescription;
};

#endif // VMCLONEACTION_H
