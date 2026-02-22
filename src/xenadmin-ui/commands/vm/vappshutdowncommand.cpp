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

#include "vappshutdowncommand.h"
#include "xenlib/xen/actions/vm/shutdownapplianceaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../selectionmanager.h"
#include <QMessageBox>

VappShutDownCommand::VappShutDownCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool VappShutDownCommand::CanRun() const
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
            const XenObjectType type = obj->GetObjectType();
            if (type == XenObjectType::VMAppliance)
            {
                QSharedPointer<VMAppliance> appliance = qSharedPointerDynamicCast<VMAppliance>(obj);
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
                if (this->canShutDownAppliance(appliance))
                    return true;
            }
            return false;
        }

        bool allVms = true;
        for (const QSharedPointer<XenObject>& obj : objects)
        {
            if (!obj || obj->GetObjectType() != XenObjectType::VM)
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
            if (applianceRef.isEmpty() || applianceRef == XENOBJECT_NULL)
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

            QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, applianceRef);
            return this->canShutDownAppliance(appliance);
        }
    }

    QString objRef = this->getSelectedObjectRef();
    XenObjectType type = this->getSelectedObjectType();

    if (objRef.isEmpty())
    {
        return false;
    }

    // Case 1: VM_appliance directly selected
    if (type == XenObjectType::VMAppliance)
    {
        QSharedPointer<XenObject> selectedObject = this->GetObject();
        if (!selectedObject)
            return false;

        XenConnection* connection = selectedObject->GetConnection();
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return false;

        QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(type, objRef);
        return this->canShutDownAppliance(appliance);
    }

    // Case 2: VM selected - check if it belongs to an appliance
    if (type == XenObjectType::VM)
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

        QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, applianceRef);
        return this->canShutDownAppliance(appliance);
    }

    return false;
}

void VappShutDownCommand::Run()
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
                const XenObjectType type = obj->GetObjectType();
                if (type == XenObjectType::VMAppliance)
                {
                    QSharedPointer<VMAppliance> appliance = qSharedPointerDynamicCast<VMAppliance>(obj);
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
                QStringList appNames;
                for (const QSharedPointer<VMAppliance>& appliance : appliances)
                    appNames.append(appliance ? appliance->GetName() : QString());

                QMessageBox::StandardButton reply = QMessageBox::question(
                    MainWindow::instance(),
                    tr("Shut Down vApp"),
                    tr("Are you sure you want to shut down vApp(s): %1?\n\n"
                       "All VMs in the appliance will be shut down gracefully.")
                        .arg(appNames.join(", ")),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply != QMessageBox::Yes)
                    return;

                for (const QSharedPointer<VMAppliance>& appliance : appliances)
                {
                    if (!this->canShutDownAppliance(appliance))
                        continue;

                    XenConnection* connection = appliance->GetConnection();
                    if (!connection || !connection->IsConnected())
                        continue;

                    ShutDownApplianceAction* action = new ShutDownApplianceAction(connection, appliance->OpaqueRef(), MainWindow::instance());
                    connect(action, &AsyncOperation::completed, action, [=]()
                    {
                        if (action->GetState() == AsyncOperation::Completed)
                        {
                            MainWindow::instance()->ShowStatusMessage(
                                tr("vApp '%1' shut down successfully").arg(appliance->GetName()), 5000);
                        } else if (action->GetState() == AsyncOperation::Failed)
                        {
                            QMessageBox::critical(MainWindow::instance(), tr("Error"), tr("Failed to shut down vApp '%1':\n%2").arg(appliance->GetName(), action->GetErrorMessage()));
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
                if (!obj || obj->GetObjectType() != XenObjectType::VM)
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
                if (applianceRef.isEmpty() || applianceRef == XENOBJECT_NULL)
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

                QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(applianceRef);
                if (!appliance || !this->canShutDownAppliance(appliance))
                    return;

                QMessageBox::StandardButton reply = QMessageBox::question(
                    MainWindow::instance(),
                    tr("Shut Down vApp"),
                    tr("Are you sure you want to shut down vApp '%1'?\n\n"
                       "All VMs in the appliance will be shut down gracefully.")
                        .arg(appliance->GetName()),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply != QMessageBox::Yes)
                    return;

                ShutDownApplianceAction* action = new ShutDownApplianceAction(connection, applianceRef, MainWindow::instance());
                connect(action, &AsyncOperation::completed, MainWindow::instance(), [=]()
                {
                    if (action->GetState() == AsyncOperation::Completed)
                    {
                        MainWindow::instance()->ShowStatusMessage(
                            tr("vApp '%1' shut down successfully").arg(appliance->GetName()), 5000);
                    } else if (action->GetState() == AsyncOperation::Failed)
                    {
                        QMessageBox::critical(MainWindow::instance(), tr("Error"),
                                              tr("Failed to shut down vApp '%1':\n%2")
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
    XenObjectType type = this->getSelectedObjectType();
    QString applianceRef;

    if (type == XenObjectType::VMAppliance)
    {
        applianceRef = objRef;
    } else if (type == XenObjectType::VM)
    {
        applianceRef = this->getApplianceRefFromVM(objRef);
        if (applianceRef.isEmpty())
        {
            QMessageBox::warning(MainWindow::instance(), tr("Not in vApp"),
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

    if (!connection)
        return;

    XenCache* cache = connection->GetCache();

    QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>(XenObjectType::VMAppliance, applianceRef);
    if (!appliance)
        return;

    QString appName = appliance->GetName();

    if (appName.isEmpty())
    {
        appName = applianceRef;
    }

    // Validate before shutting down
    if (!this->canShutDownAppliance(appliance))
    {
        QMessageBox::warning(MainWindow::instance(), tr("Cannot Shut Down vApp"), tr("VM appliance '%1' cannot be shut down").arg(appName));
        return;
    }

    // Confirm shutdown with user
    QMessageBox::StandardButton reply = QMessageBox::question(
        MainWindow::instance(),
        tr("Shut Down vApp"),
        tr("Are you sure you want to shut down vApp '%1'?\n\n"
           "All VMs in the appliance will be shut down gracefully.")
            .arg(appName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // Get connection
    if (!connection->IsConnected())
    {
        QMessageBox::warning(MainWindow::instance(), tr("Not Connected"), tr("Not connected to XenServer"));
        return;
    }

    // Create and start action (uses clean shutdown by default)
    ShutDownApplianceAction* action = new ShutDownApplianceAction(connection, applianceRef, MainWindow::instance());

    // Connect completion signal for cleanup and feedback
    connect(action, &AsyncOperation::completed, MainWindow::instance(), [=]()
    {
        if (action->GetState() == AsyncOperation::Completed)
        {
            MainWindow::instance()->ShowStatusMessage(tr("vApp '%1' shut down successfully").arg(appName), 5000);
        } else if (action->GetState() == AsyncOperation::Failed)
        {
            QMessageBox::critical(MainWindow::instance(), tr("Error"), tr("Failed to shut down vApp '%1':\n%2").arg(appName, action->GetErrorMessage()));
        }

        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->RunAsync();
}

QString VappShutDownCommand::MenuText() const
{
    return tr("Shut Down v&App");
}

QIcon VappShutDownCommand::GetIcon() const
{
    return QIcon(":/icons/shutdown.png");
}

bool VappShutDownCommand::canShutDownAppliance(const QSharedPointer<VMAppliance>& appliance) const
{
    if (!appliance)
        return false;

    // Check if "clean_shutdown" or "hard_shutdown" operation is allowed
    const QStringList allowedOps = appliance->AllowedOperations();
    return allowedOps.contains("clean_shutdown") || allowedOps.contains("hard_shutdown");
}

QString VappShutDownCommand::getApplianceRefFromVM(const QString& vmRef) const
{
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return QString();

    XenConnection* connection = selectedObject->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return QString();

    QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
    if (vmData.isEmpty())
    {
        return QString();
    }

    // Get appliance reference from VM record
    QString applianceRef = vmData.value("appliance").toString();

    // Check if it's a valid non-null reference
    if (applianceRef.isEmpty() || applianceRef == XENOBJECT_NULL)
    {
        return QString();
    }

    return applianceRef;
}
