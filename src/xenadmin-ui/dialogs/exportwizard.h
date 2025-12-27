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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QRadioButton>
#include <QSpinBox>

class QMainWindow;
class VM;

class ExportWizard : public QWizard
{
    Q_OBJECT

    public:
        explicit ExportWizard(QWidget* parent = nullptr);

        // Page IDs
        enum
        {
            Page_Format = 0,
            Page_VMs = 1,
            Page_Options = 2,
            Page_Finish = 3
        };

        // Export format
        bool exportAsXVA() const
        {
            return m_exportAsXVA;
        }
        QString exportDirectory() const
        {
            return m_exportDirectory;
        }
        QString exportFileName() const
        {
            return m_exportFileName;
        }

    private slots:
        void onFormatChanged();
        void onDirectoryBrowse();

    private:
        QWizardPage* createFormatPage();
        QWizardPage* createVMsPage();
        QWizardPage* createOptionsPage();
        QWizardPage* createFinishPage();

        void updateSummary();

        bool m_exportAsXVA;
        QString m_exportDirectory;
        QString m_exportFileName;

        // Format page widgets
        QComboBox* m_formatComboBox;
        QLineEdit* m_directoryLineEdit;
        QPushButton* m_directoryBrowseButton;
        QLineEdit* m_fileNameLineEdit;

        // VMs page widgets
        QListWidget* m_vmListWidget;

        // Options page widgets
        QCheckBox* m_createManifestCheckBox;
        QCheckBox* m_signApplianceCheckBox;
        QCheckBox* m_encryptFilesCheckBox;
        QCheckBox* m_createOVACheckBox;
        QCheckBox* m_compressOVFCheckBox;
        QCheckBox* m_verifyExportCheckBox;

        // Finish page widgets
        QPlainTextEdit* m_summaryTextEdit;
};

#endif // EXPORTWIZARD_H
