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

#ifndef IMPORTAPPLIANCEACTION_H
#define IMPORTAPPLIANCEACTION_H

#include "../../asyncoperation.h"
#include "../../../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QMap>

class XenConnection;
class OvfPackage;

/**
 * @brief Per-VIF network mapping entry: OVF network name → target XenServer network OpaqueRef
 */
struct OvfNetworkMapping
{
    QString ovfNetworkName;   ///< Network name as declared in the OVF NetworkSection
    QString targetNetworkRef; ///< OpaqueRef of the target XenServer Network
};

/**
 * @brief Per-disk storage mapping: OVF disk file href → target SR OpaqueRef
 */
struct OvfDiskMapping
{
    QString diskHref;    ///< File href from OVF References/File
    QString targetSrRef; ///< OpaqueRef of the target SR
};

/**
 * @brief Complete import configuration for one OVF VirtualSystem
 *
 * Passed per-VM from the wizard into ImportApplianceAction.
 * Maps to C# ImportApplianceAction's VmMapping class.
 */
struct OvfVmMapping
{
    QString virtualSystemId;         ///< OVF VirtualSystem ovf:id (matches envelope element)
    QString targetHostRef;           ///< Affinity host OpaqueRef (empty = no affinity)
    QString defaultSrRef;            ///< SR to use when no per-disk mapping matches
    QList<OvfDiskMapping> diskMappings;
    QList<OvfNetworkMapping> networkMappings;
};

/**
 * @brief Import an OVF/OVA appliance package.
 *
 * Ports the C# XenAdmin.Actions.OvfActions.ImportApplianceAction to Qt.
 *
 * Responsibilities:
 *  - Optional manifest/signature verification.
 *  - Per-VM OVF parsing using OvfPackage descriptor XML.
 *  - VM record creation via XenAPI::VM::create().
 *  - VDI creation and disk-content upload via HTTP PUT to /import_raw_vdi.
 *  - VBD attachment.
 *  - VIF creation using provided network mappings.
 *  - Optional VM_appliance (vApp) grouping.
 *  - Optional auto-start after import.
 *  - Partial cleanup on cancellation or failure.
 *
 * Threading: runs on the AsyncOperation worker thread; all XenAPI calls block
 * on the ConnectionWorker thread (never the UI thread).
 */
class XENLIB_EXPORT ImportApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct ImportApplianceAction
         *
         * @param connection       Active XenServer connection
         * @param ovfFilePath      Local path to .ovf, .ova, or .ova.gz file
         * @param vmMappings       Per-VM configuration (host, SR, disks, networks)
         * @param verifyManifest   Check .mf SHA hashes before import
         * @param verifySignature  Check .cert signature before import
         * @param runFixups        Run OS post-import fixup sequence
         * @param fixupIsoSrRef    SR holding the fixup ISO (empty → use pool default)
         * @param startAutomatically  Start VM(s) / vApp after successful import
         * @param parent           Qt parent object
         */
        explicit ImportApplianceAction(XenConnection* connection,
                                       const QString& ovfFilePath,
                                       const QList<OvfVmMapping>& vmMappings,
                                       bool verifyManifest,
                                       bool verifySignature,
                                       bool runFixups,
                                       const QString& fixupIsoSrRef,
                                       bool startAutomatically,
                                       QObject* parent = nullptr);
        ~ImportApplianceAction() override = default;

        // ── Result accessors (valid after completed signal) ──────────────────

        /// OpaqueRef of the imported VM (single-VM import) or empty
        QString ImportedVmRef() const { return this->importedVmRef_; }

        /// OpaqueRef of the created VM_appliance (multi-VM import) or empty
        QString ApplianceRef() const { return this->applianceRef_; }

        /// OpaqueRefs of all created VMs (single or multi)
        QStringList CreatedVmRefs() const { return this->createdVmRefs_; }

    protected:
        void run() override;

    private:
        // ── OVF parsing helpers ──────────────────────────────────────────────

        /// Parse the OVF XML descriptor and return per-VirtualSystem QDomElement maps
        QList<QVariantMap> parseVirtualSystems(const QString& xml) const;

        /// Build a VM create record from a VirtualSystem element's RASD items
        QVariantMap buildVmRecord(const QVariantMap& vsData, const OvfVmMapping& mapping) const;

        // ── XenAPI workflow helpers ──────────────────────────────────────────

        /**
         * @brief Create a VM record via XenAPI.
         * Hides the VM from XenCenter until fully configured (sets HideFromXenCenter).
         * @param vmRecord QVariantMap suitable for VM.create
         * @param applianceRef OpaqueRef of parent VM_appliance or empty
         * @return OpaqueRef of the created VM
         */
        QString createVm(const QVariantMap& vmRecord, const QString& applianceRef);

        /**
         * @brief Create a VDI and upload disk content via HTTP PUT /import_raw_vdi.
         * @param srRef          Target SR OpaqueRef
         * @param diskLabel      Human-readable disk name
         * @param diskFilePath   Local path of the disk file (.vhd or raw)
         * @param virtualSizeBytes Capacity to advertise when creating the VDI
         * @param progressStart  Lower bound of the overall percent range for this disk
         * @param progressEnd    Upper bound of the overall percent range for this disk
         * @return OpaqueRef of the created VDI
         */
        QString uploadDisk(const QString& srRef,
                           const QString& diskLabel,
                           const QString& diskFilePath,
                           qint64 virtualSizeBytes,
                           int progressStart,
                           int progressEnd);

        /**
         * @brief Attach a VDI to a VM as a VBD.
         * @param vmRef     Target VM OpaqueRef
         * @param vdiRef    VDI OpaqueRef to attach
         * @param bootable  Whether this disk is the boot disk
         * @param mode      "RW" or "RO"
         * @param type      "Disk" or "CD"
         */
        void attachDisk(const QString& vmRef,
                        const QString& vdiRef,
                        bool bootable,
                        const QString& mode,
                        const QString& type);

        /**
         * @brief Create a VIF connecting the VM to a network.
         * @param vmRef       Target VM OpaqueRef
         * @param networkRef  Target Network OpaqueRef
         * @param deviceIndex VIF device index (0, 1, 2, ...)
         * @param mac         MAC address or empty for auto-generate
         */
        void createVif(const QString& vmRef,
                       const QString& networkRef,
                       int deviceIndex,
                       const QString& mac);

        /**
         * @brief Destroy a partially-created VM and any owned VDIs/VBDs.
         * Called on failure/cancellation to clean up.
         * @param vmRef OpaqueRef of the VM to remove
         */
        void cleanupVm(const QString& vmRef);

        /**
         * @brief Throw a CancelledException if cancellation has been requested.
         */
        void checkCancelled() const;

        // ── Configuration ────────────────────────────────────────────────────
        QString ovfFilePath_;
        QList<OvfVmMapping> vmMappings_;
        bool verifyManifest_;
        bool verifySignature_;
        bool runFixups_;
        QString fixupIsoSrRef_;
        bool startAutomatically_;

        // ── Results ──────────────────────────────────────────────────────────
        QString importedVmRef_;
        QString applianceRef_;
        QStringList createdVmRefs_;
};

#endif // IMPORTAPPLIANCEACTION_H
