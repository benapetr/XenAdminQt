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

#include "scheduledsnapshotsdialog.h"
#include "ui_scheduledsnapshotsdialog.h"
#include "newpolicywizard.h"
#include "vmsspropertiesdialog.h"
#include "warningdialogs/warningdialog.h"
#include "../settingsmanager.h"
#include "xenlib/alerts/policyalert.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/message.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmss.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmssactions.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <QHash>
#include <QHeaderView>
#include <QLocale>
#include <QPointer>
#include <QTimer>
#include <algorithm>

using namespace XenLib;

namespace
{
    const int ROLE_REF = Qt::UserRole;
    const int ROLE_SORT = Qt::UserRole + 1;
    const int ROLE_MESSAGE = Qt::UserRole + 2;

    QHash<XenConnection*, QPointer<ScheduledSnapshotsDialog>> openDialogs;

    QString elide(const QString& text, int length)
    {
        return text.length() > length ? text.left(length - 1) + QChar(0x2026) : text;
    }

    // C# DATEFORMAT_DMY_HM, in the user's locale
    QString formatDateTime(const QDateTime& time)
    {
        return QLocale().toString(time.date(), QLocale::ShortFormat) + " " + QLocale().toString(time.time(), QLocale::ShortFormat);
    }

    QString resultText(PolicyAlertType type)
    {
        switch (type)
        {
            case PolicyAlertType::Error:
                return QCoreApplication::translate("ScheduledSnapshotsDialog", "Failed");
            case PolicyAlertType::Warn:
                return QCoreApplication::translate("ScheduledSnapshotsDialog", "Warning");
            default:
                return QCoreApplication::translate("ScheduledSnapshotsDialog", "Succeeded");
        }
    }

    QIcon resultIcon(PolicyAlertType type)
    {
        switch (type)
        {
            case PolicyAlertType::Error:
                return QIcon(":/icons/error_16.png");
            case PolicyAlertType::Warn:
                return QIcon(":/icons/alert_16.png");
            default:
                return QIcon(":/icons/tick_h32bit_16.png");
        }
    }

    // Sorts by ROLE_SORT when set (numbers, dates), otherwise naturally by text
    class SortItem : public QTableWidgetItem
    {
        public:
            using QTableWidgetItem::QTableWidgetItem;
            bool operator<(const QTableWidgetItem& other) const override
            {
                const QVariant a = this->data(ROLE_SORT);
                const QVariant b = other.data(ROLE_SORT);
                if (a.isValid() || b.isValid())
                {
                    if (a.userType() == QMetaType::QDateTime || b.userType() == QMetaType::QDateTime)
                        return a.toDateTime() < b.toDateTime();
                    return a.toLongLong() < b.toLongLong();
                }
                return Misc::NaturalCompare(this->text(), other.text()) < 0;
            }
    };
}

ScheduledSnapshotsDialog::ScheduledSnapshotsDialog(QSharedPointer<Pool> pool, QWidget* parent)
    : QDialog(parent), ui(new Ui::ScheduledSnapshotsDialog), m_pool(pool)
{
    this->ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->ui->historyPanel->setVisible(false);
    this->ui->tableHistory->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->ui->tableHistory->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->ui->tableHistory->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(this->ui->buttonNew, &QPushButton::clicked, this, &ScheduledSnapshotsDialog::onNew);
    connect(this->ui->buttonProperties, &QPushButton::clicked, this, &ScheduledSnapshotsDialog::onProperties);
    connect(this->ui->buttonEnable, &QPushButton::clicked, this, &ScheduledSnapshotsDialog::onEnable);
    connect(this->ui->buttonRunNow, &QPushButton::clicked, this, &ScheduledSnapshotsDialog::onRunNow);
    connect(this->ui->buttonDelete, &QPushButton::clicked, this, &ScheduledSnapshotsDialog::onDelete);
    connect(this->ui->buttonShowHistory, &QToolButton::clicked, this, &ScheduledSnapshotsDialog::onToggleHistory);
    connect(this->ui->comboBoxTimeSpan, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScheduledSnapshotsDialog::refreshHistory);
    connect(this->ui->tableHistory, &QTableWidget::cellClicked, this, &ScheduledSnapshotsDialog::onHistoryCellClicked);
    connect(this->ui->tablePolicies, &QTableWidget::itemSelectionChanged, this, [this]()
    {
        if (this->m_filling)
            return;
        this->refreshButtons();
        this->refreshHistory();
    });
    connect(this->ui->tablePolicies, &QTableWidget::itemDoubleClicked, this, &ScheduledSnapshotsDialog::onProperties);

    // C# reloads on VMSS collection changes; messages bring the last results, so follow them too
    this->m_reloadTimer = new QTimer(this);
    this->m_reloadTimer->setSingleShot(true);
    this->m_reloadTimer->setInterval(300);
    connect(this->m_reloadTimer, &QTimer::timeout, this, &ScheduledSnapshotsDialog::fillPolicies);
    XenCache* cache = pool->GetConnection()->GetCache();
    connect(cache, &XenCache::objectChanged, this, &ScheduledSnapshotsDialog::onCacheObjectChanged);
    connect(cache, &XenCache::objectRemoved, this, &ScheduledSnapshotsDialog::onCacheObjectChanged);

    this->refreshPoolTitle();
    this->refreshButtons();
    this->ui->policiesStack->setCurrentWidget(this->ui->pageLoading);
    this->loadPolicies();
}

ScheduledSnapshotsDialog::~ScheduledSnapshotsDialog()
{
    delete this->ui;
}

void ScheduledSnapshotsDialog::ShowFor(QSharedPointer<Pool> pool, QWidget* parent)
{
    if (!pool || !pool->GetConnection())
        return;

    QPointer<ScheduledSnapshotsDialog>& dialog = openDialogs[pool->GetConnection()];
    if (dialog)
    {
        dialog->raise();
        dialog->activateWindow();
        return;
    }
    dialog = new ScheduledSnapshotsDialog(pool, parent);
    dialog->show();
}

void ScheduledSnapshotsDialog::onCacheObjectChanged(QSharedPointer<XenObject> object)
{
    if (!object)
        return;
    const XenObjectType type = object->GetObjectType();
    if (type == XenObjectType::VMSS || type == XenObjectType::Message)
        this->m_reloadTimer->start();
    else if (type == XenObjectType::VM)
        this->refreshPoolTitle();
}

void ScheduledSnapshotsDialog::loadPolicies()
{
    // C# LoadPolicies: the next run times need the coordinator's local time
    QSharedPointer<Host> coordinator = this->m_pool->GetMasterHost();
    if (!coordinator)
    {
        this->fillPolicies();
        return;
    }

    auto* action = new GetServerLocalTimeAction(coordinator);
    QPointer<ScheduledSnapshotsDialog> self(this);
    connect(action, &AsyncOperation::completed, action, [self, action]()
    {
        const bool hasTime = action->HasServerTime();
        const QDateTime serverTime = action->ServerLocalTime();
        const qint64 diff = action->ServerClientTimeZoneDiff();
        QMetaObject::invokeMethod(qApp, [self, hasTime, serverTime, diff]()
        {
            if (!self)
                return;
            self->m_hasServerTime = hasTime;
            self->m_serverLocalTime = serverTime;
            self->m_diffSeconds = diff;
            self->fillPolicies();
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    action->RunAsync(true);
}

QList<QVariantMap> ScheduledSnapshotsDialog::messagesFor(const QSharedPointer<VMSS>& policy) const
{
    QList<QVariantMap> messages;
    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QSharedPointer<Message>& message : cache->GetAll<Message>(XenObjectType::Message))
    {
        if (message && message->Cls() == "VMSS" && message->ObjUuid() == policy->GetUUID())
            messages.append(message->GetData());
    }
    std::sort(messages.begin(), messages.end(), [](const QVariantMap& a, const QVariantMap& b)
    {
        return Misc::ParseXenDateTime(a.value("timestamp").toString()) > Misc::ParseXenDateTime(b.value("timestamp").toString());
    });
    return messages;
}

void ScheduledSnapshotsDialog::fillPolicies()
{
    // C# action_Completed: rebuild the rows, keeping the selection
    QStringList selected;
    for (const QSharedPointer<VMSS>& policy : this->selectedPolicies())
        selected.append(policy->OpaqueRef());

    this->m_filling = true;
    this->m_busy.clear();
    QTableWidget* table = this->ui->tablePolicies;
    table->setSortingEnabled(false);
    table->setRowCount(0);

    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QSharedPointer<VMSS>& policy : cache->GetAll<VMSS>(XenObjectType::VMSS))
    {
        if (!policy)
            continue;

        int vmCount = 0;
        for (const QString& ref : policy->GetVMRefs())
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
            if (vm && vm->IsRealVM())
                ++vmCount;
        }

        // The schedule is in the server's time zone
        QDateTime nextOnServer, nextOnClient;
        if (this->m_hasServerTime)
            nextOnServer = policy->GetNextRunTime(this->m_serverLocalTime);
        if (nextOnServer.isValid())
            nextOnClient = VMSS::RoundToNearestQuarter(nextOnServer.addSecs(this->m_diffSeconds));

        const QList<QVariantMap> messages = this->messagesFor(policy);

        const int row = table->rowCount();
        table->insertRow(row);
        auto* name = new SortItem(policy->GetName());
        name->setData(ROLE_REF, policy->OpaqueRef());
        table->setItem(row, 0, name);
        table->setItem(row, 1, new SortItem(policy->Enabled() ? tr("Enabled") : tr("Disabled")));
        auto* count = new SortItem(QString::number(vmCount));
        count->setData(ROLE_SORT, vmCount);
        table->setItem(row, 2, count);
        auto* client = new SortItem(nextOnClient.isValid() ? formatDateTime(nextOnClient) : tr("Pool coordinator unreachable"));
        client->setData(ROLE_SORT, nextOnClient);
        table->setItem(row, 3, client);
        auto* server = new SortItem(nextOnServer.isValid() ? formatDateTime(nextOnServer) : tr("Pool coordinator unreachable"));
        server->setData(ROLE_SORT, nextOnServer);
        table->setItem(row, 4, server);
        if (messages.isEmpty())
        {
            table->setItem(row, 5, new SortItem(tr("Not yet run")));
        } else
        {
            const PolicyAlertType type = PolicyAlert::FromPriority(messages.first().value("priority").toLongLong());
            table->setItem(row, 5, new SortItem(resultIcon(type), resultText(type)));
        }
    }

    table->setSortingEnabled(true);
    if (table->horizontalHeader()->sortIndicatorSection() < 0)
        table->sortItems(0, Qt::AscendingOrder);
    table->resizeColumnsToContents();

    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (selected.contains(table->item(row, 0)->data(ROLE_REF).toString()))
            table->selectRow(row);
    }
    if (table->selectionModel()->selectedRows().isEmpty() && table->rowCount() > 0)
        table->selectRow(0);

    this->m_filling = false;
    this->ui->policiesStack->setCurrentWidget(this->ui->pagePolicies);
    this->refreshPoolTitle();
    this->refreshButtons();
    this->refreshHistory();
}

void ScheduledSnapshotsDialog::refreshPoolTitle()
{
    // C# RefreshPoolTitle
    int protectedVMs = 0;
    int realVMs = 0;
    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
    for (const QSharedPointer<VM>& vm : cache->GetAll<VM>(XenObjectType::VM))
    {
        if (!vm || !vm->IsRealVM() || !vm->Show(showHidden))
            continue;
        ++realVMs;
        const QString schedule = vm->SnapshotScheduleRef();
        if (!schedule.isEmpty() && cache->ResolveObject<VMSS>(XenObjectType::VMSS, schedule))
            ++protectedVMs;
    }

    const bool isPool = cache->GetPool() != nullptr;
    const QString name = elide(this->m_pool->GetName(), 45);
    this->ui->labelPolicyTitle->setText(isPool
        ? tr("Snapshot schedules defined in pool '%1' (%2 out of %3 VMs currently in schedule):").arg(name).arg(protectedVMs).arg(realVMs)
        : tr("Snapshot schedules defined in server '%1' (%2 out of %3 VMs currently in schedule):").arg(name).arg(protectedVMs).arg(realVMs));
}

QList<QSharedPointer<VMSS>> ScheduledSnapshotsDialog::selectedPolicies() const
{
    QList<QSharedPointer<VMSS>> policies;
    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QModelIndex& index : this->ui->tablePolicies->selectionModel()->selectedRows())
    {
        const QString ref = this->ui->tablePolicies->item(index.row(), 0)->data(ROLE_REF).toString();
        if (QSharedPointer<VMSS> policy = cache->ResolveObject<VMSS>(XenObjectType::VMSS, ref))
            policies.append(policy);
    }
    return policies;
}

void ScheduledSnapshotsDialog::refreshButtons()
{
    // C# RefreshButtons
    const QList<QSharedPointer<VMSS>> selected = this->selectedPolicies();
    if (selected.size() == 1)
    {
        const QSharedPointer<VMSS> policy = selected.first();
        const bool busy = this->m_busy.contains(policy->OpaqueRef());
        this->ui->buttonEnable->setText(policy->Enabled() ? tr("Disa&ble") : tr("Ena&ble"));
        this->ui->buttonEnable->setEnabled(!busy && (policy->VMCount() != 0 || policy->Enabled()));
        this->ui->buttonProperties->setEnabled(!busy);
        this->ui->buttonRunNow->setEnabled(!busy && policy->Enabled() && policy->VMCount() != 0);
        this->ui->comboBoxTimeSpan->setEnabled(!busy);
    } else
    {
        this->ui->buttonEnable->setEnabled(false);
        this->ui->buttonProperties->setEnabled(false);
        this->ui->buttonRunNow->setEnabled(false);
        this->ui->comboBoxTimeSpan->setEnabled(false);
    }

    bool anyIdle = false;
    for (const QSharedPointer<VMSS>& policy : selected)
        anyIdle |= !this->m_busy.contains(policy->OpaqueRef());
    this->ui->buttonDelete->setEnabled(anyIdle);
}

void ScheduledSnapshotsDialog::refreshHistory()
{
    // C# RefreshHistoryLabel and RefreshHistoryGrid
    QTableWidget* table = this->ui->tableHistory;
    table->setRowCount(0);

    const QList<QSharedPointer<VMSS>> selected = this->selectedPolicies();
    if (selected.size() != 1)
    {
        this->ui->labelHistory->clear();
        return;
    }

    const QSharedPointer<VMSS> policy = selected.first();
    this->ui->labelHistory->setText(tr("Run history for policy '%1':").arg(elide(policy->GetName(), 60)));

    const QList<QVariantMap> messages = this->messagesFor(policy);
    const int span = this->ui->comboBoxTimeSpan->currentIndex();
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(span == 1 ? -24 * 3600 : -7 * 24 * 3600);

    int added = 0;
    for (const QVariantMap& data : messages)
    {
        const QDateTime time = Misc::ParseXenDateTime(data.value("timestamp").toString()).toLocalTime();
        if (span == 0 && added >= 10)
            break;
        if (span != 0 && time < cutoff)
            break;

        PolicyAlert alert(this->m_pool->GetConnection(), data);
        const PolicyAlertType type = alert.GetPolicyType();
        const QString messageRef = data.value("opaque_ref", data.value("ref")).toString();
        const bool expanded = type == PolicyAlertType::Error && this->m_expandedMessages.contains(messageRef);

        const int row = table->rowCount();
        table->insertRow(row);
        auto* expand = new QTableWidgetItem();
        if (type == PolicyAlertType::Error)
            expand->setIcon(QIcon(expanded ? ":/icons/expanded_triangle.png" : ":/icons/contracted_triangle.png"));
        expand->setData(ROLE_MESSAGE, messageRef);
        table->setItem(row, 0, expand);
        table->setItem(row, 1, new QTableWidgetItem(resultIcon(type), QString()));
        table->setItem(row, 2, new QTableWidgetItem(formatDateTime(time)));
        // C# HistoryRow: an error row expands to show the per-VM failures
        table->setItem(row, 3, new QTableWidgetItem(expanded ? alert.GetTitle() + "\n\n" + alert.GetDescription() : alert.GetTitle()));
        ++added;
    }
    table->resizeRowsToContents();
}

void ScheduledSnapshotsDialog::onHistoryCellClicked(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem* item = this->ui->tableHistory->item(row, 0);
    if (!item || item->icon().isNull())
        return;  // only error rows expand
    const QString ref = item->data(ROLE_MESSAGE).toString();
    if (this->m_expandedMessages.contains(ref))
        this->m_expandedMessages.remove(ref);
    else
        this->m_expandedMessages.insert(ref);
    this->refreshHistory();
}

void ScheduledSnapshotsDialog::onNew()
{
    NewPolicyWizard::ShowFor(this->m_pool, {}, this);
}

void ScheduledSnapshotsDialog::onProperties()
{
    const QList<QSharedPointer<VMSS>> selected = this->selectedPolicies();
    if (selected.size() != 1 || this->m_busy.contains(selected.first()->OpaqueRef()))
        return;
    VMSSPropertiesDialog dialog(selected.first(), this);
    dialog.exec();
}

void ScheduledSnapshotsDialog::onEnable()
{
    const QList<QSharedPointer<VMSS>> selected = this->selectedPolicies();
    if (selected.size() != 1)
        return;
    this->m_busy.insert(selected.first()->OpaqueRef());
    this->refreshButtons();
    (new ChangePolicyEnabledAction(selected.first()))->RunAsync(true);
}

void ScheduledSnapshotsDialog::onRunNow()
{
    const QList<QSharedPointer<VMSS>> selected = this->selectedPolicies();
    if (selected.size() != 1)
        return;
    this->m_busy.insert(selected.first()->OpaqueRef());
    this->refreshButtons();
    auto* action = new RunPolicyNowAction(selected.first());
    // Running doesn't change the schedule object, so nothing else clears the busy state
    const QString ref = selected.first()->OpaqueRef();
    QPointer<ScheduledSnapshotsDialog> self(this);
    connect(action, &AsyncOperation::completed, qApp, [self, ref]()
    {
        if (!self)
            return;
        self->m_busy.remove(ref);
        self->refreshButtons();
    }, Qt::QueuedConnection);
    action->RunAsync(true);
}

void ScheduledSnapshotsDialog::onDelete()
{
    const QList<QSharedPointer<VMSS>> policies = this->selectedPolicies();
    if (policies.isEmpty())
        return;

    int vmCount = 0;
    for (const QSharedPointer<VMSS>& policy : policies)
        vmCount += policy->VMCount();

    QString text;
    if (policies.size() == 1)
    {
        text = vmCount == 0
            ? tr("Do you want to delete policy '%1'?").arg(policies.first()->GetName())
            : tr("Do you want to delete policy '%1'?\n\n%2 VMs are currently protected with this policy and will no longer be protected if you delete the policy.")
                  .arg(policies.first()->GetName()).arg(vmCount);
    } else
    {
        text = vmCount == 0
            ? tr("Do you want to delete the selected policies?")
            : tr("Do you want to delete the selected policies?\n\n%1 VMs are currently protected with these policies and will no longer be protected if you delete the policies.")
                  .arg(vmCount);
    }

    if (WarningDialog::ShowYesNo(text, tr("Delete Snapshot Schedule"), this) != WarningDialog::Result::Yes)
        return;

    for (const QSharedPointer<VMSS>& policy : policies)
        this->m_busy.insert(policy->OpaqueRef());
    this->refreshButtons();
    (new DestroyPolicyAction(this->m_pool->GetConnection(), policies))->RunAsync(true);
}

void ScheduledSnapshotsDialog::onToggleHistory()
{
    const bool show = !this->ui->historyPanel->isVisible();
    this->ui->historyPanel->setVisible(show);
    this->ui->buttonShowHistory->setText(show ? tr("Hide Run History") : tr("Show Run History"));
    this->ui->buttonShowHistory->setArrowType(show ? Qt::UpArrow : Qt::DownArrow);
}
