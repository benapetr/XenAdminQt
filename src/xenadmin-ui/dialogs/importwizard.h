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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QComboBox>
#include <QTextEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QProgressBar>

class QWizardPage;
class XenConnection;
class OvfPackage;

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
            Page_Finish = 5
        };

        explicit ImportWizard(QWidget* parent = nullptr);
        explicit ImportWizard(XenConnection* connection, QWidget* parent = nullptr);

        // Result accessors — valid after exec() returns Accepted
        QString GetSourceFilePath() const { return this->sourceFilePath_; }
        ImportType GetImportType() const { return this->importType_; }
        QString GetSelectedHostRef() const { return this->selectedHostRef_; }
        QString GetSelectedSRRef() const { return this->selectedSRRef_; }
        QString GetSelectedNetworkRef() const { return this->selectedNetworkRef_; }
        bool GetStartAutomatically() const { return this->startVMsAutomatically_; }
        bool GetRunFixups() const { return this->runFixups_; }
        QString GetFixupIsoSrRef() const { return this->fixupIsoSrRef_; }

    protected:
        void initializePage(int id) override;
        bool validateCurrentPage() override;
        void accept() override;

    private slots:
        void onCurrentIdChanged(int id);
        void onBrowseClicked();

    private:
        void setupWizardPages();
        void populateHostCombo();
        void populateStorageCombo();
        void populateNetworkCombo();
        void populateFixupIsoCombo();
        QString detectImportType(const QString& filePath);

        // Helper functions for creating pages
        QWizardPage* createSourcePage();
        QWizardPage* createHostPage();
        QWizardPage* createStoragePage();
        QWizardPage* createNetworkPage();
        QWizardPage* createOptionsPage();
        QWizardPage* createFinishPage();

        void updateOvfMetadataDisplay();
        bool inspectXvaTar(const QString& filePath);
        void updateXvaMetadataDisplay();

        // Connection context (may be null when no server is connected)
        XenConnection* connection_;

        // Collected wizard result data
        ImportType importType_;
        QString sourceFilePath_;
        QString selectedHostRef_;
        QString selectedSRRef_;
        QString selectedNetworkRef_;
        bool verifyManifest_;
        bool startVMsAutomatically_;
        bool runFixups_;
        QString fixupIsoSrRef_;

        // OVF metadata populated after source-page validation
        QString ovfPackageName_;
        QStringList ovfVirtualSystemNames_;
        QStringList ovfNetworkNames_;
        QStringList ovfEulas_;
        bool ovfHasManifest_;
        bool ovfHasSignature_;

        // XVA metadata populated after source-page validation
        QStringList xvaVmNames_;
        qint64 xvaTotalDiskSizeBytes_;
};

#endif // IMPORTWIZARD_H
