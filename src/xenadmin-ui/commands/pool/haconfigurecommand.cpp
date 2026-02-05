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

#include "haconfigurecommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/hawizard.h"
#include "../../dialogs/editvmhaprioritiesdialog.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xencache.h"
#include <QMessageBox>

HAConfigureCommand::HAConfigureCommand(MainWindow* mainWindow, QObject* parent) : PoolCommand(mainWindow, parent)
{
}

bool HAConfigureCommand::CanRun() const
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool || !pool->IsValid())
        return false;

    // Can configure HA if:
    // - Pool is connected
    // - Pool has coordinator
    // - Pool is not locked
    return this->isPoolConnected() && this->hasCoordinator() && !this->isPoolLocked();
}

void HAConfigureCommand::Run()
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool || !pool->IsValid())
        return;

    // Check if HA is already enabled
    if (pool->HAEnabled())
    {
        // HA is already enabled - show edit dialog to modify priorities
        EditVmHaPrioritiesDialog dialog(pool, MainWindow::instance());
        dialog.exec();
        return;
    }

    // Launch HA wizard to enable HA
    HAWizard wizard(pool, MainWindow::instance());
    wizard.exec();
}

QString HAConfigureCommand::MenuText() const
{
    return "Configure...";
}

bool HAConfigureCommand::isPoolConnected() const
{
    QSharedPointer<Pool> pool = this->getPool();
    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    return conn && conn->IsConnected();
}

bool HAConfigureCommand::hasCoordinator() const
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool)
        return false;

    return !pool->GetMasterHost().isNull();
}

bool HAConfigureCommand::isPoolLocked() const
{
    QSharedPointer<Pool> pool = this->getPool();
    if (!pool)
        return false;

    // Check if pool has any operations in progress that would lock it
    return !pool->CurrentOperations().isEmpty();
}
