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

#include "assigntovappmenu.h"
#include "../../mainwindow.h"
#include "../../dialogs/newvmappliancewizard.h"
#include "../../dialogs/vmappliancesdialog.h"
#include "../../dialogs/warningdialogs/warningdialog.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/vmapplianceactions.h"
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

// AssignToVAppCommand

AssignToVAppCommand::AssignToVAppCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

QList<QSharedPointer<VM>> AssignToVAppCommand::SelectedVMs() const
{
    QList<QSharedPointer<VM>> vms;
    for (const QSharedPointer<XenObject>& object : this->getSelectedObjects())
    {
        if (QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(object))
            vms.append(vm);
    }
    return vms;
}

bool AssignToVAppCommand::CanRun() const
{
    // C# AssignVMsToGroup.CanRunCore (no licence check: XCP-ng has none)
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    if (objects.isEmpty() || !commonConnection(objects) || !commonConnection(objects)->IsConnected())
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

QString AssignToVAppCommand::MenuText() const
{
    return tr("Assign to vA&pp");
}

// AssignToVAppMenu

AssignToVAppMenu::AssignToVAppMenu(MainWindow* mainWindow, QWidget* parent) : QMenu(parent), m_mainWindow(mainWindow)
{
    this->setTitle(tr("Assign to vA&pp"));
    connect(this, &QMenu::aboutToShow, this, &AssignToVAppMenu::onAboutToShow);
}

bool AssignToVAppMenu::CanRun() const
{
    return AssignToVAppCommand(this->m_mainWindow).CanRun();
}

void AssignToVAppMenu::onAboutToShow()
{
    // C# AssignGroupToolStripMenuItem.OnDropDownOpening
    this->clear();
    const QList<QSharedPointer<VM>> vms = AssignToVAppCommand(this->m_mainWindow).SelectedVMs();
    if (vms.isEmpty() || !vms.first()->GetConnection())
        return;

    XenConnection* connection = vms.first()->GetConnection();
    QAction* newAction = this->addAction(tr("&New vApp..."));
    connect(newAction, &QAction::triggered, this, [connection, vms]()
    {
        NewVMApplianceWizard::ShowFor(connection->GetCache()->GetPoolOfOne(), vms, MainWindow::instance());
    });

    QList<QSharedPointer<VMAppliance>> appliances = connection->GetCache()->GetAll<VMAppliance>(XenObjectType::VMAppliance);
    std::sort(appliances.begin(), appliances.end(), [](const QSharedPointer<VMAppliance>& a, const QSharedPointer<VMAppliance>& b)
    {
        return Misc::NaturalCompare(a->GetName(), b->GetName()) < 0;
    });

    if (!appliances.isEmpty())
        this->addSeparator();

    for (int index = 0; index < appliances.size(); ++index)
    {
        QSharedPointer<VMAppliance> appliance = appliances.at(index);
        if (!appliance)
            continue;
        QString name = elide(appliance->GetName(), 30);
        name.replace("&", "&&");
        // C# DYNAMIC_MENUITEM_WITH_ACCESS_KEY for the first nine
        const QString text = index < 9 ? QString("&%1   %2").arg(index + 1).arg(name) : name;
        QAction* action = this->addAction(text);
        action->setToolTip(appliance->GetName());
        if (vms.size() == 1 && vms.first()->ApplianceRef() == appliance->OpaqueRef())
        {
            action->setCheckable(true);
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, appliance, vms]() { this->assign(appliance, vms); });
    }
}

void AssignToVAppMenu::assign(QSharedPointer<VMAppliance> appliance, const QList<QSharedPointer<VM>>& vms)
{
    // C# AssignGroupToVMCommand.RunCore: choosing a single VM's own vApp takes it out
    if (vms.size() == 1 && appliance->VMRefs().contains(vms.first()->OpaqueRef()))
    {
        (new RemoveVMsFromVMApplianceAction(appliance, {vms.first()->OpaqueRef()}))->RunAsync(true);
        return;
    }

    if (!ChangesOK(vms, appliance, appliance->GetName(), MainWindow::instance()))
        return;

    QStringList refs;
    for (const QSharedPointer<VM>& vm : vms)
        refs.append(vm->OpaqueRef());
    for (const QString& ref : appliance->VMRefs())
    {
        if (!refs.contains(ref))
            refs.append(ref);
    }
    (new AssignVMsToVMApplianceAction(appliance, refs, false))->RunAsync(true);
}

bool AssignToVAppMenu::ChangesOK(const QList<QSharedPointer<VM>>& vms, QSharedPointer<VMAppliance> appliance,
                                 const QString& applianceName, QWidget* parent)
{
    QList<QPair<QSharedPointer<VM>, QSharedPointer<VMAppliance>>> moving;
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (!vm || !vm->GetConnection())
            continue;
        QSharedPointer<VMAppliance> old = vm->GetConnection()->GetCache()->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, vm->ApplianceRef());
        if (old && (!appliance || old->OpaqueRef() != appliance->OpaqueRef()))
            moving.append(qMakePair(vm, old));
    }

    if (moving.isEmpty())
        return true;

    const QString text = moving.size() == 1
        ? tr("The VM '%1' is already assigned to the vApp '%2'.\n\nDo you want to assign it to the vApp '%3' instead?")
              .arg(elide(moving.first().first->GetName(), 250), elide(moving.first().second->GetName(), 250), elide(applianceName, 250))
        : tr("Some of the VMs you selected are already assigned to a different vApp.\n\nDo you want to assign these VMs to the vApp '%1' instead?")
              .arg(elide(applianceName, 250));

    return WarningDialog::ShowYesNo(text, tr("Assign VM to vApp"), parent) == WarningDialog::Result::Yes;
}

// ManageVAppsCommand

ManageVAppsCommand::ManageVAppsCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool ManageVAppsCommand::CanRun() const
{
    // C# VMGroupCommand.CanRunCore
    XenConnection* connection = commonConnection(this->getSelectedObjects());
    return connection && connection->IsConnected();
}

void ManageVAppsCommand::Run()
{
    XenConnection* connection = commonConnection(this->getSelectedObjects());
    if (!connection || !connection->GetCache())
        return;
    if (QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne())
        VMAppliancesDialog::ShowFor(pool, MainWindow::instance());
}

QString ManageVAppsCommand::MenuText() const
{
    return tr("Manage &vApps...");
}
