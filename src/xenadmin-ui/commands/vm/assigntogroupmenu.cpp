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

#include "assigntogroupmenu.h"
#include "../../mainwindow.h"
#include "../../dialogs/newvmappliancewizard.h"
#include "../../dialogs/newpolicywizard.h"
#include "../../dialogs/vmappliancesdialog.h"
#include "../../dialogs/scheduledsnapshotsdialog.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmss.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/utils/misc.h"
#include "xenlib/xencache.h"
#include <algorithm>

namespace
{
    //! The connection all the objects share (C#: PoolAncestor or HostAncestor isn't null), or null
    XenConnection* commonConnection(const QList<QSharedPointer<XenObject>>& objects)
    {
        XenConnection* connection = nullptr;
        for (const QSharedPointer<XenObject>& object : objects)
        {
            if (!object || !object->GetConnection())
                return nullptr;
            if (connection && object->GetConnection() != connection)
                return nullptr;
            connection = object->GetConnection();
        }
        return connection;
    }

    QString elide(const QString& text, int length)
    {
        return text.length() > length ? text.left(length - 1) + QChar(0x2026) : text;
    }
}

// AssignToGroupCommand

AssignToGroupCommand::AssignToGroupCommand(VMGroup::Kind kind, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_kind(kind)
{
}

QList<QSharedPointer<VM>> AssignToGroupCommand::SelectedVMs() const
{
    QList<QSharedPointer<VM>> vms;
    for (const QSharedPointer<XenObject>& object : this->getSelectedObjects())
    {
        if (QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object))
            vms.append(vm);
    }
    return vms;
}

bool AssignToGroupCommand::CanRun() const
{
    // C# AssignVMsToGroup.CanRunCore. Not ported: the licence check (XCP-ng has none) and
    // VMSS's FalconOrGreater, which every XCP-ng release meets.
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    XenConnection* connection = commonConnection(objects);
    if (objects.isEmpty() || !connection || !connection->IsConnected())
        return false;

    bool anyCanRun = false;
    for (const QSharedPointer<XenObject>& object : objects)
    {
        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object);
        if (!vm)
            return false;
        anyCanRun |= vm->IsRealVM() && !vm->IsLocked();
    }
    return anyCanRun;
}

QString AssignToGroupCommand::MenuText() const
{
    return VMGroup::AssignMainMenuString(this->m_kind);
}

// AssignToGroupMenu

AssignToGroupMenu::AssignToGroupMenu(VMGroup::Kind kind, MainWindow* mainWindow, QWidget* parent)
    : QMenu(parent), m_kind(kind), m_mainWindow(mainWindow)
{
    this->setTitle(VMGroup::AssignMainMenuString(kind));
    connect(this, &QMenu::aboutToShow, this, &AssignToGroupMenu::onAboutToShow);
}

bool AssignToGroupMenu::CanRun() const
{
    return AssignToGroupCommand(this->m_kind, this->m_mainWindow).CanRun();
}

void AssignToGroupMenu::onAboutToShow()
{
    // C# AssignGroupToolStripMenuItem.OnDropDownOpening
    this->clear();
    const QList<QSharedPointer<VM>> vms = AssignToGroupCommand(this->m_kind, this->m_mainWindow).SelectedVMs();
    if (vms.isEmpty() || !vms.first()->GetConnection())
        return;

    XenConnection* connection = vms.first()->GetConnection();
    const VMGroup::Kind kind = this->m_kind;
    QAction* newAction = this->addAction(VMGroup::NewGroupString(kind));
    connect(newAction, &QAction::triggered, this, [connection, vms, kind]()
    {
        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        if (kind == VMGroup::Kind::VMSS)
            NewPolicyWizard::ShowFor(pool, vms, MainWindow::instance());
        else
            NewVMApplianceWizard::ShowFor(pool, vms, MainWindow::instance());
    });

    QList<QSharedPointer<XenObject>> groups = VMGroup::GroupsInCache(connection, kind);
    std::sort(groups.begin(), groups.end(), [](const QSharedPointer<XenObject>& a, const QSharedPointer<XenObject>& b)
    {
        return Misc::NaturalCompare(a->GetName(), b->GetName()) < 0;
    });

    int index = 0;
    bool separatorAdded = false;
    for (const QSharedPointer<XenObject>& group : groups)
    {
        if (!group)
            continue;

        // C#: leave out quiesced-snapshot schedules unless every selected VM can take one,
        // which no server since XenServer 7.6 allows
        if (QSharedPointer<VMSS> vmss = qSharedPointerDynamicCast<VMSS>(group))
        {
            if (vmss->Type() == "snapshot_with_quiesce"
                && std::any_of(vms.begin(), vms.end(), [](const QSharedPointer<VM>& vm)
                               { return !vm->GetAllowedOperations().contains("snapshot_with_quiesce"); }))
                continue;
        }

        if (!separatorAdded)
        {
            this->addSeparator();
            separatorAdded = true;
        }

        QString name = elide(group->GetName(), 30);
        name.replace("&", "&&");
        // C# DYNAMIC_MENUITEM_WITH_ACCESS_KEY for the first nine
        const QString text = index < 9 ? QString("&%1   %2").arg(index + 1).arg(name) : name;
        ++index;
        QAction* action = this->addAction(text);
        action->setToolTip(group->GetName());
        if (vms.size() == 1 && VMGroup::VmToGroup(vms.first(), kind) == group->OpaqueRef())
        {
            action->setCheckable(true);
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, group, vms]() { this->assign(group, vms); });
    }
}

void AssignToGroupMenu::assign(QSharedPointer<XenObject> group, const QList<QSharedPointer<VM>>& vms)
{
    // C# AssignGroupToVMCommand.RunCore: choosing a single VM's own group takes it out
    const QStringList members = VMGroup::GroupToVMs(group, this->m_kind);
    if (vms.size() == 1 && members.contains(vms.first()->OpaqueRef()))
    {
        VMGroup::RemoveVMsFromGroupAction(group, {vms.first()->OpaqueRef()}, this->m_kind)->RunAsync(true);
        return;
    }

    if (!VMGroup::ChangesOK(vms, group, group->GetName(), this->m_kind, MainWindow::instance()))
        return;

    QStringList refs;
    for (const QSharedPointer<VM>& vm : vms)
        refs.append(vm->OpaqueRef());
    for (const QString& ref : members)
    {
        if (!refs.contains(ref))
            refs.append(ref);
    }
    VMGroup::AssignVMsToGroupAction(group, refs, false, this->m_kind)->RunAsync(true);
}

// ManageVMGroupsCommand

ManageVMGroupsCommand::ManageVMGroupsCommand(VMGroup::Kind kind, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_kind(kind)
{
}

bool ManageVMGroupsCommand::CanRun() const
{
    // C# VMGroupCommand.CanRunCore
    XenConnection* connection = commonConnection(this->getSelectedObjects());
    return connection && connection->IsConnected();
}

void ManageVMGroupsCommand::Run()
{
    XenConnection* connection = commonConnection(this->getSelectedObjects());
    if (!connection || !connection->GetCache())
        return;
    QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
    if (!pool)
        return;
    if (this->m_kind == VMGroup::Kind::VMSS)
        ScheduledSnapshotsDialog::ShowFor(pool, MainWindow::instance());
    else
        VMAppliancesDialog::ShowFor(pool, MainWindow::instance());
}

QString ManageVMGroupsCommand::MenuText() const
{
    return VMGroup::ManageMainMenuString(this->m_kind);
}
