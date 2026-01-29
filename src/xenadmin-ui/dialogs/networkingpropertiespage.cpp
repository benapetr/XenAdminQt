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

#include "networkingpropertiespage.h"
#include "ui_networkingpropertiespage.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/pif.h"
#include <algorithm>
#include <QSignalBlocker>
#include <QRegularExpression>

NetworkingPropertiesPage::NetworkingPropertiesPage(PageType type, QWidget* parent)
    : QWidget(parent),
      ui(new Ui::NetworkingPropertiesPage),
      m_type(type)
{
    this->ui->setupUi(this);

    connect(this->ui->dhcpRadioButton, &QRadioButton::toggled, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->staticRadioButton, &QRadioButton::toggled, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->purposeTextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onPurposeChanged);
    connect(this->ui->networkComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkingPropertiesPage::onNetworkComboChanged);
    connect(this->ui->ipAddressTextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onIPAddressChanged);
    connect(this->ui->subnetTextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->gatewayTextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->preferredDnsTextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->alternateDns1TextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->alternateDns2TextBox, &QLineEdit::textChanged, this, &NetworkingPropertiesPage::onSomethingChanged);
    connect(this->ui->deleteButton, &QPushButton::clicked, this, &NetworkingPropertiesPage::DeleteButtonClicked);

    this->refreshButtons();
}

NetworkingPropertiesPage::~NetworkingPropertiesPage()
{
    delete this->ui;
}

NetworkingPropertiesPage::PageType NetworkingPropertiesPage::Type() const
{
    return this->m_type;
}

QIcon NetworkingPropertiesPage::TabIcon() const
{
    switch (this->m_type)
    {
        case PageType::Secondary:
            return QIcon(":/icons/network-16.png");
        case PageType::Primary:
        case PageType::PrimaryWithHA:
        default:
            return QIcon(":/icons/management-interface-16.png");
    }
}

QString NetworkingPropertiesPage::TabText() const
{
    return this->m_purpose;
}

QString NetworkingPropertiesPage::SubText() const
{
    if (this->ui->networkComboBox->currentIndex() < 0)
        return tr("None");

    QString networkName = this->ui->networkComboBox->currentText();
    QString mode = this->ui->dhcpRadioButton->isChecked() ? tr("DHCP") : tr("Static");
    return tr("%1 - %2").arg(networkName, mode);
}

void NetworkingPropertiesPage::SetPool(bool pool)
{
    this->m_pool = pool;
    this->refreshButtons();
}

void NetworkingPropertiesPage::SetHostCount(int hostCount)
{
    this->m_hostCount = hostCount;
    this->refreshButtons();
}

void NetworkingPropertiesPage::SetPurpose(const QString& purpose)
{
    this->m_purpose = purpose;
    this->ui->purposeTextBox->setText(purpose);
    this->refreshButtons();
}

QString NetworkingPropertiesPage::Purpose() const
{
    return this->ui->purposeTextBox->text();
}

bool NetworkingPropertiesPage::NameValid() const
{
    return this->m_nameValid;
}

void NetworkingPropertiesPage::SetPif(const QSharedPointer<PIF>& pif)
{
    this->m_pif = pif;
    this->m_clusteringEnabled = pif ? pif->IsUsedByClustering() : false;
}

QSharedPointer<PIF> NetworkingPropertiesPage::Pif() const
{
    return this->m_pif;
}

void NetworkingPropertiesPage::LoadFromPif(const QSharedPointer<PIF>& pif)
{
    if (!pif || !pif->IsValid())
        return;

    const QString ipMode = pif->IpConfigurationMode();
    const bool isDhcp = ipMode.compare("DHCP", Qt::CaseInsensitive) == 0;
    this->ui->dhcpRadioButton->setChecked(isDhcp);
    this->ui->staticRadioButton->setChecked(!isDhcp);

    this->ui->ipAddressTextBox->setText(pif->IP());
    this->ui->subnetTextBox->setText(pif->Netmask());
    this->ui->gatewayTextBox->setText(pif->Gateway());

    const QStringList dnsList = pif->DNS().split(',', Qt::SkipEmptyParts);
    this->ui->preferredDnsTextBox->setText(dnsList.value(0).trimmed());
    this->ui->alternateDns1TextBox->setText(dnsList.value(1).trimmed());
    this->ui->alternateDns2TextBox->setText(dnsList.value(2).trimmed());

    this->refreshButtons();
}

void NetworkingPropertiesPage::SetSelectedNetworkRef(const QString& ref)
{
    const int idx = this->ui->networkComboBox->findData(ref);
    if (idx >= 0)
        this->ui->networkComboBox->setCurrentIndex(idx);
}

void NetworkingPropertiesPage::SetDefaultsForNew()
{
    this->ui->dhcpRadioButton->setChecked(true);
    this->ui->staticRadioButton->setChecked(false);
    this->ui->ipAddressTextBox->clear();
    this->ui->subnetTextBox->clear();
    this->ui->gatewayTextBox->clear();
    this->ui->preferredDnsTextBox->clear();
    this->ui->alternateDns1TextBox->clear();
    this->ui->alternateDns2TextBox->clear();
    this->refreshButtons();
}

void NetworkingPropertiesPage::RefreshNetworkComboBox(
    const QMap<QString, QList<NetworkingPropertiesPage*>>& inUseMap,
    const QString& managementNetworkRef,
    bool allowManagementOnVlan,
    const QList<QSharedPointer<Network>>& networks)
{
    this->m_inUseMap = inUseMap;
    this->m_managementNetworkRef = managementNetworkRef;

    QString selectedRef = this->SelectedNetworkRef();

    this->m_squelchNetworkComboChange = true;
    QSignalBlocker blocker(this->ui->networkComboBox);
    this->ui->networkComboBox->clear();

    QList<QSharedPointer<Network>> filtered = networks;
    if (!allowManagementOnVlan && (this->m_type == PageType::Primary || this->m_type == PageType::PrimaryWithHA))
    {
        filtered.erase(std::remove_if(filtered.begin(), filtered.end(), [](const QSharedPointer<Network>& network) {
            if (!network)
                return true;
            for (const QSharedPointer<PIF>& pif : network->GetPIFs())
            {
                if (pif && pif->IsVLAN())
                    return true;
            }
            return false;
        }), filtered.end());
    }

    filtered.erase(std::remove_if(filtered.begin(), filtered.end(), [](const QSharedPointer<Network>& network) {
        if (!network)
            return true;
        for (const QSharedPointer<PIF>& pif : network->GetPIFs())
        {
            if (pif && pif->IsSriovLogicalPIF())
                return true;
        }
        return false;
    }), filtered.end());

    std::sort(filtered.begin(), filtered.end(), [](const QSharedPointer<Network>& a, const QSharedPointer<Network>& b) {
        if (!a || !b)
            return a != nullptr;
        return a->GetName().compare(b->GetName(), Qt::CaseInsensitive) < 0;
    });

    for (const QSharedPointer<Network>& network : filtered)
    {
        if (!network)
            continue;

        const QString ref = network->OpaqueRef();
        QString label = network->GetName();

        QString otherPurpose = this->findOtherPurpose(ref);
        if (!otherPurpose.isEmpty() && otherPurpose != this->m_purpose)
            label = tr("%1 (in use by %2)").arg(label, otherPurpose);

        this->ui->networkComboBox->addItem(label, ref);
    }

    int idx = this->ui->networkComboBox->findData(selectedRef);
    if (idx >= 0)
        this->ui->networkComboBox->setCurrentIndex(idx);
    this->m_squelchNetworkComboChange = false;

    if (!this->m_triggeringChange)
        this->refreshButtons();
}

void NetworkingPropertiesPage::SelectFirstUnusedNetwork(
    const QList<QSharedPointer<Network>>& networks,
    const QMap<QString, QList<NetworkingPropertiesPage*>>& inUseMap)
{
    QList<QSharedPointer<Network>> sorted = networks;
    std::sort(sorted.begin(), sorted.end(), [](const QSharedPointer<Network>& a, const QSharedPointer<Network>& b) {
        if (!a || !b)
            return a != nullptr;
        return a->GetName().compare(b->GetName(), Qt::CaseInsensitive) < 0;
    });

    for (const QSharedPointer<Network>& network : sorted)
    {
        if (!network)
            continue;

        const QString ref = network->OpaqueRef();
        if (!inUseMap.contains(ref) || inUseMap.value(ref).isEmpty())
        {
            int idx = this->ui->networkComboBox->findData(ref);
            if (idx >= 0)
            {
                this->ui->networkComboBox->setCurrentIndex(idx);
                return;
            }
        }
    }
}

void NetworkingPropertiesPage::SelectName()
{
    this->ui->purposeTextBox->setFocus();
    this->ui->purposeTextBox->selectAll();
}

bool NetworkingPropertiesPage::IsValid() const
{
    return this->m_valid;
}

bool NetworkingPropertiesPage::ClusteringEnabled() const
{
    return this->m_clusteringEnabled;
}

QString NetworkingPropertiesPage::SelectedNetworkRef() const
{
    return this->ui->networkComboBox->currentData().toString();
}

QString NetworkingPropertiesPage::IPAddress() const
{
    return this->ui->ipAddressTextBox->text();
}

QString NetworkingPropertiesPage::Netmask() const
{
    return this->ui->subnetTextBox->text();
}

QString NetworkingPropertiesPage::Gateway() const
{
    return this->ui->gatewayTextBox->text();
}

QString NetworkingPropertiesPage::PreferredDNS() const
{
    return this->ui->preferredDnsTextBox->text();
}

QString NetworkingPropertiesPage::AlternateDNS1() const
{
    return this->ui->alternateDns1TextBox->text();
}

QString NetworkingPropertiesPage::AlternateDNS2() const
{
    return this->ui->alternateDns2TextBox->text();
}

bool NetworkingPropertiesPage::IsDhcp() const
{
    return this->ui->dhcpRadioButton->isChecked();
}

void NetworkingPropertiesPage::onSomethingChanged()
{
    this->refreshButtons();
}

void NetworkingPropertiesPage::onNetworkComboChanged()
{
    if (this->m_squelchNetworkComboChange)
        return;

    this->refreshButtons();

    this->m_triggeringChange = true;
    emit NetworkComboBoxChanged();
    this->m_triggeringChange = false;
}

void NetworkingPropertiesPage::onPurposeChanged()
{
    this->m_nameValid = !this->ui->purposeTextBox->text().trimmed().isEmpty();
    this->m_purpose = this->ui->purposeTextBox->text();
    emit ValidChanged();
    this->refreshButtons();
}

void NetworkingPropertiesPage::onIPAddressChanged()
{
    this->refreshButtons();
    if (this->m_pool && this->isValidIPAddress(this->ui->ipAddressTextBox->text()))
    {
        const QStringList parts = this->ui->ipAddressTextBox->text().split('.');
        if (parts.size() == 4)
        {
            const int last = parts[3].toInt();
            this->ui->rangeEndLabel->setText(tr("to %1.%2.%3.%4").arg(parts[0], parts[1], parts[2]).arg(last + this->m_hostCount - 1));
        }
    }
}

void NetworkingPropertiesPage::refreshButtons()
{
    this->ui->infoPanel->setVisible(false);
    const QString selectedRef = this->SelectedNetworkRef();
    const QString otherPurpose = this->findOtherPurpose(selectedRef);

    this->m_inUseWarning.clear();
    if (!otherPurpose.isEmpty() && otherPurpose != this->m_purpose)
    {
        if (this->m_type == PageType::Secondary && selectedRef == this->m_managementNetworkRef)
            this->m_inUseWarning = tr("The network %1 is already used as the management network.").arg(this->ui->networkComboBox->currentText());
        else
            this->m_inUseWarning = tr("The network %1 is already in use by %2.").arg(this->ui->networkComboBox->currentText(), otherPurpose);
    }

    const bool isSecondary = (this->m_type == PageType::Secondary);
    this->ui->purposeLabel->setVisible(isSecondary);
    this->ui->purposeTextBox->setVisible(isSecondary);
    this->ui->deleteButton->setVisible(isSecondary);
    this->ui->panelHaWarning->setVisible(this->m_type == PageType::PrimaryWithHA);

    this->setDnsControlsVisible(!isSecondary);

    ui->panelInUseWarning->setVisible(!m_inUseWarning.isEmpty());
    ui->inUseWarningText->setText(m_inUseWarning);

    ui->ipSettingsLabel->setText(isSecondary ? tr("IP settings") : tr("IP and DNS settings"));
    ui->ipAddressLabel->setText(m_pool ? tr("IP address range start") : tr("IP address"));
    ui->rangeEndLabel->setVisible(m_pool);

    ui->staticSettingsWidget->setEnabled(ui->staticRadioButton->isChecked());

    const bool ipOk = ui->dhcpRadioButton->isChecked() ||
        (isValidIPAddress(IPAddress()) && isValidNetmask(Netmask()) && isOptionalIPAddress(Gateway()));
    const bool dnsOk = isSecondary || (isOptionalIPAddress(PreferredDNS()) &&
        isOptionalIPAddress(AlternateDNS1()) && isOptionalIPAddress(AlternateDNS2()));
    const bool networkOk = !selectedRef.isEmpty();

    bool valid = m_inUseWarning.isEmpty() && networkOk && ipOk && dnsOk;
    if (valid != m_valid)
    {
        m_valid = valid;
        emit ValidChanged();
    }

    if (this->m_type == PageType::PrimaryWithHA)
    {
        for (QObject* child : children())
        {
            QWidget* widget = qobject_cast<QWidget*>(child);
            if (widget)
                widget->setEnabled(false);
        }
        this->ui->panelHaWarning->setEnabled(true);
    }

    if (this->m_pif && this->m_clusteringEnabled)
    {
        this->disableControls(tr("Cannot change IP settings while clustering is enabled on this interface."));
    }
}

void NetworkingPropertiesPage::setDnsControlsVisible(bool visible)
{
    this->ui->preferredDnsLabel->setVisible(visible);
    this->ui->preferredDnsTextBox->setVisible(visible);
    this->ui->alternateDns1Label->setVisible(visible);
    this->ui->alternateDns1TextBox->setVisible(visible);
    this->ui->alternateDns2Label->setVisible(visible);
    this->ui->alternateDns2TextBox->setVisible(visible);
}

QString NetworkingPropertiesPage::findOtherPurpose(const QString& networkRef) const
{
    if (!this->m_inUseMap.contains(networkRef))
        return QString();

    const QList<NetworkingPropertiesPage*>& pages = this->m_inUseMap.value(networkRef);
    for (NetworkingPropertiesPage* page : pages)
    {
        if (page && page != this)
            return page->Purpose();
    }
    return QString();
}

bool NetworkingPropertiesPage::isOptionalIPAddress(const QString& value) const
{
    return value.isEmpty() || this->isValidIPAddress(value);
}

bool NetworkingPropertiesPage::isValidIPAddress(const QString& value) const
{
    static const QRegularExpression ipRegex(
        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.match(value).hasMatch();
}

bool NetworkingPropertiesPage::isValidNetmask(const QString& value) const
{
    if (!this->isValidIPAddress(value))
        return false;

    const QStringList octets = value.split('.');
    if (octets.size() != 4)
        return false;

    quint32 maskValue = 0;
    for (const QString& octet : octets)
        maskValue = (maskValue << 8) | octet.toUInt();

    const quint32 inverted = ~maskValue;
    return (inverted & (inverted + 1)) == 0;
}

void NetworkingPropertiesPage::disableControls(const QString& message)
{
    this->ui->purposeLabel->setEnabled(false);
    this->ui->purposeTextBox->setEnabled(false);
    this->ui->networkLabel->setEnabled(false);
    this->ui->networkComboBox->setEnabled(false);
    this->ui->ipSettingsLabel->setEnabled(false);
    this->ui->dhcpRadioButton->setEnabled(false);
    this->ui->staticRadioButton->setEnabled(false);
    this->ui->staticSettingsWidget->setEnabled(false);
    this->ui->deleteButton->setEnabled(false);
    this->ui->infoPanel->setVisible(true);
    this->ui->infoLabel->setText(message);
}
