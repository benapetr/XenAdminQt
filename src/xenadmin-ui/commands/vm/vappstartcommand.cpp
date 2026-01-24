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

#include "vappstartcommand.h"
#include "xenlib/xen/actions/vm/startapplianceaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "../../selectionmanager.h"
#include <QMessageBox>

VappStartCommand::VappStartCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool VappStartCommand::CanRun() const
{
    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
    {
        const QList<QTreeWidgetItem*> items = selection->SelectedItems();
        const QList<QSharedPointer<XenObject>> objects = selection->SelectedObjects();
        if (objects.isEmpty() || objects.size() != items.size())
            return false;

        bool allAppliances = true;
        QList<QSharedPointer<VMAppliance>> appliances;
        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj)
                return false;
            const QString type = obj->GetObjectType();
            if (type == "vm_appliance" || type == "appliance")
            {
                QSharedPointer<VMAppliance> appliance = qSharedPointerCast<VMAppliance>(obj);
                if (appliance)
                    appliances.append(appliance);
                else
                    allAppliances = false;
            } else
            {
                allAppliances = false;
            }
        }

        if (allAppliances && !appliances.isEmpty())
        {
            for (const QSharedPointer<VMAppliance>& appliance : appliances)
            {
                if (this->canStartAppliance(appliance))
                    return true;
            }
            return false;
        }

        bool allVms = true;
        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj || obj->GetObjectType() != "vm")
            {
                allVms = false;
                break;
            }
        }

        if (allVms)
        {
            const QList<QSharedPointer<VM>> vms = selection->SelectedVMs();
            if (vms.isEmpty())
                return false;

            QString applianceRef = vms.first()->ApplianceRef();
            if (applianceRef.isEmpty() || applianceRef == "OpaqueRef:NULL")
                return false;

            for (const QSharedPointer<VM>& vm : vms)
            {
                if (!vm || vm->ApplianceRef() != applianceRef)
                    return false;
            }

            XenConnection* connection = vms.first()->GetConnection();
            XenCache* cache = connection ? connection->GetCache() : nullptr;
            if (!cache)
                return false;

            QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>("vm_appliance", applianceRef);
            return this->canStartAppliance(appliance);
        }
    }

    QString objRef = this->getSelectedObjectRef();
    QString type = this->getSelectedObjectType();

    if (objRef.isEmpty())
    {
        return false;
    }

    // Case 1: VM_appliance directly selected
    if (type == "vm_appliance" || type == "appliance")
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        if (!selectedObject)
            return false;

        XenConnection* connection = selectedObject->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return false;

        QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(type, objRef);
        return this->canStartAppliance(appliance);
    }

    // Case 2: VM selected - check if it belongs to an appliance
    if (type == "vm")
    {
        QString applianceRef = this->getApplianceRefFromVM(objRef);
        if (applianceRef.isEmpty())
        {
            return false;
        }

        QSharedPointer<XenObject> selectedObject = this->GetObject();
        if (!selectedObject)
            return false;

        XenConnection* connection = selectedObject->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return false;

        QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>("vm_appliance", applianceRef);
        return this->canStartAppliance(appliance);
    }

    return false;
}

void VappStartCommand::Run()
{
    SelectionManager* selection = this->GetSelectionManager();
    if (selection)
    {
        const QList<QTreeWidgetItem*> items = selection->SelectedItems();
        const QList<QSharedPointer<XenObject>> objects = selection->SelectedObjects();
        if (!objects.isEmpty() && objects.size() == items.size())
        {
            bool allAppliances = true;
            QList<QSharedPointer<VMAppliance>> appliances;
            for (const QSharedPointer<XenObject>& obj : objects)
            {
                if (!obj)
                    return;
                const QString type = obj->GetObjectType();
                if (type == "vm_appliance" || type == "appliance")
                {
                    QSharedPointer<VMAppliance> appliance = qSharedPointerCast<VMAppliance>(obj);
                    if (appliance)
                        appliances.append(appliance);
                    else
                        allAppliances = false;
                } else
                {
                    allAppliances = false;
                }
            }

            if (allAppliances && !appliances.isEmpty())
            {
                for (const QSharedPointer<VMAppliance>& appliance : appliances)
                {
                    if (!this->canStartAppliance(appliance))
                        continue;

                    XenConnection* connection = appliance->GetConnection();
                    if (!connection || !connection->IsConnected())
                        continue;

                    StartApplianceAction* action = new StartApplianceAction(connection, appliance->OpaqueRef(), this->mainWindow());
                    OperationManager::instance()->RegisterOperation(action);
                    connect(action, &AsyncOperation::completed, action, [=]()
                    {
                        if (action->GetState() == AsyncOperation::Completed)
                        {
                            this->mainWindow()->ShowStatusMessage(
                                tr("vApp '%1' started successfully").arg(appliance->GetName()), 5000);
                        } else if (action->GetState() == AsyncOperation::Failed)
                        {
                            QMessageBox::critical(this->mainWindow(), tr("Error"),
                                                  tr("Failed to start vApp '%1':\n%2")
                                                      .arg(appliance->GetName(), action->GetErrorMessage()));
                        }

                        action->deleteLater();
                    });
                    action->RunAsync();
                }
                return;
            }

            bool allVms = true;
            for (const QSharedPointer<XenObject>& obj : objects)
            {
                if (!obj || obj->GetObjectType() != "vm")
                {
                    allVms = false;
                    break;
                }
            }

            if (allVms)
            {
                const QList<QSharedPointer<VM>> vms = selection->SelectedVMs();
                if (vms.isEmpty())
                    return;

                QString applianceRef = vms.first()->ApplianceRef();
                if (applianceRef.isEmpty() || applianceRef == "OpaqueRef:NULL")
                    return;

                for (const QSharedPointer<VM>& vm : vms)
                {
                    if (!vm || vm->ApplianceRef() != applianceRef)
                        return;
                }

                XenConnection* connection = vms.first()->GetConnection();
                XenCache* cache = connection ? connection->GetCache() : nullptr;
                if (!cache)
                    return;

                QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>("vm_appliance", applianceRef);
                if (!appliance || !this->canStartAppliance(appliance))
                    return;

                StartApplianceAction* action = new StartApplianceAction(connection, applianceRef, this->mainWindow());
                OperationManager::instance()->RegisterOperation(action);
                connect(action, &AsyncOperation::completed, this->mainWindow(), [=]()
                {
                    if (action->GetState() == AsyncOperation::Completed)
                    {
                        this->mainWindow()->ShowStatusMessage(
                            tr("vApp '%1' started successfully").arg(appliance->GetName()), 5000);
                    } else if (action->GetState() == AsyncOperation::Failed)
                    {
                        QMessageBox::critical(this->mainWindow(), tr("Error"),
                                              tr("Failed to start vApp '%1':\n%2")
                                                  .arg(appliance->GetName(), action->GetErrorMessage()));
                    }

                    action->deleteLater();
                });
                action->RunAsync();
                return;
            }
        }
    }

    QString objRef = this->getSelectedObjectRef();
    QString type = this->getSelectedObjectType();
    QString applianceRef;

    if (type == "vm_appliance" || type == "appliance")
    {
        applianceRef = objRef;
    } else if (type == "vm")
    {
        applianceRef = this->getApplianceRefFromVM(objRef);
        if (applianceRef.isEmpty())
        {
            QMessageBox::warning(this->mainWindow(), tr("Not in vApp"),
                                 tr("Selected VM is not part of a VM appliance"));
            return;
        }
    } else
    {
        return;
    }

    // Get appliance data for name
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return;

    XenConnection* connection = selectedObject->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return;

    QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>("vm_appliance", applianceRef);
    if (!appliance)
        return;

    QString appName = appliance->GetName();

    if (appName.isEmpty())
    {
        appName = applianceRef;
    }

    // Validate before starting
    if (!this->canStartAppliance(appliance))
    {
        QMessageBox::warning(this->mainWindow(), tr("Cannot Start vApp"),
                             tr("VM appliance '%1' cannot be started").arg(appName));
        return;
    }

    // Get connection
    XenConnection* conn = connection;
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"), tr("Not connected to XenServer"));
        return;
    }

    // Create and start action
    StartApplianceAction* action = new StartApplianceAction(conn, applianceRef, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup and feedback
    connect(action, &AsyncOperation::completed, this->mainWindow(), [=]()
    {
        if (action->GetState() == AsyncOperation::Completed)
        {
            this->mainWindow()->ShowStatusMessage(tr("vApp '%1' started successfully").arg(appName), 5000);
        } else if (action->GetState() == AsyncOperation::Failed)
        {
            QMessageBox::critical(this->mainWindow(), tr("Error"), tr("Failed to start vApp '%1':\n%2").arg(appName, action->GetErrorMessage()));
        }

        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
}

QString VappStartCommand::MenuText() const
{
    return tr("Start v&App");
}

bool VappStartCommand::canStartAppliance(const QSharedPointer<VMAppliance>& appliance) const
{
    if (!appliance)
        return false;

    // Check if "start" operation is allowed
    return appliance->AllowedOperations().contains("start");
}

QString VappStartCommand::getApplianceRefFromVM(const QString& vmRef) const
{
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return QString();

    XenConnection* connection = selectedObject->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return QString();

    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);
    if (vmData.isEmpty())
    {
        return QString();
    }

    // Get appliance reference from VM record
    QString applianceRef = vmData.value("appliance").toString();

    // Check if it's a valid non-null reference
    if (applianceRef.isEmpty() || applianceRef == "OpaqueRef:NULL")
    {
        return QString();
    }

    return applianceRef;
}
