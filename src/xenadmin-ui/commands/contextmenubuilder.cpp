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

#include "contextmenubuilder.h"
#include "command.h"
#include "host/hostmaintenancemodecommand.h"
#include "host/reboothostcommand.h"
#include "host/shutdownhostcommand.h"
#include "host/hostpropertiescommand.h"
#include "host/reconnecthostcommand.h"
#include "host/removehostcommand.h"
#include "pool/joinpoolcommand.h"
#include "pool/ejecthostfrompoolcommand.h"
#include "pool/poolpropertiescommand.h"
#include "vm/startvmcommand.h"
#include "vm/stopvmcommand.h"
#include "vm/restartvmcommand.h"
#include "vm/suspendvmcommand.h"
#include "vm/resumevmcommand.h"
#include "vm/pausevmcommand.h"
#include "vm/unpausevmcommand.h"
#include "vm/migratevmcommand.h"
#include "vm/clonevmcommand.h"
#include "vm/deletevmcommand.h"
#include "vm/convertvmtotemplatecommand.h"
#include "vm/exportvmcommand.h"
#include "vm/newvmcommand.h"
#include "template/newvmfromtemplatecommand.h"
#include "vm/vmpropertiescommand.h"
#include "vm/takesnapshotcommand.h"
#include "vm/deletesnapshotcommand.h"
#include "vm/reverttosnapshotcommand.h"
#include "vm/exportsnapshotastemplatecommand.h"
#include "vm/newtemplatefromsnapshotcommand.h"
#include "storage/repairsrcommand.h"
#include "storage/detachsrcommand.h"
#include "storage/setdefaultsrcommand.h"
#include "storage/reattachsrcommand.h"
#include "storage/forgetsrcommand.h"
#include "storage/destroysrcommand.h"
#include "storage/storagepropertiescommand.h"
#include "template/deletetemplatecommand.h"
#include "template/exporttemplatecommand.h"
#include "network/networkpropertiescommand.h"
#include "../mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/xenobject.h"
#include "xen/vm.h"
#include "xen/network/connection.h"
#include <QAction>
#include <QDebug>

ContextMenuBuilder::ContextMenuBuilder(MainWindow* mainWindow, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

QMenu* ContextMenuBuilder::buildContextMenu(QTreeWidgetItem* item, QWidget* parent)
{
    if (!item)
        return nullptr;

    QVariant data = item->data(0, Qt::UserRole);
    QString objectType;
    QString objectRef;
    
    // Extract type and ref from QSharedPointer<XenObject>
    QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
    // TODO: check if it's even possible for obj to be nullptr here, I think we shouldn't really continue in such case
    // because in theory such code path should be invalid / bug, so this code can be probably drastically simplified
    if (obj)
    {
        objectType = obj->objectType();
        objectRef = obj->opaqueRef();
    } else if (data.canConvert<XenConnection*>())
    {
        objectType = "disconnected_host";
        objectRef = QString();
    }
    
    if (objectType.isEmpty())
        return nullptr;

    QString itemName = item->text(0);
    qDebug() << "ContextMenuBuilder: Building context menu for" << objectType << "item:" << itemName;

    QMenu* menu = new QMenu(parent);

    if (objectType == "vm")
    {
        if (!obj)
            return menu;

        QSharedPointer<VM> vm = qSharedPointerCast<VM>(obj);

        if (vm->isSnapshot())
        {
            this->buildSnapshotContextMenu(menu, item);
        } else
        {
            this->buildVMContextMenu(menu, item, vm);
        }
    } else if (objectType == "template")
    {
        this->buildTemplateContextMenu(menu, item);
    } else if (objectType == "host")
    {
        this->buildHostContextMenu(menu, item);
    } else if (objectType == "disconnected_host")
    {
        // C# pattern: disconnected servers show Connect, Forget Password, Remove menu items
        this->buildDisconnectedHostContextMenu(menu, item);
    } else if (objectType == "sr")
    {
        this->buildSRContextMenu(menu, item);
    } else if (objectType == "pool")
    {
        this->buildPoolContextMenu(menu, item);
    } else if (objectType == "network")
    {
        this->buildNetworkContextMenu(menu, item);
    }

    return menu;
}

void ContextMenuBuilder::buildVMContextMenu(QMenu* menu, QTreeWidgetItem* item, QSharedPointer<VM> vm)
{
    QString powerState = vm->powerState();
    QString vmRef = vm->opaqueRef();

    // Power operations based on VM state
    if (powerState == "Halted")
    {
        StartVMCommand* startCmd = new StartVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, startCmd);
    } else if (powerState == "Running")
    {
        StopVMCommand* stopCmd = new StopVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, stopCmd);

        RestartVMCommand* restartCmd = new RestartVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, restartCmd);

        PauseVMCommand* pauseCmd = new PauseVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, pauseCmd);

        SuspendVMCommand* suspendCmd = new SuspendVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, suspendCmd);

        menu->addSeparator();

        MigrateVMCommand* migrateCmd = new MigrateVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, migrateCmd);
    } else if (powerState == "Paused")
    {
        UnpauseVMCommand* unpauseCmd = new UnpauseVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, unpauseCmd);
    } else if (powerState == "Suspended")
    {
        ResumeVMCommand* resumeCmd = new ResumeVMCommand(this->m_mainWindow, this);
        this->addCommand(menu, resumeCmd);
    }

    this->addSeparator(menu);

    // VM management operations
    CloneVMCommand* cloneCmd = new CloneVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, cloneCmd);

    ExportVMCommand* exportCmd = new ExportVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, exportCmd);

    // Convert to template (only for halted VMs)
    if (powerState == "Halted")
    {
        ConvertVMToTemplateCommand* convertCmd = new ConvertVMToTemplateCommand(this->m_mainWindow, this);
        this->addCommand(menu, convertCmd);
    }

    this->addSeparator(menu);

    // Snapshot operations
    TakeSnapshotCommand* takeSnapshotCmd = new TakeSnapshotCommand(vmRef, this->m_mainWindow, this);
    this->addCommand(menu, takeSnapshotCmd);

    this->addSeparator(menu);

    DeleteVMCommand* deleteCmd = new DeleteVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    // Properties
    VMPropertiesCommand* propertiesCmd = new VMPropertiesCommand(vmRef, this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildSnapshotContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    QSharedPointer<XenObject> obj = item->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
    QString vmRef = obj ? obj->opaqueRef() : QString();

    // C# SingleSnapshot builder pattern:
    // - NewVMFromSnapshotCommand (not implemented yet)
    // - NewTemplateFromSnapshotCommand
    // - ExportSnapshotAsTemplateCommand
    // - DeleteSnapshotCommand
    // - PropertiesCommand

    // Create template from snapshot
    NewTemplateFromSnapshotCommand* newTemplateCmd = new NewTemplateFromSnapshotCommand(this->m_mainWindow, this);
    this->addCommand(menu, newTemplateCmd);

    // Export snapshot as template
    ExportSnapshotAsTemplateCommand* exportCmd = new ExportSnapshotAsTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, exportCmd);

    this->addSeparator(menu);

    // Revert to snapshot (if applicable)
    RevertToSnapshotCommand* revertCmd = new RevertToSnapshotCommand(this->m_mainWindow, this);
    this->addCommand(menu, revertCmd);

    // Delete snapshot
    DeleteSnapshotCommand* deleteCmd = new DeleteSnapshotCommand(this->m_mainWindow, this);
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    // Properties
    VMPropertiesCommand* propertiesCmd = new VMPropertiesCommand(vmRef, this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildTemplateContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    QSharedPointer<XenObject> obj = item->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
    QString templateRef = obj ? obj->opaqueRef() : QString();

    // VM Creation from template
    NewVMFromTemplateCommand* newVMFromTemplateCmd = new NewVMFromTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMFromTemplateCmd);

    this->addSeparator(menu);

    // Template operations
    ExportTemplateCommand* exportCmd = new ExportTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, exportCmd);

    this->addSeparator(menu);

    DeleteTemplateCommand* deleteCmd = new DeleteTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    menu->addAction("Properties");
}

void ContextMenuBuilder::buildHostContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    QSharedPointer<XenObject> obj = item->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
    if (!obj)
        return;

    // Use cache lookup instead of async API call to avoid spawning unhandled requests
    QVariantMap hostData = obj->data();
    bool enabled = hostData.value("enabled", true).toBool();

    // New VM command (available for both pool and standalone hosts)
    NewVMCommand* newVMCmd = new NewVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMCmd);

    this->addSeparator(menu);

    // Power operations
    RebootHostCommand* rebootCmd = new RebootHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, rebootCmd);

    ShutdownHostCommand* shutdownCmd = new ShutdownHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, shutdownCmd);

    this->addSeparator(menu);

    // Maintenance mode commands
    if (enabled)
    {
        HostMaintenanceModeCommand* enterMaintenanceCmd = new HostMaintenanceModeCommand(this->m_mainWindow, true, this);
        this->addCommand(menu, enterMaintenanceCmd);
    } else
    {
        HostMaintenanceModeCommand* exitMaintenanceCmd = new HostMaintenanceModeCommand(this->m_mainWindow, false, this);
        this->addCommand(menu, exitMaintenanceCmd);
    }

    this->addSeparator(menu);

    // Pool management
    JoinPoolCommand* joinPoolCmd = new JoinPoolCommand(this->m_mainWindow, this);
    this->addCommand(menu, joinPoolCmd);

    EjectHostFromPoolCommand* ejectCmd = new EjectHostFromPoolCommand(this->m_mainWindow, this);
    this->addCommand(menu, ejectCmd);

    this->addSeparator(menu);

    // Properties
    HostPropertiesCommand* propertiesCmd = new HostPropertiesCommand(this->m_mainWindow);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildSRContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    Q_UNUSED(item);

    RepairSRCommand* repairCmd = new RepairSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, repairCmd);

    SetDefaultSRCommand* setDefaultCmd = new SetDefaultSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, setDefaultCmd);

    this->addSeparator(menu);

    DetachSRCommand* detachCmd = new DetachSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, detachCmd);

    ReattachSRCommand* reattachCmd = new ReattachSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, reattachCmd);

    ForgetSRCommand* forgetCmd = new ForgetSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, forgetCmd);

    DestroySRCommand* destroyCmd = new DestroySRCommand(this->m_mainWindow, this);
    this->addCommand(menu, destroyCmd);

    this->addSeparator(menu);

    StoragePropertiesCommand* propertiesCmd = new StoragePropertiesCommand(this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildDisconnectedHostContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    // C# pattern: Disconnected servers (fake Host objects) show:
    // - Connect (ReconnectHostCommand)
    // - Forget Password (TODO: implement password management)
    // - Remove (RemoveHostCommand)

    Q_UNUSED(item);

    // Connect command (matches C# ReconnectHostCommand for disconnected servers)
    ReconnectHostCommand* reconnectCmd = new ReconnectHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, reconnectCmd);

    this->addSeparator(menu);

    // TODO: Add "Forget Password" command when password management is implemented
    // This would clear saved credentials for this connection

    // Remove command (matches C# RemoveHostCommand for disconnected servers)
    RemoveHostCommand* removeCmd = new RemoveHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, removeCmd);
}

void ContextMenuBuilder::buildPoolContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    QSharedPointer<XenObject> obj = item->data(0, Qt::UserRole).value<QSharedPointer<XenObject>>();
    QString poolRef = obj ? obj->opaqueRef() : QString();

    // VM Creation operations
    NewVMCommand* newVMCmd = new NewVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMCmd);

    this->addSeparator(menu);

    // Properties
    PoolPropertiesCommand* propertiesCmd = new PoolPropertiesCommand(this->m_mainWindow);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildNetworkContextMenu(QMenu* menu, QTreeWidgetItem* item)
{
    Q_UNUSED(item);

    // Properties
    NetworkPropertiesCommand* propertiesCmd = new NetworkPropertiesCommand(this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::addCommand(QMenu* menu, Command* command)
{
    if (!command || !command->canRun())
        return;

    QAction* action = menu->addAction(command->menuText());
    connect(action, &QAction::triggered, command, &Command::run);
}

void ContextMenuBuilder::addSeparator(QMenu* menu)
{
    menu->addSeparator();
}
