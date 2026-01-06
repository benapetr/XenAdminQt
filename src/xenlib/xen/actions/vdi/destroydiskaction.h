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

#ifndef DESTROYDISKACTION_H
#define DESTROYDISKACTION_H

#include "../../asyncoperation.h"

class VDI;

/**
 * @brief DestroyDiskAction - Delete a virtual disk image (VDI)
 *
 * Qt equivalent of C# DestroyDiskAction. Detaches VDI from all VMs and
 * then destroys the VDI.
 *
 * From C# XenModel/Actions/VDI/DestroyDiskAction.cs:
 * - Checks if VDI is attached to any running VMs (throws error unless allowed)
 * - Detaches VDI from all VMs (using DetachVirtualDiskAction)
 * - Destroys the VDI (async destroy)
 * - Polls to completion
 */
class XENLIB_EXPORT DestroyDiskAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Destroy a virtual disk
         * @param vdiRef VDI opaque reference
         * @param connection XenServer connection
         * @param allowRunningVMDelete Allow deletion even if attached to running VM
         * @param parent Parent object
         */
        explicit DestroyDiskAction(const QString& vdiRef,
                                   XenConnection* connection,
                                   bool allowRunningVMDelete = false,
                                   QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vdiRef;
        bool m_allowRunningVMDelete;
};

#endif // DESTROYDISKACTION_H
