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
#include "xen/connection.h"
#include "xencache.h"
#include <QHeaderView>
#include <QShowEvent>
#include <QSet>
#include <algorithm>

AffinityPicker::AffinityPicker(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::AffinityPicker),
      m_connection(nullptr),
      m_autoSelectAffinity(true),
      m_selectedOnVisibleChanged(false)
{
    ui->setupUi(this);

    ui->serversTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serversTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->serversTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serversTable->verticalHeader()->setVisible(false);
    ui->serversTable->horizontalHeader()->setStretchLastSection(true);

    connect(ui->staticRadioButton, &QRadioButton::toggled,
            this, &AffinityPicker::onStaticRadioToggled);
    connect(ui->serversTable, &QTableWidget::itemSelectionChanged,
            this, &AffinityPicker::onSelectionChanged);
}

AffinityPicker::~AffinityPicker()
{
    delete ui;
}

void AffinityPicker::setAffinity(XenConnection* connection,
                                 const QString& affinityRef,
                                 const QString& srHostRef)
{
    m_connection = connection;
    m_affinityRef = affinityRef;
    m_srHostRef = srHostRef;

    bool wlbEnabled = false;
    if (m_connection && m_connection->getCache())
    {
        QStringList poolRefs = m_connection->getCache()->getAllRefs("pool");
        if (!poolRefs.isEmpty())
        {
            QVariantMap poolData = m_connection->getCache()->resolve("pool", poolRefs.first());
            QString wlbUrl = poolData.value("wlb_url").toString();
            wlbEnabled = poolData.value("wlb_enabled").toBool() && !wlbUrl.isEmpty();
        }
    }
    ui->wlbWarningWidget->setVisible(wlbEnabled);

    loadServers();
    updateControl();
    selectRadioButtons();
    emit selectedAffinityChanged();
}

QString AffinityPicker::selectedAffinityRef() const
{
    if (ui->dynamicRadioButton->isChecked())
        return QString();

    QList<QTableWidgetItem*> selectedItems = ui->serversTable->selectedItems();
    if (selectedItems.isEmpty())
        return QString();

    QTableWidgetItem* item = ui->serversTable->item(selectedItems.first()->row(), 1);
    return item ? item->data(Qt::UserRole).toString() : QString();
}

bool AffinityPicker::validState() const
{
    return ui->dynamicRadioButton->isChecked() || !selectedAffinityRef().isEmpty();
}

void AffinityPicker::setAutoSelectAffinity(bool enabled)
{
    m_autoSelectAffinity = enabled;
}

bool AffinityPicker::autoSelectAffinity() const
{
    return m_autoSelectAffinity;
}

void AffinityPicker::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!m_selectedOnVisibleChanged)
    {
        m_selectedOnVisibleChanged = true;
        selectSomething();
    }
}

void AffinityPicker::onStaticRadioToggled(bool checked)
{
    if (checked && selectedAffinityRef().isEmpty())
        selectSomething();

    updateControl();
    emit selectedAffinityChanged();
}

void AffinityPicker::onSelectionChanged()
{
    updateControl();
    emit selectedAffinityChanged();
}

void AffinityPicker::loadServers()
{
    ui->serversTable->setRowCount(0);
    m_hosts.clear();

    if (!m_connection || !m_connection->getCache())
        return;

    QList<QVariantMap> hosts = m_connection->getCache()->getAll("host");
    std::sort(hosts.begin(), hosts.end(), [](const QVariantMap& a, const QVariantMap& b) {
        return a.value("name_label").toString().toLower()
            < b.value("name_label").toString().toLower();
    });

    for (const QVariantMap& hostData : hosts)
    {
        QString hostRef = hostData.value("ref").toString();
        if (hostRef.isEmpty())
            hostRef = hostData.value("opaqueRef").toString();
        if (hostRef.isEmpty())
            hostRef = hostData.value("_ref").toString();
        if (hostRef.isEmpty())
            continue;

        m_hosts.insert(hostRef, hostData);

        QString hostName = hostData.value("name_label").toString();
        bool isLive = isHostLive(hostData);
        QString reason = isLive ? QString()
                                : tr("This server cannot be contacted");

        int row = ui->serversTable->rowCount();
        ui->serversTable->insertRow(row);

        QTableWidgetItem* iconItem = new QTableWidgetItem();
        iconItem->setIcon(IconManager::instance().getIconForHost(hostData));
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
        ui->serversTable->setItem(row, 0, iconItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(hostName);
        nameItem->setData(Qt::UserRole, hostRef);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->serversTable->setItem(row, 1, nameItem);

        QTableWidgetItem* reasonItem = new QTableWidgetItem(reason);
        reasonItem->setFlags(reasonItem->flags() & ~Qt::ItemIsEditable);
        ui->serversTable->setItem(row, 2, reasonItem);

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
    if (!m_connection)
        return;

    bool sharedStorage = hasFullyConnectedSharedStorage();
    bool dynamicEnabled = (sharedStorage && m_srHostRef.isEmpty())
        || (m_affinityRef.isEmpty() && !m_autoSelectAffinity);

    ui->dynamicRadioButton->setEnabled(dynamicEnabled);
    ui->dynamicRadioButton->setText(sharedStorage
        ? tr("&Don't assign this VM a home server. The VM will be started on any server with the necessary resources.")
        : tr("&Don't assign this VM a home server. The VM will be started on any server with the necessary resources. (Shared storage required)."));

    ui->serversTable->setEnabled(ui->staticRadioButton->isChecked());
}

void AffinityPicker::selectRadioButtons()
{
    if (!selectAffinityServer() && ui->dynamicRadioButton->isEnabled())
    {
        ui->dynamicRadioButton->setChecked(true);
        ui->staticRadioButton->setChecked(false);
    }
    else
    {
        ui->dynamicRadioButton->setChecked(false);
        ui->staticRadioButton->setChecked(true);
    }
}

bool AffinityPicker::selectAffinityServer()
{
    return !m_affinityRef.isEmpty() && selectServer(m_affinityRef);
}

bool AffinityPicker::selectServer(const QString& hostRef)
{
    for (int row = 0; row < ui->serversTable->rowCount(); ++row)
    {
        QTableWidgetItem* item = ui->serversTable->item(row, 1);
        if (!item)
            continue;
        if (item->data(Qt::UserRole).toString() != hostRef)
            continue;
        if (!(item->flags() & Qt::ItemIsEnabled))
            return false;

        ui->serversTable->selectRow(row);
        return true;
    }
    return false;
}

bool AffinityPicker::selectSomething()
{
    bool selected = false;

    if (!m_affinityRef.isEmpty())
        selected = selectServer(m_affinityRef);

    if (!selected && !m_srHostRef.isEmpty())
        selected = selectServer(m_srHostRef);

    return selected;
}

bool AffinityPicker::hasFullyConnectedSharedStorage() const
{
    if (!m_connection || !m_connection->getCache())
        return false;

    QList<QVariantMap> hosts = m_connection->getCache()->getAll("host");
    if (hosts.isEmpty())
        return false;

    QSet<QString> hostRefs;
    for (const QVariantMap& hostData : hosts)
    {
        QString hostRef = hostData.value("_ref").toString();
        if (!hostRef.isEmpty())
            hostRefs.insert(hostRef);
    }

    if (hostRefs.size() <= 1)
        return true;

    QList<QVariantMap> srs = m_connection->getCache()->getAll("sr");
    for (const QVariantMap& srData : srs)
    {
        if (!srData.value("shared").toBool())
            continue;

        QVariantList pbdRefs = srData.value("PBDs").toList();
        QSet<QString> attachedHosts;
        for (const QVariant& pbdRefVar : pbdRefs)
        {
            QString pbdRef = pbdRefVar.toString();
            QVariantMap pbdData = m_connection->getCache()->resolve("pbd", pbdRef);
            if (pbdData.isEmpty())
                continue;
            if (!pbdData.value("currently_attached").toBool())
                continue;
            QString hostRef = pbdData.value("host").toString();
            if (!hostRef.isEmpty())
                attachedHosts.insert(hostRef);
        }

        if (attachedHosts == hostRefs)
            return true;
    }

    return false;
}

bool AffinityPicker::isHostLive(const QVariantMap& hostData) const
{
    if (hostData.contains("live"))
        return hostData.value("live").toBool();
    return hostData.value("enabled", true).toBool();
}
