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

#include "deletetemplatecommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xen/api.h"
#include "xencache.h"
#include <QMessageBox>
#include <QTimer>

DeleteTemplateCommand::DeleteTemplateCommand(MainWindow* mainWindow, QObject* parent) : DeleteVMCommand(mainWindow, parent)
{
}

bool DeleteTemplateCommand::CanRun() const
{
    QString templateRef = this->getSelectedVMRef();
    if (templateRef.isEmpty())
        return false;

    return this->canDeleteTemplate(templateRef);
}

QString DeleteTemplateCommand::MenuText() const
{
    return "Delete Template";
}

bool DeleteTemplateCommand::canDeleteTemplate(const QString& templateRef) const
{
    QSharedPointer<XenObject> object = this->GetObject();
    if (!object || !object->GetConnection())
        return false;

    // Use cache instead of async API call
    QVariantMap vmData = object->GetConnection()->GetCache()->ResolveObjectData("vm", templateRef);

    // Check if it's a template
    bool isTemplate = vmData.value("is_a_template", false).toBool();
    if (!isTemplate)
        return false;

    // Check if it's a snapshot
    bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();
    if (isSnapshot)
        return false;

    // Check if the operation is allowed
    QVariantList allowedOps = vmData.value("allowed_operations", QVariantList()).toList();
    QStringList allowedOpsStrings;
    for (const QVariant& op : allowedOps)
    {
        allowedOpsStrings << op.toString();
    }

    return allowedOpsStrings.contains("destroy");
}
