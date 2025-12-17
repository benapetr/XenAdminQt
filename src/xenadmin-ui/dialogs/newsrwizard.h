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
#include <QVariantMap>

class MainWindow;
class WizardNavigationPane;

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
        };

        explicit NewSRWizard(MainWindow* parent = nullptr);
        ~NewSRWizard() override;

        enum PageIds
        {
            Page_Type,
            Page_NameDescription,
            Page_Configuration,
            Page_Summary
        };

        SRType getSelectedSRType() const
        {
            return m_selectedSRType;
        }
        QString getSRName() const
        {
            return m_srName;
        }
        QString getSRDescription() const
        {
            return m_srDescription;
        }

        MainWindow* mainWindow() const
        {
            return m_mainWindow;
        }

        QString getServer() const
        {
            return m_server;
        }
        QString getServerPath() const
        {
            return m_serverPath;
        }
        QString getUsername() const
        {
            return m_username;
        }
        QString getPassword() const
        {
            return m_password;
        }
        int getPort() const
        {
            return m_port;
        }
        QString getLocalPath() const
        {
            return m_localPath;
        }
        QString getLocalFilesystem() const
        {
            return m_localFilesystem;
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
        QList<FibreChannelDevice> getSelectedFibreDevices() const;

        QString getSRTypeString() const;
        QString getContentType() const;
        QVariantMap getDeviceConfig() const;
        QVariantMap getSMConfig() const;
        QString formatSRTypeString(SRType srType) const;

        MainWindow* m_mainWindow;
        Ui::NewSRWizard* ui;
        WizardNavigationPane* m_navigationPane;
        QButtonGroup* m_typeButtonGroup;

        SRType m_selectedSRType;
        QString m_srName;
        QString m_srDescription;

        QString m_server;
        QString m_serverPath;
        QString m_username;
        QString m_password;
        int m_port;

        QString m_localPath;
        QString m_localFilesystem;

        QString m_iscsiTarget;
        QString m_iscsiTargetIQN;
        QString m_iscsiLUN;
        bool m_iscsiUseChap;
        QString m_iscsiChapUsername;
        QString m_iscsiChapPassword;

        QString m_selectedSRUuid;

        QList<ISCSIIqnInfo> m_discoveredIqns;
        QList<ISCSILunInfo> m_discoveredLuns;
        QList<FibreChannelDevice> m_discoveredFibreDevices;
        QMap<QString, QString> m_foundSRs;
};

#endif // NEWSRWIZARD_H
