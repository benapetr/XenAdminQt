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
#include "../../../xenlib/xen/actions/vm/startapplianceaction.h"
#include "../../../xenlib/xen/network/connection.h"
#include "../../../xenlib/xen/xenobject.h"
#include "../../../xenlib/xencache.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include <QMessageBox>

VappStartCommand::VappStartCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool VappStartCommand::CanRun() const
{
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

        QVariantMap appData = cache->ResolveObjectData(type, objRef);
        return this->canStartAppliance(appData);
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

        QVariantMap appData = cache->ResolveObjectData("vm_appliance", applianceRef);
        return this->canStartAppliance(appData);
    }

    return false;
}

void VappStartCommand::Run()
{
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

    QVariantMap appData = cache->ResolveObjectData("vm_appliance", applianceRef);
    QString appName = appData.value("name_label").toString();

    if (appName.isEmpty())
    {
        appName = applianceRef;
    }

    // Validate before starting
    if (!this->canStartAppliance(appData))
    {
        QMessageBox::warning(this->mainWindow(), tr("Cannot Start vApp"),
                             tr("VM appliance '%1' cannot be started").arg(appName));
        return;
    }

    // Get connection
    XenConnection* conn = connection;
    if (!conn || !conn->IsConnected())
    {
        QMessageBox::warning(this->mainWindow(), tr("Not Connected"),
                             tr("Not connected to XenServer"));
        return;
    }

    // Create and start action
    StartApplianceAction* action = new StartApplianceAction(conn, applianceRef, this->mainWindow());

    // Register with OperationManager for history tracking
    OperationManager::instance()->RegisterOperation(action);

    // Connect completion signal for cleanup and feedback
    connect(action, &AsyncOperation::completed, this, [=]() {
        if (action->state() == AsyncOperation::Completed)
        {
            this->mainWindow()->showStatusMessage(
                tr("vApp '%1' started successfully").arg(appName), 5000);
        } else if (action->state() == AsyncOperation::Failed)
        {
            QMessageBox::critical(this->mainWindow(), tr("Error"),
                                  tr("Failed to start vApp '%1':\n%2")
                                      .arg(appName, action->errorMessage()));
        }

        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString VappStartCommand::MenuText() const
{
    return tr("Start v&App");
}

bool VappStartCommand::canStartAppliance(const QVariantMap& applianceData) const
{
    if (applianceData.isEmpty())
    {
        return false;
    }

    // Check if "start" operation is allowed
    QVariantList allowedOps = applianceData.value("allowed_operations").toList();
    for (const QVariant& op : allowedOps)
    {
        if (op.toString() == "start")
        {
            return true;
        }
    }

    return false;
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
