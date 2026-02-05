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

#include "deletevmcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "../../dialogs/confirmvmdeletedialog.h"
#include "../../dialogs/commanderrordialog.h"
#include "xencache.h"
#include "xenlib/operations/multipleaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/actions/vm/vmdestroyaction.h"
#include <QPointer>

namespace
{
    QStringList filterVbdsForVm(const QStringList& selectedVbds, const QSharedPointer<VM>& vm)
    {
        QStringList filtered;
        if (!vm || selectedVbds.isEmpty())
            return filtered;

        const QString vmRef = vm->OpaqueRef();
        for (const QString& vbdRef : selectedVbds)
        {
            QSharedPointer<VBD> vbd = vm->GetCache()->ResolveObject<VBD>(XenObjectType::VBD, vbdRef);
            if (vbd && vbd->GetVMRef() == vmRef)
                filtered.append(vbdRef);
        }

        return filtered;
    }

    QStringList filterSnapshotsForVm(const QStringList& selectedSnapshots, const QSharedPointer<VM>& vm)
    {
        QStringList filtered;
        if (!vm || selectedSnapshots.isEmpty())
            return filtered;

        const QString vmRef = vm->OpaqueRef();
        for (const QString& snapshotRef : selectedSnapshots)
        {
            QVariantMap snapshotData = vm->GetCache()->ResolveObjectData(XenObjectType::VM, snapshotRef);
            if (snapshotData.value("snapshot_of").toString() == vmRef)
                filtered.append(snapshotRef);
        }

        return filtered;
    }
} // namespace

DeleteVMCommand::DeleteVMCommand(MainWindow* mainWindow, QObject* parent) : VMCommand(mainWindow, parent)
{
}

bool DeleteVMCommand::CanRun() const
{
    const QList<QSharedPointer<VM>> vms = this->collectSelectedVMs(false);
    if (!vms.isEmpty())
    {
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (this->canDeleteVm(vm, false))
                return true;
        }
        return false;
    }

    return this->canDeleteVm(this->getVM(), false);
}

void DeleteVMCommand::Run()
{
    const QList<QSharedPointer<VM>> vms = this->collectSelectedVMs(false);
    this->runDeleteFlow(vms, false, tr("Delete VMs"), tr("Some VMs cannot be deleted."));
}

QString DeleteVMCommand::MenuText() const
{
    return "Delete VM";
}

bool DeleteVMCommand::isVMDeletable() const
{
    return this->canDeleteVm(this->getVM(), false);
}

QList<QSharedPointer<VM>> DeleteVMCommand::collectSelectedVMs(bool includeTemplates) const
{
    QList<QSharedPointer<VM>> vms;
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj)
            continue;

        const XenObjectType type = obj->GetObjectType();
        if (type != XenObjectType::VM)
            continue;

        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
        if (!vm)
            continue;
        if (!includeTemplates && vm->IsTemplate())
            continue;
        vms.append(vm);
    }

    if (!vms.isEmpty())
        return vms;

    QSharedPointer<VM> singleVm = qSharedPointerDynamicCast<VM>(this->GetObject());
    if (singleVm)
    {
        if (!singleVm->IsTemplate() || includeTemplates)
            return { singleVm };
    }

    return {};
}

bool DeleteVMCommand::canDeleteVm(const QSharedPointer<VM>& vm, bool allowTemplates, QString* reason) const
{
    if (!vm)
    {
        if (reason)
            *reason = tr("Invalid selection.");
        return false;
    }

    if (vm->IsSnapshot())
    {
        if (reason)
            *reason = tr("Snapshots cannot be deleted here.");
        return false;
    }

    if (vm->IsTemplate() && !allowTemplates)
    {
        if (reason)
            *reason = tr("Templates cannot be deleted here.");
        return false;
    }

    if (vm->IsTemplate() && vm->DefaultTemplate())
    {
        if (reason)
            *reason = tr("Default templates cannot be deleted.");
        return false;
    }

    if (vm->IsLocked())
    {
        if (reason)
            *reason = tr("VM is locked.");
        return false;
    }

    if (!vm->GetAllowedOperations().contains("destroy"))
    {
        if (reason)
            *reason = tr("Operation is not allowed.");
        return false;
    }

    if (!vm->IsTemplate() && vm->GetPowerState() != "Halted")
    {
        if (reason)
            *reason = tr("VM must be shut down.");
        return false;
    }

    if (reason)
        reason->clear();
    return true;
}

void DeleteVMCommand::runDeleteFlow(const QList<QSharedPointer<VM>>& selected, bool allowTemplates, const QString& errorDialogTitle, const QString& errorDialogText)
{
    if (selected.isEmpty())
        return;

    QList<QSharedPointer<VM>> deletableVms;
    QHash<QSharedPointer<XenObject>, QString> cantRunReasons;
    for (const QSharedPointer<VM>& vm : selected)
    {
        QString reason;
        if (this->canDeleteVm(vm, allowTemplates, &reason))
        {
            deletableVms.append(vm);
        } else if (vm)
        {
            cantRunReasons.insert(vm, reason);
        }
    }

    if (deletableVms.isEmpty())
    {
        CommandErrorDialog dialog(errorDialogTitle, errorDialogText, cantRunReasons, CommandErrorDialog::DialogMode::Close, MainWindow::instance());
        dialog.exec();
        return;
    }

    ConfirmVMDeleteDialog dialog(selected, MainWindow::instance());
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedVbds = dialog.GetDeleteDisks();
    const QStringList selectedSnapshots = dialog.GetDeleteSnapshots();

    QList<AsyncOperation*> actions;
    actions.reserve(deletableVms.size());

    for (const QSharedPointer<VM>& vm : deletableVms)
    {
        QStringList vbdRefs = filterVbdsForVm(selectedVbds, vm);
        QStringList snapshotRefs = filterSnapshotsForVm(selectedSnapshots, vm);
        actions.append(new VMDestroyAction(vm, vbdRefs, snapshotRefs, nullptr));
    }

    if (actions.size() == 1)
    {
        VMDestroyAction* action = qobject_cast<VMDestroyAction*>(actions.first());
        if (!action)
            return;

        QPointer<MainWindow> mw = MainWindow::instance();
        const QString vmName = deletableVms.first()->GetName();

        OperationManager::instance()->RegisterOperation(action);
        connect(action, &AsyncOperation::completed, mw, [action, mw, vmName]()
        {
            if (mw)
            {
                if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
                    mw->ShowStatusMessage(QString("VM '%1' deleted successfully").arg(vmName), 5000);
                else
                    mw->ShowStatusMessage(QString("Failed to delete VM '%1'").arg(vmName), 5000);
            }
            action->deleteLater();
        });
        action->RunAsync();
    } else
    {
        XenConnection* connection = deletableVms.first() ? deletableVms.first()->GetConnection() : nullptr;
        MultipleAction* multi = new MultipleAction(
            connection,
            tr("Deleting VMs"),
            tr("Deleting selected VMs..."),
            tr("VM deletion complete"),
            actions,
            false,
            true,
            false,
            nullptr);

        OperationManager::instance()->RegisterOperation(multi);
        connect(multi, &AsyncOperation::completed, multi, &QObject::deleteLater);
        multi->RunAsync();
    }

    if (!cantRunReasons.isEmpty())
    {
        CommandErrorDialog dialog(errorDialogTitle, errorDialogText, cantRunReasons, CommandErrorDialog::DialogMode::Close, MainWindow::instance());
        dialog.exec();
    }
}
