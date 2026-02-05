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

#include "newvmfromtemplatecommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/newvmwizard.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/vm.h"
#include <QMessageBox>

NewVMFromTemplateCommand::NewVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool NewVMFromTemplateCommand::CanRun() const
{
    XenObjectType objectType = this->getSelectedObjectType();
    if (objectType != XenObjectType::VM)
        return false;

    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return false;

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return false;

    XenConnection* connection = selectedObject->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    QSharedPointer<VM> templateVm = cache->ResolveObject<VM>(XenObjectType::VM, templateRef);
    return this->canRunTemplate(templateVm);
}

void NewVMFromTemplateCommand::Run()
{
    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return;

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return;

    QSharedPointer<VM> templateVm = selectedObject->GetCache()->ResolveObject<VM>(XenObjectType::VM, templateRef);
    if (!this->canRunTemplate(templateVm))
    {
        QMessageBox::warning(MainWindow::instance(), "Cannot Create VM", "The selected template cannot be used to create a VM.");
        return;
    }

    // Launch New VM wizard with template as source
    if (!selectedObject->GetConnection())
        return;

    NewVMWizard* wizard = new NewVMWizard(selectedObject->GetConnection(), MainWindow::instance());
    // TODO: Set template as wizard source
    // wizard->setSourceTemplate(templateRef);
    wizard->show();
}

QString NewVMFromTemplateCommand::MenuText() const
{
    return "New VM from Template";
}

QString NewVMFromTemplateCommand::getSelectedTemplateRef() const
{
    if (this->getSelectedObjectType() != XenObjectType::VM)
        return QString();

    return this->getSelectedObjectRef();
}

bool NewVMFromTemplateCommand::canRunTemplate(const QSharedPointer<VM>& templateVm) const
{
    if (!templateVm)
        return false;

    // Must be a template
    if (!templateVm->IsTemplate())
        return false;

    // Must not be a snapshot
    if (templateVm->IsSnapshot())
        return false;

    // Must not be locked
    if (!templateVm->CurrentOperations().isEmpty())
        return false;

    // Connection must be connected
    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject)
        return false;

    XenConnection* connection = selectedObject->GetConnection();
    if (!connection || !connection->IsConnected())
        return false;

    // Pool must have enabled hosts
    // TODO: Implement poolHasEnabledHosts() check
    // if (!this->poolHasEnabledHosts())
    //     return false;

    return true;
}

bool NewVMFromTemplateCommand::poolHasEnabledHosts() const
{
    // TODO: Implement check for enabled hosts in pool
    // Get pool ref from connection
    // Get all hosts in pool
    // Check if any host is enabled (not disabled)
    return true; // Assume true for now
}
