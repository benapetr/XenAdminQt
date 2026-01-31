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

#include "affinitypicker.h"
#include "ui_affinitypicker.h"
#include "../iconmanager.h"
#include "xen/network/connection.h"
#include "xen/host.h"
#include "xen/hostmetrics.h"
#include "xen/pbd.h"
#include "xen/pool.h"
#include "xen/sr.h"
#include "xencache.h"
#include <QHeaderView>
#include <QShowEvent>
#include <QSet>
#include <algorithm>

AffinityPicker::AffinityPicker(QWidget* parent) : QWidget(parent), ui(new Ui::AffinityPicker), m_connection(nullptr), m_autoSelectAffinity(true), m_selectedOnVisibleChanged(false)
{
    this->ui->setupUi(this);

    this->ui->serversTable->setSelectionMode(QAbstractItemView::SingleSelection);
    this->ui->serversTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->serversTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->serversTable->verticalHeader()->setVisible(false);
    this->ui->serversTable->setIconSize(QSize(16, 16));

    QHeaderView* header = this->ui->serversTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    this->ui->serversTable->setColumnWidth(0, 24);

    connect(this->ui->staticRadioButton, &QRadioButton::toggled, this, &AffinityPicker::onStaticRadioToggled);
    connect(this->ui->serversTable, &QTableWidget::itemSelectionChanged, this, &AffinityPicker::onSelectionChanged);
}

AffinityPicker::~AffinityPicker()
{
    delete this->ui;
}

void AffinityPicker::setAffinity(XenConnection* connection, const QString& affinityRef, const QString& srHostRef)
{
    this->m_connection = connection;
    this->m_affinityRef = affinityRef;
    this->m_srHostRef = srHostRef;

    bool wlbEnabled = false;
    if (this->m_connection && this->m_connection->GetCache())
    {
        QStringList poolRefs = this->m_connection->GetCache()->GetAllRefs(XenObjectType::Pool);
        if (!poolRefs.isEmpty())
        {
            QSharedPointer<Pool> pool = this->m_connection->GetCache()->ResolveObject<Pool>(poolRefs.first());
            if (pool)
                wlbEnabled = pool->IsWLBEnabled() && !pool->WLBUrl().isEmpty();
        }
    }
    this->ui->wlbWarningWidget->setVisible(wlbEnabled);

    this->loadServers();
    this->updateControl();
    this->selectRadioButtons();
    emit this->selectedAffinityChanged();
}

QString AffinityPicker::selectedAffinityRef() const
{
    if (this->ui->dynamicRadioButton->isChecked())
        return QString();

    QList<QTableWidgetItem*> selectedItems = this->ui->serversTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    QTableWidgetItem* item = this->ui->serversTable->item(selectedItems.first()->row(), 1);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

bool AffinityPicker::validState() const
{
    return this->ui->dynamicRadioButton->isChecked() || !this->selectedAffinityRef().isEmpty();
}

void AffinityPicker::setAutoSelectAffinity(bool enabled)
{
    this->m_autoSelectAffinity = enabled;
}

bool AffinityPicker::autoSelectAffinity() const
{
    return this->m_autoSelectAffinity;
}

void AffinityPicker::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!this->m_selectedOnVisibleChanged)
    {
        this->m_selectedOnVisibleChanged = true;
        this->selectSomething();
    }
}

void AffinityPicker::onStaticRadioToggled(bool checked)
{
    if (checked && this->selectedAffinityRef().isEmpty())
        this->selectSomething();

    this->updateControl();
    emit this->selectedAffinityChanged();
}

void AffinityPicker::onSelectionChanged()
{
    this->updateControl();
    emit this->selectedAffinityChanged();
}

void AffinityPicker::loadServers()
{
    this->ui->serversTable->setRowCount(0);
    this->m_hosts.clear();

    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    QList<QSharedPointer<Host>> hosts = this->m_connection->GetCache()->GetAll<Host>();

    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
        return a->GetName().toLower() < b->GetName().toLower();
    });

    for (const QSharedPointer<Host>& host : hosts)
    {
        QString hostRef = host->OpaqueRef();
        if (hostRef.isEmpty())
            continue;

        this->m_hosts.insert(hostRef, host);

        QString hostName = host->GetName();
        bool isLive = this->isHostLive(host);
        QString reason = isLive ? QString()
                                : tr("This server cannot be contacted");

        int row = this->ui->serversTable->rowCount();
        this->ui->serversTable->insertRow(row);

        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(IconManager::instance().GetIconForHost(host.data()));
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
        this->ui->serversTable->setItem(row, 0, iconItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(hostName);
        nameItem->setData(Qt::UserRole, hostRef);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        this->ui->serversTable->setItem(row, 1, nameItem);

        QTableWidgetItem* reasonItem = new QTableWidgetItem(reason);
        reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEditable);
        this->ui->serversTable->setItem(row, 2, reasonItem);

        if (!isLive)
        {
            iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEnabled);
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEnabled);
            reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEnabled);
        }
    }
}

void AffinityPicker::updateControl()
{
    if (!this->m_connection)
        return;

    bool sharedStorage = this->hasFullyConnectedSharedStorage();
    bool dynamicEnabled = (sharedStorage && this->m_srHostRef.isEmpty())
        || (this->m_affinityRef.isEmpty() && !this->m_autoSelectAffinity);

    this->ui->dynamicRadioButton->setEnabled(dynamicEnabled);
    this->ui->dynamicRadioButton->setText(sharedStorage
        ? tr("&Don't assign this VM a home server. The VM will be started on any server with the necessary resources.")
        : tr("&Don't assign this VM a home server. The VM will be started on any server with the necessary resources. (Shared storage required)."));

    this->ui->serversTable->setEnabled(this->ui->staticRadioButton->isChecked());
}

void AffinityPicker::selectRadioButtons()
{
    if (!this->selectAffinityServer() && this->ui->dynamicRadioButton->isEnabled())
    {
        this->ui->dynamicRadioButton->setChecked(true);
        this->ui->staticRadioButton->setChecked(false);
    }
    else
    {
        this->ui->dynamicRadioButton->setChecked(false);
        this->ui->staticRadioButton->setChecked(true);
    }
}

bool AffinityPicker::selectAffinityServer()
{
    return !this->m_affinityRef.isEmpty() && this->selectServer(this->m_affinityRef);
}

bool AffinityPicker::selectServer(const QString& hostRef)
{
    for (int row = 0; row < this->ui->serversTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = this->ui->serversTable->item(row, 1);
        if (!item)
            continue;
        if (item->data(Qt::UserRole).toString() != hostRef)
            continue;
        if (!(item->flags() & Qt::ItemIsEnabled))
            return false;

        this->ui->serversTable->selectRow(row);
        return true;
    }
    return false;
}

bool AffinityPicker::selectSomething()
{
    bool selected = false;

    if (!this->m_affinityRef.isEmpty())
        selected = this->selectServer(this->m_affinityRef);

    if (!selected && !this->m_srHostRef.isEmpty())
        selected = this->selectServer(this->m_srHostRef);

    return selected;
}

bool AffinityPicker::hasFullyConnectedSharedStorage() const
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return false;

    QList<QSharedPointer<Host>> hosts = this->m_connection->GetCache()->GetAll<Host>();
    if (hosts.isEmpty())
        return false;

    QSet<QString> hostRefSet;
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host)
            continue;
        const QString hostRef = host->OpaqueRef();
        if (!hostRef.isEmpty())
            hostRefSet.insert(hostRef);
    }

    if (hostRefSet.size() <= 1)
        return true;

    QList<QSharedPointer<SR>> srs = this->m_connection->GetCache()->GetAll<SR>();
    for (const QSharedPointer<SR>& sr : srs)
    {
        if (!sr || !sr->IsValid())
            continue;
        if (!sr->IsShared())
            continue;

        QSet<QString> attachedHosts;
        const QList<QSharedPointer<PBD>> pbds = sr->GetPBDs();
        for (const QSharedPointer<PBD>& pbd : pbds)
        {
            if (!pbd || !pbd->IsCurrentlyAttached())
                continue;
            QString hostRef = pbd->GetHostRef();
            if (!hostRef.isEmpty())
                attachedHosts.insert(hostRef);
        }

        if (attachedHosts == hostRefSet)
            return true;
    }

    return false;
}

bool AffinityPicker::isHostLive(const QSharedPointer<Host>& host) const
{
    if (!host)
        return false;

    QSharedPointer<HostMetrics> metrics = host->GetMetrics();
    if (metrics && metrics->IsValid())
        return metrics->IsLive();

    return host->IsEnabled();
}
