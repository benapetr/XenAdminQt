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
#include "networkingpropertiespage.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/actions/network/changenetworkingaction.h"
#include "xenlib/xen/actions/network/setsecondarymanagementpurposeaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xencache.h"
#include "../settingsmanager.h"
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QComboBox>
#include <algorithm>
#include <QSet>
#include <stdexcept>

NetworkingPropertiesDialog::NetworkingPropertiesDialog(QSharedPointer<Host> host, QSharedPointer<Pool> pool, QSharedPointer<PIF> selectedPif, QWidget* parent)
    : QDialog(parent), ui(new Ui::NetworkingPropertiesDialog), m_host(host), m_pool(pool), m_selectedPif(selectedPif)
{
    this->ui->setupUi(this);

    if (!this->m_host && this->m_pool)
        this->m_host = this->m_pool->GetMasterHost();

    connect(this->ui->addButton, &QPushButton::clicked, this, &NetworkingPropertiesDialog::onAddClicked);
    connect(this->ui->verticalTabs, &VerticalTabWidget::currentRowChanged, this, &NetworkingPropertiesDialog::onVerticalTabChanged);

    this->configure();
}

NetworkingPropertiesDialog::~NetworkingPropertiesDialog()
{
    delete this->ui;
}

void NetworkingPropertiesDialog::configure()
{
    if (!this->m_host || !this->m_host->IsValid())
        return;

    bool restrictManagementOnVlan = false;
    const QList<QSharedPointer<Host>> hosts = this->m_host->GetCache()->GetAll<Host>("host");
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && host->RestrictManagementOnVLAN())
        {
            restrictManagementOnVlan = true;
            break;
        }
    }
    this->m_allowManagementOnVlan = !restrictManagementOnVlan;

    const QString objectName = this->m_pool ? this->m_pool->GetName() : this->m_host->GetName();
    this->ui->blurbLabel->setText(this->m_pool
                                ? tr("Configure the IP addresses for pool %1.").arg(objectName)
                                : tr("Configure the IP addresses for host %1.").arg(objectName));

    this->m_shownPifs = this->getKnownPifs(false);
    this->m_allPifs = this->getKnownPifs(true);

    QSharedPointer<PIF> managementPif;
    for (const QSharedPointer<PIF>& pif : this->m_allPifs)
    {
        if (pif && pif->IsPrimaryManagementInterface())
        {
            managementPif = pif;
            break;
        }
    }

    if (!managementPif)
        return;

    const bool haEnabled = this->m_pool && this->m_pool->GetData().value("ha_enabled", false).toBool();
    auto* managementPage = new NetworkingPropertiesPage(haEnabled ? NetworkingPropertiesPage::PageType::PrimaryWithHA : NetworkingPropertiesPage::PageType::Primary);
    managementPage->SetPool(this->m_pool != nullptr);
    managementPage->SetHostCount(this->m_pool ? this->m_pool->GetHosts().count() : 1);
    managementPage->SetPurpose(tr("Management"));
    managementPage->SetPif(managementPif);
    managementPage->LoadFromPif(managementPif);
    this->addTabContents(managementPage);
    managementPage->setProperty("pifRef", managementPif->OpaqueRef());

    for (const QSharedPointer<PIF>& pif : this->m_shownPifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (pif->OpaqueRef() == managementPif->OpaqueRef())
            continue;
        if (!pif->IsSecondaryManagementInterface(true))
            continue;

        auto* secondaryPage = new NetworkingPropertiesPage(NetworkingPropertiesPage::PageType::Secondary);
        secondaryPage->SetPool(this->m_pool != nullptr);
        secondaryPage->SetHostCount(this->m_pool ? this->m_pool->GetHosts().count() : 1);
        secondaryPage->SetPurpose(this->purposeForPif(pif));
        secondaryPage->SetPif(pif);
        secondaryPage->LoadFromPif(pif);
        this->addTabContents(secondaryPage);
        secondaryPage->setProperty("pifRef", pif->OpaqueRef());
    }

    this->m_inUseMap = this->makeProposedInUseMap();
    this->refreshNetworkComboBoxes();

    for (NetworkingPropertiesPage* page : this->m_pages)
    {
        QSharedPointer<PIF> pif = page->Pif();
        if (pif)
        {
            QSharedPointer<Network> network = pif->GetNetwork();
            if (network)
            {
                page->SetSelectedNetworkRef(network->OpaqueRef());
            }
        } else
        {
            page->SelectFirstUnusedNetwork(this->m_networks, this->m_inUseMap);
        }
    }

    if (this->m_selectedPif)
    {
        for (int i = 0; i < this->m_pages.size(); ++i)
        {
            NetworkingPropertiesPage* page = this->m_pages[i];
            if (page && page->Pif() && page->Pif()->OpaqueRef() == this->m_selectedPif->OpaqueRef())
            {
                this->ui->verticalTabs->setCurrentRow(i);
                this->ui->contentPanel->setCurrentWidget(page);
                break;
            }
        }
    } else
    {
        this->ui->verticalTabs->setCurrentRow(0);
        if (!this->m_pages.isEmpty())
            this->ui->contentPanel->setCurrentWidget(this->m_pages.first());
    }

    this->refreshButtons();
}

void NetworkingPropertiesDialog::refreshButtons()
{
    bool allValid = true;
    bool allNamesValid = true;
    for (NetworkingPropertiesPage* page : this->m_pages)
    {
        if (!page)
            continue;
        allValid = allValid && page->IsValid();
        allNamesValid = allNamesValid && page->NameValid();
    }
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allValid && allNamesValid);
    this->ui->addButton->setEnabled(this->m_shownPifs.count() > this->m_pages.count());
}

void NetworkingPropertiesDialog::addTabContents(NetworkingPropertiesPage* page)
{
    if (!page)
        return;

    this->m_pages.append(page);
    this->ui->verticalTabs->AddTab(page->TabIcon(), page->TabText(), page->SubText(), page);
    this->ui->contentPanel->addWidget(page);

    connect(page, &NetworkingPropertiesPage::ValidChanged, this, &NetworkingPropertiesDialog::onPageValidChanged);
    connect(page, &NetworkingPropertiesPage::DeleteButtonClicked, this, &NetworkingPropertiesDialog::onPageDeleteClicked);
    connect(page, &NetworkingPropertiesPage::NetworkComboBoxChanged, this, &NetworkingPropertiesDialog::onNetworkComboChanged);
}

void NetworkingPropertiesDialog::removePage(NetworkingPropertiesPage* page)
{
    if (!page)
        return;

    int index = this->m_pages.indexOf(page);
    if (index < 0)
        return;

    this->m_pages.removeAt(index);
    this->ui->contentPanel->removeWidget(page);
    delete page;

    this->ui->verticalTabs->ClearTabs();
    for (NetworkingPropertiesPage* remaining : this->m_pages)
    {
        this->ui->verticalTabs->AddTab(remaining->TabIcon(), remaining->TabText(), remaining->SubText(), remaining);
    }

    const int lastIndex = this->m_pages.isEmpty() ? -1 : static_cast<int>(this->m_pages.size()) - 1;
    const int newIndex = lastIndex < 0 ? -1 : std::min(index, lastIndex);
    this->ui->verticalTabs->setCurrentRow(newIndex);
    if (newIndex >= 0 && newIndex < this->m_pages.size())
        this->ui->contentPanel->setCurrentWidget(this->m_pages[newIndex]);
    this->refreshNetworkComboBoxes();
    this->refreshButtons();
}

void NetworkingPropertiesDialog::refreshNetworkComboBoxes()
{
    this->m_inUseMap = this->makeProposedInUseMap();
    const QString managementRef = this->managementNetworkRef();

    for (NetworkingPropertiesPage* page : this->m_pages)
    {
        if (!page)
            continue;
        page->RefreshNetworkComboBox(this->m_inUseMap, managementRef, this->m_allowManagementOnVlan, this->m_networks);
        this->ui->verticalTabs->UpdateTabText(page, page->TabText());
        this->ui->verticalTabs->UpdateTabSubText(page, page->SubText());
    }
}

QMap<QString, QList<NetworkingPropertiesPage*>> NetworkingPropertiesDialog::makeProposedInUseMap() const
{
    QMap<QString, QList<NetworkingPropertiesPage*>> map;

    for (const QSharedPointer<Network>& network : this->m_networks)
    {
        if (!network)
            continue;
        bool nonTunnelFound = false;
        for (const QSharedPointer<PIF>& pif : network->GetPIFs())
        {
            if (pif && !pif->IsTunnelAccessPIF())
            {
                nonTunnelFound = true;
                break;
            }
        }
        if (!nonTunnelFound)
            continue;
        QSharedPointer<PIF> pif = this->findPifForHost(network);
        if (pif && pif->IsInUseBondMember())
            continue;
        map[network->OpaqueRef()] = {};
    }

    for (NetworkingPropertiesPage* page : this->m_pages)
    {
        if (!page)
            continue;
        QString ref = page->SelectedNetworkRef();
        if (ref.isEmpty())
        {
            QSharedPointer<PIF> pif = page->Pif();
            if (pif)
                ref = pif->GetNetworkRef();
        }
        if (!ref.isEmpty())
            map[ref].append(page);
    }
    return map;
}

QList<QSharedPointer<PIF>> NetworkingPropertiesDialog::getKnownPifs(bool includeInvisible)
{
    QList<QSharedPointer<PIF>> result;
    XenConnection* connection = this->m_host ? this->m_host->GetConnection() : nullptr;
    if (!connection || !connection->GetCache())
        return result;

    const bool showHidden = SettingsManager::instance().getShowHiddenObjects();
    QList<QSharedPointer<Network>> networks = connection->GetCache()->GetAll<Network>("network");
    QList<QSharedPointer<Network>> filtered;
    for (const QSharedPointer<Network>& network : networks)
    {
        if (!network || !network->Show(includeInvisible || showHidden))
            continue;
        filtered.append(network);
    }
    if (this->m_networks.isEmpty())
        this->m_networks = filtered;

    for (const QSharedPointer<Network>& network : filtered)
    {
        QSharedPointer<PIF> pif = this->findPifForHost(network);
        if (pif && pif->IsInUseBondMember())
            continue;
        if (pif)
            result.append(pif);
    }
    return result;
}

QSharedPointer<PIF> NetworkingPropertiesDialog::findPifForHost(const QSharedPointer<Network>& network) const
{
    if (!network || !this->m_host)
        return QSharedPointer<PIF>();

    for (const QSharedPointer<PIF>& pif : network->GetPIFs())
    {
        if (!pif || !pif->IsValid())
            continue;
        QSharedPointer<Host> host = pif->GetHost();
        if (host && host->IsValid() && host->OpaqueRef() == this->m_host->OpaqueRef())
            return pif;
    }
    return QSharedPointer<PIF>();
}

QString NetworkingPropertiesDialog::managementNetworkRef() const
{
    if (this->m_pages.isEmpty())
        return QString();
    NetworkingPropertiesPage* page = this->m_pages.first();
    return page ? page->SelectedNetworkRef() : QString();
}

QString NetworkingPropertiesDialog::purposeForPif(const QSharedPointer<PIF>& pif) const
{
    if (!pif)
        return tr("Unknown");
    const QVariantMap otherConfig = pif->GetOtherConfig();
    const QString purpose = otherConfig.value("management_purpose").toString();
    return purpose.isEmpty() ? tr("Unknown") : purpose;
}

QString NetworkingPropertiesDialog::makeAuxTabName() const
{
    int index = 1;
    QSet<QString> existing;
    for (NetworkingPropertiesPage* page : this->m_pages)
    {
        if (page)
            existing.insert(page->TabText());
    }

    QString candidate;
    do
    {
        candidate = tr("Auxiliary %1").arg(index);
        ++index;
    } while (existing.contains(candidate));

    return candidate;
}

void NetworkingPropertiesDialog::onAddClicked()
{
    auto* page = new NetworkingPropertiesPage(NetworkingPropertiesPage::PageType::Secondary);
    page->SetPool(this->m_pool != nullptr);
    page->SetHostCount(this->m_pool ? this->m_pool->GetHosts().count() : 1);
    page->SetPurpose(this->makeAuxTabName());
    this->addTabContents(page);
    this->refreshNetworkComboBoxes();
    page->SelectFirstUnusedNetwork(this->m_networks, this->m_inUseMap);
    page->SetDefaultsForNew();
    page->SelectName();
    this->ui->verticalTabs->setCurrentRow(this->m_pages.size() - 1);
    this->ui->contentPanel->setCurrentWidget(page);
}

void NetworkingPropertiesDialog::onPageValidChanged()
{
    if (auto* page = qobject_cast<NetworkingPropertiesPage*>(sender()))
    {
        this->ui->verticalTabs->UpdateTabText(page, page->TabText());
        this->ui->verticalTabs->UpdateTabSubText(page, page->SubText());
    }
    this->refreshButtons();
}

void NetworkingPropertiesDialog::onPageDeleteClicked()
{
    NetworkingPropertiesPage* page = qobject_cast<NetworkingPropertiesPage*>(sender());
    if (!page)
        return;
    this->removePage(page);
}

void NetworkingPropertiesDialog::onNetworkComboChanged()
{
    this->refreshNetworkComboBoxes();
    this->refreshButtons();
}

void NetworkingPropertiesDialog::onVerticalTabChanged(int index)
{
    if (index < 0 || index >= this->m_pages.size())
        return;
    this->ui->contentPanel->setCurrentWidget(this->m_pages[index]);
}

bool NetworkingPropertiesDialog::ipAddressSettingsChanged(const QSharedPointer<PIF>& pif1,
                                                          const QSharedPointer<PIF>& pif2) const
{
    if (!pif1 || !pif2)
        return false;
    return pif1->IP() != pif2->IP() ||
           pif1->Netmask() != pif2->Netmask() ||
           pif1->Gateway() != pif2->Gateway();
}

bool NetworkingPropertiesDialog::managementInterfaceIpChanged(const QSharedPointer<PIF>& oldManagement,
                                                              const QSharedPointer<PIF>& newManagement) const
{
    if (!oldManagement || !newManagement)
        return false;

    const QString oldMode = oldManagement->IpConfigurationMode();
    const QString newMode = newManagement->IpConfigurationMode();

    if (oldMode.compare("DHCP", Qt::CaseInsensitive) == 0)
    {
        if (newMode.compare("DHCP", Qt::CaseInsensitive) == 0)
            return oldManagement->GetData() != newManagement->GetData();
        if (newMode.compare("Static", Qt::CaseInsensitive) == 0)
            return ipAddressSettingsChanged(oldManagement, newManagement);
    }
    else if (oldMode.compare("Static", Qt::CaseInsensitive) == 0)
    {
        if (newMode.compare("Static", Qt::CaseInsensitive) == 0)
            return ipAddressSettingsChanged(oldManagement, newManagement);
        if (newMode.compare("DHCP", Qt::CaseInsensitive) == 0)
            return true;
    }

    return false;
}

void NetworkingPropertiesDialog::collateChanges(NetworkingPropertiesPage* page,
                                                const QSharedPointer<PIF>& oldPif,
                                                QList<QPair<QSharedPointer<PIF>, bool>>& newPifs,
                                                QList<QPair<QSharedPointer<PIF>, bool>>& newNamePifs,
                                                QMap<QString, QVariantMap>& updatedPifs)
{
    QSharedPointer<PIF> pif = oldPif;
    bool changed = false;
    bool changedName = false;

    if (!pif)
    {
        QSharedPointer<Network> network;
        const QString ref = page->SelectedNetworkRef();
        for (const QSharedPointer<Network>& net : this->m_networks)
        {
            if (net && net->OpaqueRef() == ref)
            {
                network = net;
                break;
            }
        }
        pif = network ? this->findPifForHost(network) : QSharedPointer<PIF>();
        if (!pif)
            throw std::runtime_error("Network has gone away");
        changed = true;
    }
    else
    {
        QSharedPointer<Network> network = pif->GetNetwork();
        if (!network || page->SelectedNetworkRef() != network->OpaqueRef())
        {
            QSharedPointer<Network> newNetwork;
            const QString ref = page->SelectedNetworkRef();
            for (const QSharedPointer<Network>& net : this->m_networks)
            {
                if (net && net->OpaqueRef() == ref)
                {
                    newNetwork = net;
                    break;
                }
            }
            pif = newNetwork ? this->findPifForHost(newNetwork) : QSharedPointer<PIF>();
            if (!pif)
                throw std::runtime_error("Network has gone away");
            changed = true;
        }
    }

    QVariantMap newData = pif->GetData();
    QVariantMap newNameData = pif->GetData();

    if (page->IsDhcp())
    {
        newData["ip_configuration_mode"] = "DHCP";
    }
    else
    {
        newData["ip_configuration_mode"] = "Static";
        newData["IP"] = page->IPAddress();
        newData["netmask"] = page->Netmask();
        newData["gateway"] = page->Gateway();

        QStringList dns;
        if (!page->PreferredDNS().isEmpty())
            dns.append(page->PreferredDNS());
        if (!page->AlternateDNS1().isEmpty())
            dns.append(page->AlternateDNS1());
        if (!page->AlternateDNS2().isEmpty())
            dns.append(page->AlternateDNS2());
        newData["DNS"] = dns.join(",");
    }

    newData["management"] = (page->Type() != NetworkingPropertiesPage::PageType::Secondary);

    if (!changed)
    {
        const QVariantMap oldData = pif->GetData();
        auto differs = [&](const QString& key) {
            return oldData.value(key) != newData.value(key);
        };
        if (differs("ip_configuration_mode") ||
            differs("IP") ||
            differs("netmask") ||
            differs("gateway") ||
            differs("DNS") ||
            differs("management"))
        {
            changed = true;
        }
    }

    if (page->Type() == NetworkingPropertiesPage::PageType::Secondary)
    {
        QVariantMap otherConfig = pif->GetOtherConfig();
        const QString newPurpose = page->Purpose();
        if (otherConfig.value("management_purpose").toString() != newPurpose)
        {
            otherConfig["management_purpose"] = newPurpose;
            if (changed)
                newData["other_config"] = otherConfig;
            else
                newNameData["other_config"] = otherConfig;
            if (changed)
                changed = true;
            else
                changedName = true;
        }
    }

    if (changed)
    {
        updatedPifs.insert(pif->OpaqueRef(), newData);
        newPifs.append({pif, true});
    }
    else
    {
        newPifs.append({pif, false});
    }

    if (changedName)
    {
        updatedPifs.insert(pif->OpaqueRef(), newNameData);
        newNamePifs.append({pif, true});
    }
    else
    {
        newNamePifs.append({pif, false});
    }
}

void NetworkingPropertiesDialog::accept()
{
    QList<QPair<QSharedPointer<PIF>, bool>> newPifs;
    QList<QPair<QSharedPointer<PIF>, bool>> newNamePifs;
    QList<QSharedPointer<PIF>> downPifs;
    QMap<QString, QVariantMap> updatedPifs;
    QList<QSharedPointer<PIF>> updatedPurposePifs;

    for (const QSharedPointer<PIF>& pif : this->m_allPifs)
    {
        if (pif && pif->IsManagementInterface())
            downPifs.append(pif);
    }

    try
    {
        for (NetworkingPropertiesPage* page : this->m_pages)
        {
            QSharedPointer<PIF> oldPif = page->Pif();
            this->collateChanges(page, oldPif, newPifs, newNamePifs, updatedPifs);
        }
    } catch (const std::exception&)
    {
        QMessageBox::warning(this, tr("Network reconfiguration"), tr("The network configuration could not be applied."));
        QDialog::reject();
        return;
    }

    QSharedPointer<PIF> downManagement;
    for (const QSharedPointer<PIF>& pif : downPifs)
    {
        if (pif && pif->Management())
        {
            downManagement = pif;
            break;
        }
    }

    QSharedPointer<PIF> newManagement;
    for (const auto& pair : newPifs)
    {
        if (pair.first && pair.first->Management())
        {
            newManagement = pair.first;
            break;
        }
    }

    auto getPifData = [&updatedPifs](const QSharedPointer<PIF>& pif)
    {
        if (!pif)
            return QVariantMap();

        const QString ref = pif->OpaqueRef();

        if (updatedPifs.contains(ref))
            return updatedPifs.value(ref);

        return pif->GetData();
    };

    bool managementIpChanged = false;
    bool displayWarning = false;
    if (downManagement)
    {
        if (!newManagement)
            throw std::runtime_error("Missing new management interface");

        QVariantMap oldData = getPifData(downManagement);
        QVariantMap newData = getPifData(newManagement);
        const QString oldMode = oldData.value("ip_configuration_mode").toString();
        const QString newMode = newData.value("ip_configuration_mode").toString();
        auto settingsChanged = [&oldData, &newData]()
        {
            return oldData.value("IP") != newData.value("IP") ||
                   oldData.value("netmask") != newData.value("netmask") ||
                   oldData.value("gateway") != newData.value("gateway");
        };

        if (oldMode.compare("DHCP", Qt::CaseInsensitive) == 0)
        {
            if (newMode.compare("DHCP", Qt::CaseInsensitive) == 0)
                managementIpChanged = oldData != newData;
            else if (newMode.compare("Static", Qt::CaseInsensitive) == 0)
                managementIpChanged = settingsChanged();
        } else if (oldMode.compare("Static", Qt::CaseInsensitive) == 0)
        {
            if (newMode.compare("Static", Qt::CaseInsensitive) == 0)
                managementIpChanged = settingsChanged();
            else if (newMode.compare("DHCP", Qt::CaseInsensitive) == 0)
                managementIpChanged = true;
        }
        displayWarning = managementIpChanged ||
                         downManagement->OpaqueRef() != newManagement->OpaqueRef() ||
                         downManagement->IpConfigurationMode() != newManagement->IpConfigurationMode();

        if (downManagement->OpaqueRef() == newManagement->OpaqueRef())
            downManagement.reset();
    }

    downPifs.erase(std::remove_if(downPifs.begin(), downPifs.end(), [&](const QSharedPointer<PIF>& pif)
    {
        for (const auto& np : newPifs)
        {
            if (np.first && pif && np.first->GetUUID() == pif->GetUUID())
                return true;
        }
        return false;
    }), downPifs.end());

    newPifs.erase(std::remove_if(newPifs.begin(), newPifs.end(), [](const QPair<QSharedPointer<PIF>, bool>& p) {
        return !p.second;
    }), newPifs.end());

    newNamePifs.erase(std::remove_if(newNamePifs.begin(), newNamePifs.end(), [](const QPair<QSharedPointer<PIF>, bool>& p) {
        return !p.second;
    }), newNamePifs.end());

    if (!newNamePifs.isEmpty())
    {
        for (const auto& pair : newNamePifs)
        {
            if (pair.first)
                updatedPurposePifs.append(pair.first);
        }
    }

    if (!updatedPurposePifs.isEmpty())
    {
        if (XenConnection* connection = this->m_host ? this->m_host->GetConnection() : nullptr)
        {
            auto* purposeAction = new SetSecondaryManagementPurposeAction(connection, this->m_pool, updatedPurposePifs, nullptr);
            purposeAction->RunAsync(true);
        }
    }

    if (!newPifs.isEmpty() || !downPifs.isEmpty())
    {
        if (displayWarning)
        {
            const QString title = this->m_pool
                ? tr("Changing the management interface on a pool may interrupt connectivity.")
                : tr("Changing the management interface on a host may interrupt connectivity.");
            if (QMessageBox::warning(this, tr("Warning"), title,
                                     QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
            {
                return;
            }
        }

        if (!downManagement)
        {
            newManagement.reset();
        } else
        {
            downPifs.removeAll(downManagement);
        }

        // Match C# order: do management-related PIFs last to reduce risk of breaking connectivity.
        std::reverse(newPifs.begin(), newPifs.end());
        std::reverse(downPifs.begin(), downPifs.end());

        QStringList reconfigureRefs;
        for (const auto& pair : newPifs)
        {
            if (pair.first)
                reconfigureRefs.append(pair.first->OpaqueRef());
        }

        QStringList disableRefs;
        for (const QSharedPointer<PIF>& pif : downPifs)
        {
            if (pif)
                disableRefs.append(pif->OpaqueRef());
        }

        QString newManagementRef;
        QString oldManagementRef;
        if (!downManagement)
        {
            newManagement.reset();
        } else
        {
            newManagementRef = newManagement ? newManagement->OpaqueRef() : QString();
            oldManagementRef = downManagement->OpaqueRef();
        }

        if (XenConnection* connection = this->m_host ? this->m_host->GetConnection() : nullptr)
        {
            if (connection->GetCache())
            {
                for (auto it = updatedPifs.constBegin(); it != updatedPifs.constEnd(); ++it)
                {
                    QVariantMap updated = it.value();
                    updated["ref"] = it.key();
                    connection->GetCache()->Update("pif", it.key(), updated);
                }
            }
            ChangeNetworkingAction* action = new ChangeNetworkingAction(connection,
                                                                        this->m_pool,
                                                                        this->m_host,
                                                                        reconfigureRefs,
                                                                        disableRefs,
                                                                        newManagementRef,
                                                                        oldManagementRef,
                                                                        managementIpChanged,
                                                                        nullptr);
            action->RunAsync(true);
        }
    }

    QDialog::accept();
}
