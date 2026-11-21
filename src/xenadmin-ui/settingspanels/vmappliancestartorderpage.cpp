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

#include "vmappliancestartorderpage.h"
#include "ui_vmappliancestartorderpage.h"
#include "../iconmanager.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/setvmstartupoptionsaction.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <QHeaderView>
#include <QSignalBlocker>

namespace
{
    const int ROLE_REF = Qt::UserRole;
    const int ROLE_SORT = Qt::UserRole + 1;

    // Names sort like VMs elsewhere (natural order); the number columns numerically
    class SortItem : public QTableWidgetItem
    {
        public:
            using QTableWidgetItem::QTableWidgetItem;

            bool operator<(const QTableWidgetItem& other) const override
            {
                const QVariant a = this->data(ROLE_SORT);
                const QVariant b = other.data(ROLE_SORT);
                if (a.userType() == QMetaType::LongLong)
                    return a.toLongLong() < b.toLongLong();
                return Misc::NaturalCompare(a.toString(), b.toString()) < 0;
            }
    };
}

VMApplianceStartOrderPage::VMApplianceStartOrderPage(QWidget* parent) : IEditPage(parent), ui(new Ui::VMApplianceStartOrderPage)
{
    this->ui->setupUi(this);
    connect(this->ui->tableVMs, &QTableWidget::itemSelectionChanged, this, &VMApplianceStartOrderPage::onSelectionChanged);
    connect(this->ui->spinOrder, QOverload<int>::of(&QSpinBox::valueChanged), this, &VMApplianceStartOrderPage::onOrderChanged);
    connect(this->ui->spinStartDelay, QOverload<int>::of(&QSpinBox::valueChanged), this, &VMApplianceStartOrderPage::onStartDelayChanged);
    this->onSelectionChanged();
}

VMApplianceStartOrderPage::~VMApplianceStartOrderPage()
{
    delete this->ui;
}

void VMApplianceStartOrderPage::SetSelectedVMs(const QList<QSharedPointer<VM>>& vms)
{
    // C# SetSelectedVMs: edits made to VMs that are still listed survive
    QMap<QString, Settings> kept;
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (!vm)
            continue;
        const QString ref = vm->OpaqueRef();
        kept.insert(ref, this->m_settings.contains(ref) ? this->m_settings.value(ref) : Settings{vm->Order(), vm->StartDelay()});
    }
    this->m_vms = vms;
    this->m_settings = kept;
    this->refill();
}

QMap<QString, QVariantMap> VMApplianceStartOrderPage::ChangedSettings() const
{
    QMap<QString, QVariantMap> result;
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (!vm)
            continue;
        const Settings s = this->m_settings.value(vm->OpaqueRef());
        if (s.order != vm->Order() || s.startDelay != vm->StartDelay())
            result.insert(vm->OpaqueRef(), QVariantMap{{"order", s.order}, {"start_delay", s.startDelay}});
    }
    return result;
}

void VMApplianceStartOrderPage::refill()
{
    QTableWidget* table = this->ui->tableVMs;
    table->setSortingEnabled(false);
    table->setRowCount(0);
    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (!vm)
            continue;
        const int row = table->rowCount();
        table->insertRow(row);

        auto* name = new SortItem(vm->GetName());
        name->setData(ROLE_REF, vm->OpaqueRef());
        name->setData(ROLE_SORT, vm->GetName());
        name->setIcon(IconManager::instance().GetIconForObject(vm));
        table->setItem(row, 0, name);
        auto* description = new SortItem(vm->GetDescription());
        description->setData(ROLE_SORT, vm->GetDescription());
        table->setItem(row, 1, description);
        table->setItem(row, 2, new SortItem());
        table->setItem(row, 3, new SortItem());
        this->updateRow(row);
    }
    table->setSortingEnabled(true);
    table->sortItems(0, Qt::AscendingOrder);
    table->resizeColumnToContents(0);
    this->onSelectionChanged();
}

void VMApplianceStartOrderPage::updateRow(int row)
{
    QTableWidget* table = this->ui->tableVMs;
    const Settings s = this->m_settings.value(table->item(row, 0)->data(ROLE_REF).toString());
    table->item(row, 2)->setText(QString::number(s.order));
    table->item(row, 2)->setData(ROLE_SORT, s.order);
    table->item(row, 3)->setText(QString::number(s.startDelay));
    table->item(row, 3)->setData(ROLE_SORT, s.startDelay);
}

QList<int> VMApplianceStartOrderPage::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex& index : this->ui->tableVMs->selectionModel()->selectedRows())
        rows.append(index.row());
    return rows;
}

void VMApplianceStartOrderPage::setSpin(QSpinBox* spin, bool same, qint64 value)
{
    // C# blanks the box when the selected VMs differ; -1 shown as nothing stands for that
    spin->setMinimum(same ? 0 : -1);
    spin->setSpecialValueText(same ? QString() : QStringLiteral(" "));
    spin->setValue(same ? int(qMin<qint64>(value, spin->maximum())) : -1);
}

void VMApplianceStartOrderPage::onSelectionChanged()
{
    // C# updateTextBoxes
    const QList<int> rows = this->selectedRows();
    this->m_updating = true;
    this->ui->spinOrder->setEnabled(!rows.isEmpty());
    this->ui->spinStartDelay->setEnabled(!rows.isEmpty());
    if (rows.isEmpty())
    {
        this->setSpin(this->ui->spinOrder, false, 0);
        this->setSpin(this->ui->spinStartDelay, false, 0);
        this->m_updating = false;
        return;
    }

    bool sameOrder = true;
    bool sameDelay = true;
    const Settings first = this->m_settings.value(this->ui->tableVMs->item(rows.first(), 0)->data(ROLE_REF).toString());
    for (int row : rows)
    {
        const Settings s = this->m_settings.value(this->ui->tableVMs->item(row, 0)->data(ROLE_REF).toString());
        sameOrder &= s.order == first.order;
        sameDelay &= s.startDelay == first.startDelay;
    }
    this->setSpin(this->ui->spinOrder, sameOrder, first.order);
    this->setSpin(this->ui->spinStartDelay, sameDelay, first.startDelay);
    this->m_updating = false;
}

void VMApplianceStartOrderPage::onOrderChanged(int value)
{
    if (this->m_updating || value < 0)
        return;

    bool changed = false;
    for (int row : this->selectedRows())
    {
        Settings& s = this->m_settings[this->ui->tableVMs->item(row, 0)->data(ROLE_REF).toString()];
        if (s.order != value)
        {
            s.order = value;
            changed = true;
            this->updateRow(row);
        }
    }
    if (changed)
    {
        this->m_changesMade = true;
        this->onSelectionChanged();
    }
}

void VMApplianceStartOrderPage::onStartDelayChanged(int value)
{
    if (this->m_updating || value < 0)
        return;

    bool changed = false;
    for (int row : this->selectedRows())
    {
        Settings& s = this->m_settings[this->ui->tableVMs->item(row, 0)->data(ROLE_REF).toString()];
        if (s.startDelay != value)
        {
            s.startDelay = value;
            changed = true;
            this->updateRow(row);
        }
    }
    if (changed)
    {
        this->m_changesMade = true;
        this->onSelectionChanged();
    }
}

QString VMApplianceStartOrderPage::GetText() const
{
    return tr("VM Startup Sequence");
}

QString VMApplianceStartOrderPage::GetSubText() const
{
    return QString();
}

QIcon VMApplianceStartOrderPage::GetImage() const
{
    return QIcon(":/icons/reboot_vm_16.png");
}

void VMApplianceStartOrderPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    // The VMs come from the VMs page when this page is shown (C# PropertiesDialog), so start
    // with the vApp's current members
    QSharedPointer<VMAppliance> appliance = qSharedPointerDynamicCast<VMAppliance>(object);
    this->m_settings.clear();
    this->m_changesMade = false;
    if (!appliance || !appliance->GetConnection())
        return;

    QList<QSharedPointer<VM>> vms;
    XenCache* cache = appliance->GetConnection()->GetCache();
    for (const QString& ref : appliance->VMRefs())
    {
        if (QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref))
            vms.append(vm);
    }
    this->SetSelectedVMs(vms);
}

AsyncOperation* VMApplianceStartOrderPage::SaveSettings()
{
    const QMap<QString, QVariantMap> changed = this->ChangedSettings();
    if (changed.isEmpty() || !this->m_object || !this->m_object->GetConnection())
        return nullptr;

    // C# SetVMStartupOptionsAction syncs the pool database afterwards
    QSharedPointer<Pool> pool = this->m_object->GetConnection()->GetCache()->GetPoolOfOne();
    return new SetVMStartupOptionsAction(this->m_object->GetConnection(), pool ? pool->OpaqueRef() : QString(), changed);
}

bool VMApplianceStartOrderPage::IsValidToSave() const
{
    return true;
}

void VMApplianceStartOrderPage::ShowLocalValidationMessages()
{
}

void VMApplianceStartOrderPage::HideLocalValidationMessages()
{
}

void VMApplianceStartOrderPage::Cleanup()
{
    this->ui->tableVMs->setRowCount(0);
}

bool VMApplianceStartOrderPage::HasChanged() const
{
    return this->m_changesMade;
}
