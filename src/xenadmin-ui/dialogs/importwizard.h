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

#ifndef IMPORTWIZARD_H
#define IMPORTWIZARD_H

#include <QWizard>
#include <QComboBox>
#include <QSharedPointer>

namespace Ui { class ImportWizard; }

class QWizardPage;
class XenConnection;
class OvfPackage;
class Host;
class SR;
class Network;
class VM;
class ImportVmAction;

class ImportWizard : public QWizard
{
    Q_OBJECT

    public:
        // Import types
        enum ImportType
        {
            ImportType_XVA, // XenServer native format (.xva)
            ImportType_OVF, // Open Virtualization Format (.ovf, .ova)
            ImportType_VHD  // Virtual Hard Disk (.vhd, .vmdk)
        };

        // Wizard page IDs
        enum Page
        {
            Page_Source = 0,
            Page_Host = 1,
            Page_Storage = 2,
            Page_Network = 3,
            Page_Options = 4,
            Page_Finish = 5,
            Page_VmConfig = 6,  // VHD/VMDK only: VM name, vCPU, memory
            Page_Eula = 7,      // OVF only: EULA acceptance (shown when EULAs are present)
            Page_Security = 8,  // OVF only: manifest/signature review
            Page_BootOptions = 9 // VHD/VMDK only: firmware (BIOS/UEFI) and vTPM
        };

        /// Boot firmware mode — mirrors ImportImageAction::BootMode
        enum BootMode
        {
            BootMode_Bios,       ///< Legacy BIOS (HVM "BIOS order")
            BootMode_Uefi,       ///< UEFI firmware
            BootMode_UefiSecure  ///< UEFI Secure Boot
        };

        explicit ImportWizard(QWidget* parent = nullptr);
        explicit ImportWizard(XenConnection* connection, QWidget* parent = nullptr);
        explicit ImportWizard(XenConnection* connection, const QString& initialFilePath, QWidget* parent = nullptr);
        ~ImportWizard();

        // Result accessors — valid after exec() returns Accepted
        QString GetSourceFilePath() const { return this->m_sourceFilePath; }
        ImportType GetImportType() const { return this->m_importType; }
        QSharedPointer<Host> GetSelectedHost() const { return this->m_selectedHost; }
        QSharedPointer<SR> GetSelectedSR() const { return this->m_selectedSR; }
        QSharedPointer<Network> GetSelectedNetwork() const { return this->m_selectedNetwork; }
        QMap<QString, QString> GetOvfNetworkMappings() const { return this->m_ovfNetworkMappings; }
        QString GetVmName() const { return this->m_vmName; }
        int GetVcpuCount() const { return this->m_vcpuCount; }
        qint64 GetMemoryMb() const { return this->m_memoryMb; }
        qint64 GetDiskImageCapacityBytes() const { return this->m_diskImageCapacityBytes; }
        bool GetStartAutomatically() const { return this->m_startVMsAutomatically; }
        bool GetRunFixups() const { return this->m_runFixups; }
        QString GetFixupIsoSrRef() const { return this->m_fixupIsoSrRef; }
        BootMode GetBootMode() const { return this->m_bootMode; }
        bool GetAssignVtpm() const { return this->m_assignVtpm; }
        bool GetVerifyManifest() const { return this->m_verifyManifest; }
        QStringList GetOvfVirtualSystemNames() const { return this->m_ovfVirtualSystemNames; }
        QStringList GetOvfNetworkNames() const { return this->m_ovfNetworkNames; }

        // OVF routing helpers — queried by anonymous-namespace page subclasses in nextId()
        bool HasOvfEulas() const { return !this->m_ovfEulas.isEmpty(); }
        bool OvfHasSecurity() const { return this->m_ovfHasManifest || this->m_ovfHasSignature; }

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        int nextId() const override;
        void accept() override;
        void reject() override;

    private slots:
        void onCurrentIdChanged(int id);
        void onBrowseClicked();
        void onRescanStorageClicked();

    private:
        void setupWizardPages();
        void populateHostCombo();
        void populateStorageCombo();
        void populateNetworkCombo();
        void populateFixupIsoCombo();
        QString detectImportType(const QString& filePath);

        void updateOvfMetadataDisplay();
        bool inspectXvaTar(const QString& filePath);
        void updateXvaMetadataDisplay();
        bool inspectDiskImage(const QString& filePath);
        void updateDiskImageDisplay();
        void populateNetworkComboBox(QComboBox* combo);

        // Connection context (may be null when no server is connected)
        XenConnection* m_connection;

        // Collected wizard result data
        ImportType m_importType;
        QString m_sourceFilePath;
        QSharedPointer<Host> m_selectedHost;
        QSharedPointer<SR> m_selectedSR;
        QSharedPointer<Network> m_selectedNetwork;
        bool m_verifyManifest;
        bool m_startVMsAutomatically;
        bool m_runFixups;
        QString m_fixupIsoSrRef;

        // OVF metadata populated after source-page validation
        QString m_ovfPackageName;
        QStringList m_ovfVirtualSystemNames;
        QStringList m_ovfNetworkNames;
        QStringList m_ovfEulas;
        bool m_ovfHasManifest;
        bool m_ovfHasSignature;
        bool m_ovfAllowsNetworkSriov; ///< false when OVF recommendations prohibit SR-IOV

        // XVA metadata populated after source-page validation
        QStringList m_xvaVmNames;
        qint64 m_xvaTotalDiskSizeBytes;

        // VHD/VMDK metadata populated after source-page validation
        qint64 m_diskImageCapacityBytes;
        QString m_diskImageFormatName;

        // OVF per-network mappings: OVF network name → target XenServer network OpaqueRef
        QMap<QString, QString> m_ovfNetworkMappings;

        // VHD/VMDK VM config (collected from Page_VmConfig)
        QString m_vmName;
        int     m_vcpuCount;
        qint64  m_memoryMb;

        // VHD/VMDK boot options (collected from Page_BootOptions)
        BootMode m_bootMode;
        bool     m_assignVtpm;

        // XVA upload action — created on storage-page leave and owned by the wizard.
        // Matches C# StoragePickerPage.ImportXvaAction lifecycle.
        ImportVmAction*        m_xvaAction;
        QSharedPointer<VM>     m_xvaImportedVm;

        Ui::ImportWizard* ui;
};

#endif // IMPORTWIZARD_H
