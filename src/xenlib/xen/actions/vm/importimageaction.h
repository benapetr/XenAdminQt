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

#ifndef IMPORTIMAGEACTION_H
#define IMPORTIMAGEACTION_H

#include "importapplianceaction.h"
#include "../../../xenlib_global.h"
#include <QString>

class XenConnection;

/**
 * @brief Import a raw disk image (VHD or VMDK) as a new VM.
 *
 * Ports the C# XenAdmin.Actions.OvfActions.ImportImageAction to Qt.
 * Inherits from ImportApplianceAction to reuse uploadDisk / attachDisk / createVif helpers.
 *
 * Responsibilities:
 *  - Create a new VM record from user-supplied name, vCPU, and memory settings.
 *  - Upload the disk image via HTTP PUT to /import_raw_vdi.
 *  - Attach the uploaded VDI as the boot disk.
 *  - Create a VIF if a target network was selected.
 *  - Optionally start the VM after import.
 *  - Clean up on failure or cancellation.
 *
 * Threading: runs on the AsyncOperation worker thread.
 */
class XENLIB_EXPORT ImportImageAction : public ImportApplianceAction
{
    Q_OBJECT

    public:
        /// Boot firmware mode for the imported VM
        enum BootMode
        {
            BootMode_Bios,       ///< Legacy BIOS (HVM "BIOS order")
            BootMode_Uefi,       ///< UEFI firmware
            BootMode_UefiSecure  ///< UEFI Secure Boot
        };

        /**
         * @brief Construct ImportImageAction
         *
         * @param connection          Active XenServer connection
         * @param vmName              Name label for the new VM
         * @param vcpuCount           vCPU count (≥ 1)
         * @param memoryMb            Memory in MB (≥ 64)
         * @param srRef               Target SR OpaqueRef
         * @param networkRef          Target Network OpaqueRef (empty = no VIF)
         * @param hostRef             Affinity host OpaqueRef (empty = no affinity)
         * @param filePath            Local path to .vhd or .vmdk file
         * @param diskCapacityBytes   Virtual disk size in bytes (0 = use file size)
         * @param bootMode            Boot firmware mode for the VM
         * @param assignVtpm          Attach a virtual TPM module to the VM
         * @param startAutomatically  Start VM after successful import
         * @param parent              Qt parent object
         */
        explicit ImportImageAction(XenConnection* connection,
                                   const QString& vmName,
                                   int vcpuCount,
                                   qint64 memoryMb,
                                   const QString& srRef,
                                   const QString& networkRef,
                                   const QString& hostRef,
                                   const QString& filePath,
                                   qint64 diskCapacityBytes,
                                   BootMode bootMode,
                                   bool assignVtpm,
                                   bool startAutomatically,
                                   bool runFixups,
                                   const QString& fixupIsoSrRef,
                                   QObject* parent = nullptr);
        ~ImportImageAction() override = default;

        /// OpaqueRef of the imported VM (valid after completed signal)
        QString ImportedVmRef() const { return this->m_imageVmRef; }

    protected:
        void run() override;

    private:
        QString m_vmName;
        int     m_vcpuCount;
        qint64  m_memoryMb;
        QString m_srRef;
        QString m_networkRef;
        QString m_hostRef;
        qint64  m_diskCapacityBytes;
        BootMode m_bootMode;
        bool    m_assignVtpm;
        bool    m_startAutomatically;
        QString m_imageVmRef;
        bool    m_runFixups;
};

#endif // IMPORTIMAGEACTION_H
