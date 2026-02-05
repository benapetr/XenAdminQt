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

#include "vmlifecyclecommand.h"
#include "../../mainwindow.h"
#include "../../selectionmanager.h"
#include "startvmcommand.h"
#include "stopvmcommand.h"
#include "resumevmcommand.h"
#include "unpausevmcommand.h"
#include "xenlib/xen/vm.h"
#include <QTreeWidget>

VMLifeCycleCommand::VMLifeCycleCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool VMLifeCycleCommand::CanRun() const
{
    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
    {
        const QList<QTreeWidgetItem*> items = selection->SelectedItems();
        const QList<QSharedPointer<XenObject>> objects = selection->SelectedObjects();
        if (objects.isEmpty() || objects.size() != items.size())
            return false;

        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj || obj->GetObjectType() != XenObjectType::VM)
                return false;
        }
    }

    const QList<QSharedPointer<VM>> selectedVms = this->getSelectedVMs();
    if (selectedVms.isEmpty())
        return false;

    for (const QSharedPointer<VM>& vm : selectedVms)
    {
        if (!vm)
            continue;

        if (vm->IsSnapshot() || vm->IsTemplate())
            return false;

        const QStringList allowedOps = vm->GetAllowedOperations();
        const QString powerState = vm->GetPowerState();

        if (powerState == "Halted" && allowedOps.contains("start"))
            return true;
        if (powerState == "Running" && allowedOps.contains("clean_shutdown"))
            return true;
        if (powerState == "Paused" && allowedOps.contains("unpause"))
            return true;
        if (powerState == "Suspended" && allowedOps.contains("resume"))
            return true;
    }

    return false;
}

void VMLifeCycleCommand::Run()
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return;

    QString powerState = this->getVMPowerState();

    if (powerState == "Halted")
    {
        StartVMCommand cmd(MainWindow::instance());
        if (cmd.CanRun())
            cmd.Run();
    } else if (powerState == "Running")
    {
        StopVMCommand cmd(MainWindow::instance());
        if (cmd.CanRun())
            cmd.Run();
    } else if (powerState == "Paused")
    {
        UnpauseVMCommand cmd(MainWindow::instance());
        if (cmd.CanRun())
            cmd.Run();
    } else if (powerState == "Suspended")
    {
        ResumeVMCommand cmd(MainWindow::instance());
        if (cmd.CanRun())
            cmd.Run();
    }
}

QString VMLifeCycleCommand::MenuText() const
{
    QString powerState = this->getVMPowerState();

    // Return dynamic text based on VM state
    if (powerState == "Halted")
        return "Start";
    else if (powerState == "Running")
        return "Shut Down";
    else if (powerState == "Paused")
        return "Unpause";
    else if (powerState == "Suspended")
        return "Resume";

    return "Start/Shut Down";
}

QList<QSharedPointer<VM>> VMLifeCycleCommand::getSelectedVMs() const
{
    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
    {
        const QList<QSharedPointer<VM>> vms = selection->SelectedVMs();
        if (!vms.isEmpty())
            return vms;
    }

    QSharedPointer<XenObject> obj = this->GetObject();
    if (obj && obj->GetObjectType() == XenObjectType::VM)
    {
        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
        if (vm)
            return { vm };
    }

    return {};
}

QString VMLifeCycleCommand::getSelectedVMRef() const
{
    const QList<QSharedPointer<VM>> selectedVms = this->getSelectedVMs();
    if (selectedVms.isEmpty())
        return QString();

    return selectedVms.first()->OpaqueRef();
}

QString VMLifeCycleCommand::getVMPowerState() const
{
    const QList<QSharedPointer<VM>> selectedVms = this->getSelectedVMs();
    if (selectedVms.isEmpty())
        return QString();

    const QSharedPointer<VM> vm = selectedVms.first();
    return vm ? vm->GetPowerState() : QString();
}

bool VMLifeCycleCommand::isVMOperationAllowed(const QString& operation) const
{
    const QList<QSharedPointer<VM>> selectedVms = this->getSelectedVMs();
    if (selectedVms.isEmpty())
        return false;

    const QSharedPointer<VM> vm = selectedVms.first();
    if (!vm)
        return false;

    return vm->GetAllowedOperations().contains(operation);
}
