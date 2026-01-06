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

#ifndef CREATEVMFASTACTION_H
#define CREATEVMFASTACTION_H

#include "../../asyncoperation.h"
#include "../../vm.h"

/**
 * @brief CreateVMFastAction - Fast VM creation (clone + provision)
 *
 * Matches C# XenAdmin.Actions.VMActions.CreateVMFastAction
 *
 * This action:
 * 1. Clones a template using VM.async_clone (with hidden name)
 * 2. Provisions the VM using VM.async_provision
 * 3. Sets the final name
 * 4. Returns the ref of the created VM in result()
 *
 * C# location: XenModel/Actions/VM/CreateVMFastAction.cs
 */
class CreateVMFastAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor
         * @param connection XenConnection to use
         * @param templateVM Template VM to clone from
         * @param parent Parent QObject
         */
        explicit CreateVMFastAction(XenConnection* connection,
                                    QSharedPointer<VM> templateVM,
                                    QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Generate a unique VM name based on template name
         * @param baseName Base name from template
         * @return Unique VM name
         */
        QString generateUniqueName(const QString& baseName);

        QSharedPointer<VM> m_template;
};

#endif // CREATEVMFASTACTION_H
