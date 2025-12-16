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
#include "newvmcommand.h"
#include <QtWidgets>

NewVMFromTemplateCommand::NewVMFromTemplateCommand(QObject* parent)
    : Command(nullptr, parent)
{
    qDebug() << "NewVMFromTemplateCommand: Created default constructor";
}

NewVMFromTemplateCommand::NewVMFromTemplateCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
    qDebug() << "NewVMFromTemplateCommand: Created with MainWindow";
}

NewVMFromTemplateCommand::NewVMFromTemplateCommand(const QString& templateUuid, MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent), m_templateUuid(templateUuid)
{
    qDebug() << "NewVMFromTemplateCommand: Created with template UUID:" << templateUuid;
}

void NewVMFromTemplateCommand::run()
{
    qDebug() << "NewVMFromTemplateCommand: Executing New VM from Template command";

    if (!canRun())
    {
        qWarning() << "NewVMFromTemplateCommand: Cannot execute - template not valid or no suitable host";
        QMessageBox::warning(nullptr, tr("Cannot Create VM"),
                             tr("The selected template cannot be used to create a VM.\n"
                                "Please ensure the template is valid and at least one host is available."));
        return;
    }

    createVMFromTemplate();
}

bool NewVMFromTemplateCommand::canRun() const
{
    if (!mainWindow())
    {
        return false;
    }

    // Check if template is valid and we have enabled hosts
    return isValidTemplate();
}

QString NewVMFromTemplateCommand::menuText() const
{
    return tr("New VM from Template");
}

bool NewVMFromTemplateCommand::isValidTemplate() const
{
    if (m_templateUuid.isEmpty())
    {
        return false;
    }

    // TODO: Replace with actual XenAPI template validation
    // For now, assume template is valid if UUID is provided
    return true;
}

void NewVMFromTemplateCommand::createVMFromTemplate()
{
    qDebug() << "NewVMFromTemplateCommand: Creating VM from template:" << m_templateUuid;

    // Delegate to NewVMCommand with the template pre-selected
    // This follows the original C# pattern where NewVMFromTemplateCommand
    // just calls NewVMCommand with the template selection
    NewVMCommand* newVMCmd = new NewVMCommand(m_templateUuid, mainWindow(), this);
    newVMCmd->run();

    if (mainWindow())
    {
        qDebug() << "NewVMFromTemplateCommand: New VM from template requested (Template:" << m_templateUuid << ")";
    }
}