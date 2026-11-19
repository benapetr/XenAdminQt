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

#include "vtpmmanagementdialog.h"
#include "ui_vtpmmanagementdialog.h"
#include "actionprogressdialog.h"
#include "warningdialogs/warningdialog.h"
#include "xenlib/xen/actions/vm/vtpmaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vtpm.h"
#include "xenlib/xencache.h"

QHash<QString, QPointer<VtpmManagementDialog>> VtpmManagementDialog::s_openDialogs;

namespace
{
    QString ellipsise(const QString& text, int maxChars)
    {
        return text.length() > maxChars ? text.left(maxChars - 1) + QChar(0x2026) : text;
    }
}

QString VtpmManagementDialog::key(const QSharedPointer<VM>& vm)
{
    return QString::number(reinterpret_cast<quintptr>(vm->GetConnection())) + QLatin1Char('/') + vm->OpaqueRef();
}

void VtpmManagementDialog::ShowForVM(const QSharedPointer<VM>& vm, QWidget* parent)
{
    if (!vm)
        return;

    QPointer<VtpmManagementDialog> dialog = s_openDialogs.value(key(vm));
    if (!dialog)
    {
        dialog = new VtpmManagementDialog(vm, parent);
        s_openDialogs.insert(key(vm), dialog);
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

VtpmManagementDialog::VtpmManagementDialog(const QSharedPointer<VM>& vm, QWidget* parent)
    : QDialog(parent), ui(new Ui::VtpmManagementDialog), m_vm(vm)
{
    this->ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle(this->windowTitle().arg(ellipsise(vm->GetName(), 50)));
    this->ui->blurbLabel->setText(this->ui->blurbLabel->text().arg(ellipsise(vm->GetName(), 80)));

    connect(this->ui->addButton, &QPushButton::clicked, this, &VtpmManagementDialog::addVtpm);
    connect(this->ui->removeButton, &QPushButton::clicked, this, &VtpmManagementDialog::removeVtpm);
    connect(this->ui->vtpmList, &QListWidget::currentRowChanged, this, &VtpmManagementDialog::updateDetails);

    // C# listens to the VM (power_state, VTPMs) and each vTPM (is_protected, is_unique,
    // allowed_operations)
    if (XenCache* cache = vm->GetCache())
        connect(cache, &XenCache::itemChanged, this, &VtpmManagementDialog::onCacheItemChanged);

    this->refresh();
}

VtpmManagementDialog::~VtpmManagementDialog()
{
    delete this->ui;
}

void VtpmManagementDialog::onCacheItemChanged(XenConnection* connection, XenObjectType type, const QString& ref)
{
    if (connection != this->m_vm->GetConnection())
        return;
    if ((type == XenObjectType::VM && ref == this->m_vm->OpaqueRef()) || type == XenObjectType::VTPM)
        this->refresh();
}

QSharedPointer<VTPM> VtpmManagementDialog::selectedVtpm() const
{
    QListWidgetItem* item = this->ui->vtpmList->currentItem();
    if (!item || !this->m_vm->GetCache())
        return QSharedPointer<VTPM>();
    return this->m_vm->GetCache()->ResolveObject<VTPM>(item->data(Qt::UserRole).toString());
}

// C# RefreshTabs: keep the selection, list the VM's current vTPMs
void VtpmManagementDialog::refresh()
{
    const QSharedPointer<VTPM> previous = this->selectedVtpm();
    const QString previousRef = previous ? previous->OpaqueRef() : QString();

    const QSignalBlocker blocker(this->ui->vtpmList);
    this->ui->vtpmList->clear();
    int selectRow = 0;
    for (const QSharedPointer<VTPM>& vtpm : this->m_vm->GetVTPMs())
    {
        QListWidgetItem* item = new QListWidgetItem(QIcon(":/icons/tpm.png"), tr("Trusted Platform Module"), this->ui->vtpmList);
        item->setData(Qt::UserRole, vtpm->OpaqueRef());
        if (vtpm->OpaqueRef() == previousRef)
            selectRow = this->ui->vtpmList->count() - 1;
    }
    if (this->ui->vtpmList->count() > 0)
        this->ui->vtpmList->setCurrentRow(selectRow);

    QString cannotReason;
    this->ui->addButton->setEnabled(this->m_vm->CanAddVtpm(&cannotReason));
    this->ui->addButton->setToolTip(cannotReason);

    this->updateDetails();
}

// C# VtpmManagementPage.Repopulate
void VtpmManagementDialog::updateDetails()
{
    const QSharedPointer<VTPM> vtpm = this->selectedVtpm();
    if (!vtpm)
    {
        this->ui->pages->setCurrentIndex(0);
        return;
    }

    this->ui->pages->setCurrentIndex(1);
    this->ui->protectedValue->setText(vtpm->IsProtected() ? tr("Yes") : tr("No"));
    this->ui->uniqueValue->setText(vtpm->IsUnique() ? tr("Yes") : tr("No"));

    QString cannotReason;
    this->ui->removeButton->setEnabled(this->canRemoveVtpm(vtpm, &cannotReason));
    this->ui->removeButton->setToolTip(cannotReason);
}

bool VtpmManagementDialog::canRemoveVtpm(const QSharedPointer<VTPM>& vtpm, QString* cannotReason) const
{
    if (cannotReason)
        cannotReason->clear();

    if (Host::XapiEqualOrGreater(vtpm->GetConnection(), "23.11.0"))
    {
        if (vtpm->AllowedOperations().contains("destroy"))
            return true;
        if (cannotReason)
            *cannotReason = tr("Removing this vTPM from the VM is currently not allowed.");
        return false;
    }

    return this->m_vm->CanRemoveVtpm(cannotReason);
}

void VtpmManagementDialog::addVtpm()
{
    NewVtpmAction* action = new NewVtpmAction(this->m_vm->GetConnection(), this->m_vm);
    ActionProgressDialog dialog(action, this);
    dialog.exec();
    action->deleteLater();
    this->refresh();
}

void VtpmManagementDialog::removeVtpm()
{
    const QSharedPointer<VTPM> vtpm = this->selectedVtpm();
    if (!vtpm || !this->canRemoveVtpm(vtpm, nullptr))
        return;

    if (WarningDialog::ShowYesNo(tr("Removing the vTPM may result in the deactivation of some security features on the VM "
                                    "and prevent it from starting. Do you want to continue?"),
                                 tr("Remove vTPM"), this) != WarningDialog::Result::Yes)
        return;

    RemoveVtpmAction* action = new RemoveVtpmAction(vtpm->GetConnection(), vtpm, this->m_vm);
    ActionProgressDialog dialog(action, this);
    dialog.exec();
    action->deleteLater();
    this->refresh();
}
