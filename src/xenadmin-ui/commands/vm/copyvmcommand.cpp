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

#include "copyvmcommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include <QMessageBox>

CopyVMCommand::CopyVMCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool CopyVMCommand::canRun() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return false;

    // Can copy if VM is not locked and copy/clone operation is allowed
    return !this->isVMLocked() && this->canVMBeCopied();
}

void CopyVMCommand::run()
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return;

    // TODO: Launch Copy VM wizard/dialog
    QMessageBox::information(this->mainWindow(), "Not Implemented",
                             "Copy VM to Shared Storage functionality will be implemented in future phase.\n\n"
                             "This allows copying a VM to shared storage for migration purposes.");
}

QString CopyVMCommand::menuText() const
{
    return "Copy VM to Shared Storage...";
}

QString CopyVMCommand::getSelectedVMRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

bool CopyVMCommand::isVMLocked() const
{
    QString vmRef = this->getSelectedVMRef();
    if (vmRef.isEmpty())
        return true;

    if (!this->mainWindow()->xenLib())
        return true;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return true;

    QVariantMap vmData = cache->resolve("vm", vmRef);

    // Check if VM has current operations that would lock it
    QVariantMap currentOps = vmData.value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}

bool CopyVMCommand::canVMBeCopied() const
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

    // Check if VM is a template or snapshot
    if (vmData.value("is_a_template", false).toBool())
        return false;
    if (vmData.value("is_a_snapshot", false).toBool())
        return false;

    // Check if clone operation is allowed
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    return allowedOps.contains("clone") || allowedOps.contains("copy");
}
