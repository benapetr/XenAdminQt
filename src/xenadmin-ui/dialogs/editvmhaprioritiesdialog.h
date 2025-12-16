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

#ifndef EDITVMHAPRIORITIESDIALOG_H
#define EDITVMHAPRIORITIESDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMap>
#include <QVariantMap>

class XenLib;

/**
 * @brief EditVmHaPrioritiesDialog - Dialog for editing VM HA priorities when HA is already enabled.
 *
 * Matches C# XenAdmin/Dialogs/EditVmHaPrioritiesDialog.cs
 *
 * This dialog allows modifying:
 * - VM restart priorities (Restart, Best Effort, Do Not Restart)
 * - VM start order
 * - VM start delay
 * - NTOL (number of host failures to tolerate)
 *
 * Unlike the HAWizard, this dialog does not change the heartbeat SR.
 */
class EditVmHaPrioritiesDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param xenLib XenLib instance
     * @param poolRef Pool opaque reference (must have HA enabled)
     * @param parent Parent widget
     */
    explicit EditVmHaPrioritiesDialog(XenLib* xenLib, const QString& poolRef, QWidget* parent = nullptr);

private slots:
    void onNtolChanged(int value);
    void updateOkButtonState();
    void accept() override;

private:
    void setupUi();
    void populateVMTable();
    void updateNtolCalculation();
    bool hasChanges() const;
    QMap<QString, QVariantMap> buildVmStartupOptions() const;

    XenLib* m_xenLib;
    QString m_poolRef;
    QString m_poolName;
    qint64 m_originalNtol;

    // UI widgets
    QLabel* m_headerLabel;
    QLabel* m_warningIcon;
    QLabel* m_warningLabel;

    // NTOL controls
    QGroupBox* m_ntolGroup;
    QSpinBox* m_ntolSpinBox;
    QLabel* m_ntolStatusLabel;
    QLabel* m_maxNtolLabel;

    // VM table
    QTableWidget* m_vmTable;

    // Buttons
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    // State
    qint64 m_ntol;
    qint64 m_maxNtol;
    QMap<QString, QVariantMap> m_originalSettings; // Original VM settings for change detection
};

#endif // EDITVMHAPRIORITIESDIALOG_H
