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

#include "instantvmfromtemplatecommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/vm.h"
#include "xencache.h"
#include <QMessageBox>

InstantVMFromTemplateCommand::InstantVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool InstantVMFromTemplateCommand::CanRun() const
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return false;

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return false;

    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return false;

    QSharedPointer<VM> templateVm = object->GetConnection()->GetCache()->ResolveObject<VM>("vm", templateRef);
    return this->canRunTemplate(templateVm);
}

void InstantVMFromTemplateCommand::Run()
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return;

    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return;

    QSharedPointer<VM> templateVm = object->GetConnection()->GetCache()->ResolveObject<VM>("vm", templateRef);
    if (!this->canRunTemplate(templateVm))
    {
        QMessageBox::warning(this->mainWindow(), "Cannot Create VM",
                             "The selected template cannot be used for instant VM creation.");
        return;
    }

    // TODO: Implement fast VM creation
    // CreateVMFastAction action = new CreateVMFastAction(connection, template);
    // action.Completed += (sender) => {
    //     if (action.Succeeded) {
    //         VMStartAction startAction = new VMStartAction(vm, ...);
    //         startAction.RunAsync();
    //     }
    // };
    // action.RunAsync();

    QMessageBox::information(this->mainWindow(), "Not Implemented",
                             "Instant VM creation will be implemented using CreateVMFastAction + auto-start.");
}

QString InstantVMFromTemplateCommand::MenuText() const
{
    return "Instant VM from Template";
}

QString InstantVMFromTemplateCommand::getSelectedTemplateRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

bool InstantVMFromTemplateCommand::canRunTemplate(const QSharedPointer<VM>& templateVm) const
{
    if (!templateVm)
        return false;

    // Must be a template
    if (!templateVm->IsTemplate())
        return false;

    // Must not be locked
    if (!templateVm->CurrentOperations().isEmpty())
        return false;

    // Must not be a snapshot
    if (templateVm->IsSnapshot())
        return false;

    // Must be an instant template
    if (!this->isInstantTemplate(templateVm))
        return false;

    return true;
}

bool InstantVMFromTemplateCommand::isInstantTemplate(const QSharedPointer<VM>& templateVm) const
{
    // TODO: Check if template has InstantTemplate flag
    // In C#: vm.InstantTemplate()
    // May need to check other_config["instant"] or similar field

    // For now, check if template has provisions field
    QVariantMap provisions = templateVm->GetData().value("provisions", QVariantMap()).toMap();
    return !provisions.isEmpty();
}
