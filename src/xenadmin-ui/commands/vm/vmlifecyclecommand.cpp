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
#include "xencache.h"
#include <QMessageBox>
#include <QMessageBox>

VMLifeCycleCommand::VMLifeCycleCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool VMLifeCycleCommand::CanRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    QString powerState = this->getVMPowerState();

    // Can run if VM is in Halted, Running, Paused, or Suspended state
    return (powerState == "Halted" && this->isVMOperationAllowed("start")) ||
           (powerState == "Running" && this->isVMOperationAllowed("clean_shutdown")) ||
           (powerState == "Paused" && this->isVMOperationAllowed("unpause")) ||
           (powerState == "Suspended" && this->isVMOperationAllowed("resume"));
}

void VMLifeCycleCommand::Run()
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return;

    QString powerState = this->getVMPowerState();

    // TODO: Implement using existing VM commands (StartVMCommand, etc)
    if (powerState == "Halted")
    {
        this->mainWindow()->ShowStatusMessage("Start VM not implemented yet");
        QMessageBox::information(this->mainWindow(), "Not Implemented", "Start VM");
    } else if (powerState == "Running")
    {
        this->mainWindow()->ShowStatusMessage("Shut Down VM not implemented yet");
        QMessageBox::information(this->mainWindow(), "Not Implemented", "Shut Down VM");
    } else if (powerState == "Paused")
    {
        this->mainWindow()->ShowStatusMessage("Unpause VM not implemented yet");
        QMessageBox::information(this->mainWindow(), "Not Implemented", "Unpause VM");
    } else if (powerState == "Suspended")
    {
        this->mainWindow()->ShowStatusMessage("Resume VM not implemented yet");
        QMessageBox::information(this->mainWindow(), "Not Implemented", "Resume VM");
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

QString VMLifeCycleCommand::getSelectedVMRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

QString VMLifeCycleCommand::getVMPowerState() const
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return QString();

    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return QString();

    QVariantMap vmData = object->GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
    return vmData.value("power_state", "").toString();
}

bool VMLifeCycleCommand::isVMOperationAllowed(const QString& operation) const
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return false;

    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    QVariantMap vmData = object->GetConnection()->GetCache()->ResolveObjectData("vm", vmRef);
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();

    return allowedOps.contains(operation);
}
