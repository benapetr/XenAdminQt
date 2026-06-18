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

#ifndef EXPORTAPPLIANCEACTION_H
#define EXPORTAPPLIANCEACTION_H

#include "../../asyncoperation.h"
#include "../../vm.h"
#include "../../../ovf/ovfwriter.h"
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

/**
 * @brief Export one or more VMs as an OVF appliance folder or OVA archive.
 *
 * Qt/C++ equivalent of C# XenAdmin.Actions.OvfActions.ExportApplianceAction.
 *
 * For each selected VM:
 *   1. Builds an OVF envelope (VM metadata, hardware, networks).
 *   2. Downloads each non-CD VDI as a VHD file via XenServer's
 *      @c /export_raw_vdi HTTP endpoint.
 *   3. Merges per-VM envelopes into a single appliance envelope.
 *   4. Writes the @c .ovf descriptor file.
 *   5. Optionally creates a manifest (.mf) and/or packs the folder into
 *      a single OVA archive (.ova).
 *
 * All long-running I/O runs on the action thread, not the UI thread.
 */
class ExportApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct ExportApplianceAction.
         *
         * @param vms              VMs to export (all must be Halted or Suspended).
         * @param applianceDir     Destination directory (e.g. @c /home/user/exports/).
         * @param applianceName    Package base name (e.g. @c "myappliance");
         *                         a sub-directory with this name is created inside
         *                         @p applianceDir, matching C# ExportApplianceAction.
         * @param eulas            EULA file paths to embed (may be empty).
         * @param signAppliance    Whether to sign the package with an X.509 certificate.
         * @param createManifest   Whether to create a SHA-256 manifest (.mf).
         * @param certPath         Path to PKCS#12 certificate file (only used when @p signAppliance is true).
         * @param certPassword     Password for the PKCS#12 file.
         * @param createOva        Whether to pack the folder into a single OVA file.
         * @param compressFiles    Whether to gzip-compress VHD files before writing the OVF.
         * @param shouldVerify     Whether to verify the package checksums after export.
         * @param parent           Parent QObject.
         */
        explicit ExportApplianceAction(const QList<QSharedPointer<VM>>& vms,
                                       const QString& applianceDir,
                                       const QString& applianceName,
                                       const QStringList& eulas,
                                       bool signAppliance,
                                       bool createManifest,
                                       const QString& certPath,
                                       const QString& certPassword,
                                       bool createOva,
                                       bool compressFiles,
                                       bool shouldVerify,
                                       QObject* parent = nullptr);

        ~ExportApplianceAction() override = default;

    protected:
        void run() override;

    private:
        /**
         * @brief Build the OvfVirtualSystemEntry for one VM.
         *
         * Downloads VHD files for each non-CD VBD and fills in all
         * metadata fields from the live cache.
         *
         * @param vm             VM to describe.
         * @param appFolder      Directory where VHD files will be written.
         * @param vmIndex        0-based index within the export list (for progress).
         * @param vmTotal        Total number of VMs (for progress).
         * @param[out] sys       Populated entry.
         * @return true on success, false on error or cancellation.
         */
        bool buildVirtualSystem(const QSharedPointer<VM>& vm,
                                const QString& appFolder,
                                int vmIndex,
                                int vmTotal,
                                OvfVirtualSystemEntry& sys);

        /**
         * @brief Download one VDI as a VHD file via /export_raw_vdi.
         *
         * @param vdiUuid    UUID of the VDI to export.
         * @param vdiSize    VDI virtual_size (bytes) for progress denominator.
         * @param vhdPath    Destination file path.
         * @param diskLabel  Human-readable label for progress messages.
         * @param vmTotal    Total VM count (for overall progress scaling).
         * @param vmIndex    Current VM index (for overall progress scaling).
         * @param vbdIndex   Current VBD index within this VM.
         * @param vbdTotal   Total VBD count for this VM.
         * @return true on success.
         */
        bool downloadVhd(const QString& vdiUuid,
                         qint64 vdiSize,
                         const QString& vhdPath,
                         const QString& diskLabel,
                         int vmTotal, int vmIndex,
                         int vbdIndex, int vbdTotal);

        /// Remove the appliance folder on error/cancel if we created it.
        void cleanupOnError(const QString& appFolder);

        QList<QSharedPointer<VM>> m_vms;
        QString m_applianceDir;
        QString m_applianceName;
        QStringList m_eulaFilePaths;
        bool m_signAppliance;
        bool m_createManifest;
        QString m_certPath;
        QString m_certPassword;
        bool m_createOva;
        bool m_compressFiles;
        bool m_shouldVerify;
        bool m_createdAppFolder = false; ///< true if run() created the appliance subfolder
};

#endif // EXPORTAPPLIANCEACTION_H
