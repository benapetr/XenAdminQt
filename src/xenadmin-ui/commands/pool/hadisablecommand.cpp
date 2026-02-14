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

#include "hadisablecommand.h"
#include "../../mainwindow.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/actions/pool/disablehaaction.h"
#include <QMessageBox>

HADisableCommand::HADisableCommand(MainWindow* mainWindow, QObject* parent) : HACommand(mainWindow, parent)
{
}

bool HADisableCommand::CanRun() const
{
    return this->CanRunHACommand();
}

void HADisableCommand::Run()
{
    const CantRunReason cantRun = this->GetCantRunReason();
    if (cantRun != CantRunReason::None)
    {
        QMessageBox::warning(MainWindow::instance(), tr("Disable High Availability"), this->GetCantRunReasonText(cantRun));
        return;
    }

    QSharedPointer<Pool> pool = this->getTargetPool();
    if (!pool || !pool->IsValid())
        return;

    if (this->allStatefilesUnresolvable(pool))
    {
        QMessageBox::critical(MainWindow::instance(), tr("Disable High Availability"), tr("Cannot resolve HA statefile VDI for pool '%1'.").arg(pool->GetName()));
        return;
    }

    QString poolName = pool->GetName();

    // Show confirmation dialog
    int ret = QMessageBox::question(MainWindow::instance(), "Disable High Availability",
                                    QString("Are you sure you want to disable High Availability for pool '%1'?\n\n"
                                            "VMs will no longer restart automatically if a host fails.")
                                        .arg(poolName),
                                    QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    DisableHAAction* action = new DisableHAAction(pool);
    this->RunMultipleActions({action}, tr("Disabling High Availability"), tr("Disabling HA on pool '%1'...").arg(poolName), tr("High Availability disabled for pool '%1'.").arg(poolName), false, true);
}

QString HADisableCommand::MenuText() const
{
    return "Disable";
}

bool HADisableCommand::CanRunOnPool(const QSharedPointer<Pool>& pool) const
{
    return pool && pool->HAEnabled();
}
