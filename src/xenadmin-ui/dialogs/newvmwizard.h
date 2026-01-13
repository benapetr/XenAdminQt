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

#ifndef NEWVMWIZARD_H
#define NEWVMWIZARD_H

#include <QWizard>
#include <QVector>
#include <QMap>
#include <QVariant>
//#include "xen/actions/vm/createvmaction.h"

class QWizardPage;
class XenConnection;
class XenCache;
class QTreeWidgetItem;
class WizardNavigationPane;

namespace Ui
{
    class NewVMWizard;
}

class NewVMWizard : public QWizard
{
    Q_OBJECT

    public:
        // Wizard page IDs
        enum Page
        {
            Page_Template = 0,
            Page_Name,
            Page_InstallationMedia,
            Page_HomeServer,
            Page_CpuMemory,
            Page_Storage,
            Page_Network,
            Page_Finish
        };

        explicit NewVMWizard(XenConnection* connection, QWidget* parent = nullptr);
        ~NewVMWizard() override;

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void accept() override;

    private slots:
        void onCurrentIdChanged(int id);
        void onVmNameChanged(const QString& text);
        void onAutoHomeServerToggled(bool checked);
        void onSpecificHomeServerToggled(bool checked);
        void onCopyBiosStringsToggled(bool checked);
        void onVcpusMaxChanged(int value);
        void onMemoryStaticMaxChanged(int value);
        void onMemoryDynamicMaxChanged(int value);
        void onIsoRadioToggled(bool checked);
        void onUrlRadioToggled(bool checked);
        void onDefaultSrChanged(int index);
        void onCoresPerSocketChanged(int index);
        void onDiskTableSelectionChanged();
        void onAddDiskClicked();
        void onEditDiskClicked();
        void onRemoveDiskClicked();
        void onDisklessToggled(bool checked);
        void onNetworkTableSelectionChanged();
        void onAddNetworkClicked();
        void onEditNetworkClicked();
        void onRemoveNetworkClicked();
        void onAttachIsoLibraryClicked();
        void onNetworkContextMenuRequested(const QPoint& pos);
        void onDiskContextMenuRequested(const QPoint& pos);

    private:
        XenCache* cache() const;
        void setupUiPages();
        void createVirtualMachine();
        void loadTemplates();
        void filterTemplates(const QString& filterText);
        void handleTemplateSelectionChanged();
        void loadTemplateDevices();
        void loadHosts();
        void loadStorageRepositories();
        void loadNetworks();
        void updateDiskTable();
        void updateNetworkTable();
        void updateHomeServerPage();
        void updateSummaryPage();
        void updateHomeServerControls(bool enableSelection);
        void updateIsoControls();
        void updateVcpuControls();
        void updateTopologyOptions(int vcpusMax);
        bool isValidVcpu(int vcpus) const;
        void enforceVcpuTopology();
        void applyDefaultSRToDisks(const QString& srRef);
        void updateNavigationSelection();

        struct TemplateInfo
        {
            QString ref;
            QString name;
            QString type;
            QString description;
            QTreeWidgetItem* item = nullptr;
        };

        struct HostInfo
        {
            QString ref;
            QString name;
            QString hostname;
        };

        struct StorageRepositoryInfo
        {
            QString ref;
            QString name;
            QString type;
        };

        struct NetworkInfo
        {
            QString ref;
            QString name;
        };

        // Store wizard data
        XenConnection* m_connection;
        Ui::NewVMWizard* ui;

        QString m_selectedTemplate;
        QString m_vmName;
        QString m_vmDescription;
        QString m_selectedHost;
        QString m_lastTemplateName;
        bool m_vmNameDirty = false;
        bool m_settingVmName = false;
        int m_vcpuCount = 1;
        int m_vcpuMax = 1;
        int m_coresPerSocket = 1;
        int m_originalVcpuAtStartup = 1;
        int m_originalCoresPerSocket = 1;
        bool m_supportsVcpuHotplug = false;
        int m_minVcpus = 1;
        int m_maxVcpusAllowed = 1;
        int m_maxCoresPerSocket = 1;
        long m_memorySize = 1024;
        int m_memoryDynamicMin = 0;
        int m_memoryDynamicMax = 0;
        int m_memoryStaticMax = 0;
        QVariantMap m_selectedTemplateRecord;

        bool m_assignVtpm = false;
        QString m_installUrl;
        QString m_selectedIso;
        QString m_bootMode;
        QString m_pvArgs;
        WizardNavigationPane* m_navigationPane = nullptr;

        // Storage configuration: list of (VDI ref, SR ref, size, device)
        // This is just a temporary disk descriptor that actual VDB / VDI
        // is created from later. C# works directly with VDIs, but this
        // approach seems easier. No need to create real VDI objects until
        // we are sure they will be provisioned.
        struct DiskConfig
        {
            QString vdiRef;   // Source VDI reference (for copying)
            QString srRef;    // Target SR
            qint64 sizeBytes; // Disk size in bytes
            QString device;   // Device name (e.g., "0", "1", etc.)
            bool bootable;    // Is this disk bootable?
            QString name;
            QString description;
            QString mode = "RW";
            QString vdiType = "user";
            bool sharable = false;
            bool readOnly = false;
            bool canDelete = true;
            bool canResize = true;
            qint64 minSizeBytes = 0;
        };
        QList<DiskConfig> m_disks;

        // Network configuration: list of (network ref, device, MAC)
        struct NetworkConfig
        {
            QString networkRef; // Network to connect to
            QString device;     // Device index (e.g., "0", "1", etc.)
            QString mac;        // MAC address (empty = auto-generate)
        };
        QList<NetworkConfig> m_networks;

        QVector<TemplateInfo> m_templateItems;
        QVector<HostInfo> m_hosts;
        QVector<StorageRepositoryInfo> m_storageRepositories;
        QVector<NetworkInfo> m_availableNetworks;
};

#endif // NEWVMWIZARD_H
