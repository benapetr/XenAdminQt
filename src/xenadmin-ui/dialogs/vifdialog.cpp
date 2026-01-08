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

#include "vifdialog.h"
#include "ui_vifdialog.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QDebug>

VIFDialog::VIFDialog(XenConnection* connection, const QString& vmRef, int deviceId, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::VIFDialog),
      m_connection(connection),
      m_vmRef(vmRef),
      m_deviceId(deviceId),
      m_isEditMode(false)
{
    this->ui->setupUi(this);
    this->setWindowTitle(this->tr("Add Network Interface"));

    // Connect signals
    connect(this->ui->comboBoxNetwork, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VIFDialog::onNetworkChanged);
    connect(this->ui->radioButtonAutogenerate, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->radioButtonManual, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->lineEditMAC, &QLineEdit::textChanged, this, &VIFDialog::onMACTextChanged);
    connect(this->ui->checkBoxQoS, &QCheckBox::toggled, this, &VIFDialog::onQoSCheckboxChanged);
    connect(this->ui->spinBoxQoS, QOverload<int>::of(&QSpinBox::valueChanged), this, &VIFDialog::onQoSValueChanged);
}

VIFDialog::VIFDialog(XenConnection* connection, const QString& vifRef, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::VIFDialog),
      m_connection(connection),
      m_vifRef(vifRef),
      m_deviceId(0),
      m_isEditMode(true)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("Virtual Interface Properties"));

    // Get existing VIF data
    if (this->m_connection && this->m_connection->GetCache() && !this->m_vifRef.isEmpty())
    {
        this->m_existingVif = this->m_connection->GetCache()->ResolveObjectData("VIF", this->m_vifRef);
        this->m_vmRef = this->m_existingVif.value("VM").toString();
        this->m_deviceId = this->m_existingVif.value("device").toInt();
    }

    // Connect signals
    connect(this->ui->comboBoxNetwork, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VIFDialog::onNetworkChanged);
    connect(this->ui->radioButtonAutogenerate, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->radioButtonManual, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->lineEditMAC, &QLineEdit::textChanged, this, &VIFDialog::onMACTextChanged);
    connect(this->ui->checkBoxQoS, &QCheckBox::toggled, this, &VIFDialog::onQoSCheckboxChanged);
    connect(this->ui->spinBoxQoS, QOverload<int>::of(&QSpinBox::valueChanged), this, &VIFDialog::onQoSValueChanged);
}

VIFDialog::VIFDialog(XenConnection* connection, const QVariantMap& existingVif, int deviceId, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::VIFDialog),
      m_connection(connection),
      m_deviceId(deviceId),
      m_existingVif(existingVif),
      m_isEditMode(true)
{
    this->ui->setupUi(this);
    this->setWindowTitle(tr("Virtual Interface Properties"));

    if (this->m_existingVif.contains("VM"))
        this->m_vmRef = this->m_existingVif.value("VM").toString();

    // Connect signals
    connect(this->ui->comboBoxNetwork, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VIFDialog::onNetworkChanged);
    connect(this->ui->radioButtonAutogenerate, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->radioButtonManual, &QRadioButton::toggled, this, &VIFDialog::onMACRadioChanged);
    connect(this->ui->lineEditMAC, &QLineEdit::textChanged, this, &VIFDialog::onMACTextChanged);
    connect(this->ui->checkBoxQoS, &QCheckBox::toggled, this, &VIFDialog::onQoSCheckboxChanged);
    connect(this->ui->spinBoxQoS, QOverload<int>::of(&QSpinBox::valueChanged), this, &VIFDialog::onQoSValueChanged);
}

VIFDialog::~VIFDialog()
{
    delete this->ui;
}

void VIFDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    this->loadNetworks();
    this->loadVifDetails();
    this->validateInput();
}

void VIFDialog::loadNetworks()
{
    // C#: LoadNetworks() - loads all networks and filters them
    this->ui->comboBoxNetwork->clear();

    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    QStringList networkRefs = this->m_connection->GetCache()->GetAllRefs("network");
    QList<QPair<QString, QString>> networks; // <ref, name>

    for (const QString& networkRef : networkRefs)
    {
        QVariantMap networkData = this->m_connection->GetCache()->ResolveObjectData("network", networkRef);

        // C#: if (!network.Show(Properties.Settings.Default.ShowHiddenVMs) || network.IsMember() || (network.IsSriov() && !allowSriov))
        //     continue;

        // Check if network should be shown
        QString nameLabel = networkData.value("name_label").toString();
        QString nameDescription = networkData.value("name_description").toString();

        // Skip hidden networks (guest installer network - HIMN)
        if (nameLabel.contains("Host internal management network", Qt::CaseInsensitive) ||
            nameDescription.contains("Host internal management network", Qt::CaseInsensitive))
            continue;

        // Skip if it's a network bond member (has PIFs but they're all bond members)
        // For now, we'll show all networks - C# IsMember() is complex

        networks.append(qMakePair(networkRef, nameLabel));
    }

    // Sort by name
    std::sort(networks.begin(), networks.end(),
              [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                  return a.second < b.second;
              });

    // Add to combobox
    for (const auto& network : networks)
    {
        this->ui->comboBoxNetwork->addItem(network.second, network.first);
    }

    if (this->ui->comboBoxNetwork->count() == 0)
    {
        this->ui->comboBoxNetwork->addItem(this->tr("<None>"), QString());
    }

    this->ui->comboBoxNetwork->setCurrentIndex(0);
}

void VIFDialog::loadVifDetails()
{
    // C#: LoadDetails() - loads VIF settings if editing

    if (!this->m_isEditMode || this->m_existingVif.isEmpty())
    {
        // New VIF - use defaults
        this->ui->radioButtonAutogenerate->setChecked(true);
        this->ui->checkBoxQoS->setChecked(false);
        return;
    }

    // Editing existing VIF - load its settings

    // Select the network
    QString networkRef = this->m_existingVif.value("network").toString();
    int networkIndex = this->ui->comboBoxNetwork->findData(networkRef);
    if (networkIndex >= 0)
        this->ui->comboBoxNetwork->setCurrentIndex(networkIndex);

    // Load MAC address
    QString mac = this->m_existingVif.value("MAC").toString();
    if (!mac.isEmpty())
    {
        this->ui->radioButtonManual->setChecked(true);
        this->ui->lineEditMAC->setText(mac);
    } else
    {
        this->ui->radioButtonAutogenerate->setChecked(true);
    }

    // Load QoS settings
    QString qosAlgorithm = this->m_existingVif.value("qos_algorithm_type").toString();
    if (qosAlgorithm == "ratelimit")
    {
        this->ui->checkBoxQoS->setChecked(true);

        QVariantMap qosParams = this->m_existingVif.value("qos_algorithm_params").toMap();
        if (qosParams.contains("kbps"))
        {
            int kbps = qosParams.value("kbps").toInt();
            this->ui->spinBoxQoS->setValue(kbps);
        }
    } else
    {
        this->ui->checkBoxQoS->setChecked(false);
    }
}

QVariantMap VIFDialog::getVifSettings() const
{
    // C#: GetNewSettings() or NewVif() - returns VIF record

    QVariantMap vif;

    // Copy existing VIF data if editing
    if (this->m_isEditMode && !this->m_existingVif.isEmpty())
    {
        vif = this->m_existingVif;
    }

    // Set/update fields
    vif["network"] = this->getSelectedNetworkRef();
    vif["MAC"] = this->getSelectedMAC();
    vif["device"] = QString::number(this->m_deviceId);
    vif["VM"] = this->m_vmRef;

    // Set MTU from the network (XenServer requires this field)
    // C# Network class has default MTU of 1500 (Network.cs line 875)
    QString networkRef = this->getSelectedNetworkRef();
    if (!networkRef.isEmpty() && this->m_connection && this->m_connection->GetCache())
    {
        QVariantMap networkData = this->m_connection->GetCache()->ResolveObjectData("network", networkRef);
        if (!networkData.isEmpty())
        {
            vif["MTU"] = networkData.value("MTU", 1500).toLongLong();
        } else
        {
            vif["MTU"] = 1500; // Default MTU (standard Ethernet)
        }
    } else
    {
        vif["MTU"] = 1500; // Default MTU
    }

    // Set other_config as empty map (required by XenServer API)
    // C# VIF class has default: new Dictionary<string, string>() {} (VIF.cs line 1241)
    vif["other_config"] = QVariantMap();

    // QoS settings - MUST set both qos_algorithm_type and qos_algorithm_params
    // C# VIFDialog.cs NewVif() method (lines 209-223)
    if (this->ui->checkBoxQoS->isChecked())
    {
        vif["qos_algorithm_type"] = "ratelimit";

        QVariantMap qosParams;
        qosParams["kbps"] = QString::number(this->ui->spinBoxQoS->value());
        vif["qos_algorithm_params"] = qosParams;
    } else
    {
        // Always set qos_algorithm_type to empty string when QoS disabled
        // C# VIF class default: private string _qos_algorithm_type = ""; (VIF.cs line 1337)
        vif["qos_algorithm_type"] = "";

        // Preserve the params even if QoS is disabled (C# behavior)
        if (this->ui->spinBoxQoS->value() > 0)
        {
            QVariantMap qosParams;
            qosParams["kbps"] = QString::number(this->ui->spinBoxQoS->value());
            vif["qos_algorithm_params"] = qosParams;
        } else
        {
            // Set empty params map if no value (C# VIF class default)
            vif["qos_algorithm_params"] = QVariantMap();
        }
    }

    return vif;
}

bool VIFDialog::hasChanges() const
{
    // C#: ChangesHaveBeenMade property

    if (!this->m_isEditMode)
        return true;

    if (this->m_existingVif.isEmpty())
        return true;

    // Check network
    if (this->m_existingVif.value("network").toString() != this->getSelectedNetworkRef())
        return true;

    // Check MAC
    if (this->m_existingVif.value("MAC").toString() != this->getSelectedMAC())
        return true;

    // Check device
    if (this->m_existingVif.value("device").toInt() != this->m_deviceId)
        return true;

    // Check QoS
    QString existingQoS = this->m_existingVif.value("qos_algorithm_type").toString();
    if (existingQoS == "ratelimit")
    {
        if (!this->ui->checkBoxQoS->isChecked())
            return true;

        QVariantMap existingParams = this->m_existingVif.value("qos_algorithm_params").toMap();
        QString existingKbps = existingParams.value("kbps").toString();
        QString currentKbps = QString::number(this->ui->spinBoxQoS->value());

        if (existingKbps != currentKbps)
            return true;
    } else if (existingQoS.isEmpty())
    {
        if (this->ui->checkBoxQoS->isChecked())
            return true;
    }

    return false;
}

QString VIFDialog::getSelectedNetworkRef() const
{
    int index = this->ui->comboBoxNetwork->currentIndex();
    if (index < 0)
        return QString();

    return this->ui->comboBoxNetwork->itemData(index).toString();
}

QString VIFDialog::getSelectedMAC() const
{
    // C#: SelectedMac property
    if (this->ui->radioButtonAutogenerate->isChecked())
        return QString(); // Empty string means autogenerate

    return this->ui->lineEditMAC->text();
}

bool VIFDialog::isValidNetwork(QString* error) const
{
    // C#: IsValidNetwork()
    QString networkRef = this->getSelectedNetworkRef();

    if (networkRef.isEmpty())
    {
        if (error)
            *error = this->tr("Please select a network");
        return false;
    }

    return true;
}

bool VIFDialog::isValidMAC(QString* error) const
{
    // C#: IsValidMAc()

    if (this->ui->radioButtonAutogenerate->isChecked())
        return true;

    QString mac = this->ui->lineEditMAC->text().trimmed();

    if (mac.isEmpty())
    {
        if (error)
            *error = this->tr("Please enter a MAC address or select autogenerate");
        return false;
    }

    // Validate MAC address format: aa:bb:cc:dd:ee:ff or aabbccddeeff
    // C#: Helpers.IsValidMAC()
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$|^([0-9A-Fa-f]{12})$");
    if (!macRegex.match(mac).hasMatch())
    {
        if (error)
            *error = this->tr("Invalid MAC address format. Use format: aa:bb:cc:dd:ee:ff");
        return false;
    }

    return true;
}

bool VIFDialog::isValidQoS(QString* error) const
{
    // C#: IsValidQoSLimit()

    if (!this->ui->checkBoxQoS->isChecked())
        return true;

    int value = this->ui->spinBoxQoS->value();
    if (value <= 0)
    {
        if (error)
            *error = this->tr("Please enter a valid QoS limit (must be greater than 0)");
        return false;
    }

    return true;
}

bool VIFDialog::isDuplicateMAC(const QString& mac) const
{
    // C#: Check for duplicate MAC in VIFDialog_FormClosing

    if (mac.isEmpty())
        return false; // Autogenerated MACs won't conflict

    if (!this->m_connection || !this->m_connection->GetCache())
        return false;

    QString normalizedMAC = mac.toLower().remove(':').remove('-');

    // Check all VIFs in the cache
    QStringList vifRefs = this->m_connection->GetCache()->GetAllRefs("VIF");
    for (const QString& vifRef : vifRefs)
    {
        // Skip the VIF we're editing
        if (this->m_isEditMode && vifRef == this->m_vifRef)
            continue;

        QVariantMap vifData = this->m_connection->GetCache()->ResolveObjectData("VIF", vifRef);
        QString existingMAC = vifData.value("MAC").toString();
        QString normalizedExisting = existingMAC.toLower().remove(':').remove('-');

        if (normalizedMAC == normalizedExisting)
        {
            // Check if the VM is a real VM (not a template)
            QString vmRef = vifData.value("VM").toString();
            QVariantMap vmData = this->m_connection->GetCache()->ResolveObjectData("vm", vmRef);
            bool isTemplate = vmData.value("is_a_template", false).toBool();
            bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();

            if (!isTemplate && !isSnapshot)
                return true;
        }
    }

    return false;
}

void VIFDialog::validateInput()
{
    // C#: ValidateInput()

    QString error;
    bool valid = true;

    if (!this->isValidNetwork(&error))
    {
        valid = false;
    } else if (!this->isValidMAC(&error))
    {
        valid = false;
    } else if (!this->isValidQoS(&error))
    {
        valid = false;
    }

    // Update OK button and error display
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);

    if (!valid && !error.isEmpty())
    {
        this->ui->errorWidget->setVisible(true);
        this->ui->labelError->setText(error);
    } else
    {
        this->ui->errorWidget->setVisible(false);
    }
}

void VIFDialog::onNetworkChanged()
{
    this->validateInput();
}

void VIFDialog::onMACRadioChanged()
{
    // Enable/disable MAC text field based on radio selection
    bool manualMode = this->ui->radioButtonManual->isChecked();
    this->ui->lineEditMAC->setEnabled(manualMode);

    if (manualMode)
        this->ui->lineEditMAC->setFocus();

    this->validateInput();
}

void VIFDialog::onMACTextChanged()
{
    // Auto-select manual radio when typing
    if (!this->ui->lineEditMAC->text().isEmpty())
        this->ui->radioButtonManual->setChecked(true);

    this->validateInput();
}

void VIFDialog::onQoSCheckboxChanged()
{
    // Enable/disable QoS fields based on checkbox
    bool enabled = this->ui->checkBoxQoS->isChecked();
    this->ui->labelQoS->setEnabled(enabled);
    this->ui->spinBoxQoS->setEnabled(enabled);

    this->validateInput();
}

void VIFDialog::onQoSValueChanged()
{
    // Auto-check QoS checkbox when changing value
    if (this->ui->spinBoxQoS->value() > 0 && !this->ui->checkBoxQoS->isChecked())
        this->ui->checkBoxQoS->setChecked(true);

    this->validateInput();
}
