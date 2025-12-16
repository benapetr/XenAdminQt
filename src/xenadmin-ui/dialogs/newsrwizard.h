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
#include <QWizardPage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QFrame>
#include <QPushButton>
#include <QDebug>

class MainWindow;

// Storage Repository Types
enum class SRType
{
    NFS,          // Network File System
    iSCSI,        // Internet Small Computer Systems Interface
    LocalStorage, // Local disk storage (ext, lvm, xfs)
    CIFS,         // Common Internet File System
    HBA,          // Host Bus Adapter (Fibre Channel)
    FCoE,         // Fibre Channel over Ethernet
    NFS_ISO,      // NFS ISO Library
    CIFS_ISO      // CIFS ISO Library
};

// Forward declarations for wizard pages
class SRTypeSelectionPage;
class SRNameDescriptionPage;
class SRConfigurationPage;
class SRSummaryPage;

class NewSRWizard : public QWizard
{
    Q_OBJECT

public:
    explicit NewSRWizard(MainWindow* parent = nullptr);

    // Page IDs
    enum
    {
        Page_Type,
        Page_NameDescription,
        Page_Configuration,
        Page_Summary
    };

    // Getters for wizard data
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

    // Configuration getters (varies by SR type)
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

    // Local storage specific
    QString getLocalPath() const
    {
        return m_localPath;
    }
    QString getLocalFilesystem() const
    {
        return m_localFilesystem;
    }

public slots:
    void accept() override; // Override to perform SR creation

private slots:
    void onSRTypeChanged(SRType srType);
    bool validateCurrentPage();
    void onPageChanged(int pageId);

private:
    void setupPages();
    void updateConfigurationPage();

    // Helper methods for SR creation
    QString getSRTypeString() const;
    QString getContentType() const;
    QVariantMap getDeviceConfig() const;
    QVariantMap getSMConfig() const;

    MainWindow* m_mainWindow;

    // Wizard data
    SRType m_selectedSRType;
    QString m_srName;
    QString m_srDescription;

    // Network storage configuration
    QString m_server;
    QString m_serverPath;
    QString m_username;
    QString m_password;
    int m_port;

    // Local storage configuration
    QString m_localPath;
    QString m_localFilesystem;

    // iSCSI configuration
    QString m_iscsiTarget;
    QString m_iscsiTargetIQN;
    QString m_iscsiLUN;
    bool m_iscsiUseChap;
    QString m_iscsiChapUsername;
    QString m_iscsiChapPassword;

    // SR Reattach support
    QString m_selectedSRUuid; // UUID of existing SR to reattach (empty = create new)

    // Page references
    SRTypeSelectionPage* m_typePage;
    SRNameDescriptionPage* m_namePage;
    SRConfigurationPage* m_configPage;
    SRSummaryPage* m_summaryPage;
};

// Page 1: Storage Repository Type Selection
class SRTypeSelectionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit SRTypeSelectionPage(QWidget* parent = nullptr);

    bool isComplete() const override;
    int nextId() const override;

    SRType getSelectedType() const;

signals:
    void srTypeChanged(SRType srType);

private slots:
    void onTypeSelectionChanged();

private:
    void setupUI();
    void setupSRTypeOptions();

    QVBoxLayout* m_layout;
    QButtonGroup* m_typeButtonGroup;

    // Radio buttons for each SR type
    QRadioButton* m_nfsRadio;
    QRadioButton* m_iscsiRadio;
    QRadioButton* m_localStorageRadio;
    QRadioButton* m_cifsRadio;
    QRadioButton* m_hbaRadio;
    QRadioButton* m_fcoeRadio;
    QRadioButton* m_nfsIsoRadio;
    QRadioButton* m_cifsIsoRadio;

    // Description labels
    QLabel* m_typeDescriptionLabel;
    QTextEdit* m_typeDescriptionText;
};

// Page 2: Name and Description
class SRNameDescriptionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit SRNameDescriptionPage(QWidget* parent = nullptr);

    bool isComplete() const override;
    void initializePage() override;

    QString getSRName() const;
    QString getSRDescription() const;

private:
    void setupUI();
    void generateDefaultName();

    QFormLayout* m_layout;
    QLineEdit* m_nameLineEdit;
    QTextEdit* m_descriptionTextEdit;

    SRType m_currentSRType;
};

// Page 3: Configuration (content varies by SR type)
class SRConfigurationPage : public QWizardPage
{
    Q_OBJECT

public:
    // FibreChannel device info (HBA/FCoE) - must be public for getter return type
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
        QString eth; // For FCoE
        bool poolMetadataDetected;
    };

    explicit SRConfigurationPage(QWidget* parent = nullptr);

    bool isComplete() const override;
    void initializePage() override;

    void updateForSRType(SRType srType);

    // Network storage getters
    QString getServer() const;
    QString getServerPath() const;
    QString getUsername() const;
    QString getPassword() const;
    int getPort() const;

    // Local storage getters
    QString getLocalPath() const;
    QString getLocalFilesystem() const;

    // iSCSI getters
    QString getISCSITarget() const;
    QString getISCSITargetIQN() const;
    QString getISCSILUN() const;
    bool getISCSIUseChap() const;
    QString getISCSIChapUsername() const;
    QString getISCSIChapPassword() const;

    // SR Reattach getters
    QString getSelectedSRUuid() const;
    bool isCreatingNewSR() const;

    // HBA/FCoE getters
    QList<FibreChannelDevice> getSelectedFibreDevices() const;

private slots:
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

private:
    void setupUI();
    void setupNetworkStorageConfig();
    void setupLocalStorageConfig();
    void setupISCSIConfig();
    void setupFibreConfig();
    void hideAllConfigurations();
    bool validateNetworkConfig() const;
    bool validateLocalConfig() const;
    bool validateISCSIConfig() const;
    bool validateFibreConfig() const;

    QVBoxLayout* m_mainLayout;

    // Network storage configuration (NFS, CIFS)
    QGroupBox* m_networkConfigGroup;
    QFormLayout* m_networkLayout;
    QLineEdit* m_serverLineEdit;
    QLineEdit* m_serverPathLineEdit;
    QLineEdit* m_usernameLineEdit;
    QLineEdit* m_passwordLineEdit;
    QSpinBox* m_portSpinBox;
    QPushButton* m_testConnectionButton;
    QLabel* m_connectionStatusLabel;

    // iSCSI specific configuration
    QGroupBox* m_iscsiConfigGroup;
    QFormLayout* m_iscsiLayout;
    QLineEdit* m_iscsiTargetLineEdit;
    QPushButton* m_scanISCSIButton;
    QComboBox* m_iscsiIqnComboBox;
    QLabel* m_iscsiIqnLabel;
    QComboBox* m_iscsiLunComboBox;
    QLabel* m_iscsiLunLabel;
    QCheckBox* m_iscsiChapCheckBox;
    QLineEdit* m_iscsiChapUsernameLineEdit;
    QLineEdit* m_iscsiChapPasswordLineEdit;

    // iSCSI discovery data
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
    QList<ISCSIIqnInfo> m_discoveredIqns;
    QList<ISCSILunInfo> m_discoveredLuns;

    // List of discovered Fibre Channel devices
    QList<FibreChannelDevice> m_discoveredFibreDevices;

    // Local storage configuration
    QGroupBox* m_localConfigGroup;
    QFormLayout* m_localLayout;
    QLineEdit* m_localPathLineEdit;
    QPushButton* m_browseButton;
    QComboBox* m_filesystemComboBox;
    QLabel* m_diskSpaceLabel;

    // HBA/FCoE configuration
    QGroupBox* m_fibreConfigGroup;
    QVBoxLayout* m_fibreLayout;
    QListWidget* m_fibreDevicesList;
    QPushButton* m_scanFibreButton;
    QPushButton* m_selectAllFibreButton;
    QPushButton* m_clearAllFibreButton;
    QLabel* m_fibreStatusLabel;

    // SR Reattach UI
    QRadioButton* m_createNewSRRadio;
    QRadioButton* m_reattachExistingSRRadio;
    QListWidget* m_existingSRsList;
    QLabel* m_existingSRsLabel;

    SRType m_currentSRType;

    // Store found SRs from probe (UUID -> SR name)
    QMap<QString, QString> m_foundSRs;
};

// Page 4: Summary and Confirmation
class SRSummaryPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit SRSummaryPage(QWidget* parent = nullptr);

    void initializePage() override;
    bool isComplete() const override;

private:
    void setupUI();
    void updateSummary();
    QString formatSRTypeString(SRType srType);

    QVBoxLayout* m_layout;
    QLabel* m_summaryTitleLabel;
    QTextEdit* m_summaryTextEdit;
    QProgressBar* m_creationProgressBar;
    QLabel* m_creationStatusLabel;

    bool m_creationStarted;
};

#endif // NEWSRWIZARD_H