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

#include "vmgroup.h"
#include "../../dialogs/warningdialogs/warningdialog.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/vmss.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmapplianceactions.h"
#include "xenlib/xen/actions/vm/vmssactions.h"
#include "xenlib/xencache.h"
#include <QCoreApplication>

namespace
{
    QString tr(const char* text)
    {
        return QCoreApplication::translate("VMGroup", text);
    }

    QString elide(const QString& text, int length)
    {
        return text.length() > length ? text.left(length - 1) + QChar(0x2026) : text;
    }
}

namespace VMGroup
{
    QList<QSharedPointer<XenObject>> GroupsInCache(XenConnection* connection, Kind kind)
    {
        if (!connection || !connection->GetCache())
            return {};
        return connection->GetCache()->GetAll(kind == Kind::VMSS ? XenObjectType::VMSS : XenObjectType::VMAppliance);
    }

    QString VmToGroup(const QSharedPointer<VM>& vm, Kind kind)
    {
        if (!vm)
            return QString();
        const QString ref = kind == Kind::VMSS ? vm->SnapshotScheduleRef() : vm->ApplianceRef();
        return ref == XENOBJECT_NULL ? QString() : ref;
    }

    QStringList GroupToVMs(const QSharedPointer<XenObject>& group, Kind kind)
    {
        if (kind == Kind::VMSS)
        {
            QSharedPointer<VMSS> vmss = qSharedPointerDynamicCast<VMSS>(group);
            return vmss ? vmss->GetVMRefs() : QStringList();
        }
        QSharedPointer<VMAppliance> appliance = qSharedPointerDynamicCast<VMAppliance>(group);
        return appliance ? appliance->VMRefs() : QStringList();
    }

    AsyncOperation* AssignVMsToGroupAction(const QSharedPointer<XenObject>& group, const QStringList& vmRefs, bool suppressHistory, Kind kind)
    {
        if (kind == Kind::VMSS)
            return new AssignVMsToPolicyAction(qSharedPointerDynamicCast<VMSS>(group), vmRefs, suppressHistory);
        return new AssignVMsToVMApplianceAction(qSharedPointerDynamicCast<VMAppliance>(group), vmRefs, suppressHistory);
    }

    AsyncOperation* RemoveVMsFromGroupAction(const QSharedPointer<XenObject>& group, const QStringList& vmRefs, Kind kind)
    {
        if (kind == Kind::VMSS)
            return new RemoveVMsFromPolicyAction(qSharedPointerDynamicCast<VMSS>(group), vmRefs);
        return new RemoveVMsFromVMApplianceAction(qSharedPointerDynamicCast<VMAppliance>(group), vmRefs);
    }

    bool ChangesOK(const QList<QSharedPointer<VM>>& vms, const QSharedPointer<XenObject>& group,
                   const QString& groupName, Kind kind, QWidget* parent)
    {
        QList<QPair<QSharedPointer<VM>, QSharedPointer<XenObject>>> moving;
        for (const QSharedPointer<VM>& vm : vms)
        {
            if (!vm || !vm->GetConnection())
                continue;
            const QString oldRef = VmToGroup(vm, kind);
            if (oldRef.isEmpty())
                continue;
            QSharedPointer<XenObject> old = vm->GetConnection()->GetCache()->ResolveObject(
                kind == Kind::VMSS ? XenObjectType::VMSS : XenObjectType::VMAppliance, oldRef);
            if (old && (!group || old->OpaqueRef() != group->OpaqueRef()))
                moving.append(qMakePair(vm, old));
        }

        if (moving.isEmpty())
            return true;

        QString text;
        if (moving.size() == 1)
        {
            const QString vmName = elide(moving.first().first->GetName(), 250);
            const QString oldName = elide(moving.first().second->GetName(), 250);
            text = kind == Kind::VMSS
                ? tr("The VM '%1' is already assigned to the snapshot schedule '%2'.\n\nDo you want to assign it to the schedule '%3' instead?")
                : tr("The VM '%1' is already assigned to the vApp '%2'.\n\nDo you want to assign it to the vApp '%3' instead?");
            text = text.arg(vmName, oldName, elide(groupName, 250));
        } else
        {
            text = kind == Kind::VMSS
                ? tr("Some of the VMs you selected are already assigned to a different snapshot schedule.\n\nDo you want to assign these VMs to the schedule '%1' instead?")
                : tr("Some of the VMs you selected are already assigned to a different vApp.\n\nDo you want to assign these VMs to the vApp '%1' instead?");
            text = text.arg(elide(groupName, 250));
        }

        const QString title = kind == Kind::VMSS ? tr("Assign VM to snapshot schedule") : tr("Assign VM to vApp");
        return WarningDialog::ShowYesNo(text, title, parent) == WarningDialog::Result::Yes;
    }

    QString NewGroupString(Kind kind)
    {
        return kind == Kind::VMSS ? tr("&New Snapshot Schedule...") : tr("&New vApp...");
    }

    QString AssignMainMenuString(Kind kind)
    {
        return kind == Kind::VMSS ? tr("Assign to Snapshot Schedu&le") : tr("Assign to vA&pp");
    }

    QString ManageMainMenuString(Kind kind)
    {
        return kind == Kind::VMSS ? tr("VM Snapsh&ot Schedules...") : tr("Manage &vApps...");
    }

    QString ChooseVMsPageText(Kind)
    {
        return tr("Virtual Machines");
    }

    QString ChooseVMsPageRubric(Kind kind)
    {
        return kind == Kind::VMSS
            ? tr("Select a checkbox to add a VM to the snapshot schedule; clear its checkbox to remove it from the schedule.")
            : tr("Select a checkbox to add a VM to the vApp; clear its checkbox to remove it from the vApp.");
    }

    QString ChooseVMsPageCurrentGroup(Kind kind)
    {
        return kind == Kind::VMSS ? tr("Current schedule") : tr("Current vApp");
    }
}
