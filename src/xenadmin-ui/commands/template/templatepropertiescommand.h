/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef TEMPLATEPROPERTIESCOMMAND_H
#define TEMPLATEPROPERTIESCOMMAND_H

#include "templatecommand.h"
#include "../../dialogs/vmpropertiesdialog.h"
#include "../../mainwindow.h"
#include "xenlib/xen/vm.h"

class TemplatePropertiesCommand : public TemplateCommand
{
    public:
        explicit TemplatePropertiesCommand(MainWindow* mainWindow, QObject* parent = nullptr) : TemplateCommand(mainWindow, parent)
        {
        }

        void Run() override
        {
            if (!this->CanRun())
                return;

            QSharedPointer<VM> templateVm = this->getTemplate();
            if (!templateVm)
                return;

            VMPropertiesDialog dialog(templateVm, MainWindow::instance());
            if (dialog.exec() == QDialog::Accepted && this->mainWindow())
                this->mainWindow()->RefreshServerTree();
        }

        bool CanRun() const override
        {
            const QList<QSharedPointer<XenObject>> selected = this->getSelectedObjects();
            if (selected.size() != 1)
                return false;

            QSharedPointer<VM> templateVm = this->getTemplate();
            if (!templateVm)
                return false;

            if (!templateVm->GetConnection() || !templateVm->GetConnection()->IsConnected())
                return false;

            return templateVm->IsTemplate() && !templateVm->IsSnapshot() && !templateVm->IsLocked();
        }

        QString MenuText() const override
        {
            return tr("Properties...");
        }
};

#endif // TEMPLATEPROPERTIESCOMMAND_H
