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

#include "exporttemplatecommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/exportwizard.h"
#include "xenlib.h"
#include "xen/api.h"
#include "xencache.h"
#include <QMessageBox>

ExportTemplateCommand::ExportTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool ExportTemplateCommand::canRun() const
{
    QString templateRef = this->getSelectedTemplateRef();
    if (templateRef.isEmpty())
        return false;

    return this->canExportTemplate(templateRef);
}

void ExportTemplateCommand::run()
{
    QString templateRef = this->getSelectedTemplateRef();
    QString templateName = this->getSelectedTemplateName();

    if (templateRef.isEmpty() || templateName.isEmpty())
        return;

    // Launch the export wizard
    // Note: The wizard will allow the user to select which templates/VMs to export
    // The currently selected template will be the default choice
    ExportWizard wizard(this->mainWindow());

    if (wizard.exec() == QDialog::Accepted)
    {
        this->mainWindow()->showStatusMessage(QString("Template '%1' export completed").arg(templateName), 5000);
    }
}

QString ExportTemplateCommand::menuText() const
{
    return "Export Template";
}

QString ExportTemplateCommand::getSelectedTemplateRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "template")
        return QString();

    return this->getSelectedObjectRef();
}

QString ExportTemplateCommand::getSelectedTemplateName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "template")
        return QString();

    return item->text(0);
}

bool ExportTemplateCommand::canExportTemplate(const QString& templateRef) const
{
    // Use cache instead of async API call
    QVariantMap vmData = this->mainWindow()->xenLib()->getCache()->resolve("vm", templateRef);

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

    return allowedOpsStrings.contains("export");
}
