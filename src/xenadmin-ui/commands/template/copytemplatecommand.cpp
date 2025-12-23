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

#include "copytemplatecommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xencache.h"
#include <QMessageBox>

CopyTemplateCommand::CopyTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool CopyTemplateCommand::CanRun() const
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

    QVariantMap templateData = cache->ResolveObjectData("vm", templateRef);

    return this->canRunTemplate(templateData);
}

void CopyTemplateCommand::Run()
{
    QString templateRef = this->getSelectedObjectRef();
    if (templateRef.isEmpty())
        return;

    if (!this->mainWindow()->xenLib())
        return;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return;

    QVariantMap templateData = cache->ResolveObjectData("vm", templateRef);

    if (!this->canRunTemplate(templateData))
    {
        QMessageBox::warning(this->mainWindow(), "Cannot Copy Template",
                             "The selected template cannot be copied.");
        return;
    }

    // TODO: Implement template copy
    // if (canLaunchMigrateWizard(templateData)) {
    //     // Launch CrossPoolMigrateWizard
    // } else {
    //     // Launch CopyVMDialog for local copy
    // }

    QMessageBox::information(this->mainWindow(), "Not Implemented",
                             "Template copy will be implemented using CopyVMDialog or CrossPoolMigrateWizard.");
}

QString CopyTemplateCommand::MenuText() const
{
    return "Copy Template";
}

QString CopyTemplateCommand::getSelectedTemplateRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm")
        return QString();

    return this->getSelectedObjectRef();
}

bool CopyTemplateCommand::canRunTemplate(const QVariantMap& templateData) const
{
    if (templateData.isEmpty())
        return false;

    // Must be a template
    if (!templateData.value("is_a_template", false).toBool())
        return false;

    // Must not be a snapshot
    if (templateData.value("is_a_snapshot", false).toBool())
        return false;

    // Must not be locked
    QVariantMap currentOps = templateData.value("current_operations", QVariantMap()).toMap();
    if (!currentOps.isEmpty())
        return false;

    // Check allowed_operations is not null
    QVariantList allowedOps = templateData.value("allowed_operations", QVariantList()).toList();
    if (allowedOps.isEmpty())
        return false;

    // Must not be an internal template
    if (this->isInternalTemplate(templateData))
        return false;

    // Can launch migrate wizard, OR template supports clone/copy
    if (this->canLaunchMigrateWizard(templateData))
        return true;

    // Check if clone or copy operations are allowed
    bool cloneAllowed = false;
    bool copyAllowed = false;

    for (const QVariant& op : allowedOps)
    {
        QString opStr = op.toString();
        if (opStr == "clone")
            cloneAllowed = true;
        if (opStr == "copy")
            copyAllowed = true;
    }

    return cloneAllowed || copyAllowed;
}

bool CopyTemplateCommand::isInternalTemplate(const QVariantMap& templateData) const
{
    // Check if template is internal (built-in XenServer template)
    // In C#: vm.InternalTemplate()
    // Usually checks other_config["default_template"] or similar

    QVariantMap otherConfig = templateData.value("other_config", QVariantMap()).toMap();
    return otherConfig.value("default_template", false).toBool() ||
           otherConfig.value("base_template_name", "").toString().isEmpty() == false;
}

bool CopyTemplateCommand::isDefaultTemplate(const QVariantMap& templateData) const
{
    // Check if template is a default template
    QVariantMap otherConfig = templateData.value("other_config", QVariantMap()).toMap();
    return otherConfig.value("default_template", false).toBool();
}

bool CopyTemplateCommand::canLaunchMigrateWizard(const QVariantMap& templateData) const
{
    // Can launch migrate wizard if:
    // 1. Not a default template
    // 2. CrossPoolMigrateCommand can run

    if (this->isDefaultTemplate(templateData))
        return false;

    // TODO: Implement CrossPoolMigrateCommand.CanRun check
    // This would check if template can be migrated to another pool

    return false; // Disabled for now
}
