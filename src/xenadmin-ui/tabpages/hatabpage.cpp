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

#include "hatabpage.h"
#include "ui_hatabpage.h"
#include "../commands/pool/haconfigurecommand.h"
#include "../commands/pool/hadisablecommand.h"
#include "../mainwindow.h"
#include "xenlib/operations/operationmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/pool/disablehaaction.h"
#include "xenlib/xen/actions/pool/enablehaaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include <QHeaderView>
#include <QTimer>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <algorithm>

namespace
{
    constexpr int kHeartbeatInitializationDelayMs = 30000;

    QString connectionKey(const QSharedPointer<Pool>& pool)
    {
        if (!pool || !pool->GetConnection())
            return QString();
        return QString::number(reinterpret_cast<quintptr>(pool->GetConnection()));
    }
} // namespace

HATabPage::HATabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::HATabPage)
{
    this->ui->setupUi(this);

    this->ui->heartbeatTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->heartbeatTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->heartbeatTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->ui->heartbeatTable->verticalHeader()->setVisible(false);
    this->ui->heartbeatTable->horizontalHeader()->setStretchLastSection(false);
    this->ui->heartbeatTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->ui->heartbeatTable->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this->ui->configureButton, &QPushButton::clicked, this, &HATabPage::onConfigureClicked);
    connect(this->ui->disableButton, &QPushButton::clicked, this, &HATabPage::onDisableClicked);
    connect(this->ui->heartbeatTable, &QWidget::customContextMenuRequested, this, &HATabPage::onHeartbeatTableContextMenuRequested);

    this->m_heartbeatInitDelayTimer = new QTimer(this);
    this->m_heartbeatInitDelayTimer->setSingleShot(true);
    connect(this->m_heartbeatInitDelayTimer, &QTimer::timeout, this, &HATabPage::onHeartbeatInitializationElapsed);
}

HATabPage::~HATabPage()
{
    this->removeObject();
    delete this->ui;
}

bool HATabPage::IsApplicableForObjectType(const QString& objectType) const
{
    return objectType == "pool";
}

void HATabPage::OnPageShown()
{
    BaseTabPage::OnPageShown();
    this->refreshContent();
}

void HATabPage::OnPageHidden()
{
    BaseTabPage::OnPageHidden();
}

void HATabPage::removeObject()
{
    if (this->m_heartbeatInitDelayTimer)
        this->m_heartbeatInitDelayTimer->stop();

    if (this->m_connection && this->m_connection->GetCache())
    {
        XenCache* cache = this->m_connection->GetCache();
        disconnect(cache, &XenCache::objectChanged, this, &HATabPage::onCacheObjectChanged);
        disconnect(cache, &XenCache::objectRemoved, this, &HATabPage::onCacheObjectRemoved);
        disconnect(cache, &XenCache::bulkUpdateComplete, this, &HATabPage::onCacheBulkUpdateComplete);
        disconnect(cache, &XenCache::cacheCleared, this, &HATabPage::onCacheCleared);
    }

    OperationManager* opManager = OperationManager::instance();
    disconnect(opManager, &OperationManager::recordAdded, this, &HATabPage::onOperationUpdated);
    disconnect(opManager, &OperationManager::recordUpdated, this, &HATabPage::onOperationUpdated);
    disconnect(opManager, &OperationManager::recordRemoved, this, &HATabPage::onOperationUpdated);
}

void HATabPage::updateObject()
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    XenCache* cache = this->m_connection->GetCache();
    connect(cache, &XenCache::objectChanged, this, &HATabPage::onCacheObjectChanged, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &HATabPage::onCacheObjectRemoved, Qt::UniqueConnection);
    connect(cache, &XenCache::bulkUpdateComplete, this, &HATabPage::onCacheBulkUpdateComplete, Qt::UniqueConnection);
    connect(cache, &XenCache::cacheCleared, this, &HATabPage::onCacheCleared, Qt::UniqueConnection);

    OperationManager* opManager = OperationManager::instance();
    connect(opManager, &OperationManager::recordAdded, this, &HATabPage::onOperationUpdated, Qt::UniqueConnection);
    connect(opManager, &OperationManager::recordUpdated, this, &HATabPage::onOperationUpdated, Qt::UniqueConnection);
    connect(opManager, &OperationManager::recordRemoved, this, &HATabPage::onOperationUpdated, Qt::UniqueConnection);
}

QSharedPointer<Pool> HATabPage::getPool() const
{
    return qSharedPointerDynamicCast<Pool>(this->m_object);
}

QList<QSharedPointer<SR>> HATabPage::getHeartbeatSRs(const QSharedPointer<Pool>& pool) const
{
    QList<QSharedPointer<SR>> heartbeatSrs;
    if (!pool || !pool->GetCache())
        return heartbeatSrs;

    XenCache* cache = pool->GetCache();
    const QStringList statefiles = pool->HAStatefiles();
    for (const QString& vdiRef : statefiles)
    {
        QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(vdiRef);
        if (!vdi)
            continue;
        QSharedPointer<SR> sr = cache->ResolveObject<SR>(vdi->SRRef());
        if (!sr || heartbeatSrs.contains(sr))
            continue;
        heartbeatSrs.append(sr);
    }

    std::sort(heartbeatSrs.begin(), heartbeatSrs.end(), [](const QSharedPointer<SR>& a, const QSharedPointer<SR>& b) {
        const QString an = a ? a->GetName() : QString();
        const QString bn = b ? b->GetName() : QString();
        return QString::compare(an, bn, Qt::CaseInsensitive) < 0;
    });

    return heartbeatSrs;
}

bool HATabPage::hasActiveHAAction(const QSharedPointer<Pool>& pool) const
{
    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    if (!conn)
        return false;

    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->GetRecords();
    for (OperationManager::OperationRecord* record : records)
    {
        if (!record || !record->operation)
            continue;
        if (record->state != AsyncOperation::NotStarted && record->state != AsyncOperation::Running)
            continue;
        AsyncOperation* op = record->operation;
        if (op->GetConnection() != conn)
            continue;
        if (qobject_cast<EnableHAAction*>(op) || qobject_cast<DisableHAAction*>(op))
            return true;
    }

    return false;
}

void HATabPage::ensureHeartbeatInitializationTimer(const QSharedPointer<Pool>& pool)
{
    if (!pool || !pool->HAEnabled())
    {
        this->m_heartbeatInitDelayTimer->stop();
        this->m_heartbeatInitDelayElapsed = false;
        this->m_heartbeatInitConnectionId.clear();
        return;
    }

    const QString key = connectionKey(pool);
    if (key != this->m_heartbeatInitConnectionId)
    {
        this->m_heartbeatInitConnectionId = key;
        this->m_heartbeatInitDelayElapsed = false;
        this->m_heartbeatInitDelayTimer->start(kHeartbeatInitializationDelayMs);
    }
}

QString HATabPage::formatCurrentCapacity(const QSharedPointer<Pool>& pool) const
{
    if (!pool)
        return QString();
    const qint64 ntol = pool->HAHostFailuresToTolerate();
    const qint64 plan = pool->HAPlanExistsFor();
    if (ntol <= plan)
        return QString::number(plan);
    return tr("%1 (overcommitted)").arg(plan);
}

void HATabPage::rebuildHeartbeatTable(const QSharedPointer<Pool>& pool)
{
    this->ui->heartbeatTable->clear();
    this->ui->heartbeatTable->setRowCount(0);
    this->ui->heartbeatTable->setColumnCount(0);

    if (!pool || !pool->HAEnabled())
        return;

    QList<QSharedPointer<Host>> hosts = pool->GetHosts();
    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
        const QString an = a ? a->GetName() : QString();
        const QString bn = b ? b->GetName() : QString();
        return QString::compare(an, bn, Qt::CaseInsensitive) < 0;
    });

    const QList<QSharedPointer<SR>> heartbeatSrs = this->getHeartbeatSRs(pool);
    const int columns = 2 + heartbeatSrs.size();
    this->ui->heartbeatTable->setColumnCount(columns);
    this->ui->heartbeatTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    QStringList headers;
    headers << tr("Host") << tr("Network");
    for (const QSharedPointer<SR>& sr : heartbeatSrs)
        headers << (sr ? sr->GetName() : tr("Heartbeat SR"));
    this->ui->heartbeatTable->setHorizontalHeaderLabels(headers);
    this->ui->heartbeatTable->setRowCount(hosts.size());

    XenCache* cache = pool->GetCache();
    for (int row = 0; row < hosts.size(); ++row)
    {
        const QSharedPointer<Host>& host = hosts.at(row);
        if (!host)
            continue;

        this->ui->heartbeatTable->setItem(row, 0, new QTableWidgetItem(host->GetName()));

        const int totalHosts = hosts.size();
        const int peerCount = host->HANetworkPeers().size();
        QString netStatus;
        if (!this->m_heartbeatInitDelayElapsed)
            netStatus = tr("Initializing...");
        else if (peerCount == 0)
            netStatus = tr("Unhealthy");
        else if (peerCount >= totalHosts)
            netStatus = tr("Healthy");
        else
            netStatus = tr("%1/%2 reachable").arg(peerCount).arg(totalHosts);
        this->ui->heartbeatTable->setItem(row, 1, new QTableWidgetItem(netStatus));

        const QStringList hostStatefiles = host->HAStatefiles();
        for (int i = 0; i < heartbeatSrs.size(); ++i)
        {
            const QSharedPointer<SR>& sr = heartbeatSrs.at(i);
            bool healthy = false;

            if (cache && sr)
            {
                for (const QString& stateVdiRef : hostStatefiles)
                {
                    QSharedPointer<VDI> vdi = cache->ResolveObject<VDI>(stateVdiRef);
                    if (!vdi)
                        continue;
                    if (vdi->SRRef() == sr->OpaqueRef())
                    {
                        healthy = true;
                        break;
                    }
                }
            }

            this->ui->heartbeatTable->setItem(row, 2 + i, new QTableWidgetItem(healthy ? tr("Healthy") : tr("Unhealthy")));
        }
    }
}

void HATabPage::updateCommandButtonStates(const QSharedPointer<Pool>& pool)
{
    HAConfigureCommand configureCommand(MainWindow::instance(), this);
    HADisableCommand disableCommand(MainWindow::instance(), this);
    if (pool)
    {
        configureCommand.SetSelectionOverride({pool});
        disableCommand.SetSelectionOverride({pool});
    }
    this->ui->configureButton->setEnabled(configureCommand.CanRun());
    this->ui->disableButton->setEnabled(disableCommand.CanRun());
}

void HATabPage::refreshContent()
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool || !pool->IsValid())
    {
        this->ui->statusLabel->setText(tr("No pool selected."));
        this->ui->configuredCapacityLabel->clear();
        this->ui->currentCapacityLabel->clear();
        this->ui->heartbeatTable->clear();
        this->ui->heartbeatTable->setRowCount(0);
        this->ui->heartbeatTable->setColumnCount(0);
        this->ui->configureButton->setEnabled(false);
        this->ui->disableButton->setEnabled(false);
        return;
    }

    this->ensureHeartbeatInitializationTimer(pool);

    if (this->hasActiveHAAction(pool))
    {
        this->ui->statusLabel->setText(tr("High Availability operation is in progress for this pool."));
    } else if (pool->HAEnabled())
    {
        this->ui->statusLabel->setText(tr("HA is configured for pool '%1'.").arg(pool->GetName()));
    } else
    {
        this->ui->statusLabel->setText(tr("HA is not configured for pool '%1'.").arg(pool->GetName()));
    }

    const qint64 ntol = pool->HAHostFailuresToTolerate();
    this->ui->configuredCapacityLabel->setText(tr("Configured capacity: %1").arg(ntol));
    this->ui->currentCapacityLabel->setText(tr("Current capacity: %1").arg(this->formatCurrentCapacity(pool)));

    if (pool->HAEnabled() && ntol == 0)
        this->ui->configuredCapacityLabel->setStyleSheet("color:#d9534f; font-weight:600;");
    else
        this->ui->configuredCapacityLabel->setStyleSheet(QString());

    if (pool->HAEnabled() && pool->HAHostFailuresToTolerate() > pool->HAPlanExistsFor())
        this->ui->currentCapacityLabel->setStyleSheet("color:#d9534f; font-weight:600;");
    else
        this->ui->currentCapacityLabel->setStyleSheet(QString());

    this->rebuildHeartbeatTable(pool);
    this->updateCommandButtonStates(pool);
}

void HATabPage::onConfigureClicked()
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool)
        return;

    HAConfigureCommand command(MainWindow::instance(), this);
    command.SetSelectionOverride({pool});
    if (command.CanRun())
        command.Run();
}

void HATabPage::onDisableClicked()
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool)
        return;

    HADisableCommand command(MainWindow::instance(), this);
    command.SetSelectionOverride({pool});
    if (command.CanRun())
        command.Run();
}

void HATabPage::onHeartbeatTableContextMenuRequested(const QPoint& pos)
{
    if (!this->ui->heartbeatTable)
        return;

    QMenu menu(this);
    QAction* copyAction = menu.addAction(tr("Copy"));
    connect(copyAction, &QAction::triggered, this, &HATabPage::onCopyHeartbeatRows);
    menu.exec(this->ui->heartbeatTable->viewport()->mapToGlobal(pos));
}

void HATabPage::onCopyHeartbeatRows()
{
    if (!this->ui->heartbeatTable)
        return;

    QList<int> rows;
    const auto selected = this->ui->heartbeatTable->selectionModel()
                              ? this->ui->heartbeatTable->selectionModel()->selectedRows()
                              : QModelIndexList();
    for (const QModelIndex& index : selected)
    {
        if (!rows.contains(index.row()))
            rows.append(index.row());
    }
    if (rows.isEmpty() && this->ui->heartbeatTable->currentRow() >= 0)
        rows.append(this->ui->heartbeatTable->currentRow());
    std::sort(rows.begin(), rows.end());

    QStringList lines;
    for (int row : rows)
    {
        QStringList cols;
        for (int col = 0; col < this->ui->heartbeatTable->columnCount(); ++col)
        {
            QTableWidgetItem* item = this->ui->heartbeatTable->item(row, col);
            cols.append(item ? item->text() : QString());
        }
        lines.append(cols.join("\t"));
    }

    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join("\n"));
}

void HATabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->m_connection || connection != this->m_connection)
        return;
    Q_UNUSED(ref);
    if (type == "pool" || type == "host" || type == "vdi")
        this->refreshContent();
}

void HATabPage::onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref)
{
    this->onCacheObjectChanged(connection, type, ref);
}

void HATabPage::onCacheBulkUpdateComplete(const QString& type, int count)
{
    Q_UNUSED(count);
    if (type == "pool" || type == "host" || type == "vdi")
        this->refreshContent();
}

void HATabPage::onCacheCleared()
{
    this->refreshContent();
}

void HATabPage::onOperationUpdated()
{
    this->refreshContent();
}

void HATabPage::onHeartbeatInitializationElapsed()
{
    this->m_heartbeatInitDelayElapsed = true;
    this->refreshContent();
}
