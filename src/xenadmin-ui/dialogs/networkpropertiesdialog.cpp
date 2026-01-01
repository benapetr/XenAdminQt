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
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/xenapi/xenapi_Network.h"
#include "xencache.h"
#include <QDebug>
#include <QMessageBox>
#include <QHBoxLayout>

NetworkPropertiesDialog::NetworkPropertiesDialog(XenConnection* connection, const QString& networkUuid, QWidget* parent)
    : QDialog(parent), m_connection(connection), m_networkUuid(networkUuid)
{
    setWindowTitle("Network Properties");
    setMinimumSize(600, 500);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Tab widget
    this->m_tabWidget = new QTabWidget(this);
    setupGeneralTab();
    setupAdvancedTab();
    mainLayout->addWidget(this->m_tabWidget);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    this->m_okButton = new QPushButton("OK", this);
    this->m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(this->m_okButton);
    buttonLayout->addWidget(this->m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(this->m_okButton, &QPushButton::clicked, this, &NetworkPropertiesDialog::onOkClicked);
    connect(this->m_cancelButton, &QPushButton::clicked, this, &NetworkPropertiesDialog::onCancelClicked);

    // Load data from cache
    loadNetworkData();
}

void NetworkPropertiesDialog::setupGeneralTab()
{
    QWidget* generalTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(generalTab);

    // Basic Information Group
    QGroupBox* basicGroup = new QGroupBox("Basic Information");
    QFormLayout* basicLayout = new QFormLayout();

    this->m_nameEdit = new QLineEdit();
    this->m_descriptionEdit = new QTextEdit();
    this->m_descriptionEdit->setMaximumHeight(80);

    basicLayout->addRow("Name:", this->m_nameEdit);
    basicLayout->addRow("Description:", this->m_descriptionEdit);
    basicGroup->setLayout(basicLayout);
    layout->addWidget(basicGroup);

    // Network Information Group
    QGroupBox* networkGroup = new QGroupBox("Network Information");
    QFormLayout* networkLayout = new QFormLayout();

    this->m_uuidLabel = new QLabel();
    this->m_bridgeLabel = new QLabel();
    this->m_mtuLabel = new QLabel();
    this->m_managedLabel = new QLabel();

    networkLayout->addRow("UUID:", this->m_uuidLabel);
    networkLayout->addRow("Bridge:", this->m_bridgeLabel);
    networkLayout->addRow("MTU:", this->m_mtuLabel);
    networkLayout->addRow("Managed:", this->m_managedLabel);

    networkGroup->setLayout(networkLayout);
    layout->addWidget(networkGroup);

    layout->addStretch();
    this->m_tabWidget->addTab(generalTab, "General");
}

void NetworkPropertiesDialog::setupAdvancedTab()
{
    QWidget* advancedTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(advancedTab);

    // Tags Group
    QGroupBox* tagsGroup = new QGroupBox("Tags");
    QVBoxLayout* tagsLayout = new QVBoxLayout();
    this->m_tagsEdit = new QLineEdit();
    this->m_tagsEdit->setPlaceholderText("Comma-separated tags");
    tagsLayout->addWidget(this->m_tagsEdit);
    tagsGroup->setLayout(tagsLayout);
    layout->addWidget(tagsGroup);

    // Other Configuration Group
    QGroupBox* configGroup = new QGroupBox("Other Configuration");
    QVBoxLayout* configLayout = new QVBoxLayout();
    this->m_otherConfigList = new QListWidget();
    this->m_otherConfigList->setSelectionMode(QAbstractItemView::NoSelection);
    configLayout->addWidget(this->m_otherConfigList);
    configGroup->setLayout(configLayout);
    layout->addWidget(configGroup);

    this->m_tabWidget->addTab(advancedTab, "Advanced");
}

void NetworkPropertiesDialog::loadNetworkData()
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    QVariantMap networkRecord = this->m_connection->GetCache()->ResolveObjectData("network", this->m_networkUuid);
    if (!networkRecord.isEmpty())
    {
        this->m_networkRecord = networkRecord;
        this->m_networkRef = networkRecord.value("ref").toString();
        if (this->m_networkRef.isEmpty())
            this->m_networkRef = networkRecord.value("opaqueRef").toString();
        if (this->m_networkRef.isEmpty())
            this->m_networkRef = networkRecord.value("_ref").toString();
        populateNetworkData();
        return;
    }

    QList<QVariantMap> networks = this->m_connection->GetCache()->GetAllData("network");
    for (const QVariantMap& record : networks)
    {
        QString uuid = record.value("uuid").toString();
        QString ref = record.value("ref").toString();
        if (uuid == this->m_networkUuid || ref == this->m_networkUuid)
        {
            this->m_networkRecord = record;
            this->m_networkRef = ref;
            if (this->m_networkRef.isEmpty())
                this->m_networkRef = record.value("opaqueRef").toString();
            if (this->m_networkRef.isEmpty())
                this->m_networkRef = record.value("_ref").toString();
            populateNetworkData();
            return;
        }
    }

    qWarning() << "Network not found in cache:" << this->m_networkUuid;
}

void NetworkPropertiesDialog::populateNetworkData()
{
    if (this->m_networkRecord.isEmpty())
    {
        return;
    }

    // General tab - editable fields
    QString name = this->m_networkRecord["name_label"].toString();
    QString description = this->m_networkRecord["name_description"].toString();
    this->m_nameEdit->setText(name);
    this->m_descriptionEdit->setPlainText(description);

    // General tab - read-only labels
    QString uuidText = this->m_networkRecord.value("uuid").toString();
    if (uuidText.isEmpty())
        uuidText = this->m_networkUuid;
    this->m_uuidLabel->setText(uuidText);

    QString bridge = this->m_networkRecord["bridge"].toString();
    this->m_bridgeLabel->setText(bridge.isEmpty() ? "N/A" : bridge);

    QVariantMap otherConfig = this->m_networkRecord["other_config"].toMap();
    if (otherConfig.contains("mtu"))
    {
        this->m_mtuLabel->setText(otherConfig["mtu"].toString());
    } else
    {
        this->m_mtuLabel->setText("1500"); // Default MTU
    }

    bool managed = this->m_networkRecord["managed"].toBool();
    this->m_managedLabel->setText(managed ? "Yes" : "No");

    // Advanced tab - tags (editable)
    QVariantList tags = this->m_networkRecord["tags"].toList();
    QStringList tagStrings;
    for (const QVariant& tag : tags)
    {
        tagStrings << tag.toString();
    }
    this->m_tagsEdit->setText(tagStrings.join(", "));

    // Other config
    this->m_otherConfigList->clear();
    for (auto it = otherConfig.begin(); it != otherConfig.end(); ++it)
    {
        QString item = QString("%1: %2").arg(it.key(), it.value().toString());
        this->m_otherConfigList->addItem(item);
    }
}

void NetworkPropertiesDialog::saveNetworkData()
{
    if (!this->m_connection || this->m_networkRef.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No network reference available");
        return;
    }

    QString newName = this->m_nameEdit->text().trimmed();
    QString newDescription = this->m_descriptionEdit->toPlainText().trimmed();
    QString currentName = this->m_networkRecord.value("name_label").toString();
    QString currentDescription = this->m_networkRecord.value("name_description").toString();

    // Update name if changed
    if (newName != currentName && !newName.isEmpty())
    {
        XenAPI::Session* session = this->m_connection ? this->m_connection->GetSession() : nullptr;
        if (!session || !session->IsLoggedIn())
        {
            QMessageBox::warning(this, "Error", "No active session to update network name");
            return;
        }
        try
        {
            XenAPI::Network::set_name_label(session, this->m_networkRef, newName);
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(this, "Error",
                                 QString("Failed to update network name: %1").arg(ex.what()));
            return;
        }
    }

    // Update description if changed
    if (newDescription != currentDescription)
    {
        XenAPI::Session* session = this->m_connection ? this->m_connection->GetSession() : nullptr;
        if (!session || !session->IsLoggedIn())
        {
            QMessageBox::warning(this, "Error", "No active session to update network description");
            return;
        }
        try
        {
            XenAPI::Network::set_name_description(session, this->m_networkRef, newDescription);
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(this, "Error",
                                 QString("Failed to update network description: %1").arg(ex.what()));
            return;
        }
    }

    // Update tags if changed
    QStringList currentTags = this->m_networkRecord.value("tags").toStringList();
    QString newTagsStr = this->m_tagsEdit->text().trimmed();
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
        XenAPI::Session* session = this->m_connection ? this->m_connection->GetSession() : nullptr;
        if (!session || !session->IsLoggedIn())
        {
            QMessageBox::warning(this, "Error", "No active session to update network tags");
            return;
        }
        try
        {
            XenAPI::Network::set_tags(session, this->m_networkRef, newTags);
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(this, "Error",
                                 QString("Failed to update network tags: %1").arg(ex.what()));
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
