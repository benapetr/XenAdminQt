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

#ifndef EXPORTWIZARD_H
#define EXPORTWIZARD_H

#include <QWizard>
#include <QSharedPointer>
#include <QList>
#include <QListWidget>

namespace Ui { class ExportWizard; }

class VM;
class XenConnection;

class ExportWizard : public QWizard
{
    Q_OBJECT

    public:
        explicit ExportWizard(QWidget* parent = nullptr);
        explicit ExportWizard(XenConnection* connection, QWidget* parent = nullptr);
        ~ExportWizard();

        // Page IDs
        enum
        {
            Page_Format = 0,
            Page_VMs = 1,
            Page_Options = 2,
            Page_Finish = 3,
            Page_Rbac = 4,  // Programmatic page inserted after Page_Format when RBAC check is needed
            Page_Eula = 5   // Programmatic page between VM selection and options (OVF/OVA only)
        };

        // Export format / destination accessors (valid after exec() returns Accepted)
        bool exportAsXVA() const { return this->m_exportAsXVA; }
        QString exportDirectory() const { return this->m_exportDirectory; }
        QString exportFileName() const { return this->m_exportFileName; }
        bool verifyExport() const;
        bool createManifest() const;
        bool createOva() const;
        bool compressOVF() const;
        bool signAppliance() const;
        void setSelectedObjectName(const QString& name) { this->m_selectedObjectName = name; }
        void setExportFileName(const QString& fileName);

        // Connection and VM context
        void SetConnection(XenConnection* connection);
        void SetPreselectedVMs(const QList<QSharedPointer<VM>>& vms);

        /**
         * @brief Lock the wizard to OVF/OVA mode only (XVA option hidden).
         *
         * Matches C# ExportAppliancePage.OvfModeOnly.
         * Call before exec() when exporting a vApp (which is always OVF/OVA).
         */
        void SetOvfModeOnly();

        // Returns checked VMs (OVF) or preselected VMs (XVA)
        QList<QSharedPointer<VM>> GetSelectedVMs() const;

        /**
         * @brief Returns the list of EULA file paths the user added on the EULA page.
         *
         * Matches C# ExportEulaPage.Eulas.  Only meaningful for OVF/OVA exports.
         * An empty list is valid (no EULAs in the appliance).
         */
        QStringList GetEulas() const;

        /**
         * @brief Returns the certificate file path selected for signing.
         * Empty string when signing is disabled.
         */
        QString GetCertificatePath() const;

        /**
         * @brief Returns the certificate password entered by the user.
         * Empty string if none was entered.
         */
        QString GetCertificatePassword() const;

        // Validate XVA destination and build the full output path.
        // Returns true when valid, sets *fullPath. Shows QMessageBox on failure.
        bool ValidateXvaDestination(QWidget* parent, QString* fullPath) const;

        // Validate OVF/OVA destination directory and appliance name.
        // Returns true when valid.  Sets *resolvedPath to a path of the form
        // /dir/name.{ovf,ova} — callers use QFileInfo(resolvedPath).absolutePath()
        // and QFileInfo(resolvedPath).baseName() to recover the parent directory and
        // appliance name passed to ExportApplianceAction.
        // NOTE: for OVF the action writes the package into a sub-folder
        //   /dir/name/name.ovf  (not /dir/name.ovf).
        // For OVA the action writes a single file /dir/name.ova.
        // Shows QMessageBox on failure.  Mirrors C# CheckDestinationFolderExists +
        // CheckPermissions + CheckApplianceExists from ExportAppliancePage.
        bool ValidateOvfDestination(QWidget* parent, QString* resolvedPath) const;

    private slots:
        void onFormatChanged();
        void onDirectoryBrowse();
        void onAddEula();
        void onRemoveEula();
        void onCertBrowse();
        void onSignApplianceToggled(bool checked);
        void onManifestToggled(bool checked);

    protected:
        int nextId() const override;
        void initializePage(int id) override;
        bool validateCurrentPage() override;

    private:
        void setupWizardPages();
        void updateSummary();
        void updateRbacPage();
        void populateVmList();
        bool hasApiPermissions(const QStringList& methods, QStringList* missing) const;

        Ui::ExportWizard* ui;

        // Connection and preselected VM context
        XenConnection* m_connection;
        QList<QSharedPointer<VM>> m_preselectedVMs;

        bool m_exportAsXVA;
        QString m_exportDirectory;
        QString m_exportFileName;
        QString m_selectedObjectName;

        // RBAC state (populated when leaving Page_Format)
        QStringList m_blockingRbacMissing;

        // Programmatic EULA page widgets (owned by the QWizardPage parent)
        QListWidget* m_eulaListWidget;
};

#endif // EXPORTWIZARD_H
