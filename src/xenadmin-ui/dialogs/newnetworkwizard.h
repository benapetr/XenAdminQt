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

#ifndef NEWNETWORKWIZARD_H
#define NEWNETWORKWIZARD_H

#include <QWizard>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QGroupBox>
#include <QButtonGroup>

class NewNetworkWizard : public QWizard
{
    Q_OBJECT

public:
    explicit NewNetworkWizard(QWidget* parent = nullptr);

    // Network Types
    enum NetworkType
    {
        External = 0,
        Internal = 1,
        Bonded = 2,
        CrossHost = 3,
        SRIOV = 4
    };

    // Page IDs
    enum
    {
        Page_NetworkType = 0,
        Page_Name = 1,
        Page_Details = 2,
        Page_Finish = 3
    };

    // Getters for configured network properties
    QString networkName() const
    {
        return m_networkName;
    }
    QString networkDescription() const
    {
        return m_networkDescription;
    }
    NetworkType networkType() const
    {
        return m_selectedNetworkType;
    }
    int vlanId() const
    {
        return m_vlanId;
    }
    int mtu() const
    {
        return m_mtu;
    }
    bool autoAddToVMs() const
    {
        return m_autoAddToVMs;
    }
    bool autoPlug() const
    {
        return m_autoPlug;
    }

protected:
    void initializePage(int id) override;
    bool validateCurrentPage() override;
    void accept() override;

private slots:
    void onNetworkTypeChanged();

private:
    QWizardPage* createNetworkTypePage();
    QWizardPage* createNamePage();
    QWizardPage* createDetailsPage();
    QWizardPage* createFinishPage();

    void updateDetailsPage();
    void updateSummary();

    // Network type selection
    QRadioButton* m_externalRadio;
    QRadioButton* m_internalRadio;
    QRadioButton* m_bondedRadio;
    QRadioButton* m_crossHostRadio;
    QRadioButton* m_sriovRadio;
    QButtonGroup* m_networkTypeGroup;

    // Name and description
    QLineEdit* m_nameLineEdit;
    QTextEdit* m_descriptionTextEdit;

    // Details page widgets
    QComboBox* m_nicComboBox;
    QSpinBox* m_vlanSpinBox;
    QSpinBox* m_mtuSpinBox;
    QCheckBox* m_autoAddNicCheckBox;
    QCheckBox* m_autoPlugCheckBox;

    // Bonding specific
    QLabel* m_bondInterfacesLabel;
    QListWidget* m_bondInterfacesList;
    QLabel* m_bondModeLabel;
    QComboBox* m_bondModeComboBox;

    // Finish page
    QTextEdit* m_summaryTextEdit;

    // Data storage
    NetworkType m_selectedNetworkType;
    QString m_networkName;
    QString m_networkDescription;
    int m_vlanId;
    int m_mtu;
    bool m_autoAddToVMs;
    bool m_autoPlug;
};

#endif // NEWNETWORKWIZARD_H
