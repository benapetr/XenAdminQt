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

// C# equivalent: xenadmin/XenAdmin/Core/VMGroup.cs (VMGroup<VM_appliance> and VMGroup<VMSS>)

#ifndef VMGROUP_H
#define VMGROUP_H

#include <QSharedPointer>
#include <QString>
#include <QStringList>

class AsyncOperation;
class Pool;
class VM;
class XenConnection;
class XenObject;
class QWidget;

/**
 * What differs between the two kinds of VM group, vApps and snapshot schedules, for the code
 * they share: the VMs page, the assign menu and the "VM already in another group" question.
 */
namespace VMGroup
{
    enum class Kind
    {
        VMAppliance,
        VMSS
    };

    //! The connection's groups of this kind (C# GroupsInCache)
    QList<QSharedPointer<XenObject>> GroupsInCache(XenConnection* connection, Kind kind);
    //! The group the VM is in, or an empty string (C# VmToGroup)
    QString VmToGroup(const QSharedPointer<VM>& vm, Kind kind);
    //! The group's VMs (C# GroupToVMs)
    QStringList GroupToVMs(const QSharedPointer<XenObject>& group, Kind kind);

    //! Makes the group's members exactly \a vmRefs (C# AssignVMsToGroupAction)
    AsyncOperation* AssignVMsToGroupAction(const QSharedPointer<XenObject>& group, const QStringList& vmRefs, bool suppressHistory, Kind kind);
    //! Takes \a vmRefs out of the group (C# RemoveVMsFromGroupAction)
    AsyncOperation* RemoveVMsFromGroupAction(const QSharedPointer<XenObject>& group, const QStringList& vmRefs, Kind kind);

    /**
     * C# AssignGroupToVMCommand.ChangesOK: if any of \a vms is already in a group of this kind
     * other than \a group (null for a new one, named \a groupName), asks whether to move them.
     * Returns whether to go on.
     */
    bool ChangesOK(const QList<QSharedPointer<VM>>& vms, const QSharedPointer<XenObject>& group,
                   const QString& groupName, Kind kind, QWidget* parent);

    // Texts (C# VMGroup<T> string properties)
    QString NewGroupString(Kind kind);
    QString AssignMainMenuString(Kind kind);
    QString ManageMainMenuString(Kind kind);
    QString ChooseVMsPageText(Kind kind);
    QString ChooseVMsPageRubric(Kind kind);
    QString ChooseVMsPageCurrentGroup(Kind kind);
}

#endif // VMGROUP_H
