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

#include "vmappliancevmspage.h"
#include "ui_vmappliancevmspage.h"
#include "../commands/vm/assigntovappmenu.h"
#include "../iconmanager.h"
#include "../settingsmanager.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmapplianceactions.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <QHeaderView>

namespace
{
    const int ROLE_REF = Qt::UserRole;
    const int ROLE_NAME = Qt::UserRole + 1;

    // C# sorts the name column by VM (natural name order, then ref) and the check column
    // by check mark first, then by name (CA-69817)
    class VMItem : public QTableWidgetItem
    {
        public:
            using QTableWidgetItem::QTableWidgetItem;

            bool operator<(const QTableWidgetItem& other) const override
            {
                if (this->column() == 0 && this->checkState() != other.checkState())
                    return this->checkState() == Qt::Checked;  // checked rows first

                const int r = Misc::NaturalCompare(this->data(ROLE_NAME).toString(), other.data(ROLE_NAME).toString());
                if (r != 0)
                    return r < 0;
                return this->data(ROLE_REF).toString() < other.data(ROLE_REF).toString();
            }
    };
}

VMApplianceVMsPage::VMApplianceVMsPage(QWidget* parent) : IEditPage(parent), ui(new Ui::VMApplianceVMsPage)
{
    this->ui->setupUi(this);
    this->ui->tableVMs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(this->ui->tableVMs, &QTableWidget::itemChanged, this, &VMApplianceVMsPage::onItemChanged);
    connect(this->ui->buttonSelectAll, &QPushButton::clicked, this, &VMApplianceVMsPage::onSelectAll);
    connect(this->ui->buttonClearAll, &QPushButton::clicked, this, &VMApplianceVMsPage::onClearAll);
    connect(this->ui->searchBox, &QLineEdit::textChanged, this, &VMApplianceVMsPage::refill);
    this->updateCounterAndButtons();
}

VMApplianceVMsPage::~VMApplianceVMsPage()
{
    delete this->ui;
}

void VMApplianceVMsPage::SetPool(QSharedPointer<Pool> pool)
{
    this->m_pool = pool;
    if (pool && pool->GetConnection())
    {
        const bool isPool = pool->GetConnection()->GetCache()->GetPool() != nullptr;
        const QString name = pool->GetName();
        this->ui->labelVMsIn->setText(isPool ? tr("&VMs in pool '%1':").arg(name) : tr("&VMs in server '%1':").arg(name));
    }
    this->refill();
}

void VMApplianceVMsPage::SetSelectedVMs(const QList<QSharedPointer<VM>>& vms)
{
    this->m_selectedRefs.clear();
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (vm && !this->m_selectedRefs.contains(vm->OpaqueRef()))
            this->m_selectedRefs.append(vm->OpaqueRef());
    }
    this->refill();
}

QList<QSharedPointer<VM>> VMApplianceVMsPage::SelectedVMs() const
{
    QList<QSharedPointer<VM>> vms;
    if (!this->m_pool || !this->m_pool->GetConnection())
        return vms;

    XenCache* cache = this->m_pool->GetConnection()->GetCache();
    for (const QString& ref : this->m_selectedRefs)
    {
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, ref);
        if (vm)
            vms.append(vm);
    }
    return vms;
}

QStringList VMApplianceVMsPage::SelectedVMRefs() const
{
    return this->m_selectedRefs;
}

void VMApplianceVMsPage::refill()
{
    // C# RefreshTab / searchTextBox1_TextChanged: real VMs that are shown, filtered by the search box
    this->m_filling = true;
    QTableWidget* table = this->ui->tableVMs;
    const int sortColumn = table->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = table->horizontalHeader()->sortIndicatorOrder();
    table->setSortingEnabled(false);
    table->setRowCount(0);

    if (this->m_pool && this->m_pool->GetConnection())
    {
        XenCache* cache = this->m_pool->GetConnection()->GetCache();
        const bool showHidden = SettingsManager::instance().GetShowHiddenObjects();
        const QString filter = this->ui->searchBox->text().trimmed();

        for (const QSharedPointer<VM>& vm : cache->GetAll<VM>(XenObjectType::VM))
        {
            if (!vm || !vm->IsRealVM() || !vm->Show(showHidden))
                continue;
            if (!filter.isEmpty() && !vm->GetName().contains(filter, Qt::CaseInsensitive))
                continue;

            const QString ref = vm->OpaqueRef();
            QSharedPointer<VMAppliance> current = cache->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, vm->ApplianceRef());

            const int row = table->rowCount();
            table->insertRow(row);
            auto makeItem = [&](const QString& text)
            {
                auto* item = new VMItem(text);
                item->setData(ROLE_REF, ref);
                item->setData(ROLE_NAME, vm->GetName());
                return item;
            };
            QTableWidgetItem* check = makeItem(QString());
            check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            check->setCheckState(this->m_selectedRefs.contains(ref) ? Qt::Checked : Qt::Unchecked);
            table->setItem(row, 0, check);
            QTableWidgetItem* name = makeItem(vm->GetName());
            name->setIcon(IconManager::instance().GetIconForObject(vm));
            table->setItem(row, 1, name);
            table->setItem(row, 2, new QTableWidgetItem(vm->GetDescription()));
            table->setItem(row, 3, new QTableWidgetItem(current ? current->GetName() : tr("<None>")));
        }
    }

    table->setSortingEnabled(true);
    // C# sorts by the check column first time round
    if (sortColumn < 0 || sortColumn >= table->columnCount())
        table->sortItems(0, Qt::AscendingOrder);
    else
        table->sortItems(sortColumn, sortOrder);
    table->resizeColumnToContents(1);
    this->m_filling = false;
    this->updateCounterAndButtons();
}

void VMApplianceVMsPage::onItemChanged(QTableWidgetItem* item)
{
    if (this->m_filling || !item || item->column() != 0)
        return;

    const QString ref = item->data(ROLE_REF).toString();
    if (item->checkState() == Qt::Checked)
    {
        if (!this->m_selectedRefs.contains(ref))
            this->m_selectedRefs.append(ref);
    } else
    {
        this->m_selectedRefs.removeAll(ref);
    }
    this->m_warningAccepted = false;
    this->updateCounterAndButtons();
    emit this->selectionChanged();
}

void VMApplianceVMsPage::setAllChecked(bool checked)
{
    QTableWidget* table = this->ui->tableVMs;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (QTableWidgetItem* item = table->item(row, 0))
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void VMApplianceVMsPage::onSelectAll()
{
    this->setAllChecked(true);
}

void VMApplianceVMsPage::onClearAll()
{
    this->setAllChecked(false);
}

void VMApplianceVMsPage::updateCounterAndButtons()
{
    const int count = this->m_selectedRefs.size();
    this->ui->labelCounter->setText(count == 1 ? tr("1 VM selected") : tr("%1 VMs selected").arg(count));
    this->ui->buttonClearAll->setEnabled(count > 0);
    this->ui->buttonSelectAll->setEnabled(count < this->ui->tableVMs->rowCount());
}

bool VMApplianceVMsPage::UserAcceptsWarning()
{
    if (this->m_warningAccepted)
        return true;

    const QString groupName = this->m_appliance ? this->m_appliance->GetName() : this->m_groupName;
    this->m_warningAccepted = AssignToVAppMenu::ChangesOK(this->SelectedVMs(), this->m_appliance, groupName, this);
    return this->m_warningAccepted;
}

QString VMApplianceVMsPage::GetText() const
{
    return tr("Virtual Machines");
}

QString VMApplianceVMsPage::GetSubText() const
{
    return this->ui->labelCounter->text();
}

QIcon VMApplianceVMsPage::GetImage() const
{
    return QIcon(":/tree-icons/vm_generic.png");
}

void VMApplianceVMsPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    this->m_appliance = qSharedPointerDynamicCast<VMAppliance>(object);
    if (!this->m_appliance || !this->m_appliance->GetConnection())
        return;

    // C# SetXenObjects: the vApp's current members start checked
    this->m_selectedRefs = this->m_appliance->VMRefs();
    this->m_warningAccepted = false;
    this->SetPool(this->m_appliance->GetConnection()->GetCache()->GetPoolOfOne());
}

AsyncOperation* VMApplianceVMsPage::SaveSettings()
{
    if (!this->m_appliance || !this->HasChanged())
        return nullptr;
    return new AssignVMsToVMApplianceAction(this->m_appliance, this->m_selectedRefs, true);
}

bool VMApplianceVMsPage::IsValidToSave() const
{
    // C# ValidToSave runs the same "already in another vApp" question
    return const_cast<VMApplianceVMsPage*>(this)->UserAcceptsWarning();
}

void VMApplianceVMsPage::ShowLocalValidationMessages()
{
}

void VMApplianceVMsPage::HideLocalValidationMessages()
{
}

void VMApplianceVMsPage::Cleanup()
{
    this->ui->tableVMs->setRowCount(0);
}

bool VMApplianceVMsPage::HasChanged() const
{
    if (!this->m_appliance)
        return false;
    // C# Helpers.CompareLists: same members in any order
    const QStringList current = this->m_appliance->VMRefs();
    return QSet<QString>(current.begin(), current.end()) != QSet<QString>(this->m_selectedRefs.begin(), this->m_selectedRefs.end());
}
