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

#ifndef NEWSRWIZARD_H
#define NEWSRWIZARD_H

#include <QWizard>
#include <QButtonGroup>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QVariantMap>

class MainWindow;
class WizardNavigationPane;
class XenConnection;
class SR;
class Host;
class AsyncOperation;
class QGroupBox;
class QRadioButton;

namespace Ui
{
    class NewSRWizard;
}

// Storage Repository Types
enum class SRType
{
    NFS,
    iSCSI,
    LocalStorage,
    CIFS,
    HBA,
    FCoE,
    NFS_ISO,
    CIFS_ISO
};

class NewSRWizard : public QWizard
{
    Q_OBJECT

    public:
        struct ISCSIIqnInfo
        {
            QString targetIQN;
            QString ipAddress;
            quint16 port;
            int index;
        };

        struct ISCSILunInfo
        {
            int lunId;
            QString scsiId;
            QString vendor;
            QString serial;
            qint64 size;
        };

        struct FibreChannelDevice
        {
            QString scsiId;
            QString vendor;
            QString serial;
            QString path;
            qint64 size;
            QString adapter;
            QString channel;
            QString id;
            QString lun;
            QString nameLabel;
            QString nameDescription;
            QString eth;
            bool poolMetadataDetected;
            QString existingSrUuid;
            QVariantMap existingSrConfiguration;
        };

        explicit NewSRWizard(XenConnection* connection, MainWindow* parent = nullptr);
        explicit NewSRWizard(XenConnection* connection, const QSharedPointer<SR>& srToReattach, MainWindow* parent = nullptr);
        ~NewSRWizard() override;

        enum PageIds
        {
            Page_Type,
            Page_NameDescription,
            Page_Configuration,
            Page_Summary
        };

        SRType GetSelectedSRType() const
        {
            return this->m_selectedSRType;
        }
        QString GetSRName() const
        {
            return this->m_srName;
        }
        QString GetSRDescription() const
        {
            return this->m_srDescription;
        }

        void SetInitialSrType(SRType srType, bool lockTypes);

        MainWindow* GetMainWindow() const
        {
            return this->m_mainWindow;
        }

        QString GetServer() const
        {
            return this->m_server;
        }
        QString GetServerPath() const
        {
            return this->m_serverPath;
        }
        QString GetUsername() const
        {
            return this->m_username;
        }
        QString GetPassword() const
        {
            return this->m_password;
        }
        int GetPort() const
        {
            return this->m_port;
        }
        QString GetLocalPath() const
        {
            return this->m_localPath;
        }
        QString GetLocalFilesystem() const
        {
            return this->m_localFilesystem;
        }

    public slots:
        void accept() override;

    protected:
        bool validateCurrentPage() override;

    private slots:
        void onSRTypeChanged();
        void onNameTextChanged();
        void onConfigurationChanged();
        void onTestConnection();
        void onBrowseLocalPath();
        void onCreateNewSRToggled(bool checked);
        void onExistingSRSelected();
        void onScanISCSITarget();
        void onISCSIIqnSelected(int index);
        void onISCSILunSelected(int index);
        void onScanFibreDevices();
        void onFibreDeviceSelectionChanged();
        void onSelectAllFibreDevices();
        void onClearAllFibreDevices();
        void onChapToggled(bool checked);
        void onPageChanged(int pageId);

    private:
        enum class ActionMode
        {
            Create,
            Introduce,
            Reattach
        };

        struct PlannedAction
        {
            ActionMode mode = ActionMode::Create;
            QSharedPointer<Host> coordinatorHost;
            QSharedPointer<SR> srToReattach;
            QString srUuid;
            QString srName;
            QString srDescription;
            QString srType;
            QString contentType;
            QVariantMap deviceConfig;
            QVariantMap smConfig;
        };

        enum class ExistingSrDecision
        {
            Cancel,
            Reattach,
            Format
        };

        void setupPages();
        void setupNavigationPane();
        void initializeTypePage();
        void initializeNamePage();
        void initializeConfigurationPage();
        void initializeSummaryPage();
        void updateConfigurationSection();
        void updateNavigationSelection();
        void generateDefaultName();
        void updateSummary();
        bool validateTypePage() const;
        bool validateNamePage() const;
        bool validateConfigurationPage() const;
        bool validateNetworkConfig() const;
        bool validateLocalConfig() const;
        bool validateISCSIConfig() const;
        bool validateFibreConfig() const;
        void collectNameAndDescription();
        void collectConfiguration();
        void resetISCSIState();
        void resetFibreState();
        void updateNetworkReattachUI(bool enabled);
        void hideAllConfigurations();
        void applyReattachDefaults(const QSharedPointer<SR>& srToReattach);
        void setSrTypeSelection(SRType srType, bool lockTypes);
        QList<FibreChannelDevice> getSelectedFibreDevices() const;
        bool runProbeExtWithProgress(const QString& title, const QString& masterRef, const QVariantMap& deviceConfig, const QString& srType, QVariantList& probeResult, QString& errorMessage);
        QList<PlannedAction> buildPlannedActions(const QSharedPointer<Host>& coordinatorHost, QString& error) const;
        AsyncOperation* createActionFromPlan(const PlannedAction& plan) const;
        QString getExistingSRRefByUuid(const QString& srUuid) const;
        bool shouldUseIntroduce(const QString& srUuid) const;
        QString getLocalSrTypeString() const;
        QString getSelectedBlockSrType() const;
        QString getAlternativeBlockSrType(const QString& srType) const;
        bool isSrUuidInAnyConnectedPool(const QString& srUuid, XenConnection** outConnection = nullptr, QString* outName = nullptr) const;
        ExistingSrDecision askExistingSrDecision(const QString& title, const QString& details, bool foundExisting, bool allowFormat, bool showRepeatCheckbox, bool& repeatForRemaining) const;
        bool evaluateIscsiProbeDecision();
        bool evaluateFibreProbeDecision();
        QList<QVariantMap> probeForExistingSrs(const QVariantMap& deviceConfig, QString& usedSrType, QString& error) const;
        static QVariantMap normalizeProbeConfig(const QVariantMap& config);
        void clearPlannedProbeSelections();

        QString getSRTypeString() const;
        QString getContentType() const;
        QVariantMap getDeviceConfig() const;
        QVariantMap getSMConfig() const;
        QString formatSRTypeString(SRType srType) const;

        MainWindow* m_mainWindow;
        XenConnection* m_connection;
        Ui::NewSRWizard* ui;
        WizardNavigationPane* m_navigationPane = nullptr;
        QButtonGroup* m_typeButtonGroup = nullptr;

        SRType m_selectedSRType = SRType::NFS;
        QString m_srName;
        QString m_srDescription;

        QString m_server;
        QString m_serverPath;
        QString m_username;
        QString m_password;
        int m_port = 2049;

        QString m_localPath;
        QString m_localFilesystem;

        QString m_iscsiTarget;
        QString m_iscsiTargetIQN;
        QString m_iscsiLUN;
        bool m_iscsiUseChap = false;
        QString m_iscsiChapUsername;
        QString m_iscsiChapPassword;

        QString m_selectedSRUuid;

        QList<ISCSIIqnInfo> m_discoveredIqns;
        QList<ISCSILunInfo> m_discoveredLuns;
        QList<FibreChannelDevice> m_discoveredFibreDevices;
        QMap<QString, QString> m_foundSRs;
        QVariantMap m_iscsiProbeSelectedConfig;
        QList<FibreChannelDevice> m_plannedFibreDevices;
        bool m_hasPlannedFibreDevices = false;
        bool m_hasEvaluatedProbeDecisions = false;

        QString m_reattachSrRef;
        bool m_forceReattach = false;
        QSharedPointer<SR> m_srToReattach;

        QGroupBox* m_provisioningGroup = nullptr;
        QRadioButton* m_standardProvisioningRadio = nullptr;
        QRadioButton* m_gfs2ProvisioningRadio = nullptr;
};

#endif // NEWSRWIZARD_H
