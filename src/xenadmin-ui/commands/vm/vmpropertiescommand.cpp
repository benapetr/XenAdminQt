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

#include "vmpropertiescommand.h"
#include <QDebug>
#include "../../mainwindow.h"
#include <QDebug>
#include "../../dialogs/vmpropertiesdialog.h"
#include <QDebug>
#include "xenlib.h"
#include <QDebug>
#include <QtWidgets>

VMPropertiesCommand::VMPropertiesCommand(QObject* parent)
    : Command(nullptr, parent)
{
    // qDebug() << "VMPropertiesCommand: Created default constructor";
}

VMPropertiesCommand::VMPropertiesCommand(const QString& vmUuid, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_vmUuid(vmUuid)
{
    // qDebug() << "VMPropertiesCommand: Created with VM UUID:" << vmUuid;
}

void VMPropertiesCommand::run()
{
    // qDebug() << "VMPropertiesCommand: Executing VM Properties command for VM:" << this->m_vmUuid;

    if (!this->canRun())
    {
        qWarning() << "VMPropertiesCommand: Cannot execute - no VM selected or invalid state";
        QMessageBox::warning(nullptr, tr("Cannot Show Properties"),
                             tr("No VM is selected or the VM is in an invalid state."));
        return;
    }

    this->showPropertiesDialog();
}

bool VMPropertiesCommand::canRun() const
{
    // Match C# logic:
    // VMPropertiesCommand: selection is VM AND not template AND not snapshot AND not locked
    // TemplatePropertiesCommand: selection is VM AND is_a_template AND not snapshot AND not locked
    //
    // Since Qt uses the same VMPropertiesDialog for both, we accept both vm and template types

    if (!this->mainWindow())
        return false;

    QString objectType = this->getSelectedObjectType();
    if (objectType != "vm" && objectType != "template")
        return false;

    // Check if we have a valid VM/template reference
    QString vmRef = this->getSelectedObjectRef();
    if (vmRef.isEmpty())
        return false;

    // TODO: Check locked state and snapshot state when implemented
    // For now, allow if we have a VM/template selected and connected
    return this->xenLib() && this->xenLib()->isConnected();
}

QString VMPropertiesCommand::menuText() const
{
    return tr("Properties...");
}

QString VMPropertiesCommand::toolTip() const
{
    return tr("Show virtual machine properties and configuration");
}

QIcon VMPropertiesCommand::icon() const
{
    // Use standard Qt icon for now - should be replaced with proper properties icon
    return QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView);
}

void VMPropertiesCommand::showPropertiesDialog()
{
    qDebug() << "VMPropertiesCommand: Opening VM Properties Dialog for VM:" << this->m_vmUuid;

    // Get selected VM ref from command base class
    QString vmRef = this->getSelectedObjectRef();
    if (vmRef.isEmpty())
    {
        qWarning() << "VMPropertiesCommand: No VM selected";
        return;
    }

    // Get connection from xenLib
    XenConnection* connection = xenLib()->getConnection();
    if (!connection)
    {
        qWarning() << "VMPropertiesCommand: No connection available";
        QMessageBox::warning(mainWindow(), tr("No Connection"),
                             tr("Not connected to XenServer."));
        return;
    }

    VMPropertiesDialog dialog(connection, vmRef, mainWindow());

    if (dialog.exec() == QDialog::Accepted)
    {
        qDebug() << "VMPropertiesCommand: VM Properties dialog completed with changes";
        mainWindow()->refreshServerTree();
        qDebug() << "VMPropertiesCommand: VM properties updated successfully";
    } else
    {
        qDebug() << "VMPropertiesCommand: VM Properties dialog cancelled";
    }
}
