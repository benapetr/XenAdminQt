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
#include "host/restarttoolstackcommand.h"
#include "host/shutdownhostcommand.h"
#include "host/hostpropertiescommand.h"
#include "host/reconnecthostcommand.h"
#include "host/removehostcommand.h"
#include "pool/joinpoolcommand.h"
#include "pool/ejecthostfrompoolcommand.h"
#include "pool/disconnectpoolcommand.h"
#include "pool/poolpropertiescommand.h"
#include "vm/startvmcommand.h"
#include "vm/stopvmcommand.h"
#include "vm/restartvmcommand.h"
#include "vm/suspendvmcommand.h"
#include "vm/resumevmcommand.h"
#include "vm/pausevmcommand.h"
#include "vm/unpausevmcommand.h"
#include "../controls/vmoperationmenu.h"
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
#include "../settingsmanager.h"
#include "../connectionprofile.h"
#include "xen/xenobject.h"
#include "xen/vm.h"
#include "xen/host.h"
#include "xen/pool.h"
#include "xen/sr.h"
#include "xen/network.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include <QAction>
#include <QDebug>
#include <QTreeWidget>

ContextMenuBuilder::ContextMenuBuilder(MainWindow* mainWindow, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

QMenu* ContextMenuBuilder::BuildContextMenu(QTreeWidgetItem* item, QWidget* parent)
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
        objectType = obj->GetObjectType();
        objectRef = obj->OpaqueRef();
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

    const QString itemType = item->data(0, Qt::UserRole + 1).toString();
    bool isDisconnectedHost = (objectType == "disconnected_host" || itemType == "disconnected_host");
    if (!isDisconnectedHost && obj && objectType == "host")
    {
        XenConnection* connection = obj->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (cache)
        {
            const QVariantMap record = cache->ResolveObjectData("host", obj->OpaqueRef());
            isDisconnectedHost = record.value("is_disconnected").toBool();
        }
    }

    if (isDisconnectedHost)
    {
        // C# pattern: disconnected servers show Connect, Forget Password, Remove menu items
        this->buildDisconnectedHostContextMenu(menu, item);
        return menu;
    }

    if (!obj)
        return menu;

    if (objectType == "vm")
    {
        QSharedPointer<VM> vm = qSharedPointerCast<VM>(obj);

        if (vm->IsSnapshot())
            this->buildSnapshotContextMenu(menu, vm);
        else
            this->buildVMContextMenu(menu, vm);
    } else if (objectType == "template")
    {
        QSharedPointer<VM> templateVM = qSharedPointerCast<VM>(obj);
        this->buildTemplateContextMenu(menu, templateVM);
    } else if (objectType == "host")
    {
        QSharedPointer<Host> host = qSharedPointerCast<Host>(obj);
        this->buildHostContextMenu(menu, host);
    } else if (objectType == "sr")
    {
        QSharedPointer<SR> sr = qSharedPointerCast<SR>(obj);
        this->buildSRContextMenu(menu, sr);
    } else if (objectType == "pool")
    {
        QSharedPointer<Pool> pool = qSharedPointerCast<Pool>(obj);
        this->buildPoolContextMenu(menu, pool);
    } else if (objectType == "network")
    {
        QSharedPointer<Network> network = qSharedPointerCast<Network>(obj);
        this->buildNetworkContextMenu(menu, network);
    }

    return menu;
}

void ContextMenuBuilder::buildVMContextMenu(QMenu* menu, QSharedPointer<VM> vm)
{
    QString powerState = vm->GetPowerState();
    QList<QSharedPointer<VM>> selectedVms;
    QTreeWidget* tree = this->m_mainWindow->GetServerTreeWidget();

    if (tree)
    {
        const QList<QTreeWidgetItem*> selectedItems = tree->selectedItems();
        for (QTreeWidgetItem* item : selectedItems)
        {
            if (!item)
                continue;

            QVariant data = item->data(0, Qt::UserRole);
            if (!data.canConvert<QSharedPointer<XenObject>>())
                continue;

            QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
            if (!obj || obj->GetObjectType() != "vm")
                continue;

            QSharedPointer<VM> selectedVm = qSharedPointerCast<VM>(obj);
            if (!selectedVm || selectedVm->IsSnapshot() || selectedVm->IsTemplate())
                continue;

            selectedVms.append(selectedVm);
        }
    }

    if (selectedVms.isEmpty() && vm)
        selectedVms.append(vm);

    auto selectionConnection = [&selectedVms]() -> XenConnection* {
        if (selectedVms.isEmpty())
            return nullptr;
        XenConnection* connection = selectedVms.first()->GetConnection();
        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (!item || item->GetConnection() != connection)
                return nullptr;
        }
        return connection;
    };

    auto hostCount = [](XenConnection* connection) -> int {
        if (!connection || !connection->GetCache())
            return 0;
        return connection->GetCache()->GetAllRefs("host").size();
    };

    auto anyEnabledHost = [](XenConnection* connection) -> bool {
        if (!connection || !connection->GetCache())
            return false;
        const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>("host");
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->IsEnabled())
                return true;
        }
        return false;
    };

    auto canShowStartOn = [&]() -> bool {
        XenConnection* connection = selectionConnection();
        if (!connection || hostCount(connection) <= 1 || !anyEnabledHost(connection))
            return false;

        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (!item || item->IsTemplate() || item->IsLocked())
                return false;

            if (item->GetAllowedOperations().contains("start"))
                return true;
        }
        return false;
    };

    auto canShowResumeOn = [&]() -> bool {
        XenConnection* connection = selectionConnection();
        if (!connection || hostCount(connection) <= 1 || !anyEnabledHost(connection))
            return false;

        bool atLeastOne = false;
        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (!item || item->IsTemplate() || item->IsLocked())
                return false;

            if (item->GetAllowedOperations().contains("suspend"))
                return false;

            if (item->GetAllowedOperations().contains("resume"))
                atLeastOne = true;
        }
        return atLeastOne;
    };

    auto canShowMigrate = [&]() -> bool {
        XenConnection* connection = selectionConnection();
        if (!connection)
            return false;

        XenCache* cache = connection->GetCache();
        if (!cache)
            return false;

        const QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>("host");
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->RestrictIntraPoolMigrate())
                return false;
        }

        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (!item || item->IsTemplate() || item->IsLocked())
                return false;

            if (item->GetAllowedOperations().contains("pool_migrate"))
                return true;
        }
        return false;
    };

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

    if (canShowStartOn())
    {
        VMOperationMenu* startOnMenu = new VMOperationMenu(this->m_mainWindow, selectedVms, VMOperationMenu::Operation::StartOn, menu);
        menu->addMenu(startOnMenu);
    }

    if (canShowResumeOn())
    {
        VMOperationMenu* resumeOnMenu = new VMOperationMenu(this->m_mainWindow, selectedVms, VMOperationMenu::Operation::ResumeOn, menu);
        menu->addMenu(resumeOnMenu);
    }

    if (canShowMigrate())
    {
        VMOperationMenu* migrateMenu = new VMOperationMenu(this->m_mainWindow, selectedVms, VMOperationMenu::Operation::Migrate, menu);
        menu->addMenu(migrateMenu);
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
    TakeSnapshotCommand* takeSnapshotCmd = new TakeSnapshotCommand(vm, this->m_mainWindow, this);
    this->addCommand(menu, takeSnapshotCmd);

    this->addSeparator(menu);

    DeleteVMCommand* deleteCmd = new DeleteVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    // Properties
    VMPropertiesCommand* propertiesCmd = new VMPropertiesCommand(vm->OpaqueRef(), this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildSnapshotContextMenu(QMenu* menu, QSharedPointer<VM> snapshot)
{
    QString vmRef = snapshot->OpaqueRef();

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

void ContextMenuBuilder::buildTemplateContextMenu(QMenu* menu, QSharedPointer<VM> templateVM)
{
    QString templateRef = templateVM->OpaqueRef();

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

void ContextMenuBuilder::buildHostContextMenu(QMenu* menu, QSharedPointer<Host> host)
{
    bool enabled = host->IsEnabled();

    // New VM command (available for both pool and standalone hosts)
    NewVMCommand* newVMCmd = new NewVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMCmd);

    this->addSeparator(menu);

    // Power operations
    RebootHostCommand* rebootCmd = new RebootHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, rebootCmd);

    ShutdownHostCommand* shutdownCmd = new ShutdownHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, shutdownCmd);

    RestartToolstackCommand* restartToolstackCmd = new RestartToolstackCommand(this->m_mainWindow, this);
    this->addCommand(menu, restartToolstackCmd);

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

void ContextMenuBuilder::buildSRContextMenu(QMenu* menu, QSharedPointer<SR> sr)
{
    Q_UNUSED(sr);

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

    // Connect command (matches C# ReconnectHostCommand for disconnected servers)
    ReconnectHostCommand* reconnectCmd = new ReconnectHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, reconnectCmd);

    this->addSeparator(menu);

    QAction* forgetPasswordAction = menu->addAction(tr("Forget Password"));
    connect(forgetPasswordAction, &QAction::triggered, this, [item]() {
        if (!item)
            return;

        XenConnection* connection = nullptr;
        QVariant data = item->data(0, Qt::UserRole);
        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (obj)
            connection = obj->GetConnection();
        else if (data.canConvert<XenConnection*>())
            connection = data.value<XenConnection*>();

        if (!connection)
            return;

        QList<ConnectionProfile> profiles = SettingsManager::instance().loadConnectionProfiles();
        bool updated = false;
        for (ConnectionProfile& profile : profiles)
        {
            if (profile.GetHostname() == connection->GetHostname() &&
                profile.GetPort() == connection->GetPort())
            {
                profile.SetPassword(QString());
                profile.SetRememberPassword(false);
                SettingsManager::instance().saveConnectionProfile(profile);
                updated = true;
                break;
            }
        }

        if (updated)
            connection->SetPassword(QString());
    });

    // Remove command (matches C# RemoveHostCommand for disconnected servers)
    RemoveHostCommand* removeCmd = new RemoveHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, removeCmd);
}

void ContextMenuBuilder::buildPoolContextMenu(QMenu* menu, QSharedPointer<Pool> pool)
{
    QString poolRef = pool->OpaqueRef();

    // VM Creation operations
    NewVMCommand* newVMCmd = new NewVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMCmd);

    this->addSeparator(menu);

    DisconnectPoolCommand* disconnectCmd = new DisconnectPoolCommand(this->m_mainWindow, this);
    this->addCommand(menu, disconnectCmd);

    this->addSeparator(menu);

    // Properties
    PoolPropertiesCommand* propertiesCmd = new PoolPropertiesCommand(this->m_mainWindow);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildNetworkContextMenu(QMenu* menu, QSharedPointer<Network> network)
{
    Q_UNUSED(network);

    // Properties
    NetworkPropertiesCommand* propertiesCmd = new NetworkPropertiesCommand(this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::addCommand(QMenu* menu, Command* command)
{
    if (!command || !command->CanRun())
        return;

    QAction* action = menu->addAction(command->MenuText());
    connect(action, &QAction::triggered, command, &Command::Run);
}

void ContextMenuBuilder::addSeparator(QMenu* menu)
{
    menu->addSeparator();
}
