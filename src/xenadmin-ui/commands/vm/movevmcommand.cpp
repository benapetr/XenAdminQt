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

#include "movevmcommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/crosspoolmigratewizard.h"
#include "xen/vm.h"
#include "xen/sr.h"
#include "xen/actions/vm/vmmoveaction.h"
#include "../../operations/operationmanager.h"
#include "../vm/crosspoolmigratecommand.h"
#include "xencache.h"
#include <QInputDialog>
#include <QMessageBox>

MoveVMCommand::MoveVMCommand(MainWindow* mainWindow, QObject* parent)
    : VMCommand(mainWindow, parent)
{
}

bool MoveVMCommand::CanRun() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    XenConnection* connection = vm->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return false;

    QStringList vbdRefs = vm->VBDRefs();
    for (const QString& vbdRef : vbdRefs)
    {
        QVariantMap vbdData = cache->ResolveObjectData("vbd", vbdRef);
        QString vdiRef = vbdData.value("VDI").toString();
        if (vdiRef.isEmpty())
            continue;

        QVariantMap vdiData = cache->ResolveObjectData("vdi", vdiRef);
        if (vdiData.value("cbt_enabled", false).toBool())
            return false;
    }

    return this->canVMBeMoved();
}

void MoveVMCommand::Run()
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return;

    CrossPoolMigrateCommand crossPoolCmd(this->mainWindow(),
                                         CrossPoolMigrateWizard::WizardMode::Move,
                                         this->mainWindow());
    if (crossPoolCmd.CanRun())
    {
        CrossPoolMigrateWizard wizard(this->mainWindow(), vm, CrossPoolMigrateWizard::WizardMode::Move);
        wizard.exec();
        return;
    }

    XenConnection* connection = vm->GetConnection();
    XenCache* cache = connection ? connection->GetCache() : nullptr;
    if (!cache)
        return;

    QStringList srRefs = cache->GetAllRefs("sr");
    QStringList srNames;
    QList<QString> eligibleRefs;
    for (const QString& srRef : srRefs)
    {
        QVariantMap srData = cache->ResolveObjectData("sr", srRef);
        QString type = srData.value("type").toString();
        QString content = srData.value("content_type").toString();
        if (type == "iso" || content == "iso")
            continue;
        srNames.append(srData.value("name_label", "SR").toString());
        eligibleRefs.append(srRef);
    }

    if (eligibleRefs.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), tr("Move VM"), tr("No suitable SRs available for move."));
        return;
    }

    bool ok = false;
    QString selectedName = QInputDialog::getItem(this->mainWindow(),
                                                 tr("Move VM"),
                                                 tr("Select target SR:"),
                                                 srNames,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || selectedName.isEmpty())
        return;

    int idx = srNames.indexOf(selectedName);
    if (idx < 0 || idx >= eligibleRefs.size())
        return;

    QString srRef = eligibleRefs.at(idx);
    QSharedPointer<SR> srObj = cache->ResolveObject<SR>("sr", srRef);
    if (!srObj || !srObj->IsValid())
        return;

    VMMoveAction* action = new VMMoveAction(connection,
                                            vm.data(),
                                            srObj.data(),
                                            nullptr,
                                            this->mainWindow());
    OperationManager::instance()->registerOperation(action);
    action->runAsync();
}

QString MoveVMCommand::MenuText() const
{
    return "Move VM...";
}

bool MoveVMCommand::canVMBeMoved() const
{
    QSharedPointer<VM> vm = this->getVM();
    if (!vm)
        return false;

    return vm->CanBeMoved();
}
