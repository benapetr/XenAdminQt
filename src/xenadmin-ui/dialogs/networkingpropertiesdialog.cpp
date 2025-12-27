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

#include "networkingpropertiesdialog.h"
#include "ui_networkingpropertiesdialog.h"
#include "xen/network/connection.h"
#include "xen/session.h"
#include "xen/xenapi/xenapi_PIF.h"
#include "xencache.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

NetworkingPropertiesDialog::NetworkingPropertiesDialog(XenConnection* connection, const QString& pifRef, QWidget* parent)
    : QDialog(parent), ui(new Ui::NetworkingPropertiesDialog), m_connection(connection), m_pifRef(pifRef)
{
    ui->setupUi(this);

    // Set up IP address validators
    QRegularExpression ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    QRegularExpressionValidator* ipValidator = new QRegularExpressionValidator(ipRegex, this);

    ui->lineEditIPAddress->setValidator(ipValidator);
    ui->lineEditSubnetMask->setValidator(ipValidator);
    ui->lineEditGateway->setValidator(ipValidator);
    ui->lineEditPrimaryDNS->setValidator(ipValidator);
    ui->lineEditSecondaryDNS->setValidator(ipValidator);

    // Connect signals
    connect(ui->radioButtonDHCP, &QRadioButton::toggled, this, &NetworkingPropertiesDialog::onIPModeChanged);
    connect(ui->radioButtonStatic, &QRadioButton::toggled, this, &NetworkingPropertiesDialog::onIPModeChanged);

    connect(ui->lineEditIPAddress, &QLineEdit::textChanged, this, &NetworkingPropertiesDialog::onInputChanged);
    connect(ui->lineEditSubnetMask, &QLineEdit::textChanged, this, &NetworkingPropertiesDialog::onInputChanged);
    connect(ui->lineEditGateway, &QLineEdit::textChanged, this, &NetworkingPropertiesDialog::onInputChanged);
    connect(ui->lineEditPrimaryDNS, &QLineEdit::textChanged, this, &NetworkingPropertiesDialog::onInputChanged);
    connect(ui->lineEditSecondaryDNS, &QLineEdit::textChanged, this, &NetworkingPropertiesDialog::onInputChanged);

    // Load PIF data
    loadPIFData();

    // Initial validation
    validateAndUpdateUI();
}

NetworkingPropertiesDialog::~NetworkingPropertiesDialog()
{
    delete ui;
}

void NetworkingPropertiesDialog::loadPIFData()
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    // Get PIF data from cache
    QVariant pifDataVar = this->m_connection->GetCache()->ResolveObjectData("pif", this->m_pifRef);
    if (pifDataVar.isNull())
    {
        QMessageBox::warning(this, "Error", "Failed to load network interface data.");
        return;
    }

    this->m_pifData = pifDataVar.value<QVariantMap>();

    // Display interface information
    QString device = this->m_pifData.value("device").toString();
    QString mac = this->m_pifData.value("MAC").toString();
    QString networkRef = this->m_pifData.value("network").toString();

    ui->labelDeviceValue->setText(device);
    ui->labelMACValue->setText(mac);

    // Get network name
    QVariant networkDataVar = this->m_connection->GetCache()->ResolveObjectData("network", networkRef);
    if (!networkDataVar.isNull())
    {
        QVariantMap networkData = networkDataVar.value<QVariantMap>();
        QString networkName = networkData.value("name_label").toString();
        ui->labelNetworkValue->setText(networkName);
    }

    // Load IP configuration
    QString ipConfigMode = this->m_pifData.value("ip_configuration_mode").toString();
    bool isDHCP = (ipConfigMode == "DHCP" || ipConfigMode == "dhcp");

    if (isDHCP)
    {
        ui->radioButtonDHCP->setChecked(true);
    } else
    {
        ui->radioButtonStatic->setChecked(true);

        // Load static IP settings
        QString ip = this->m_pifData.value("IP").toString();
        QString netmask = this->m_pifData.value("netmask").toString();
        QString gateway = this->m_pifData.value("gateway").toString();

        ui->lineEditIPAddress->setText(ip);
        ui->lineEditSubnetMask->setText(netmask);
        ui->lineEditGateway->setText(gateway);
    }

    // Load DNS settings
    QString dnsString = this->m_pifData.value("DNS").toString();
    QStringList dnsList = dnsString.split(',', Qt::SkipEmptyParts);

    if (dnsList.size() > 0)
        ui->lineEditPrimaryDNS->setText(dnsList[0].trimmed());
    if (dnsList.size() > 1)
        ui->lineEditSecondaryDNS->setText(dnsList[1].trimmed());
}

void NetworkingPropertiesDialog::onIPModeChanged()
{
    bool staticMode = ui->radioButtonStatic->isChecked();
    ui->widgetStaticSettings->setEnabled(staticMode);
    validateAndUpdateUI();
}

void NetworkingPropertiesDialog::onInputChanged()
{
    validateAndUpdateUI();
}

void NetworkingPropertiesDialog::validateAndUpdateUI()
{
    bool isValid = true;

    if (ui->radioButtonStatic->isChecked())
    {
        // Validate IP address (required)
        QString ip = ui->lineEditIPAddress->text();
        if (!validateIP(ip, false))
        {
            isValid = false;
        }

        // Validate subnet mask (required)
        QString mask = ui->lineEditSubnetMask->text();
        if (!validateSubnetMask(mask))
        {
            isValid = false;
        }

        // Validate gateway (optional)
        QString gateway = ui->lineEditGateway->text();
        if (!gateway.isEmpty() && !validateIP(gateway, true))
        {
            isValid = false;
        }
    }

    // Validate DNS (optional)
    QString primaryDNS = ui->lineEditPrimaryDNS->text();
    if (!primaryDNS.isEmpty() && !validateIP(primaryDNS, true))
    {
        isValid = false;
    }

    QString secondaryDNS = ui->lineEditSecondaryDNS->text();
    if (!secondaryDNS.isEmpty() && !validateIP(secondaryDNS, true))
    {
        isValid = false;
    }

    // Enable/disable OK button
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isValid);
}

bool NetworkingPropertiesDialog::validateIP(const QString& ip, bool allowEmpty)
{
    if (ip.isEmpty())
        return allowEmpty;

    // IP address regex: 0-255.0-255.0-255.0-255
    QRegularExpression ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.match(ip).hasMatch();
}

bool NetworkingPropertiesDialog::validateSubnetMask(const QString& mask)
{
    if (mask.isEmpty())
        return false;

    if (!validateIP(mask, false))
        return false;

    // Check if it's a valid subnet mask (must be contiguous 1s followed by 0s)
    QStringList octets = mask.split('.');
    if (octets.size() != 4)
        return false;

    quint32 maskValue = 0;
    for (const QString& octet : octets)
    {
        maskValue = (maskValue << 8) | octet.toUInt();
    }

    // Check if mask is contiguous (no 0s between 1s)
    // Valid masks: ~maskValue + 1 should be a power of 2
    quint32 inverted = ~maskValue;
    return (inverted & (inverted + 1)) == 0;
}

void NetworkingPropertiesDialog::applyChanges()
{
    if (!this->m_connection)
        return;

    XenAPI::Session* session = this->m_connection->GetSession();
    if (!session || !session->IsLoggedIn())
    {
        QMessageBox::critical(this, "Error",
                              "No active session. Please reconnect and try again.");
        return;
    }

    bool success = false;

    if (ui->radioButtonDHCP->isChecked())
    {
        // Configure for DHCP
        try
        {
            XenAPI::PIF::reconfigure_ip(session, this->m_pifRef, "DHCP", "", "", "", "");
            success = true;
        }
        catch (const std::exception& ex)
        {
            qWarning() << "Failed to configure PIF DHCP:" << ex.what();
        }
    } else
    {
        // Configure for static IP
        QString ip = ui->lineEditIPAddress->text();
        QString netmask = ui->lineEditSubnetMask->text();
        QString gateway = ui->lineEditGateway->text();

        // Combine DNS servers
        QStringList dnsList;
        QString primaryDNS = ui->lineEditPrimaryDNS->text();
        QString secondaryDNS = ui->lineEditSecondaryDNS->text();

        if (!primaryDNS.isEmpty())
            dnsList.append(primaryDNS);
        if (!secondaryDNS.isEmpty())
            dnsList.append(secondaryDNS);

        QString dns = dnsList.join(",");

        try
        {
            XenAPI::PIF::reconfigure_ip(session, this->m_pifRef, "Static", ip, netmask, gateway, dns);
            success = true;
        }
        catch (const std::exception& ex)
        {
            qWarning() << "Failed to configure PIF static IP:" << ex.what();
        }
    }

    if (!success)
    {
        QMessageBox::critical(this, "Error",
                              "Failed to configure network interface. Please check the server logs.");
    }
}

void NetworkingPropertiesDialog::accept()
{
    applyChanges();
    QDialog::accept();
}
