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
#include "xenlib.h"
#include "xencache.h"
#include <QMessageBox>

InstantVMFromTemplateCommand::InstantVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool InstantVMFromTemplateCommand::canRun() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return false;

    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap templateData = cache->resolve("vm", templateRef);

    return this->canRunTemplate(templateData);
}

void InstantVMFromTemplateCommand::run()
{
    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return;

    if (!this->mainWindow()->xenLib())
        return;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return;

    QVariantMap templateData = cache->resolve("vm", templateRef);

    if (!this->canRunTemplate(templateData))
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

QString InstantVMFromTemplateCommand::menuText() const
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

bool InstantVMFromTemplateCommand::canRunTemplate(const QVariantMap& templateData) const
{
    if (templateData.isEmpty())
        return false;

    // Must be a template
    if (!templateData.value("is_a_template", false).toBool())
        return false;

    // Must not be locked
    QVariantMap currentOps = templateData.value("current_operations", QVariantMap()).toMap();
    if (!currentOps.isEmpty())
        return false;

    // Must not be a snapshot
    if (templateData.value("is_a_snapshot", false).toBool())
        return false;

    // Must be an instant template
    if (!this->isInstantTemplate(templateData))
        return false;

    return true;
}

bool InstantVMFromTemplateCommand::isInstantTemplate(const QVariantMap& templateData) const
{
    // TODO: Check if template has InstantTemplate flag
    // In C#: vm.InstantTemplate()
    // May need to check other_config["instant"] or similar field

    // For now, check if template has provisions field
    QVariantMap provisions = templateData.value("provisions", QVariantMap()).toMap();
    return !provisions.isEmpty();
}
