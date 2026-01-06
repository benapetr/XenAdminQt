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

#ifndef DETACHVIRTUALDISKACTION_H
#define DETACHVIRTUALDISKACTION_H

#include "../../asyncoperation.h"

class VDI;
class VBD;
class VM;

/**
 * @brief DetachVirtualDiskAction - Detach a VDI from a VM
 *
 * Qt equivalent of C# DetachVirtualDiskAction. Unplugs and destroys the VBD
 * connecting a VDI to a VM.
 *
 * From C# XenModel/Actions/VDI/DetachVirtualDiskAction.cs:
 * - Unplugs VBD if currently attached (async unplug)
 * - Destroys VBD (async destroy)
 * - Polls both operations to completion
 */
class XENLIB_EXPORT DetachVirtualDiskAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Detach a virtual disk from a VM
         * @param vdiRef VDI opaque reference
         * @param vm VM to detach from
         * @param parent Parent object
         */
        explicit DetachVirtualDiskAction(const QString& vdiRef,
                                         VM* vm,
                                         QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_vdiRef;
        QString m_vbdRef;
        VM* m_vm;
};

#endif // DETACHVIRTUALDISKACTION_H
