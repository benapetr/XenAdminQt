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

#include "networkpropertiesdialog.h"
#include "xenlib.h"
#include <QMessageBox>
#include <QHBoxLayout>

NetworkPropertiesDialog::NetworkPropertiesDialog(XenLib* xenLib, const QString& networkUuid, QWidget* parent)
    : QDialog(parent), m_xenLib(xenLib), m_networkUuid(networkUuid)
{
    setWindowTitle("Network Properties");
    setMinimumSize(600, 500);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    setupGeneralTab();
    setupAdvancedTab();
    mainLayout->addWidget(m_tabWidget);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &NetworkPropertiesDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &NetworkPropertiesDialog::onCancelClicked);

    // Connect async signal
    connect(m_xenLib, &XenLib::networksReceived,
            this, &NetworkPropertiesDialog::onNetworksReceived);

    // Load data asynchronously
    requestNetworkData();
}

void NetworkPropertiesDialog::setupGeneralTab()
{
    QWidget* generalTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(generalTab);

    // Basic Information Group
    QGroupBox* basicGroup = new QGroupBox("Basic Information");
    QFormLayout* basicLayout = new QFormLayout();

    m_nameEdit = new QLineEdit();
    m_descriptionEdit = new QTextEdit();
    m_descriptionEdit->setMaximumHeight(80);

    basicLayout->addRow("Name:", m_nameEdit);
    basicLayout->addRow("Description:", m_descriptionEdit);
    basicGroup->setLayout(basicLayout);
    layout->addWidget(basicGroup);

    // Network Information Group
    QGroupBox* networkGroup = new QGroupBox("Network Information");
    QFormLayout* networkLayout = new QFormLayout();

    m_uuidLabel = new QLabel();
    m_bridgeLabel = new QLabel();
    m_mtuLabel = new QLabel();
    m_managedLabel = new QLabel();

    networkLayout->addRow("UUID:", m_uuidLabel);
    networkLayout->addRow("Bridge:", m_bridgeLabel);
    networkLayout->addRow("MTU:", m_mtuLabel);
    networkLayout->addRow("Managed:", m_managedLabel);

    networkGroup->setLayout(networkLayout);
    layout->addWidget(networkGroup);

    layout->addStretch();
    m_tabWidget->addTab(generalTab, "General");
}

void NetworkPropertiesDialog::setupAdvancedTab()
{
    QWidget* advancedTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(advancedTab);

    // Tags Group
    QGroupBox* tagsGroup = new QGroupBox("Tags");
    QVBoxLayout* tagsLayout = new QVBoxLayout();
    m_tagsEdit = new QLineEdit();
    m_tagsEdit->setPlaceholderText("Comma-separated tags");
    tagsLayout->addWidget(m_tagsEdit);
    tagsGroup->setLayout(tagsLayout);
    layout->addWidget(tagsGroup);

    // Other Configuration Group
    QGroupBox* configGroup = new QGroupBox("Other Configuration");
    QVBoxLayout* configLayout = new QVBoxLayout();
    m_otherConfigList = new QListWidget();
    m_otherConfigList->setSelectionMode(QAbstractItemView::NoSelection);
    configLayout->addWidget(m_otherConfigList);
    configGroup->setLayout(configLayout);
    layout->addWidget(configGroup);

    m_tabWidget->addTab(advancedTab, "Advanced");
}

void NetworkPropertiesDialog::requestNetworkData()
{
    // Request networks asynchronously
    m_xenLib->requestNetworks();
}

void NetworkPropertiesDialog::onNetworksReceived(const QVariantList& networks)
{
    // Find our network by UUID
    for (const QVariant& networkVar : networks)
    {
        QVariantMap networkMap = networkVar.toMap();
        if (networkMap.contains(m_networkUuid))
        {
            // Get the network record
            QVariantMap networkRecord = networkMap[m_networkUuid].toMap();
            m_networkRecord = networkRecord;

            // Populate UI with network data
            populateNetworkData();
            return;
        }
    }

    qWarning() << "Network not found:" << m_networkUuid;
}

void NetworkPropertiesDialog::populateNetworkData()
{
    if (m_networkRecord.isEmpty())
    {
        return;
    }

    // General tab - editable fields
    QString name = m_networkRecord["name_label"].toString();
    QString description = m_networkRecord["name_description"].toString();
    m_nameEdit->setText(name);
    m_descriptionEdit->setPlainText(description);

    // General tab - read-only labels
    m_uuidLabel->setText(m_networkUuid);

    QString bridge = m_networkRecord["bridge"].toString();
    m_bridgeLabel->setText(bridge.isEmpty() ? "N/A" : bridge);

    QVariantMap otherConfig = m_networkRecord["other_config"].toMap();
    if (otherConfig.contains("mtu"))
    {
        m_mtuLabel->setText(otherConfig["mtu"].toString());
    } else
    {
        m_mtuLabel->setText("1500"); // Default MTU
    }

    bool managed = m_networkRecord["managed"].toBool();
    m_managedLabel->setText(managed ? "Yes" : "No");

    // Advanced tab - tags (editable)
    QVariantList tags = m_networkRecord["tags"].toList();
    QStringList tagStrings;
    for (const QVariant& tag : tags)
    {
        tagStrings << tag.toString();
    }
    m_tagsEdit->setText(tagStrings.join(", "));

    // Other config
    m_otherConfigList->clear();
    for (auto it = otherConfig.begin(); it != otherConfig.end(); ++it)
    {
        QString item = QString("%1: %2").arg(it.key(), it.value().toString());
        m_otherConfigList->addItem(item);
    }
}

void NetworkPropertiesDialog::saveNetworkData()
{
    QString newName = m_nameEdit->text().trimmed();
    QString newDescription = m_descriptionEdit->toPlainText().trimmed();
    QString currentName = m_networkRecord.value("name_label").toString();
    QString currentDescription = m_networkRecord.value("name_description").toString();

    // Update name if changed
    if (newName != currentName && !newName.isEmpty())
    {
        if (!m_xenLib->setNetworkName(m_networkRef, newName))
        {
            QMessageBox::warning(this, "Error", "Failed to update network name");
            return;
        }
    }

    // Update description if changed
    if (newDescription != currentDescription)
    {
        if (!m_xenLib->setNetworkDescription(m_networkRef, newDescription))
        {
            QMessageBox::warning(this, "Error", "Failed to update network description");
            return;
        }
    }

    // Update tags if changed
    QStringList currentTags = m_networkRecord.value("tags").toStringList();
    QString newTagsStr = m_tagsEdit->text().trimmed();
    QStringList newTags;
    if (!newTagsStr.isEmpty())
    {
        newTags = newTagsStr.split(",");
        for (QString& tag : newTags)
        {
            tag = tag.trimmed();
        }
    }

    if (newTags != currentTags)
    {
        if (!m_xenLib->setNetworkTags(m_networkRef, newTags))
        {
            QMessageBox::warning(this, "Error", "Failed to update network tags");
            return;
        }
    }
}

void NetworkPropertiesDialog::onOkClicked()
{
    saveNetworkData();
    accept();
}

void NetworkPropertiesDialog::onCancelClicked()
{
    reject();
}
