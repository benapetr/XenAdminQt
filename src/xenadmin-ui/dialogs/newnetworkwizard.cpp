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

#include "newnetworkwizard.h"
#include <QtWidgets>

NewNetworkWizard::NewNetworkWizard(QWidget* parent)
    : QWizard(parent), m_selectedNetworkType(External), m_vlanId(0), m_mtu(1500), m_autoAddToVMs(true), m_autoPlug(true)
{
    this->setWindowTitle(tr("New Network Wizard"));
    this->setWindowIcon(QIcon(":/icons/network-32.png"));
    this->setWizardStyle(QWizard::ModernStyle);
    this->setOption(QWizard::HaveHelpButton, true);
    this->setOption(QWizard::HelpButtonOnRight, false);

    // Setup pages
    this->setPage(Page_NetworkType, this->createNetworkTypePage());
    this->setPage(Page_Name, this->createNamePage());
    this->setPage(Page_Details, this->createDetailsPage());
    this->setPage(Page_Finish, this->createFinishPage());

    this->setStartId(Page_NetworkType);

    qDebug() << "NewNetworkWizard: Created New Network Wizard";
}

QWizardPage* NewNetworkWizard::createNetworkTypePage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Network Type"));
    page->setSubTitle(tr("Select the type of network to create."));

    // Network type radio buttons
    this->m_externalRadio = new QRadioButton(tr("&External network"));
    this->m_internalRadio = new QRadioButton(tr("&Internal network"));
    this->m_bondedRadio = new QRadioButton(tr("&Bonded network"));
    this->m_crossHostRadio = new QRadioButton(tr("&Cross-host internal network"));
    this->m_sriovRadio = new QRadioButton(tr("&SR-IOV network"));

    this->m_externalRadio->setChecked(true); // Default selection

    // Group buttons
    this->m_networkTypeGroup = new QButtonGroup(this);
    this->m_networkTypeGroup->addButton(this->m_externalRadio, External);
    this->m_networkTypeGroup->addButton(this->m_internalRadio, Internal);
    this->m_networkTypeGroup->addButton(this->m_bondedRadio, Bonded);
    this->m_networkTypeGroup->addButton(this->m_crossHostRadio, CrossHost);
    this->m_networkTypeGroup->addButton(this->m_sriovRadio, SRIOV);

    // Connect signals
    connect(this->m_networkTypeGroup, &QButtonGroup::idClicked,
            this, &NewNetworkWizard::onNetworkTypeChanged);

    // Descriptions for each type
    QLabel* extDesc = new QLabel(tr("Network connected to physical network interface"));
    QLabel* intDesc = new QLabel(tr("Private network isolated to this host"));
    QLabel* bondDesc = new QLabel(tr("Combine multiple network interfaces for redundancy"));
    QLabel* chinDesc = new QLabel(tr("Private network spanning multiple hosts"));
    QLabel* sriovDesc = new QLabel(tr("High-performance network using SR-IOV technology"));

    extDesc->setStyleSheet("QLabel { color: #666; margin-left: 20px; }");
    intDesc->setStyleSheet("QLabel { color: #666; margin-left: 20px; }");
    bondDesc->setStyleSheet("QLabel { color: #666; margin-left: 20px; }");
    chinDesc->setStyleSheet("QLabel { color: #666; margin-left: 20px; }");
    sriovDesc->setStyleSheet("QLabel { color: #666; margin-left: 20px; }");

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(this->m_externalRadio);
    layout->addWidget(extDesc);
    layout->addSpacing(10);
    layout->addWidget(this->m_internalRadio);
    layout->addWidget(intDesc);
    layout->addSpacing(10);
    layout->addWidget(this->m_bondedRadio);
    layout->addWidget(bondDesc);
    layout->addSpacing(10);
    layout->addWidget(this->m_crossHostRadio);
    layout->addWidget(chinDesc);
    layout->addSpacing(10);
    layout->addWidget(this->m_sriovRadio);
    layout->addWidget(sriovDesc);
    layout->addStretch();

    page->setLayout(layout);
    return page;
}

QWizardPage* NewNetworkWizard::createNamePage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Network Name"));
    page->setSubTitle(tr("Enter a name and description for the network."));

    this->m_nameLineEdit = new QLineEdit;
    this->m_nameLineEdit->setObjectName("networkName");
    this->m_nameLineEdit->setPlaceholderText(tr("Enter network name"));

    this->m_descriptionTextEdit = new QTextEdit;
    this->m_descriptionTextEdit->setObjectName("networkDescription");
    this->m_descriptionTextEdit->setPlaceholderText(tr("Enter network description (optional)"));
    this->m_descriptionTextEdit->setMaximumHeight(80);

    QFormLayout* layout = new QFormLayout;
    layout->addRow(tr("&Name:"), this->m_nameLineEdit);
    layout->addRow(tr("&Description:"), this->m_descriptionTextEdit);

    page->setLayout(layout);
    return page;
}

QWizardPage* NewNetworkWizard::createDetailsPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Network Details"));
    page->setSubTitle(tr("Configure network settings."));

    // Network interface selection (for external networks)
    QLabel* nicLabel = new QLabel(tr("Network Interface:"));
    this->m_nicComboBox = new QComboBox;
    this->m_nicComboBox->addItem(tr("eth0 - Primary Network Interface"), "eth0");
    this->m_nicComboBox->addItem(tr("eth1 - Secondary Network Interface"), "eth1");
    this->m_nicComboBox->addItem(tr("Auto-select best interface"), "auto");

    // VLAN configuration
    QLabel* vlanLabel = new QLabel(tr("VLAN ID:"));
    this->m_vlanSpinBox = new QSpinBox;
    this->m_vlanSpinBox->setRange(0, 4094);
    this->m_vlanSpinBox->setValue(0);
    this->m_vlanSpinBox->setSpecialValueText(tr("None"));

    // MTU configuration
    QLabel* mtuLabel = new QLabel(tr("MTU:"));
    this->m_mtuSpinBox = new QSpinBox;
    this->m_mtuSpinBox->setRange(68, 9000);
    this->m_mtuSpinBox->setValue(1500);
    this->m_mtuSpinBox->setSuffix(tr(" bytes"));

    // Auto-configuration options
    this->m_autoAddNicCheckBox = new QCheckBox(tr("Automatically add network interface to new VMs"));
    this->m_autoAddNicCheckBox->setChecked(true);

    this->m_autoPlugCheckBox = new QCheckBox(tr("Automatically attach network interface when VM starts"));
    this->m_autoPlugCheckBox->setChecked(true);

    // Bonding interface selection (for bonded networks)
    this->m_bondInterfacesLabel = new QLabel(tr("Interfaces to Bond:"));
    this->m_bondInterfacesList = new QListWidget;
    this->m_bondInterfacesList->setSelectionMode(QAbstractItemView::MultiSelection);
    this->m_bondInterfacesList->addItem("eth0 - Primary Interface");
    this->m_bondInterfacesList->addItem("eth1 - Secondary Interface");
    this->m_bondInterfacesList->addItem("eth2 - Tertiary Interface");
    this->m_bondInterfacesList->setMaximumHeight(100);

    this->m_bondModeLabel = new QLabel(tr("Bonding Mode:"));
    this->m_bondModeComboBox = new QComboBox;
    this->m_bondModeComboBox->addItem(tr("Active-Backup (Failover)"), "active-backup");
    this->m_bondModeComboBox->addItem(tr("Load Balancing"), "balance-slb");
    this->m_bondModeComboBox->addItem(tr("LACP (802.3ad)"), "balance-tcp");

    QFormLayout* layout = new QFormLayout;
    layout->addRow(nicLabel, this->m_nicComboBox);
    layout->addRow(vlanLabel, this->m_vlanSpinBox);
    layout->addRow(mtuLabel, this->m_mtuSpinBox);
    layout->addRow("", new QLabel("")); // Spacer
    layout->addRow(this->m_autoAddNicCheckBox);
    layout->addRow(this->m_autoPlugCheckBox);
    layout->addRow("", new QLabel("")); // Spacer
    layout->addRow(this->m_bondInterfacesLabel);
    layout->addRow(this->m_bondInterfacesList);
    layout->addRow(this->m_bondModeLabel, this->m_bondModeComboBox);

    page->setLayout(layout);
    return page;
}

QWizardPage* NewNetworkWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage;
    page->setTitle(tr("Finish"));
    page->setSubTitle(tr("Ready to create the network."));

    this->m_summaryTextEdit = new QTextEdit;
    this->m_summaryTextEdit->setObjectName("summaryText");
    this->m_summaryTextEdit->setReadOnly(true);
    this->m_summaryTextEdit->setMaximumHeight(200);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(new QLabel(tr("Summary of network configuration:")));
    layout->addWidget(this->m_summaryTextEdit);
    layout->addStretch();

    page->setLayout(layout);
    return page;
}

void NewNetworkWizard::initializePage(int id)
{
    if (id == Page_Details)
    {
        this->updateDetailsPage();
    } else if (id == Page_Finish)
    {
        this->updateSummary();
    }

    QWizard::initializePage(id);
}

void NewNetworkWizard::updateDetailsPage()
{
    // Show/hide widgets based on network type
    NetworkType type = static_cast<NetworkType>(this->m_networkTypeGroup->checkedId());

    // Find all widgets
    QFormLayout* layout = qobject_cast<QFormLayout*>(this->page(Page_Details)->layout());
    if (!layout)
        return;

    // Hide/show based on network type
    bool showNicSelection = (type == External || type == SRIOV);
    bool showVlan = (type == External);
    bool showBonding = (type == Bonded);

    // Update widget visibility
    this->m_nicComboBox->setVisible(showNicSelection);
    this->m_vlanSpinBox->setVisible(showVlan);
    this->m_bondInterfacesList->setVisible(showBonding);
    this->m_bondModeComboBox->setVisible(showBonding);
    this->m_bondInterfacesLabel->setVisible(showBonding);
    this->m_bondModeLabel->setVisible(showBonding);

    // Update labels
    for (int i = 0; i < layout->rowCount(); ++i)
    {
        QLayoutItem* labelItem = layout->itemAt(i, QFormLayout::LabelRole);
        if (labelItem && labelItem->widget())
        {
            QLabel* label = qobject_cast<QLabel*>(labelItem->widget());
            if (label)
            {
                QString text = label->text();
                if (text.contains("Network Interface") && !showNicSelection)
                {
                    label->hide();
                } else if (text.contains("VLAN") && !showVlan)
                {
                    label->hide();
                } else if (text.contains("Bond") && !showBonding)
                {
                    label->hide();
                } else if (!text.isEmpty())
                {
                    label->show();
                }
            }
        }
    }
}

bool NewNetworkWizard::validateCurrentPage()
{
    int currentId = this->currentId();

    if (currentId == Page_Name)
    {
        QString name = this->m_nameLineEdit->text().trimmed();
        if (name.isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid Input"),
                                 tr("Please enter a name for the network."));
            return false;
        }
        this->m_networkName = name;
        this->m_networkDescription = this->m_descriptionTextEdit->toPlainText().trimmed();
    } else if (currentId == Page_Details)
    {
        this->m_vlanId = this->m_vlanSpinBox->value();
        this->m_mtu = this->m_mtuSpinBox->value();
        this->m_autoAddToVMs = this->m_autoAddNicCheckBox->isChecked();
        this->m_autoPlug = this->m_autoPlugCheckBox->isChecked();
    }

    return QWizard::validateCurrentPage();
}

void NewNetworkWizard::accept()
{
    // Store final configuration
    this->m_selectedNetworkType = static_cast<NetworkType>(this->m_networkTypeGroup->checkedId());

    QWizard::accept();
}

void NewNetworkWizard::onNetworkTypeChanged()
{
    // Update the selected network type when radio buttons change
    this->m_selectedNetworkType = static_cast<NetworkType>(this->m_networkTypeGroup->checkedId());
    qDebug() << "NewNetworkWizard: Network type changed to:" << this->m_selectedNetworkType;
}

void NewNetworkWizard::updateSummary()
{
    QString summary;

    // Network type
    QString typeStr;
    switch (this->m_selectedNetworkType)
    {
    case External:
        typeStr = tr("External network");
        break;
    case Internal:
        typeStr = tr("Internal network");
        break;
    case Bonded:
        typeStr = tr("Bonded network");
        break;
    case CrossHost:
        typeStr = tr("Cross-host internal network");
        break;
    case SRIOV:
        typeStr = tr("SR-IOV network");
        break;
    }

    summary += tr("Network Name: %1\n").arg(this->m_networkName);
    summary += tr("Network Type: %1\n").arg(typeStr);

    if (!this->m_networkDescription.isEmpty())
    {
        summary += tr("Description: %1\n").arg(this->m_networkDescription);
    }

    if (this->m_selectedNetworkType == External && this->m_vlanId > 0)
    {
        summary += tr("VLAN ID: %1\n").arg(this->m_vlanId);
    }

    summary += tr("MTU: %1 bytes\n").arg(this->m_mtu);
    summary += tr("Auto-add to VMs: %1\n").arg(this->m_autoAddToVMs ? tr("Yes") : tr("No"));
    summary += tr("Auto-attach: %1\n").arg(this->m_autoPlug ? tr("Yes") : tr("No"));

    if (this->m_selectedNetworkType == Bonded)
    {
        summary += tr("\nBonding Configuration:\n");
        summary += tr("Mode: %1\n").arg(this->m_bondModeComboBox->currentText());
        summary += tr("Interfaces: ");
        QStringList interfaces;
        for (int i = 0; i < this->m_bondInterfacesList->count(); ++i)
        {
            QListWidgetItem* item = this->m_bondInterfacesList->item(i);
            if (item->isSelected())
            {
                interfaces << item->text();
            }
        }
        summary += interfaces.join(", ");
    }

    this->m_summaryTextEdit->setPlainText(summary);
}
