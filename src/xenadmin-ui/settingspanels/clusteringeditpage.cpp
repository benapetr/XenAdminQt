/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "clusteringeditpage.h"
#include "ui_clusteringeditpage.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/actions/pool/clusteringactions.h"
#include "xenlib/xen/cluster.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/xenapi/xenapi_Cluster.h"
#include "xenlib/xencache.h"
#include <QPointer>
#include <QToolTip>
#include <memory>

ClusteringEditPage::ClusteringEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::ClusteringEditPage)
{
    this->ui->setupUi(this);
    this->ui->tableLayoutPanelNetworkWarning->setVisible(false);
    connect(this->ui->comboBoxNetwork, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClusteringEditPage::onNetworkChanged);
}

ClusteringEditPage::~ClusteringEditPage()
{
    delete this->ui;
}

QString ClusteringEditPage::GetText() const
{
    return tr("Clustering");
}

QString ClusteringEditPage::GetSubText() const
{
    return this->ui->CheckBoxEnableClustering->isChecked() ? tr("Enabled") : tr("Disabled");
}

QIcon ClusteringEditPage::GetImage() const
{
    return QIcon(":/icons/storage.png");
}

void ClusteringEditPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    XenCache* cache = object ? object->GetCache() : nullptr;
    if (!cache)
        return;

    this->m_pool = cache->GetPoolOfOne();
    if (!this->m_pool)
        return;

    const QList<QSharedPointer<Cluster>> clusters = cache->GetAll<Cluster>(XenObjectType::Cluster);
    QSharedPointer<Cluster> existingCluster = clusters.isEmpty() ? QSharedPointer<Cluster>() : clusters.first();
    this->m_clusteringEnabled = !existingCluster.isNull();

    this->ui->labelWarning->setVisible(false);
    this->ui->pictureBoxInfo1->setVisible(false);
    this->loadNetworks(existingCluster);
    this->setPage();
}

void ClusteringEditPage::loadNetworks(QSharedPointer<Cluster> cluster)
{
    if (!cluster)
    {
        this->populateNetworks(QString());
        if (this->ui->comboBoxNetwork->count() == 0)
            this->disableControls(tr("Clustering cannot be enabled because there are no suitable networks."));
        return;
    }

    // C#: the cluster's network is asked for, then preselected
    auto networkRef = std::make_shared<QString>();
    auto* action = new DelegatedAsyncOperation(this->m_pool->GetConnection(), QString(), QString(), true,
        [clusterRef = cluster->OpaqueRef(), networkRef](DelegatedAsyncOperation* self)
        {
            *networkRef = XenAPI::Cluster::get_network(self->GetSession(), clusterRef);
        });
    QPointer<ClusteringEditPage> self(this);
    connect(action, &AsyncOperation::completed, this, [self, networkRef]()
    {
        if (!self)
            return;
        self->populateNetworks(*networkRef);
        self->ui->tableLayoutPanelNetworkWarning->setVisible(networkRef->isEmpty() || self->ui->comboBoxNetwork->currentIndex() < 0);
    });
    action->RunAsync(true);
}

void ClusteringEditPage::populateNetworks(const QString& selectRef)
{
    QComboBox* combo = this->ui->comboBoxNetwork;
    QSignalBlocker blocker(combo);
    combo->clear();

    XenCache* cache = this->m_pool->GetCache();
    QStringList added;
    int selectIndex = -1;
    for (const QSharedPointer<PIF>& pif : cache->GetAll<PIF>(XenObjectType::PIF))
    {
        if (!pif)
            continue;
        const QSharedPointer<Network> network = pif->GetNetwork();
        // C#: one entry per network, and only networks with an IP address (management interfaces)
        if (!network || added.contains(network->OpaqueRef()))
            continue;
        if (!pif->IsPrimaryManagementInterface() && !pif->IsSecondaryManagementInterface(false))
            continue;

        added << network->OpaqueRef();
        const QString name = pif->Management() ? tr("%1 (management)").arg(network->GetName()) : network->GetName();
        combo->addItem(name, network->OpaqueRef());
        if (selectIndex < 0 && !selectRef.isEmpty() && network->OpaqueRef() == selectRef)
            selectIndex = combo->count() - 1;
    }

    // C#: nothing is selected unless the match says so
    combo->setCurrentIndex(selectIndex);
}

void ClusteringEditPage::setPage()
{
    this->ui->CheckBoxEnableClustering->setChecked(this->m_clusteringEnabled);
    XenCache* cache = this->m_pool->GetCache();

    if (this->m_clusteringEnabled)
    {
        bool gfs2Attached = false;
        for (const QSharedPointer<SR>& sr : cache->GetAll<SR>(XenObjectType::SR))
        {
            if (sr && sr->GetType().toLower() == "gfs2" && !sr->IsDetached())
                gfs2Attached = true;
        }

        if (gfs2Attached)
        {
            this->disableControls(tr("Clustering cannot be disabled because there are thinly provisioned (GFS2) storage repositories attached to this pool."));
        } else
        {
            this->ui->comboBoxNetwork->setEnabled(false);
            this->ui->labelNetwork->setEnabled(false);
        }
    } else if (this->m_pool->HAEnabled())
    {
        this->disableControls(tr("You must disable HA before clustering can be enabled."));
    } else if (this->m_pool->IsPsrPending())
    {
        // C# also requires that no host is licence-restricted from pool secret rotation
        this->disableControls(tr("You cannot enable clustering while a pool secret rotation is in progress."));
    }

    this->ui->labelHostCountWarning->setVisible(cache->GetAll<Host>(XenObjectType::Host).size() < 3);
}

void ClusteringEditPage::disableControls(const QString& message)
{
    this->ui->comboBoxNetwork->setEnabled(false);
    this->ui->labelNetwork->setEnabled(false);
    this->ui->CheckBoxEnableClustering->setEnabled(false);
    this->ui->labelWarning->setVisible(true);
    this->ui->pictureBoxInfo1->setVisible(true);
    this->ui->labelWarning->setText(message);
}

void ClusteringEditPage::onNetworkChanged()
{
    QToolTip::hideText();
    if (!this->m_clusteringEnabled)
        this->ui->CheckBoxEnableClustering->setChecked(this->ui->comboBoxNetwork->currentIndex() >= 0);
}

QSharedPointer<Network> ClusteringEditPage::selectedNetwork() const
{
    const QString ref = this->ui->comboBoxNetwork->currentData().toString();
    XenCache* cache = this->m_pool ? this->m_pool->GetCache() : nullptr;
    return cache && !ref.isEmpty() ? cache->ResolveObject<Network>(ref) : QSharedPointer<Network>();
}

AsyncOperation* ClusteringEditPage::SaveSettings()
{
    if (!this->m_pool || !this->HasChanged())
        return nullptr;

    if (!this->ui->CheckBoxEnableClustering->isChecked())
        return new DisableClusteringAction(this->m_pool);

    QSharedPointer<Network> network = this->selectedNetwork();
    return network ? new EnableClusteringAction(this->m_pool, network) : nullptr;
}

bool ClusteringEditPage::IsValidToSave() const
{
    return this->m_clusteringEnabled || this->ui->comboBoxNetwork->currentIndex() >= 0
           || !this->ui->CheckBoxEnableClustering->isChecked();
}

void ClusteringEditPage::ShowLocalValidationMessages()
{
    if (!this->IsValidToSave())
    {
        QComboBox* combo = this->ui->comboBoxNetwork;
        QToolTip::showText(combo->mapToGlobal(QPoint(0, combo->height())), tr("You must select a network"), combo);
    }
}

void ClusteringEditPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void ClusteringEditPage::Cleanup()
{
}

bool ClusteringEditPage::HasChanged() const
{
    return this->m_clusteringEnabled != this->ui->CheckBoxEnableClustering->isChecked();
}
