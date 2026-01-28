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

#include "bonddetailswidget.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>
#include <algorithm>
#include "xenlib/xencache.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/pifmetrics.h"
#include "../settingsmanager.h"

namespace
{
    QString formatMac(const QString& mac)
    {
        QString result = mac;
        if (result.length() == 12 && !result.contains(":"))
        {
            QString formatted;
            for (int i = 0; i < result.length(); i += 2)
            {
                if (i > 0)
                    formatted += ":";
                formatted += result.mid(i, 2);
            }
            result = formatted;
        }
        return result;
    }

    QString nicSpeedText(qint64 speed, bool carrier)
    {
        if (!carrier || speed <= 0)
            return QStringLiteral("-");
        return QString("%1 Mbps").arg(speed);
    }

    QString nicDuplexText(bool duplex, bool carrier)
    {
        if (!carrier)
            return QStringLiteral("-");
        return duplex ? QStringLiteral("Full") : QStringLiteral("Half");
    }
}

BondDetailsWidget::BondDetailsWidget(QWidget* parent) : QWidget(parent)
{
    this->ui.setupUi(this);

    if (this->ui.bondNicsTable->horizontalHeader())
    {
        this->ui.bondNicsTable->horizontalHeader()->setStretchLastSection(true);
        this->ui.bondNicsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    }

    this->ui.bondModeSlb->setChecked(true);
    this->ui.bondHashSrcMac->setChecked(true);
    this->ui.bondAutoPlugCheck->setChecked(true);
    this->ui.bondLacpWarningFrame->setVisible(false);

    this->ui.bondMtuSpin->setRange(68, 9000);
    this->ui.bondMtuSpin->setValue(1500);

    connect(this->ui.bondModeActiveBackup, &QRadioButton::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondModeSlb, &QRadioButton::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondModeLacp, &QRadioButton::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondHashSrcMac, &QRadioButton::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondHashTcpUdp, &QRadioButton::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondMtuSpin, qOverload<int>(&QSpinBox::valueChanged), this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondAutoPlugCheck, &QCheckBox::toggled, this, &BondDetailsWidget::onInputsChanged);
    connect(this->ui.bondNicsTable, &QTableWidget::itemChanged, this, &BondDetailsWidget::onInputsChanged);
}

void BondDetailsWidget::SetHost(const QSharedPointer<Host>& host)
{
    this->m_host = host;
    this->m_pool.reset();
    this->Refresh();
}

void BondDetailsWidget::SetPool(const QSharedPointer<Pool>& pool)
{
    this->m_pool = pool;
    this->m_host.reset();
    this->Refresh();
}

void BondDetailsWidget::Refresh()
{
    this->populateBondNics();
    this->updateMtuBounds();
    this->updateLacpVisibility();
    this->refreshSelectionState();
}

bool BondDetailsWidget::IsValid() const
{
    return this->m_valid;
}

bool BondDetailsWidget::CanCreateBond(QWidget* parent)
{
    XenCache* cache = this->coordinatorHost() ? this->coordinatorHost()->GetCache() : nullptr;
    if (!cache)
        return false;

    QList<QSharedPointer<PIF>> selectedPifs;
    for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
        if (!item || item->checkState() != Qt::Checked)
            continue;

        const QString pifRef = item->data(Qt::UserRole).toString();
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
        if (pif && pif->IsValid())
            selectedPifs.append(pif);
    }

    const int limit = this->bondSizeLimit();
    if (selectedPifs.count() < 2 || selectedPifs.count() > limit)
    {
        QMessageBox::warning(parent, tr("Bond Details"), tr("Please select between 2 and %1 interfaces for the bond.").arg(limit));
        return false;
    }

    bool hasPrimary = false;
    bool hasSecondary = false;
    bool hasClustering = false;
    for (const QSharedPointer<PIF>& pif : selectedPifs)
    {
        hasPrimary = hasPrimary || pif->IsPrimaryManagementInterface();
        hasSecondary = hasSecondary || pif->IsSecondaryManagementInterface(true);
        hasClustering = hasClustering || pif->IsUsedByClustering();
    }

    if (hasPrimary && hasSecondary)
    {
        QMessageBox::critical(parent, tr("Bond Details"), tr("Cannot create a bond that includes both the primary and secondary management interfaces."));
        return false;
    }

    if (hasPrimary)
    {
        QSharedPointer<Pool> pool = this->m_pool;
        if (!pool && this->m_host)
            pool = this->m_host->GetPool();
        if (pool && pool->HAEnabled())
        {
            QMessageBox::critical(parent, tr("Bond Details"), tr("Cannot create a bond that includes the primary management interface while HA is enabled."));
            return false;
        }

        QMessageBox warning(parent);
        warning.setIcon(QMessageBox::Warning);
        warning.setWindowTitle(tr("Bond Details"));
        warning.setText(tr("This bond includes the primary management interface and will disrupt management connectivity briefly."));
        warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        warning.setDefaultButton(QMessageBox::Cancel);
        if (warning.exec() != QMessageBox::Ok)
            return false;
    }

    if (hasSecondary)
    {
        QMessageBox warning(parent);
        warning.setIcon(QMessageBox::Warning);
        warning.setWindowTitle(tr("Bond Details"));
        warning.setText(hasClustering
                            ? tr("This bond includes a clustering interface. Do you want to continue?")
                            : tr("This bond includes a secondary management interface. Do you want to continue?"));
        warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        warning.setDefaultButton(QMessageBox::Cancel);
        if (warning.exec() != QMessageBox::Ok)
            return false;
    }

    return true;
}

QString BondDetailsWidget::BondName() const
{
    QStringList deviceNumbers;
    XenCache* cache = this->coordinatorHost() ? this->coordinatorHost()->GetCache() : nullptr;
    if (!cache)
        return QStringLiteral("Bond");

    for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
        if (!item || item->checkState() != Qt::Checked)
            continue;
        const QString pifRef = item->data(Qt::UserRole).toString();
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
        if (!pif || !pif->IsValid())
            continue;
        QString number = pif->GetDevice();
        number.remove("eth");
        if (!number.isEmpty())
            deviceNumbers.append(number);
    }

    deviceNumbers.sort();
    return tr("Bond %1").arg(deviceNumbers.join("+"));
}

QString BondDetailsWidget::BondMode() const
{
    if (this->ui.bondModeActiveBackup->isChecked())
        return QStringLiteral("active-backup");
    if (this->ui.bondModeSlb->isChecked())
        return QStringLiteral("balance-slb");
    return QStringLiteral("lacp");
}

QString BondDetailsWidget::HashingAlgorithm() const
{
    if (this->ui.bondHashTcpUdp->isChecked())
        return QStringLiteral("tcpudp_ports");
    return QStringLiteral("src_mac");
}

qint64 BondDetailsWidget::MTU() const
{
    return this->ui.bondMtuSpin->value();
}

bool BondDetailsWidget::AutoPlug() const
{
    return this->ui.bondAutoPlugCheck->isChecked();
}

QStringList BondDetailsWidget::SelectedPifRefs() const
{
    QStringList pifRefs;
    for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
        if (!item || item->checkState() != Qt::Checked)
            continue;
        const QString pifRef = item->data(Qt::UserRole).toString();
        if (!pifRef.isEmpty())
            pifRefs.append(pifRef);
    }
    return pifRefs;
}

void BondDetailsWidget::onInputsChanged()
{
    if (this->m_populatingBond)
        return;
    this->refreshSelectionState();
    this->updateMtuBounds();
    this->updateLacpVisibility();
}

void BondDetailsWidget::populateBondNics()
{
    if (this->m_populatingBond)
        return;

    this->m_populatingBond = true;
    this->ui.bondNicsTable->setRowCount(0);

    XenCache* cache = nullptr;
    if (this->m_pool)
        cache = this->m_pool->GetCache();
    else if (this->m_host)
        cache = this->m_host->GetCache();

    if (!cache)
    {
        this->m_populatingBond = false;
        return;
    }

    QSharedPointer<Pool> pool = this->m_pool;
    QSharedPointer<Host> host = pool ? QSharedPointer<Host>() : this->m_host;
    const QList<QSharedPointer<PIF>> pifs = cache->GetAll<PIF>("pif");
    const bool showHidden = SettingsManager::instance().getShowHiddenObjects();
    QSet<QString> seenDevices;

    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;
        if (host && pif->GetHostRef() != host->OpaqueRef())
            continue;
        if (pool && !host && !pool->GetHostRefs().contains(pif->GetHostRef()))
            continue;
        if (!pif->IsPhysical())
            continue;
        if (pif->IsBondNIC())
            continue;
        if (!pif->Show(showHidden))
            continue;

        const QString deviceName = pif->GetDevice();
        if (pool && seenDevices.contains(deviceName))
            continue;
        seenDevices.insert(deviceName);

        int row = this->ui.bondNicsTable->rowCount();
        this->ui.bondNicsTable->insertRow(row);

        QTableWidgetItem* useItem = new QTableWidgetItem();
        useItem->setCheckState(Qt::Unchecked);
        useItem->setData(Qt::UserRole, pif->OpaqueRef());
        useItem->setData(Qt::UserRole + 1, pif->IsBondMember());
        this->ui.bondNicsTable->setItem(row, 0, useItem);

        QString nicLabel = pif->GetName();
        if (pif->IsBondMember())
            nicLabel = tr("%1 (already in bond)").arg(nicLabel);
        this->ui.bondNicsTable->setItem(row, 1, new QTableWidgetItem(nicLabel));
        this->ui.bondNicsTable->setItem(row, 2, new QTableWidgetItem(formatMac(pif->GetMAC())));

        QString linkStatus = pif->GetLinkStatusString();
        this->ui.bondNicsTable->setItem(row, 3, new QTableWidgetItem(linkStatus));

        QSharedPointer<PIFMetrics> metrics;
        QString metricsRef = pif->MetricsRef();
        if (!metricsRef.isEmpty() && metricsRef != XENOBJECT_NULL)
            metrics = cache->ResolveObject<PIFMetrics>("pif_metrics", metricsRef);

        const bool carrier = metrics ? metrics->Carrier() : false;
        const qint64 speed = metrics ? metrics->Speed() : 0;
        const bool duplex = metrics ? metrics->Duplex() : false;
        const QString vendor = metrics ? metrics->VendorName() : QString();
        const QString metricsDevice = metrics ? metrics->DeviceName() : QString();
        const QString pci = metrics ? metrics->PciBusPath() : QString();

        this->ui.bondNicsTable->setItem(row, 4, new QTableWidgetItem(nicSpeedText(speed, carrier)));
        this->ui.bondNicsTable->setItem(row, 5, new QTableWidgetItem(nicDuplexText(duplex, carrier)));
        this->ui.bondNicsTable->setItem(row, 6, new QTableWidgetItem(vendor));
        this->ui.bondNicsTable->setItem(row, 7, new QTableWidgetItem(metricsDevice));
        this->ui.bondNicsTable->setItem(row, 8, new QTableWidgetItem(pci));
    }

    this->m_populatingBond = false;
}

void BondDetailsWidget::refreshSelectionState()
{
    QSignalBlocker blocker(this->ui.bondNicsTable);
    int selectedCount = 0;
    const int limit = this->bondSizeLimit();
    for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
        if (item && item->checkState() == Qt::Checked)
            selectedCount++;
    }

    const bool valid = selectedCount >= 2 && selectedCount <= limit;
    this->ui.bondNicsGroup->setTitle(tr("Network Interfaces (select at least 2, up to %1)").arg(limit));

    for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
        if (!item)
            continue;

        const bool alreadyInBond = item->data(Qt::UserRole + 1).toBool();
        const bool checked = item->checkState() == Qt::Checked;
        const bool canSelectMore = selectedCount < limit;

        Qt::ItemFlags flags = item->flags();
        if (alreadyInBond || (!checked && !canSelectMore))
            flags &= ~Qt::ItemIsEnabled;
        else
            flags |= Qt::ItemIsEnabled;
        item->setFlags(flags);
    }

    if (valid != this->m_valid)
    {
        this->m_valid = valid;
        emit ValidChanged(valid);
    }
}

void BondDetailsWidget::updateMtuBounds()
{
    int minMtu = 68;
    int maxMtu = 9000;

    XenCache* cache = nullptr;
    if (this->m_pool)
        cache = this->m_pool->GetCache();
    else if (this->m_host)
        cache = this->m_host->GetCache();

    if (cache)
    {
        QList<qint64> mtus;
        for (int row = 0; row < this->ui.bondNicsTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = this->ui.bondNicsTable->item(row, 0);
            if (!item || item->checkState() != Qt::Checked)
                continue;
            const QString pifRef = item->data(Qt::UserRole).toString();
            QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
            if (pif && pif->IsValid())
                mtus.append(pif->GetMTU());
        }

        if (!mtus.isEmpty())
            maxMtu = std::min(maxMtu, static_cast<int>(*std::min_element(mtus.begin(), mtus.end())));
    }

    this->ui.bondMtuSpin->setMinimum(minMtu);
    this->ui.bondMtuSpin->setMaximum(maxMtu);
    if (this->ui.bondMtuSpin->value() < minMtu || this->ui.bondMtuSpin->value() > maxMtu)
        this->ui.bondMtuSpin->setValue(minMtu);

    if (minMtu == maxMtu)
    {
        this->ui.bondMtuSpin->setEnabled(false);
        this->ui.bondMtuInfoLabel->setText(tr("Allowed MTU value: %1").arg(minMtu));
    }
    else
    {
        this->ui.bondMtuSpin->setEnabled(true);
        this->ui.bondMtuInfoLabel->setText(tr("Allowed MTU range: %1 to %2").arg(minMtu).arg(maxMtu));
    }
}

void BondDetailsWidget::updateLacpVisibility()
{
    const QSharedPointer<Host> host = this->coordinatorHost();
    const bool supportsLacp = host && host->vSwitchNetworkBackend();

    this->ui.bondModeLacp->setVisible(supportsLacp);
    if (!supportsLacp && this->ui.bondModeLacp->isChecked())
        this->ui.bondModeActiveBackup->setChecked(true);

    const bool lacp = supportsLacp && this->ui.bondModeLacp->isChecked();
    this->ui.bondHashLabel->setVisible(lacp);
    this->ui.bondHashSrcMac->setVisible(lacp);
    this->ui.bondHashTcpUdp->setVisible(lacp);
    this->ui.bondLacpWarningFrame->setVisible(lacp);
}

int BondDetailsWidget::bondSizeLimit() const
{
    const QSharedPointer<Host> host = this->coordinatorHost();
    return (host && host->vSwitchNetworkBackend()) ? 4 : 2;
}

QSharedPointer<Host> BondDetailsWidget::coordinatorHost() const
{
    if (this->m_host && this->m_host->IsValid())
        return this->m_host;
    if (this->m_pool && this->m_pool->IsValid())
        return this->m_pool->GetMasterHost();
    return QSharedPointer<Host>();
}
