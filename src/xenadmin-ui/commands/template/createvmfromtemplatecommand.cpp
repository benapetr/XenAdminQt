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

#include "createvmfromtemplatecommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"

CreateVMFromTemplateCommand::CreateVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool CreateVMFromTemplateCommand::CanRun() const
{
    // Can run if template is selected (delegates to submenu items)
    return this->isTemplateSelected();
}

void CreateVMFromTemplateCommand::Run()
{
    // This is a submenu, run() doesn't get called
}

QString CreateVMFromTemplateCommand::MenuText() const
{
    return "Create VM from Template";
}

QString CreateVMFromTemplateCommand::getSelectedTemplateRef() const
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return QString();

    XenObjectType objectType = this->getSelectedObjectType();
    if (objectType != XenObjectType::VM)
        return QString();

    QString vmRef = this->getSelectedObjectRef();
    if (vmRef.isEmpty())
        return QString();

    QSharedPointer<VM> vm = object->GetConnection()->GetCache()->ResolveObject<VM>(XenObjectType::VM, vmRef);
    if (!vm)
        return QString();

    // Check if it's a template
    if (!vm->IsTemplate())
        return QString();

    return vmRef;
}

bool CreateVMFromTemplateCommand::isTemplateSelected() const
{
    return !this->getSelectedTemplateRef().isEmpty();
}
