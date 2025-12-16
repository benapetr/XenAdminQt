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

#include "installtoolscommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include <QMessageBox>

InstallToolsCommand::InstallToolsCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool InstallToolsCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    // Can install tools if VM is running and tools are not already installed/current
    return this->isVMRunning() && this->canInstallTools();
}

void InstallToolsCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::question(this->mainWindow(), "Install XenServer Tools",
                                    "This will mount the XenServer Tools ISO in the VM.\n\n"
                                    "You can then install the tools from within the VM's guest OS.\n\n"
                                    "Continue?",
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage("Mounting XenServer Tools ISO...");

        // TODO: Implement VM.assert_can_mount_tools and VM.mount_tools_iso
        QMessageBox::information(this->mainWindow(), "Not Implemented",
                                 "Install Tools functionality will be implemented with XenAPI VM bindings.\n\n"
                                 "This will mount xs-tools.iso in the VM's CD drive.");
    }
}

QString InstallToolsCommand::menuText() const
{
    return "Install XenServer Tools...";
}

QString InstallToolsCommand::getSelectedVMRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

bool InstallToolsCommand::isVMRunning() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap vmData = cache->resolve("vm", vmRef);
    QString powerState = vmData.value("power_state", "").toString();

    return powerState == "Running";
}

bool InstallToolsCommand::canInstallTools() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap vmData = cache->resolve("vm", vmRef);

    // Check guest metrics for tools version
    QString guestMetricsRef = vmData.value("guest_metrics", "").toString();
    if (!guestMetricsRef.isEmpty() && guestMetricsRef != "OpaqueRef:NULL")
    {
        QVariantMap guestMetrics = cache->resolve("vm_guest_metrics", guestMetricsRef);
        QVariantMap pvDriversVersion = guestMetrics.value("PV_drivers_version", QVariantMap()).toMap();

        // If PV drivers are already installed and up-to-date, don't allow install
        // For now, always allow install (TODO: check version properly)
    }

    return true;
}
