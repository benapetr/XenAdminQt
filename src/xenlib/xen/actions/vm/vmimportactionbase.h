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

#ifndef VMIMPORTACTIONBASE_H
#define VMIMPORTACTIONBASE_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"
#include <QString>
#include <QVariantMap>

class XenConnection;

/**
 * @brief Base class providing shared XenAPI helpers for VM/appliance import operations.
 *
 * Extracted from ImportApplianceAction so that both ImportApplianceAction and
 * ImportImageAction can use the same VM, VDI, VBD, VIF, and fixup helpers without
 * one depending on the other.
 *
 * Subclasses must implement run() and may override onCancel().
 * All helpers are protected; none are virtual.
 */
class XENLIB_EXPORT VmImportActionBase : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit VmImportActionBase(XenConnection* connection,
                                    const QString& title,
                                    const QString& description,
                                    QObject* parent = nullptr);

        ~VmImportActionBase() override = default;

    protected:
        // ── Exception type used by checkCancelled() ──────────────────────────
        // Defined here so both base and subclasses catch it by name.
        struct CancelledException {};

        /**
         * @brief Throw CancelledException if cancellation has been requested.
         * Subclasses should call this at safe points during run().
         */
        void checkCancelled() const;

        // ── XenAPI workflow helpers ──────────────────────────────────────────

        /**
         * @brief Create a VM record via XenAPI::VM::create.
         * @param vmRecord     QVariantMap suitable for VM.create
         * @param applianceRef OpaqueRef of parent VM_appliance or empty string
         * @return OpaqueRef of the created VM
         * @throws std::runtime_error on failure
         */
        QString createVm(const QVariantMap& vmRecord, const QString& applianceRef);

        /**
         * @brief Create a VDI and upload disk content via HTTP PUT /import_raw_vdi.
         * @param srRef             Target SR OpaqueRef
         * @param diskLabel         Human-readable disk name
         * @param diskFilePath      Local path of the disk file (.vhd or raw)
         * @param virtualSizeBytes  Capacity to advertise when creating the VDI (0 = use file size)
         * @param progressStart     Lower bound of the overall percent range for this disk
         * @param progressEnd       Upper bound of the overall percent range for this disk
         * @return OpaqueRef of the created VDI
         * @throws std::runtime_error on failure
         */
        QString uploadDisk(const QString& srRef,
                           const QString& diskLabel,
                           const QString& diskFilePath,
                           qint64 virtualSizeBytes,
                           int progressStart,
                           int progressEnd);

        /**
         * @brief Attach a VDI to a VM as a VBD.
         * @throws std::runtime_error on failure
         */
        void attachDisk(const QString& vmRef,
                        const QString& vdiRef,
                        bool bootable,
                        const QString& mode,
                        const QString& type);

        /**
         * @brief Create a VIF connecting the VM to a network.
         * @throws std::runtime_error on failure
         */
        void createVif(const QString& vmRef,
                       const QString& networkRef,
                       int deviceIndex,
                       const QString& mac);

        /**
         * @brief Destroy a partially-created VM and any owned VDIs/VBDs.
         * Non-throwing; logs warnings on failure.
         */
        void cleanupVm(const QString& vmRef);

        /**
         * @brief Attach the OS fixup ISO as a CDROM and adjust boot order to boot from it first.
         *
         * Searches @p fixupIsoSrRef for a VDI whose name contains "linuxfixup" or
         * "xenserver-linuxfixup".  If found, creates a read-only CD VBD and prepends
         * "d" to the VM's HVM_boot_params["order"].
         *
         * @param vmRef          OpaqueRef of the VM to configure
         * @param fixupIsoSrRef  SR to search for the fixup ISO
         * @return true if the fixup CDROM was attached successfully
         */
        bool applyFixups(const QString& vmRef, const QString& fixupIsoSrRef);

        //! Source file path used for VDI description strings (set by subclass)
        QString m_sourcePath;
};

#endif // VMIMPORTACTIONBASE_H
