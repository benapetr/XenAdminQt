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

#include "attachusbdialog.h"
#include "ui_attachusbdialog.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/actions/usb/createvusbaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pusb.h"
#include "xenlib/xen/usbgroup.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenapi/xenapi_VM.h"
#include "xenlib/xencache.h"
#include <QPointer>
#include <QPushButton>
#include <memory>

namespace
{
    const int PUSB_ROLE = Qt::UserRole + 1;
}

AttachUsbDialog::AttachUsbDialog(QSharedPointer<VM> vm, QWidget* parent) : QDialog(parent), ui(new Ui::AttachUsbDialog), m_vm(vm)
{
    this->ui->setupUi(this);

    this->m_attachButton = this->ui->buttonBox->addButton(tr("&Attach"), QDialogButtonBox::AcceptRole);
    this->m_attachButton->setDefault(true);
    connect(this->m_attachButton, &QPushButton::clicked, this, &AttachUsbDialog::attach);
    connect(this->ui->treeUsbList, &QTreeWidget::itemSelectionChanged, this, &AttachUsbDialog::updateAttachButton);

    this->buildList();
    this->updateAttachButton();

    // C#: the list is rebuilt once VM.get_possible_hosts returns
    const QString name = vm->GetName();
    QPointer<AttachUsbDialog> self(this);
    auto hostRefs = std::make_shared<QStringList>();
    auto* op = new DelegatedAsyncOperation(vm->GetConnection(),
        tr("Fetch possible hosts of VM %1").arg(name),
        tr("Fetching possible hosts of VM %1").arg(name),
        true,
        [vmRef = vm->OpaqueRef(), name, hostRefs](DelegatedAsyncOperation* action)
        {
            *hostRefs = XenAPI::VM::get_possible_hosts(action->GetSession(), vmRef);
            action->SetDescription(QObject::tr("Fetched possible hosts of VM %1").arg(name));
        });
    connect(op, &AsyncOperation::completed, this, [self, hostRefs]()
    {
        if (!self)
            return;
        XenCache* cache = self->m_vm->GetCache();
        self->m_possibleHosts.clear();
        for (const QString& ref : *hostRefs)
        {
            QSharedPointer<Host> host = cache ? cache->ResolveObject<Host>(ref) : QSharedPointer<Host>();
            if (host)
                self->m_possibleHosts.append(host);
        }
        self->buildList();
    });
    op->RunAsync(true);
}

AttachUsbDialog::~AttachUsbDialog()
{
    delete this->ui;
}

void AttachUsbDialog::buildList()
{
    this->ui->labelWarningLine3->setVisible(!this->m_vm->UsingUpstreamQemu());

    QTreeWidget* tree = this->ui->treeUsbList;
    tree->clear();

    for (const QSharedPointer<Host>& host : this->m_possibleHosts)
    {
        QList<QTreeWidgetItem*> usbItems;
        XenCache* cache = host->GetCache();
        for (const QString& pusbRef : host->PUSBRefs())
        {
            QSharedPointer<PUSB> pusb = cache ? cache->ResolveObject<PUSB>(pusbRef) : QSharedPointer<PUSB>();
            if (!pusb)
                continue;

            // C#: offer only passthrough-enabled devices not already given to a VM
            const QSharedPointer<USBGroup> group = pusb->GetUSBGroup();
            const bool attached = group && !group->GetVUSBRefs().isEmpty();
            if (!pusb->PassthroughEnabled() || attached)
                continue;

            auto* item = new QTreeWidgetItem(QStringList(QString("%1 %2").arg(pusb->Path(), pusb->Description())));
            item->setData(0, PUSB_ROLE, pusb->OpaqueRef());
            usbItems.append(item);
        }

        // C#: a host appears only when it has devices to offer
        if (usbItems.isEmpty())
            continue;
        auto* hostItem = new QTreeWidgetItem(QStringList(host->GetName()));
        hostItem->setFlags(Qt::ItemIsEnabled);
        hostItem->addChildren(usbItems);
        tree->addTopLevelItem(hostItem);
        hostItem->setExpanded(true);
    }

    if (tree->topLevelItemCount() == 0)
    {
        auto* none = new QTreeWidgetItem(QStringList(tr("No devices available")));
        none->setFlags(Qt::ItemIsEnabled);
        tree->addTopLevelItem(none);
    }

    this->updateAttachButton();
}

void AttachUsbDialog::updateAttachButton()
{
    const QList<QTreeWidgetItem*> selected = this->ui->treeUsbList->selectedItems();
    this->m_attachButton->setEnabled(!selected.isEmpty() && !selected.first()->data(0, PUSB_ROLE).toString().isEmpty());
}

void AttachUsbDialog::attach()
{
    const QList<QTreeWidgetItem*> selected = this->ui->treeUsbList->selectedItems();
    if (selected.isEmpty())
        return;

    XenCache* cache = this->m_vm->GetCache();
    QSharedPointer<PUSB> pusb = cache ? cache->ResolveObject<PUSB>(selected.first()->data(0, PUSB_ROLE).toString()) : QSharedPointer<PUSB>();
    if (!pusb)
        return;

    (new CreateVUSBAction(pusb, this->m_vm))->RunAsync(true);
    this->accept();
}
