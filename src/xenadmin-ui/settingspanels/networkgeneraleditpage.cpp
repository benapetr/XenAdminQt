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

#include "networkgeneraleditpage.h"
#include "ui_networkgeneraleditpage.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/bond.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vif.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/network/networkaction.h"
#include "iconmanager.h"
#include <QMessageBox>

NetworkGeneralEditPage::NetworkGeneralEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::NetworkGeneralEditPage), m_runningVMsWithoutTools_(false)
{
    this->ui->setupUi(this);

    // Connect signals
    connect(this->ui->nicComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NetworkGeneralEditPage::onNicSelectionChanged);
    connect(this->ui->vlanSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &NetworkGeneralEditPage::onVlanValueChanged);
    connect(this->ui->mtuSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &NetworkGeneralEditPage::onMtuValueChanged);
    connect(this->ui->radioBalanceSlb, &QRadioButton::toggled, this, &NetworkGeneralEditPage::onBondModeChanged);
    connect(this->ui->radioActiveBackup, &QRadioButton::toggled, this, &NetworkGeneralEditPage::onBondModeChanged);
    connect(this->ui->radioLacpSrcMac, &QRadioButton::toggled, this, &NetworkGeneralEditPage::onBondModeChanged);
    connect(this->ui->radioLacpTcpUdp, &QRadioButton::toggled, this, &NetworkGeneralEditPage::onBondModeChanged);
}

NetworkGeneralEditPage::~NetworkGeneralEditPage()
{
    delete this->ui;
}

QString NetworkGeneralEditPage::GetText() const
{
    return tr("Network Settings");
}

QString NetworkGeneralEditPage::GetSubText() const
{
    if (this->m_networkRef_.isEmpty())
        return QString();

    // Determine network type for subtext
    if (this->isSelectedInternal())
        return tr("Internal network");

    QString nicName = this->ui->nicComboBox->currentText();
    int vlan = this->ui->vlanSpinBox->value();
    
    if (vlan > 0)
        return tr("NIC %1, VLAN %2").arg(nicName).arg(vlan);
    else
        return tr("NIC %1").arg(nicName);
}

QIcon NetworkGeneralEditPage::GetImage() const
{
    return IconManager::instance().GetIconForNetwork(this->m_objectDataCopy_);
}

void NetworkGeneralEditPage::SetXenObject(QSharedPointer<XenObject> object,
                                          const QVariantMap& objectDataBefore,
                                          const QVariantMap& objectDataCopy)
{
    this->m_object = object;
    this->m_networkRef_.clear();
    this->m_objectDataBefore_.clear();
    this->m_objectDataCopy_.clear();
    this->m_hostRef_.clear();
    this->m_runningVMsWithoutTools_ = false;

    if (object.isNull() || object->GetObjectType() != XenObjectType::Network)
        return;

    this->m_networkRef_ = object->OpaqueRef();
    this->m_objectDataBefore_ = objectDataBefore;
    this->m_objectDataCopy_ = objectDataCopy;

    XenConnection* conn = this->connection();
    if (!conn || !conn->GetCache())
        return;

    // Get coordinator host for this network (matches C# Helpers.GetCoordinator)
    QSharedPointer<Pool> pool = conn->GetCache()->GetPoolOfOne();
    if (!pool.isNull())
        this->m_hostRef_ = pool->GetMasterHostRef();

    if (this->m_hostRef_.isEmpty())
    {
        // Fall back to a host referenced by this network's PIFs
        QVariantList pifRefs = this->m_objectDataCopy_.value("PIFs").toList();
        for (const QVariant& pifRefVar : pifRefs)
        {
            QString pifRef = pifRefVar.toString();
            QSharedPointer<PIF> pif = conn->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
            if (!pif || !pif->IsValid())
                continue;

            QString hostRef = pif->GetHostRef();
            if (!hostRef.isEmpty())
            {
                this->m_hostRef_ = hostRef;
                break;
            }
        }
    }

    if (this->m_hostRef_.isEmpty())
    {
        // Standalone host
        QList<QSharedPointer<Host>> hosts = conn->GetCache()->GetAll<Host>(XenObjectType::Host);
        if (!hosts.isEmpty() && hosts.first())
            this->m_hostRef_ = hosts.first()->OpaqueRef();
    }

    // Populate UI from network data
    long mtu = this->m_objectDataCopy_.value("MTU", 1500).toLongLong();
    this->ui->mtuSpinBox->setValue(mtu);

    // Auto-add to VMs checkbox
    QVariantMap otherConfig = this->m_objectDataCopy_.value("other_config").toMap();
    QString automaticValue = otherConfig.value("automatic", "false").toString();
    bool autoAdd = (automaticValue != "false");
    // TODO saving of this modified value doesn't seem to work right now
    this->ui->autoAddCheckBox->setChecked(autoAdd);

    // Check if this is guest installer network
    bool isGuestInstaller = otherConfig.value("is_guest_installer_network", false).toBool();
    this->ui->autoAddCheckBox->setEnabled(!isGuestInstaller);

    // Populate NIC list
    this->populateNicList();

    // Check for running VMs without tools (affects MTU changes)
    QVariantList vifRefs = this->m_objectDataCopy_.value("VIFs").toList();
    for (const QVariant& vifRefVariant : vifRefs)
    {
        QString vifRef = vifRefVariant.toString();
        QSharedPointer<VIF> vif = conn->GetCache()->ResolveObject<VIF>(XenObjectType::VIF, vifRef);
        if (!vif || !vif->IsValid())
            continue;

        QString vmRef = vif->GetData().value("VM").toString();
        QSharedPointer<VM> vm = conn->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
        if (!vm || !vm->IsValid())
            continue;

        QString powerState = vm->GetData().value("power_state").toString();
        if (powerState == "Running")
        {
            // Check if VM has tools installed (simplified - C# checks virtualization status)
            this->m_runningVMsWithoutTools_ = true;
            break;
        }
    }

    // Update Bond mode UI
    this->updateBondModeVisibility();
    this->updateMtuEnablement();
    this->updateControlsEnablement();
}

void NetworkGeneralEditPage::populateNicList()
{
    this->ui->nicComboBox->clear();

    if (!this->connection() || !this->connection()->GetCache())
        return;

    XenCache* cache = this->connection()->GetCache();
    
    // Add "Internal" option first
    this->ui->nicComboBox->addItem(tr("Internal network"));

    // Get all PIFs for the coordinator host
    if (this->m_hostRef_.isEmpty())
        return;

    QList<QSharedPointer<PIF>> allPifs = cache->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : allPifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        QString pifHost = pif->GetHostRef();
        
        // Only PIFs from coordinator host
        if (pifHost != this->m_hostRef_)
            continue;

        // Only physical PIFs, not bond members
        bool isPhysical = pif->IsPhysical();
        if (!isPhysical)
            continue;

        // Check if it's a bond member
        if (pif->IsBondSlave())
            continue;

        // Get PIF name
        QString pifName = pif->GetName();
        this->ui->nicComboBox->addItem(pifName, pif->OpaqueRef());
    }

    // Select the appropriate NIC based on current network's PIFs
    QVariantList networkPifs = this->m_objectDataCopy_.value("PIFs").toList();
    if (networkPifs.isEmpty())
    {
        // Internal network
        this->ui->nicComboBox->setCurrentIndex(0);
        this->ui->vlanSpinBox->setValue(0);
        this->ui->vlanSpinBox->setEnabled(false);
    } else
    {
        // External network - find the PIF for this host
        QString networkPifRef = this->getNetworkPifRef();
        if (!networkPifRef.isEmpty())
        {
            QSharedPointer<PIF> networkPif = cache->ResolveObject<PIF>(XenObjectType::PIF, networkPifRef);
            if (networkPif && networkPif->IsValid())
            {
                QVariantMap pifData = networkPif->GetData();
                int vlan = pifData.value("VLAN", -1).toInt();
                this->ui->vlanSpinBox->setValue(vlan >= 0 ? vlan : 0);

                // Find physical PIF
                QString physicalPifRef = this->getPhysicalPifRef();
                if (!physicalPifRef.isEmpty())
                {
                    QSharedPointer<PIF> physPif = cache->ResolveObject<PIF>(XenObjectType::PIF, physicalPifRef);
                    if (physPif && physPif->IsValid())
                    {
                        int index = this->ui->nicComboBox->findData(physicalPifRef);
                        if (index < 0)
                        {
                            QString pifName = physPif->GetName();
                            index = this->ui->nicComboBox->findText(pifName);
                        }
                        if (index >= 0)
                            this->ui->nicComboBox->setCurrentIndex(index);
                    }
                }
            }
        }
    }
}

void NetworkGeneralEditPage::updateBondModeVisibility()
{
    if (!this->connection() || !this->connection()->GetCache())
    {
        this->ui->bondModeGroupBox->setVisible(false);
        return;
    }

    // Check if this network has bonds
    QVariantList bondRefs = this->m_objectDataCopy_.value("bonds", QVariantList()).toList();
    // Alternative: look for PIFs with bond_master_of
    if (bondRefs.isEmpty())
    {
        QVariantList pifRefs = this->m_objectDataCopy_.value("PIFs").toList();
        for (const QVariant& pifRefVar : pifRefs)
        {
            QString pifRef = pifRefVar.toString();
            QSharedPointer<PIF> pif = this->connection()->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
            if (!pif || !pif->IsValid())
                continue;

            QVariantList bondMasterOf = pif->GetData().value("bond_master_of").toList();
            if (!bondMasterOf.isEmpty())
            {
                bondRefs.append(bondMasterOf);
                break;
            }
        }
    }

    bool hasBond = !bondRefs.isEmpty();
    this->ui->bondModeGroupBox->setVisible(hasBond);

    if (hasBond && !bondRefs.isEmpty())
    {
        // Get bond mode from first bond
        QString bondRef = bondRefs.first().toString();
        QSharedPointer<Bond> bond = this->connection()->GetCache()->ResolveObject<Bond>(XenObjectType::Bond, bondRef);
        if (bond && bond->IsValid())
        {
            QString mode = bond->Mode();
            this->m_originalBondMode_ = mode;

            if (mode == "balance-slb")
            {
                this->ui->radioBalanceSlb->setChecked(true);
            } else if (mode == "active-backup")
            {
                this->ui->radioActiveBackup->setChecked(true);
            } else if (mode == "lacp")
            {
                // Check hashing algorithm
                QMap<QString, QString> properties = bond->Properties();
                QString hashing = properties.value("hashing_algorithm", "src_mac");
                this->m_originalHashingAlgorithm_ = hashing;

                if (hashing == "tcpudp_ports")
                    this->ui->radioLacpTcpUdp->setChecked(true);
                else
                    this->ui->radioLacpSrcMac->setChecked(true);
            }
        }
    }
}

void NetworkGeneralEditPage::updateMtuEnablement()
{
    // MTU can't be changed if:
    // 1. This is a management interface
    // 2. There are running VMs without tools attached to this network
    // 3. Network doesn't support jumbo frames (e.g., CHIN)

    // Check if network supports jumbo frames
    bool canUseJumbo = true;  // Simplified - C# checks for CHIN networks
    
    if (!canUseJumbo)
    {
        this->ui->mtuSpinBox->setEnabled(false);
        this->ui->mtuWarningLabel->setVisible(false);
        return;
    }

    // Check if this is internal network
    if (this->isSelectedInternal())
    {
        this->ui->mtuSpinBox->setEnabled(false);
        this->ui->mtuWarningLabel->setVisible(false);
        return;
    }

    // Check if this is management interface
    QString pifRef = this->getNetworkPifRef();
    if (!pifRef.isEmpty())
    {
        QSharedPointer<PIF> pif = this->connection()->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
        if (pif && pif->IsValid())
        {
            bool isManagement = pif->GetData().value("management").toBool();
            if (isManagement)
            {
                this->ui->mtuSpinBox->setEnabled(false);
                this->ui->mtuWarningLabel->setText(tr("Cannot configure MTU on management interface"));
                this->ui->mtuWarningLabel->setVisible(true);
                return;
            }
        }
    }

    // Check for running VMs without tools
    if (this->m_runningVMsWithoutTools_)
    {
        this->ui->mtuSpinBox->setEnabled(false);
        this->ui->mtuWarningLabel->setText(tr("Cannot configure MTU while VMs without tools are running"));
        this->ui->mtuWarningLabel->setVisible(true);
        return;
    }

    this->ui->mtuSpinBox->setEnabled(true);
    this->ui->mtuWarningLabel->setVisible(false);
}

void NetworkGeneralEditPage::updateControlsEnablement()
{
    // Check if VMs are attached
    bool vmsAttached = false;
    QVariantList vifRefs = this->m_objectDataCopy_.value("VIFs").toList();
    for (const QVariant& vifRefVar : vifRefs)
    {
        QString vifRef = vifRefVar.toString();
        QSharedPointer<VIF> vif = this->connection()->GetCache()->ResolveObject<VIF>(XenObjectType::VIF, vifRef);
        if (!vif || !vif->IsValid())
            continue;

        bool currentlyAttached = vif->GetData().value("currently_attached").toBool();
        if (currentlyAttached)
        {
            vmsAttached = true;
            break;
        }
    }

    // Check if this is management interface
    bool isManagement = false;
    QString pifRef = this->getNetworkPifRef();
    if (!pifRef.isEmpty())
    {
        QSharedPointer<PIF> pif = this->connection()->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
        if (pif && pif->IsValid())
            isManagement = pif->GetData().value("management").toBool();
    }

    bool blockDueToAttached = vmsAttached || isManagement;

    const bool nicVlanEditable = this->isNicVlanEditable();
    this->ui->nicLabel->setVisible(nicVlanEditable);
    this->ui->nicComboBox->setVisible(nicVlanEditable);
    this->ui->vlanLabel->setVisible(nicVlanEditable);
    this->ui->vlanSpinBox->setVisible(nicVlanEditable);

    if (!nicVlanEditable)
    {
        this->ui->warningLabel->setVisible(false);
        this->ui->disruptionWarningLabel->setVisible(this->willDisrupt());
        return;
    }

    // NIC/VLAN controls enabled only if no VMs attached and not management
    QVariantList pifRefs = this->m_objectDataCopy_.value("PIFs").toList();
    if (pifRefs.isEmpty())
    {
        // Internal network - allow changes
        this->ui->nicComboBox->setEnabled(true);
        this->ui->vlanSpinBox->setEnabled(false);
        this->ui->warningLabel->setVisible(false);
    }
    else
    {
        this->ui->nicComboBox->setEnabled(!blockDueToAttached);
        this->ui->vlanSpinBox->setEnabled(!blockDueToAttached && !this->isSelectedInternal());
        
        if (blockDueToAttached)
        {
            this->ui->warningLabel->setText(
                isManagement 
                    ? tr("Cannot reconfigure network settings on management interface")
                    : tr("Cannot reconfigure network while VMs are attached"));
            this->ui->warningLabel->setVisible(true);
        }
        else
        {
            this->ui->warningLabel->setVisible(false);
        }
    }

    // Show disruption warning if changes will cause network disruption
    bool showDisruption = this->willDisrupt();
    this->ui->disruptionWarningLabel->setVisible(showDisruption);
}

bool NetworkGeneralEditPage::isSelectedInternal() const
{
    if (!this->isNicVlanEditable())
        return this->m_objectDataCopy_.value("PIFs").toList().isEmpty();

    return this->ui->nicComboBox->currentIndex() == 0;
}

QString NetworkGeneralEditPage::getNetworkPifRef() const
{
    if (this->m_hostRef_.isEmpty())
        return QString();

    QVariantList pifRefs = this->m_objectDataCopy_.value("PIFs").toList();
    for (const QVariant& pifRefVar : pifRefs)
    {
        QString pifRef = pifRefVar.toString();
        QSharedPointer<PIF> pif = this->connection()->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, pifRef);
        if (!pif || !pif->IsValid())
            continue;

        QString pifHost = pif->GetData().value("host").toString();
        if (pifHost == this->m_hostRef_)
            return pifRef;
    }

    return QString();
}

QString NetworkGeneralEditPage::getPhysicalPifRef() const
{
    QString networkPifRef = this->getNetworkPifRef();
    if (networkPifRef.isEmpty())
        return QString();

    QSharedPointer<PIF> networkPif = this->connection()->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, networkPifRef);
    if (!networkPif || !networkPif->IsValid())
        return QString();

    QVariantMap pifData = networkPif->GetData();
    bool isPhysical = pifData.value("physical").toBool();
    
    if (isPhysical)
        return networkPifRef;

    // Find physical PIF with same device
    QString device = pifData.value("device").toString();
    QString host = pifData.value("host").toString();

    QList<QSharedPointer<PIF>> allPifs = this->connection()->GetCache()->GetAll<PIF>(XenObjectType::PIF);
    for (const QSharedPointer<PIF>& pif : allPifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        QVariantMap otherPifData = pif->GetData();
        if (otherPifData.value("physical").toBool() &&
            otherPifData.value("host").toString() == host &&
            otherPifData.value("device").toString() == device)
        {
            return pif->OpaqueRef();
        }
    }

    return QString();
}

AsyncOperation* NetworkGeneralEditPage::SaveSettings()
{
    if (this->m_networkRef_.isEmpty())
        return nullptr;

    XenConnection* conn = this->connection();
    if (!conn || !conn->GetCache())
        return nullptr;

    QSharedPointer<Network> network = conn->GetCache()->ResolveObject<Network>(this->m_networkRef_);
    if (!network || !network->IsValid())
        return nullptr;

    // Check if we need to change PIFs (switching between internal/external or changing NIC/VLAN)
    bool needsPifChange = this->nicOrVlanHasChanged();

    // Simple property changes (MTU, auto-add) - update m_objectDataCopy_
    if (this->mtuHasChanged())
    {
        this->m_objectDataCopy_["MTU"] = this->ui->mtuSpinBox->value();
    }

    QVariantMap otherConfig = this->m_objectDataCopy_.value("other_config").toMap();
    bool autoAdd = this->ui->autoAddCheckBox->isChecked();
    otherConfig["automatic"] = autoAdd ? "true" : "false";
    this->m_objectDataCopy_["other_config"] = otherConfig;

    // If no PIF changes are needed, let VerticallyTabbedDialog handle simple property changes
    // Bond mode/hashing changes would need DelegatedAsyncOperation (TODO: port UnplugPlugNetworkAction)
    if (!needsPifChange)
        return nullptr;

    // Complex PIF reconfiguration requires NetworkAction
    bool isNowInternal = this->isSelectedInternal();
    bool wasInternal = this->m_objectDataBefore_.value("PIFs").toList().isEmpty();

    if (wasInternal && isNowInternal)
    {
        // Internal -> Internal: no PIF change needed
        return nullptr;
    }

    if (!wasInternal && !isNowInternal)
    {
        // External -> External: changing NIC or VLAN
        QString selectedPifRef = this->ui->nicComboBox->currentData().toString();
        QSharedPointer<PIF> basePif = conn->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, selectedPifRef);
        if (!basePif || !basePif->IsValid())
            return nullptr;

        qint64 vlan = this->ui->vlanSpinBox->value();
        
        // Use update constructor with changePIFs=true
        NetworkAction* action = new NetworkAction(network, true, true, basePif, vlan, false, this);
        action->SetDescription(tr("Reconfiguring network '%1'").arg(network->GetName()));
        return action;
    }

    if (wasInternal && !isNowInternal)
    {
        // Internal -> External: create VLAN
        QString selectedPifRef = this->ui->nicComboBox->currentData().toString();
        QSharedPointer<PIF> basePif = conn->GetCache()->ResolveObject<PIF>(XenObjectType::PIF, selectedPifRef);
        if (!basePif || !basePif->IsValid())
            return nullptr;

        qint64 vlan = this->ui->vlanSpinBox->value();
        
        NetworkAction* action = new NetworkAction(network, basePif, vlan, this);
        action->SetDescription(tr("Creating external network '%1'").arg(network->GetName()));
        return action;
    }

    if (!wasInternal && isNowInternal)
    {
        // External -> Internal: destroy VLANs and convert to internal
        NetworkAction* action = new NetworkAction(network, true, false, nullptr, 0, false, this);
        action->SetDescription(tr("Converting network '%1' to internal").arg(network->GetName()));
        return action;
    }

    return nullptr;
}

bool NetworkGeneralEditPage::HasChanged() const
{
    if (this->m_networkRef_.isEmpty())
        return false;

    // NOTE: Name and Description changes are handled by GeneralEditPage now

    // Check auto-add change
    QVariantMap origOtherConfig = this->m_objectDataBefore_.value("other_config").toMap();
    QString origAutomatic = origOtherConfig.value("automatic", "false").toString();
    bool origAutoAdd = (origAutomatic != "false");
    bool newAutoAdd = this->ui->autoAddCheckBox->isChecked();
    if (origAutoAdd != newAutoAdd)
        return true;

    // Check MTU change
    if (this->mtuHasChanged())
        return true;

    // Check Bond mode change
    if (this->bondModeHasChanged())
        return true;

    // Check hashing algorithm change
    if (this->hashingAlgorithmHasChanged())
        return true;

    // Check for NIC/VLAN changes
    if (this->nicOrVlanHasChanged())
        return true;

    return false;
}

QVariantMap NetworkGeneralEditPage::GetModifiedObjectData() const
{
    return this->m_objectDataCopy_;
}

bool NetworkGeneralEditPage::IsValidToSave() const
{
    // NOTE: Name validation is handled by GeneralEditPage

    // MTU must be in valid range
    int mtu = this->ui->mtuSpinBox->value();
    if (mtu < 1500 || mtu > 9216)
        return false;

    return true;
}

void NetworkGeneralEditPage::ShowLocalValidationMessages()
{
    // NOTE: Name validation is handled by GeneralEditPage
    // No local validation needed for this page currently
}

void NetworkGeneralEditPage::HideLocalValidationMessages()
{
    // NOTE: Name validation is handled by GeneralEditPage
    // No local validation to hide
}

void NetworkGeneralEditPage::Cleanup()
{
    // Nothing to clean up
}

bool NetworkGeneralEditPage::mtuHasChanged() const
{
    if (!this->ui->mtuSpinBox->isEnabled())
        return false;

    long origMtu = this->m_objectDataBefore_.value("MTU", 1500).toLongLong();
    long newMtu = this->ui->mtuSpinBox->value();
    return origMtu != newMtu;
}

bool NetworkGeneralEditPage::bondModeHasChanged() const
{
    if (!this->ui->bondModeGroupBox->isVisible() || !this->ui->bondModeGroupBox->isEnabled())
        return false;

    QString newMode = this->getSelectedBondMode();
    return this->m_originalBondMode_ != newMode;
}

bool NetworkGeneralEditPage::hashingAlgorithmHasChanged() const
{
    if (!this->ui->bondModeGroupBox->isVisible() || !this->ui->bondModeGroupBox->isEnabled())
        return false;

    QString newAlgorithm = this->getSelectedHashingAlgorithm();
    return !newAlgorithm.isEmpty() && this->m_originalHashingAlgorithm_ != newAlgorithm;
}

bool NetworkGeneralEditPage::nicOrVlanHasChanged() const
{
    if (this->m_networkRef_.isEmpty())
        return false;

    if (!this->isNicVlanEditable())
        return false;

    bool wasInternal = this->m_objectDataBefore_.value("PIFs").toList().isEmpty();
    bool isNowInternal = this->isSelectedInternal();

    // Switching between internal and external
    if (wasInternal != isNowInternal)
        return true;

    // Both internal - no PIF change
    if (wasInternal && isNowInternal)
        return false;

    // Both external - check if NIC or VLAN changed
    QString originalPifRef = this->getNetworkPifRef();
    if (originalPifRef.isEmpty())
        return false;

    QSharedPointer<PIF> originalPif = this->connection()->GetCache()->ResolveObject<PIF>(originalPifRef);
    if (!originalPif || !originalPif->IsValid())
        return false;

    QVariantMap originalPifData = originalPif->GetData();
    int originalVlan = originalPifData.value("VLAN", -1).toInt();
    int newVlan = this->ui->vlanSpinBox->value();

    if (originalVlan != newVlan)
        return true;

    // Check if physical NIC changed
    QString originalPhysicalPifRef = this->getPhysicalPifRef();
    QString selectedPifRef = this->ui->nicComboBox->currentData().toString();

    return originalPhysicalPifRef != selectedPifRef;
}

bool NetworkGeneralEditPage::isNicVlanEditable() const
{
    QString networkPifRef = this->getNetworkPifRef();
    if (networkPifRef.isEmpty())
        return true;

    QSharedPointer<PIF> networkPif = this->connection()->GetCache()->ResolveObject<PIF>(networkPifRef);
    if (!networkPif || !networkPif->IsValid())
        return false;

    return !networkPif->IsPhysical() && !networkPif->IsTunnelAccessPIF() && !networkPif->IsSriovLogicalPIF();
}

QString NetworkGeneralEditPage::getSelectedBondMode() const
{
    if (this->ui->radioBalanceSlb->isChecked())
        return "balance-slb";
    else if (this->ui->radioActiveBackup->isChecked())
        return "active-backup";
    else if (this->ui->radioLacpSrcMac->isChecked() || this->ui->radioLacpTcpUdp->isChecked())
        return "lacp";
    
    return QString();
}

QString NetworkGeneralEditPage::getSelectedHashingAlgorithm() const
{
    if (!this->ui->bondModeGroupBox->isVisible())
        return QString();

    if (this->ui->radioLacpSrcMac->isChecked())
        return "src_mac";
    else if (this->ui->radioLacpTcpUdp->isChecked())
        return "tcpudp_ports";
    
    return QString();
}

bool NetworkGeneralEditPage::willDisrupt() const
{
    // Check if changes will cause network disruption
    return this->mtuHasChanged() || this->bondModeHasChanged() || this->hashingAlgorithmHasChanged();
}

void NetworkGeneralEditPage::onNicSelectionChanged()
{
    this->updateControlsEnablement();
    this->updateMtuEnablement();
}

void NetworkGeneralEditPage::onVlanValueChanged()
{
    this->updateControlsEnablement();
}

void NetworkGeneralEditPage::onMtuValueChanged()
{
    this->updateControlsEnablement();
}

void NetworkGeneralEditPage::onBondModeChanged()
{
    this->updateControlsEnablement();
}
