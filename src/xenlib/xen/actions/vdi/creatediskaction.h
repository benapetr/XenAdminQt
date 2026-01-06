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

#ifndef CREATEDISKACTION_H
#define CREATEDISKACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>

class VDI;
class VBD;
class VM;

/**
 * @brief CreateDiskAction - Create a new virtual disk image (VDI)
 *
 * Qt equivalent of C# CreateDiskAction. Creates a new VDI and optionally
 * attaches it to a VM via a VBD.
 *
 * From C# XenModel/Actions/VDI/CreateDiskAction.cs:
 * - Creates VDI using VDI.create()
 * - If VM specified, also creates VBD and attaches to VM
 * - Checks for existing bootable disks
 * - Only makes new disk bootable if userdevice=0 and no other bootable disk exists
 */
class XENLIB_EXPORT CreateDiskAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Create a new VDI without attaching to a VM
         * @param vdiRecord VDI record with properties (name_label, virtual_size, SR, etc.)
         * @param connection XenServer connection
         * @param parent Parent object
         */
        explicit CreateDiskAction(const QVariantMap& vdiRecord,
                                  XenConnection* connection,
                                  QObject* parent = nullptr);

        /**
         * @brief Create a new VDI and attach to a VM
         * @param vdiRecord VDI record with properties
         * @param vbdRecord VBD record with properties (device, mode, type, etc.)
         * @param vm Parent VM to attach disk to
         * @param parent Parent object
         */
        explicit CreateDiskAction(const QVariantMap& vdiRecord,
                                  const QVariantMap& vbdRecord,
                                  VM* vm,
                                  QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Check if VM already has a bootable disk
         * @return true if VM has at least one bootable VBD
         */
        bool hasBootableDisk();

        QVariantMap m_vdiRecord;
        QVariantMap m_vbdRecord;
        VM* m_vm;
        bool m_attachToVM;
};

#endif // CREATEDISKACTION_H
