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
#include "host/poweronhostcommand.h"
#include "host/hostpropertiescommand.h"
#include "host/destroyhostcommand.h"
#include "host/certificatecommand.h"
#include "host/disconnecthostcommand.h"
#include "host/reconnecthostcommand.h"
#include "host/removehostcommand.h"
#include "host/hostreconnectascommand.h"
#include "connection/cancelhostconnectioncommand.h"
#include "connection/disconnecthostsandpoolscommand.h"
#include "connection/forgetsavedpasswordcommand.h"
#include "../selectionmanager.h"
#include "pool/joinpoolcommand.h"
#include "pool/removehostfrompoolcommand.h"
#include "pool/disconnectpoolcommand.h"
#include "pool/poolpropertiescommand.h"
#include "pool/addselectedhosttopoolmenu.h"
#include "vm/startvmcommand.h"
#include "vm/stopvmcommand.h"
#include "vm/restartvmcommand.h"
#include "vm/suspendvmcommand.h"
#include "vm/resumevmcommand.h"
#include "vm/pausevmcommand.h"
#include "vm/unpausevmcommand.h"
#include "vm/forceshutdownvmcommand.h"
#include "vm/forcerebootvmcommand.h"
#include "../controls/vmoperationmenu.h"
#include "vm/clonevmcommand.h"
#include "vm/copyvmcommand.h"
#include "vm/movevmcommand.h"
#include "vm/deletevmcommand.h"
#include "vm/deletevmsandtemplatescommand.h"
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
#include "storage/newsrcommand.h"
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
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include <QAction>
#include <QDebug>
#include <QTreeWidget>
#include <QMap>
#include <QSet>

ContextMenuBuilder::ContextMenuBuilder(MainWindow* mainWindow, QObject* parent) : QObject(parent), m_mainWindow(mainWindow)
{
}

QMenu* ContextMenuBuilder::BuildContextMenu(QTreeWidgetItem* item, QWidget* parent)
{
    if (!item)
        return nullptr;

    QVariant data = item->data(0, Qt::UserRole);
    XenObjectType objectType = XenObjectType::Null;
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
        objectType = XenObjectType::DisconnectedHost;
        objectRef = QString();
    }

    if (objectType == XenObjectType::Null)
        return nullptr;

    QString itemName = item->text(0);
    qDebug() << "ContextMenuBuilder: Building context menu for" << XenObject::TypeToString(objectType) << "item:" << itemName;

    QMenu* menu = new QMenu(parent);

    const QString itemType = item->data(0, Qt::UserRole + 1).toString();
    bool isDisconnectedHost = (objectType == XenObjectType::DisconnectedHost || itemType == "disconnected_host");
    if (!isDisconnectedHost && obj && objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        if (host)
            isDisconnectedHost = !host->IsConnected();
    }

    if (isDisconnectedHost)
    {
        // C# pattern: disconnected servers show Connect, Forget Password, Remove menu items
        this->buildDisconnectedHostContextMenu(menu, item);
        return menu;
    }

    if (!obj)
        return menu;

    switch (objectType)
    {
        case XenObjectType::VM:
        {
            QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
            if (!vm)
                break;
            if (vm->IsSnapshot())
                this->buildSnapshotContextMenu(menu, vm);
            else if (vm->IsTemplate())
                this->buildTemplateContextMenu(menu, vm);
            else
                this->buildVMContextMenu(menu, vm);
            break;
        }
        case XenObjectType::Host:
        {
            QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
            this->buildHostContextMenu(menu, host);
            break;
        }
        case XenObjectType::SR:
        {
            QSharedPointer<SR> sr = qSharedPointerDynamicCast<SR>(obj);
            this->buildSRContextMenu(menu, sr);
            break;
        }
        case XenObjectType::Pool:
        {
            QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(obj);
            this->buildPoolContextMenu(menu, pool);
            break;
        }
        case XenObjectType::Network:
        {
            QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(obj);
            this->buildNetworkContextMenu(menu, network);
            break;
        }
        default:
            break;
    }

    return menu;
}

void ContextMenuBuilder::buildVMContextMenu(QMenu* menu, QSharedPointer<VM> vm)
{
    if (!vm)
        return;

    QString powerState = vm->GetPowerState();
    bool mixedVmTemplateSelection = false;
    if (this->m_mainWindow && this->m_mainWindow->GetSelectionManager())
    {
        bool hasTemplate = false;
        bool hasVm = false;
        const QList<QSharedPointer<XenObject>> objects = this->m_mainWindow->GetSelectionManager()->SelectedObjects();
        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj || obj->GetObjectType() != XenObjectType::VM)
                continue;

            QSharedPointer<VM> selectedVm = qSharedPointerDynamicCast<VM>(obj);
            if (!selectedVm)
                continue;

            if (selectedVm->IsTemplate())
                hasTemplate = true;
            else
                hasVm = true;

            if (hasTemplate && hasVm)
                break;
        }
        mixedVmTemplateSelection = hasTemplate && hasVm;
    }
    QList<QSharedPointer<VM>> selectedVms = this->getSelectedVMs();
    QList<QSharedPointer<VM>> filteredVms;
    QMap<QString, QSharedPointer<Host>> vmHostAncestors;
    QTreeWidget* tree = this->m_mainWindow->GetServerTreeWidget();

    for (const QSharedPointer<VM>& selectedVm : selectedVms)
    {
        if (!selectedVm || selectedVm->IsSnapshot() || selectedVm->IsTemplate())
            continue;
        filteredVms.append(selectedVm);
    }

    selectedVms = filteredVms;

    if (selectedVms.isEmpty() && vm)
        selectedVms.append(vm);

    if (tree)
    {
        QSet<QString> selectedRefs;
        for (const QSharedPointer<VM>& selectedVm : selectedVms)
            selectedRefs.insert(selectedVm->OpaqueRef());

        const QList<QTreeWidgetItem*> selectedItems = tree->selectedItems();
        for (QTreeWidgetItem* item : selectedItems)
        {
            if (!item)
                continue;

            QVariant data = item->data(0, Qt::UserRole);
            if (!data.canConvert<QSharedPointer<XenObject>>())
                continue;

            QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
            if (!obj || obj->GetObjectType() != XenObjectType::VM)
                continue;

            if (!selectedRefs.contains(obj->OpaqueRef()))
                continue;

            QSharedPointer<Host> hostAncestor;
            QTreeWidgetItem* parent = item->parent();
            while (parent)
            {
                QVariant parentData = parent->data(0, Qt::UserRole);
                if (parentData.canConvert<QSharedPointer<XenObject>>())
                {
                    QSharedPointer<XenObject> parentObj = parentData.value<QSharedPointer<XenObject>>();
                    if (parentObj && parentObj->GetObjectType() == XenObjectType::Host)
                    {
                        hostAncestor = qSharedPointerDynamicCast<Host>(parentObj);
                        break;
                    }
                }
                parent = parent->parent();
            }

            if (hostAncestor)
                vmHostAncestors.insert(obj->OpaqueRef(), hostAncestor);
        }
    }

    auto selectionConnection = [&selectedVms]() -> XenConnection*
    {
        if (selectedVms.isEmpty())
            return nullptr;
        XenConnection* connection = selectedVms.first()->GetConnection();
        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (item->GetConnection() != connection)
                return nullptr;
        }
        return connection;
    };

    auto hostCount = [](XenConnection* connection) -> int
    {
        if (!connection || !connection->GetCache())
            return 0;
        return connection->GetCache()->GetAllRefs(XenObjectType::Host).size();
    };

    auto anyEnabledHost = [](XenConnection* connection) -> bool
    {
        if (!connection || !connection->GetCache())
            return false;
        const QList<QSharedPointer<Host>> hosts = connection->GetCache()->GetAll<Host>(XenObjectType::Host);
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && host->IsEnabled())
                return true;
        }
        return false;
    };

    auto enabledTargetExists = [&](const QSharedPointer<VM>& item, XenConnection* connection) -> bool
    {
        QSharedPointer<Host> hostAncestor = vmHostAncestors.value(item->OpaqueRef());
        if (hostAncestor)
            return hostAncestor->IsEnabled();

        return anyEnabledHost(connection);
    };

    auto canShowStartOn = [&]() -> bool
    {
        XenConnection* connection = selectionConnection();
        if (!connection || hostCount(connection) <= 1)
            return false;

        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (item->IsLocked())
                continue;

            if (item->GetAllowedOperations().contains("start") && enabledTargetExists(item, connection))
                return true;
        }
        return false;
    };

    auto canShowResumeOn = [&]() -> bool
    {
        XenConnection* connection = selectionConnection();
        if (!connection || hostCount(connection) <= 1)
            return false;

        bool atLeastOne = false;
        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (item->IsLocked())
                continue;

            if (item->GetAllowedOperations().contains("suspend"))
                continue;

            if (item->GetAllowedOperations().contains("resume") && enabledTargetExists(item, connection))
                atLeastOne = true;
        }
        return atLeastOne;
    };

    auto canShowMigrate = [&]() -> bool
    {
        XenConnection* connection = selectionConnection();
        if (!connection)
            return false;

        XenCache* cache = connection->GetCache();

        const QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>(XenObjectType::Host);
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host->RestrictIntraPoolMigrate())
                return false;
        }

        for (const QSharedPointer<VM>& item : selectedVms)
        {
            if (item->IsLocked())
                continue;

            if (item->GetAllowedOperations().contains("pool_migrate"))
                return true;
        }
        return false;
    };

    // Power operations are always present; enablement depends on selected VMs.
    StartVMCommand* startCmd = new StartVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, startCmd);

    StopVMCommand* stopCmd = new StopVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, stopCmd);

    SuspendVMCommand* suspendCmd = new SuspendVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, suspendCmd);

    RestartVMCommand* restartCmd = new RestartVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, restartCmd);

    PauseVMCommand* pauseCmd = new PauseVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, pauseCmd);

    UnpauseVMCommand* unpauseCmd = new UnpauseVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, unpauseCmd);

    ResumeVMCommand* resumeCmd = new ResumeVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, resumeCmd);

    ForceShutdownVMCommand* forceShutdownCmd = new ForceShutdownVMCommand(this->m_mainWindow, this);
    ForceRebootVMCommand* forceRebootCmd = new ForceRebootVMCommand(this->m_mainWindow, this);
    if (forceShutdownCmd->CanRun() || forceRebootCmd->CanRun())
    {
        this->addSeparator(menu);
        this->addCommand(menu, forceShutdownCmd);
        this->addCommand(menu, forceRebootCmd);
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

    // VM management operations (match C# context menu)
    CopyVMCommand* copyCmd = new CopyVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, copyCmd);

    MoveVMCommand* moveCmd = new MoveVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, moveCmd);

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

    Command* deleteCmd = mixedVmTemplateSelection
        ? static_cast<Command*>(new DeleteVMsAndTemplatesCommand(this->m_mainWindow, this))
        : static_cast<Command*>(new DeleteVMCommand(this->m_mainWindow, this));
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    // Properties
    VMPropertiesCommand* propertiesCmd = new VMPropertiesCommand(vm->OpaqueRef(), this->m_mainWindow, this);
    this->addCommand(menu, propertiesCmd);
}

void ContextMenuBuilder::buildSnapshotContextMenu(QMenu* menu, QSharedPointer<VM> snapshot)
{
    if (!snapshot)
        return;

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
    if (!templateVM)
        return;

    QString templateRef = templateVM->OpaqueRef();
    bool mixedVmTemplateSelection = false;
    if (this->m_mainWindow && this->m_mainWindow->GetSelectionManager())
    {
        bool hasTemplate = false;
        bool hasVm = false;
        const QList<QSharedPointer<XenObject>> objects = this->m_mainWindow->GetSelectionManager()->SelectedObjects();
        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj || obj->GetObjectType() != XenObjectType::VM)
                continue;

            QSharedPointer<VM> selectedVm = qSharedPointerDynamicCast<VM>(obj);
            if (!selectedVm)
                continue;

            if (selectedVm->IsTemplate())
                hasTemplate = true;
            else
                hasVm = true;

            if (hasTemplate && hasVm)
                break;
        }
        mixedVmTemplateSelection = hasTemplate && hasVm;
    }

    // VM Creation from template
    NewVMFromTemplateCommand* newVMFromTemplateCmd = new NewVMFromTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMFromTemplateCmd);

    this->addSeparator(menu);

    // Template operations
    ExportTemplateCommand* exportCmd = new ExportTemplateCommand(this->m_mainWindow, this);
    this->addCommand(menu, exportCmd);

    this->addSeparator(menu);

    Command* deleteCmd = mixedVmTemplateSelection
        ? static_cast<Command*>(new DeleteVMsAndTemplatesCommand(this->m_mainWindow, this))
        : static_cast<Command*>(new DeleteTemplateCommand(this->m_mainWindow, this));
    this->addCommand(menu, deleteCmd);

    this->addSeparator(menu);

    menu->addAction("Properties");
}

void ContextMenuBuilder::buildHostContextMenu(QMenu* menu, QSharedPointer<Host> host)
{
    if (!host)
        return;

    QList<QSharedPointer<Host>> selectedHosts = this->getSelectedHosts();
    if (selectedHosts.isEmpty())
        selectedHosts.append(host);

    const int count = selectedHosts.size();
    bool anyLive = false;
    bool anyDead = false;
    for (const QSharedPointer<Host>& item : selectedHosts)
    {
        if (!item)
            continue;

        if (item->IsLive())
            anyLive = true;
        else
            anyDead = true;
    }

    if (count > 1)
    {
        if (anyLive && anyDead)
        {
            ShutdownHostCommand* shutdownCmd = new ShutdownHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, shutdownCmd);

            PowerOnHostCommand* powerOnCmd = new PowerOnHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, powerOnCmd);

            RestartToolstackCommand* restartToolstackCmd =
                new RestartToolstackCommand(this->m_mainWindow, this);
            this->addCommand(menu, restartToolstackCmd);
        } else if (anyLive)
        {
            AddSelectedHostToPoolMenu* addToPoolMenu = new AddSelectedHostToPoolMenu(this->m_mainWindow, menu);
            if (addToPoolMenu->CanRun())
                menu->addMenu(addToPoolMenu);

            DisconnectHostCommand* disconnectCmd = new DisconnectHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, disconnectCmd);

            RebootHostCommand* rebootCmd = new RebootHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, rebootCmd);

            ShutdownHostCommand* shutdownCmd = new ShutdownHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, shutdownCmd);

            RestartToolstackCommand* restartToolstackCmd =
                new RestartToolstackCommand(this->m_mainWindow, this);
            this->addCommand(menu, restartToolstackCmd);
        } else
        {
            PowerOnHostCommand* powerOnCmd = new PowerOnHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, powerOnCmd);

            DestroyHostCommand* destroyCmd = new DestroyHostCommand(this->m_mainWindow, this);
            this->addCommand(menu, destroyCmd);
        }
        return;
    }

    QSharedPointer<Host> selectedHost = selectedHosts.first();
    if (!selectedHost)
        return;

    if (!selectedHost->IsLive())
    {
        PowerOnHostCommand* powerOnCmd = new PowerOnHostCommand(this->m_mainWindow, this);
        this->addCommand(menu, powerOnCmd);

        DestroyHostCommand* destroyCmd = new DestroyHostCommand(this->m_mainWindow, this);
        this->addCommand(menu, destroyCmd);

        this->addSeparator(menu);

        HostPropertiesCommand* propertiesCmd = new HostPropertiesCommand(this->m_mainWindow, this);
        this->addCommand(menu, propertiesCmd);
        return;
    }

    const bool inPool = !selectedHost->GetPoolRef().isEmpty();

    // New VM command (available for both pool and standalone hosts)
    NewVMCommand* newVMCmd = new NewVMCommand(this->m_mainWindow, this);
    this->addCommand(menu, newVMCmd);
    NewSRCommand* newSRCmd = new NewSRCommand(this->m_mainWindow, this);
    this->addCommand(menu, newSRCmd);

    this->addSeparator(menu);

    if (!inPool)
    {
        AddSelectedHostToPoolMenu* addToPoolMenu = new AddSelectedHostToPoolMenu(this->m_mainWindow, menu);
        if (addToPoolMenu->CanRun())
            menu->addMenu(addToPoolMenu);
        this->addSeparator(menu);
    }

    CertificateCommand* certCmd = new CertificateCommand(this->m_mainWindow, menu);
    if (certCmd->CanRun())
    {
        QMenu* certMenu = menu->addMenu(certCmd->MenuText());
        InstallCertificateCommand* installCmd =
            new InstallCertificateCommand(this->m_mainWindow, certMenu);
        this->addCommand(certMenu, installCmd);
        ResetCertificateCommand* resetCmd =
            new ResetCertificateCommand(this->m_mainWindow, certMenu);
        this->addCommand(certMenu, resetCmd);
        this->addSeparator(menu);
    }

    // Maintenance mode commands
    if (selectedHost->IsEnabled())
    {
        HostMaintenanceModeCommand* enterMaintenanceCmd = new HostMaintenanceModeCommand(this->m_mainWindow, true, this);
        this->addCommand(menu, enterMaintenanceCmd);
    } else
    {
        HostMaintenanceModeCommand* exitMaintenanceCmd = new HostMaintenanceModeCommand(this->m_mainWindow, false, this);
        this->addCommand(menu, exitMaintenanceCmd);
    }

    this->addSeparator(menu);

    // Power operations
    RebootHostCommand* rebootCmd = new RebootHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, rebootCmd);

    ShutdownHostCommand* shutdownCmd = new ShutdownHostCommand(this->m_mainWindow, this);
    this->addCommand(menu, shutdownCmd);

    if (!inPool)
    {
        PowerOnHostCommand* powerOnCmd = new PowerOnHostCommand(this->m_mainWindow, this);
        this->addCommand(menu, powerOnCmd);
    }

    RestartToolstackCommand* restartToolstackCmd =
        new RestartToolstackCommand(this->m_mainWindow, this);
    this->addCommand(menu, restartToolstackCmd);

    this->addSeparator(menu);

    if (inPool)
    {
        RemoveHostFromPoolCommand* removeCmd = new RemoveHostFromPoolCommand(this->m_mainWindow, this);
        this->addCommand(menu, removeCmd);
    } else
    {
        DisconnectHostCommand* disconnectCmd = new DisconnectHostCommand(this->m_mainWindow, this);
        this->addCommand(menu, disconnectCmd);

        HostReconnectAsCommand* reconnectAsCmd = new HostReconnectAsCommand(this->m_mainWindow, this);
        this->addCommand(menu, reconnectAsCmd);
    }

    this->addSeparator(menu);

    HostPropertiesCommand* propertiesCmd = new HostPropertiesCommand(this->m_mainWindow, this);
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
    QList<XenConnection*> connections = this->getSelectedConnections();
    if (connections.isEmpty())
    {
        QVariant data = item ? item->data(0, Qt::UserRole) : QVariant();
        if (data.canConvert<XenConnection*>())
            connections.append(data.value<XenConnection*>());
    }

    bool anyInProgress = false;
    for (XenConnection* connection : connections)
    {
        if (connection && connection->InProgress() && !connection->IsConnected())
        {
            anyInProgress = true;
            break;
        }
    }

    if (anyInProgress)
    {
        CancelHostConnectionCommand* cancelCmd = new CancelHostConnectionCommand(connections, this->m_mainWindow, this);
        this->addCommand(menu, cancelCmd);
    } else
    {
        DisconnectHostsAndPoolsCommand* disconnectCmd = new DisconnectHostsAndPoolsCommand(connections, this->m_mainWindow, this);
        this->addCommand(menu, disconnectCmd);

        ReconnectHostCommand* reconnectCmd = new ReconnectHostCommand(connections, this->m_mainWindow, this);
        this->addCommand(menu, reconnectCmd);

        ForgetSavedPasswordCommand* forgetCmd = new ForgetSavedPasswordCommand(connections, this->m_mainWindow, this);
        this->addCommand(menu, forgetCmd);

        RemoveHostCommand* removeCmd = new RemoveHostCommand(connections, this->m_mainWindow, this);
        this->addCommand(menu, removeCmd);

        RestartToolstackCommand* restartToolstackCmd = new RestartToolstackCommand(this->m_mainWindow, this);
        this->addCommand(menu, restartToolstackCmd);
    }
}

void ContextMenuBuilder::buildPoolContextMenu(QMenu* menu, QSharedPointer<Pool> pool)
{
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
    QIcon icon = command->GetIcon();
    if (!icon.isNull())
        action->setIcon(icon);
    connect(action, &QAction::triggered, command, &Command::Run);
}

void ContextMenuBuilder::addSeparator(QMenu* menu)
{
    menu->addSeparator();
}

QList<QSharedPointer<VM>> ContextMenuBuilder::getSelectedVMs() const
{
    return this->m_mainWindow && this->m_mainWindow->GetSelectionManager()
        ? this->m_mainWindow->GetSelectionManager()->SelectedVMs()
        : QList<QSharedPointer<VM>>();
}

QList<QSharedPointer<Host>> ContextMenuBuilder::getSelectedHosts() const
{
    return this->m_mainWindow && this->m_mainWindow->GetSelectionManager()
        ? this->m_mainWindow->GetSelectionManager()->SelectedHosts()
        : QList<QSharedPointer<Host>>();
}

QList<XenConnection*> ContextMenuBuilder::getSelectedConnections() const
{
    return this->m_mainWindow && this->m_mainWindow->GetSelectionManager()
        ? this->m_mainWindow->GetSelectionManager()->SelectedConnections()
        : QList<XenConnection*>();
}
