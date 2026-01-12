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

#ifndef VMMIGRATEACTION_H
#define VMMIGRATEACTION_H

#include "../../asyncoperation.h"
#include <QString>

class VM;
class Host;

/**
 * @brief Action to migrate a VM to another host in the same pool
 *
 * Performs live migration of a running or suspended VM to a different
 * host within the same resource pool using VM.async_pool_migrate.
 *
 * Equivalent to C# XenAdmin VMMigrateAction.
 */
class VMMigrateAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct VM migration action
         * @param connection XenServer connection
         * @param vmRef VM opaque reference
         * @param destinationHostRef Destination host opaque reference
         * @param parent Parent QObject
         */
        explicit VMMigrateAction(QSharedPointer<VM> vm, QSharedPointer<Host> host, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QSharedPointer<VM> m_vm;
        QSharedPointer<Host> m_host;
};

#endif // VMMIGRATEACTION_H
