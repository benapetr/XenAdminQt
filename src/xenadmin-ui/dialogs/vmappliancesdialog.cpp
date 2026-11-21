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

#include "vmappliancesdialog.h"
#include "ui_vmappliancesdialog.h"
#include "newvmappliancewizard.h"
#include "vmappliancepropertiesdialog.h"
#include "warningdialogs/warningdialog.h"
#include "../iconmanager.h"
#include "../mainwindow.h"
#include "../commands/vm/importvmcommand.h"
#include "../commands/vm/vappexportcommand.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmapplianceactions.h"
#include "xenlib/xen/actions/vm/startapplianceaction.h"
#include "xenlib/xen/actions/vm/shutdownapplianceaction.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <QHash>
#include <QHeaderView>
#include <QPointer>
#include <QTimer>
#include <algorithm>

namespace
{
    const int ROLE_REF = Qt::UserRole;

    QHash<XenConnection*, QPointer<VMAppliancesDialog>> openDialogs;

    QString elide(const QString& text, int length)
    {
        return text.length() > length ? text.left(length - 1) + QChar(0x2026) : text;
    }

    class NameItem : public QTableWidgetItem
    {
        public:
            using QTableWidgetItem::QTableWidgetItem;
            bool operator<(const QTableWidgetItem& other) const override
            {
                const QVariant a = this->data(Qt::UserRole + 1);
                if (a.userType() == QMetaType::Int)
                    return a.toInt() < other.data(Qt::UserRole + 1).toInt();
                return Misc::NaturalCompare(this->text(), other.text()) < 0;
            }
    };
}

VMAppliancesDialog::VMAppliancesDialog(QSharedPointer<Pool> pool, QWidget* parent)
    : QDialog(parent), ui(new Ui::VMAppliancesDialog), m_pool(pool)
{
    this->ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);

    const QString name = pool->GetName();
    this->setWindowTitle(tr("Manage vApps - pool %1").arg(name));
    const bool isPool = pool->GetConnection()->GetCache()->GetPool() != nullptr;
    this->ui->labelInPool->setText(isPool ? tr("vApps defined in pool '%1': ").arg(elide(name, 250))
                                          : tr("vApps defined in server '%1': ").arg(elide(name, 250)));

    this->ui->buttonProperties->setIcon(QIcon(":/icons/edit_16.png"));
    this->ui->buttonStart->setIcon(QIcon(":/icons/start_vm.png"));
    this->ui->buttonShutdown->setIcon(QIcon(":/icons/shutdown.png"));
    this->ui->buttonExport->setIcon(QIcon(":/icons/export-16.png"));
    this->ui->splitter->setStretchFactor(0, 3);
    this->ui->splitter->setStretchFactor(1, 1);

    connect(this->ui->buttonNew, &QToolButton::clicked, this, &VMAppliancesDialog::onNew);
    connect(this->ui->buttonDelete, &QToolButton::clicked, this, &VMAppliancesDialog::onDelete);
    connect(this->ui->buttonProperties, &QToolButton::clicked, this, &VMAppliancesDialog::onProperties);
    connect(this->ui->buttonStart, &QToolButton::clicked, this, &VMAppliancesDialog::onStart);
    connect(this->ui->buttonShutdown, &QToolButton::clicked, this, &VMAppliancesDialog::onShutdown);
    connect(this->ui->buttonImport, &QToolButton::clicked, this, &VMAppliancesDialog::onImport);
    connect(this->ui->buttonExport, &QToolButton::clicked, this, &VMAppliancesDialog::onExport);
    connect(this->ui->tableAppliances, &QTableWidget::itemSelectionChanged, this, [this]()
    {
        this->refreshButtons();
        this->refreshMembers();
    });
    connect(this->ui->tableAppliances, &QTableWidget::itemDoubleClicked, this, &VMAppliancesDialog::onProperties);

    // C# follows vApp and VM changes in the cache; coalesce them into one reload
    this->m_reloadTimer = new QTimer(this);
    this->m_reloadTimer->setSingleShot(true);
    this->m_reloadTimer->setInterval(200);
    connect(this->m_reloadTimer, &QTimer::timeout, this, &VMAppliancesDialog::reload);
    XenCache* cache = pool->GetConnection()->GetCache();
    connect(cache, &XenCache::objectChanged, this, &VMAppliancesDialog::onCacheObjectChanged);
    connect(cache, &XenCache::objectRemoved, this, &VMAppliancesDialog::onCacheObjectChanged);

    this->reload();
}

VMAppliancesDialog::~VMAppliancesDialog()
{
    delete this->ui;
}

void VMAppliancesDialog::ShowFor(QSharedPointer<Pool> pool, QWidget* parent)
{
    if (!pool || !pool->GetConnection())
        return;

    QPointer<VMAppliancesDialog>& dialog = openDialogs[pool->GetConnection()];
    if (dialog)
    {
        dialog->raise();
        dialog->activateWindow();
        return;
    }
    dialog = new VMAppliancesDialog(pool, parent);
    dialog->show();
}

void VMAppliancesDialog::onCacheObjectChanged(QSharedPointer<XenObject> object)
{
    if (object && (object->GetObjectType() == XenObjectType::VMAppliance || object->GetObjectType() == XenObjectType::VM))
        this->m_reloadTimer->start();
}

QList<QSharedPointer<VMAppliance>> VMAppliancesDialog::selectedAppliances() const
{
    QList<QSharedPointer<VMAppliance>> appliances;
    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QModelIndex& index : this->ui->tableAppliances->selectionModel()->selectedRows())
    {
        const QString ref = this->ui->tableAppliances->item(index.row(), 0)->data(ROLE_REF).toString();
        if (QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, ref))
            appliances.append(appliance);
    }
    return appliances;
}

void VMAppliancesDialog::reload()
{
    // C# LoadVmAppliances, keeping the selection
    QStringList selected;
    for (const QSharedPointer<VMAppliance>& appliance : this->selectedAppliances())
        selected.append(appliance->OpaqueRef());

    QTableWidget* table = this->ui->tableAppliances;
    const QSignalBlocker blocker(table);
    table->setSortingEnabled(false);
    table->setRowCount(0);

    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QSharedPointer<VMAppliance>& appliance : cache->GetAll<VMAppliance>(XenObjectType::VMAppliance))
    {
        if (!appliance)
            continue;
        const int row = table->rowCount();
        table->insertRow(row);
        auto* name = new NameItem(appliance->GetName());
        name->setData(ROLE_REF, appliance->OpaqueRef());
        table->setItem(row, 0, name);
        table->setItem(row, 1, new NameItem(appliance->GetDescription()));
        auto* count = new NameItem(QString::number(appliance->VMRefs().size()));
        count->setData(Qt::UserRole + 1, int(appliance->VMRefs().size()));
        table->setItem(row, 2, count);
    }
    table->setSortingEnabled(true);
    if (table->horizontalHeader()->sortIndicatorSection() < 0)
        table->sortItems(0, Qt::AscendingOrder);
    table->resizeColumnToContents(0);

    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (selected.contains(table->item(row, 0)->data(ROLE_REF).toString()))
            table->selectRow(row);
    }

    this->refreshButtons();
    this->refreshMembers();
}

void VMAppliancesDialog::refreshButtons()
{
    // C# RefreshButtons
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    QSharedPointer<VMAppliance> current = selected.size() == 1 ? selected.first() : QSharedPointer<VMAppliance>();

    this->ui->buttonProperties->setEnabled(current != nullptr);
    this->ui->buttonDelete->setEnabled(!selected.isEmpty());

    bool canExport = current != nullptr;
    if (current)
    {
        XenCache* cache = current->GetConnection()->GetCache();
        for (const QString& ref : current->VMRefs())
        {
            QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
            if (!vm || vm->IsTemplate() || vm->IsLocked() || !vm->GetAllowedOperations().contains("export"))
            {
                canExport = false;
                break;
            }
        }
    }
    this->ui->buttonExport->setEnabled(canExport);

    const QStringList operations = current ? current->AllowedOperations() : QStringList();
    this->ui->buttonStart->setEnabled(operations.contains("start"));
    this->ui->buttonShutdown->setEnabled(operations.contains("clean_shutdown") || operations.contains("hard_shutdown"));

    this->ui->membersPanel->setVisible(current != nullptr);
}

void VMAppliancesDialog::refreshMembers()
{
    // C# RefreshGroupMembersPanel
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    this->ui->listVMs->clear();
    if (selected.size() != 1)
    {
        this->ui->labelApplianceName->clear();
        return;
    }

    QSharedPointer<VMAppliance> appliance = selected.first();
    this->ui->labelApplianceName->setText(elide(appliance->GetName(), 120));

    QList<QSharedPointer<VM>> vms;
    XenCache* cache = appliance->GetConnection()->GetCache();
    for (const QString& ref : appliance->VMRefs())
    {
        if (QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref))
            vms.append(vm);
    }
    std::sort(vms.begin(), vms.end(), [](const QSharedPointer<VM>& a, const QSharedPointer<VM>& b)
    {
        return Misc::NaturalCompare(a->GetName(), b->GetName()) < 0;
    });
    for (const QSharedPointer<VM>& vm : vms)
        this->ui->listVMs->addItem(new QListWidgetItem(IconManager::instance().GetIconForObject(vm), vm->GetName()));
}

void VMAppliancesDialog::onNew()
{
    NewVMApplianceWizard::ShowFor(this->m_pool, {}, this);
}

void VMAppliancesDialog::onDelete()
{
    const QList<QSharedPointer<VMAppliance>> appliances = this->selectedAppliances();
    if (appliances.isEmpty())
        return;

    int vmCount = 0;
    for (const QSharedPointer<VMAppliance>& appliance : appliances)
        vmCount += appliance->VMRefs().size();

    QString text;
    if (appliances.size() == 1)
    {
        const QString name = elide(appliances.first()->GetName(), 120);
        if (vmCount == 0)
            text = tr("Do you want to delete vApp '%1'?").arg(name);
        else if (vmCount == 1)
            text = tr("Do you want to delete vApp '%1'?\n\n%2 VM is currently included in this vApp.").arg(name).arg(vmCount);
        else
            text = tr("Do you want to delete vApp '%1'?\n\n%2 VMs are currently included in this vApp.").arg(name).arg(vmCount);
    } else
    {
        if (vmCount == 0)
            text = tr("Do you want to delete the selected vApps?");
        else if (vmCount == 1)
            text = tr("Do you want to delete the selected vApps?\n\n%1 VM is currently included in one of these vApps.").arg(vmCount);
        else
            text = tr("Do you want to delete the selected vApps?\n\n%1 VMs are currently included in these vApps.").arg(vmCount);
    }

    if (WarningDialog::ShowYesNo(text, tr("Delete vApp"), this) == WarningDialog::Result::Yes)
        (new DestroyVMApplianceAction(this->m_pool->GetConnection(), appliances))->RunAsync(true);
}

void VMAppliancesDialog::onProperties()
{
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    if (selected.size() != 1)
        return;
    VMAppliancePropertiesDialog dialog(selected.first(), this);
    dialog.exec();
}

void VMAppliancesDialog::onStart()
{
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    if (selected.size() != 1)
        return;
    (new StartApplianceAction(selected.first()->GetConnection(), selected.first()->OpaqueRef(), false))->RunAsync(true);
}

void VMAppliancesDialog::onShutdown()
{
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    if (selected.size() != 1)
        return;

    if (WarningDialog::ShowYesNo(tr("Are you sure you want to shut down the selected vApp?"),
                                 tr("Shut down all VMs in vApp"), this) != WarningDialog::Result::Yes)
        return;
    (new ShutDownApplianceAction(selected.first()->GetConnection(), selected.first()->OpaqueRef()))->RunAsync(true);
}

void VMAppliancesDialog::onImport()
{
    ImportVMCommand command(MainWindow::instance());
    if (command.CanRun())
        command.Run();
}

void VMAppliancesDialog::onExport()
{
    const QList<QSharedPointer<VMAppliance>> selected = this->selectedAppliances();
    if (selected.size() != 1)
        return;
    VappExportCommand command(MainWindow::instance());
    command.SetSelectionOverride({selected.first()});
    if (command.CanRun())
        command.Run();
}
