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

#ifndef CROSSPOOLMIGRATEWIZARD_H
#define CROSSPOOLMIGRATEWIZARD_H

#include <QWizard>
#include <QSharedPointer>
#include <QMap>
#include <QList>
#include <QVector>
#include "xen/mappings/vmmapping.h"

class MainWindow;
class VM;
class XenConnection;
class QComboBox;
class SrPicker;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QWizardPage;
class QLabel;
class QCheckBox;
class WizardNavigationPane;
namespace Ui { class CrossPoolMigrateWizard; }

/**
 * @brief Cross pool migration wizard
 *
 * Qt equivalent of the XenAdmin CrossPoolMigrateWizard (simplified UI).
 */
class CrossPoolMigrateWizard : public QWizard
{
    Q_OBJECT

    public:
        enum class WizardMode
        {
            Migrate,
            Move,
            Copy
        };

        explicit CrossPoolMigrateWizard(MainWindow* mainWindow, const QSharedPointer<VM>& vm, WizardMode mode, bool resumeAfterMigrate = false, QWidget* parent = nullptr);

        explicit CrossPoolMigrateWizard(MainWindow* mainWindow, const QList<QSharedPointer<VM>>& vms, WizardMode mode, bool resumeAfterMigrate = false, QWidget* parent = nullptr);
        ~CrossPoolMigrateWizard() override;

        bool requiresRbacWarning() const;
        bool isIntraPoolCopySelected() const;
        bool shouldShowNetworkPage() const;
        bool shouldShowTransferNetworkPage() const;

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void accept() override;

    public:
        enum PageId
        {
            Page_Destination = 0,
            Page_Storage = 1,
            Page_Network = 2,
            Page_TransferNetwork = 3,
            Page_RbacWarning = 4,
            Page_Finish = 5,
            Page_CopyMode = 6,
            Page_IntraPoolCopy = 7
        };

    private:
        Ui::CrossPoolMigrateWizard* ui;
        MainWindow* m_mainWindow;
        QList<QSharedPointer<VM>> m_vms;
        XenConnection* m_sourceConnection = nullptr;
        XenConnection* m_targetConnection = nullptr;
        WizardMode m_mode;
        bool m_requiresRbacWarning = false;
        bool m_resumeAfterMigrate = false;

        QString m_targetHostRef;
        QString m_targetPoolRef;
        QString m_transferNetworkRef;

        QWizardPage* m_rbacPage = nullptr;
        QLabel* m_rbacInfoLabel = nullptr;
        QCheckBox* m_rbacConfirm = nullptr;
        QWizardPage* m_copyModePage = nullptr;
        QWizardPage* m_intraPoolCopyPage = nullptr;
        QWizardPage* m_destinationPage = nullptr;
        QWizardPage* m_storagePage = nullptr;
        QWizardPage* m_networkPage = nullptr;
        QWizardPage* m_transferPage = nullptr;
        QWizardPage* m_finishPage = nullptr;
        QComboBox* m_poolCombo = nullptr;
        QComboBox* m_hostCombo = nullptr;
        QTableWidget* m_storageTable = nullptr;
        QTableWidget* m_networkTable = nullptr;
        QComboBox* m_transferNetworkCombo = nullptr;
        QTextEdit* m_summaryText = nullptr;
        QMap<QString, class VmMapping> m_vmMappings;
        WizardNavigationPane* m_navigationPane = nullptr;
        QVector<int> m_navigationSteps;

        void setupWizardPages();
        void setupNavigationPane();
        void updateNavigationPane();
        void updateNavigationSelection();
        void updateWizardPages();
        void updateNextButtonVisibility();
        QWizardPage* createDestinationPage();
        QWizardPage* createStoragePage();
        QWizardPage* createNetworkPage();
        QWizardPage* createTransferNetworkPage();
        QWizardPage* createRbacWarningPage();
        QWizardPage* createFinishPage();
        QWizardPage* createCopyModePage();
        QWizardPage* createIntraPoolCopyPage();

        void populateDestinationHosts();
        void populateDestinationPools();
        void populateHostsForPool(const QString& poolRef, XenConnection* connection, const QString& standaloneHostRef);
        void updateDestinationMapping();
        void updateStorageMapping();
        void updateNetworkMapping();
        void populateStorageMappings();
        void populateNetworkMappings();
        void populateTransferNetworks();
        void updateSummary();
        void updateRbacRequirement();
        QStringList requiredRbacMethods() const;
        bool hasRbacPermissions(XenConnection* connection, const QStringList& methods) const;
        bool allVMsAvailable() const;
        bool isIntraPoolMigration() const;
        bool isIntraPoolMove() const;
        bool requiresTransferNetwork() const;
        bool isCopyCloneSelected() const;
        QString copyName() const;
        QString copyDescription() const;
        QString copyTargetSrRef() const;
        void ensureMappingForVm(const QSharedPointer<VM>& vm);
        bool canMigrateVmToHost(const QSharedPointer<VM>& vm,
                                XenConnection* targetConnection,
                                const QString& hostRef,
                                QString* reason) const;
        bool canDoStorageMigration(const QSharedPointer<VM>& vm,
                                   XenConnection* targetConnection,
                                   const QString& hostRef,
                                   QString* reason) const;
        bool snapshotsContainExtraVifs(const QSharedPointer<VM>& vm) const;
        QStringList collectSnapshotVifRefs(const QSharedPointer<VM>& vm) const;

        XenConnection* resolveTargetConnection(const QString& hostRef) const;
};

#endif // CROSSPOOLMIGRATEWIZARD_H
