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

#ifndef VMMOVEACTION_H
#define VMMOVEACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QMap>

class XenConnection;
class VM;
class SR;
class Host;

/**
 * @brief VMMoveAction moves a VM's virtual disks to different storage repositories.
 *
 * This action moves VDIs associated with a VM from their current SRs to new target SRs.
 * It copies each VDI to the new SR, creates new VBDs, and destroys the old VDIs.
 * Optionally sets a new suspend SR for the VM.
 * Matches C# XenModel/Actions/VM/VMMoveAction.cs
 */
class VMMoveAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for VM move action with explicit storage mapping
         * @param connection XenServer connection
         * @param vm VM to move
         * @param storageMapping Map of old VDI refs to target SRs
         * @param host Target host (can be nullptr)
         */
        VMMoveAction(XenConnection* connection,
                     VM* vm,
                     const QMap<QString, SR*>& storageMapping,
                     Host* host,
                     QObject* parent = nullptr);

        /**
         * @brief Constructor for VM move action with single SR target
         * @param connection XenServer connection
         * @param vm VM to move
         * @param sr Target SR for all VDIs
         * @param host Target host (can be nullptr)
         */
        VMMoveAction(XenConnection* connection,
                     VM* vm,
                     SR* sr,
                     Host* host,
                     QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        static QMap<QString, SR*> getStorageMapping(VM* vm, SR* sr);

        VM* m_vm;
        Host* m_host;
        SR* m_sr;
        QMap<QString, SR*> m_storageMapping;
};

#endif // VMMOVEACTION_H
