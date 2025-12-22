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
#include "xenlib.h"
#include "xencache.h"

CreateVMFromTemplateCommand::CreateVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool CreateVMFromTemplateCommand::canRun() const
{
    // Can run if template is selected (delegates to submenu items)
    return this->isTemplateSelected();
}

void CreateVMFromTemplateCommand::run()
{
    // This is a submenu, run() doesn't get called
}

QString CreateVMFromTemplateCommand::menuText() const
{
    return "Create VM from Template";
}

QString CreateVMFromTemplateCommand::getSelectedTemplateRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    QString vmRef = this->getSelectedObjectRef();
    if (vmRef.isEmpty())
        return QString();

    if (!this->mainWindow()->xenLib())
        return QString();

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return QString();

    QVariantMap vmData = cache->ResolveObjectData("vm", vmRef);

    // Check if it's a template
    if (!vmData.value("is_a_template", false).toBool())
        return QString();

    return vmRef;
}

bool CreateVMFromTemplateCommand::isTemplateSelected() const
{
    return !this->getSelectedTemplateRef().isEmpty();
}
