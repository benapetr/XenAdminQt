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
#include "xenlib/xen/pool.h"
#include <QMessageBox>

HAConfigureCommand::HAConfigureCommand(MainWindow* mainWindow, QObject* parent) : HACommand(mainWindow, parent)
{
}

bool HAConfigureCommand::CanRun() const
{
    return this->CanRunHACommand();
}

void HAConfigureCommand::Run()
{
    const CantRunReason cantRun = this->GetCantRunReason();
    if (cantRun != CantRunReason::None)
    {
        QMessageBox::warning(MainWindow::instance(), tr("Configure High Availability"), this->GetCantRunReasonText(cantRun));
        return;
    }

    QSharedPointer<Pool> pool = this->getTargetPool();
    if (!pool || !pool->IsValid())
        return;

    if (this->hasPendingPoolSecretRotationConflict(pool))
    {
        QMessageBox::warning(MainWindow::instance(), tr("Configure High Availability"), tr("HA cannot be configured while pool secret rotation is pending."));
        return;
    }

    // Check if HA is already enabled
    if (pool->HAEnabled())
    {
        if (this->allStatefilesUnresolvable(pool))
        {
            QMessageBox::critical(MainWindow::instance(), tr("Configure High Availability"), tr("Cannot resolve HA statefile VDI for pool '%1'.").arg(pool->GetName()));
            return;
        }

        // HA is already enabled - show edit dialog to modify priorities
        const QStringList requiredMethods = {"pool.set_ha_host_failures_to_tolerate",
                                             "pool.sync_database",
                                             "vm.set_ha_restart_priority",
                                             "pool.ha_compute_hypothetical_max_host_failures_to_tolerate"};
        QStringList missingMethods;
        if (!this->HasRequiredPermissions(pool, requiredMethods, &missingMethods))
        {
            QMessageBox::warning(MainWindow::instance(),
                                 tr("Configure High Availability"),
                                 tr("Your current role is not authorized to edit HA priorities.\nMissing permissions:\n%1")
                                     .arg(missingMethods.join("\n")));
            return;
        }

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

bool HAConfigureCommand::CanRunOnPool(const QSharedPointer<Pool>& pool) const
{
    return pool && !this->hasPendingPoolSecretRotationConflict(pool);
}
