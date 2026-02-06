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

#include "controldomainmemorydialog.h"
#include "ui_controldomainmemorydialog.h"
#include "../mainwindow.h"
#include "warningdialogs/warningdialog.h"
#include "../commands/host/hostmaintenancemodecommand.h"
#include "operations/multipleactionlauncher.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/host/changecontroldomainmemoryaction.h"
#include "xenlib/xen/actions/host/reboothostaction.h"
#include <QMessageBox>

const int ControlDomainMemoryDialog::MaximumDom0MemoryMB = 256 * 1024;

ControlDomainMemoryDialog::ControlDomainMemoryDialog(QSharedPointer<Host> host, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ControlDomainMemoryDialog),
      m_host(host),
      m_originalMemoryMB(0)
{
    this->ui->setupUi(this);

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &ControlDomainMemoryDialog::onAccepted);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->enterMaintenanceModeButton, &QPushButton::clicked, this, &ControlDomainMemoryDialog::onEnterMaintenanceMode);

    this->setWindowTitle(tr("Control Domain Memory - %1").arg(this->m_host ? this->m_host->GetName() : tr("Host")));

    this->populate();
}

ControlDomainMemoryDialog::~ControlDomainMemoryDialog()
{
    delete this->ui;
}

void ControlDomainMemoryDialog::onAccepted()
{
    if (this->saveChanges())
        this->accept();
}

void ControlDomainMemoryDialog::onEnterMaintenanceMode()
{
    if (!this->m_host)
        return;

    HostMaintenanceModeCommand command(MainWindow::instance(), this->m_host, true, this);
    command.Run();
    this->populate();
}

void ControlDomainMemoryDialog::populate()
{
    if (!this->m_host)
        return;

    QSharedPointer<VM> vm = this->m_host->ControlDomainZero();
    if (!vm)
        return;

    const qint64 dynamicMax = vm->GetMemoryDynamicMax();
    const qint64 dynamicMin = vm->GetMemoryDynamicMin();
    if (dynamicMax >= dynamicMin)
    {
        qint64 minBytes = vm->GetMemoryStaticMin();
        qint64 maxBytes = qMin(dynamicMin + this->m_host->MemoryAvailableCalc(),
                               static_cast<qint64>(MaximumDom0MemoryMB) * 1024LL * 1024LL);
        const qint64 valueBytes = dynamicMin;

        if (valueBytes > maxBytes)
            maxBytes = valueBytes;
        if (valueBytes < minBytes)
            minBytes = valueBytes;

        int minMB = static_cast<int>(minBytes / (1024LL * 1024LL));
        int maxMB = static_cast<int>(maxBytes / (1024LL * 1024LL));
        int valueMB = static_cast<int>(valueBytes / (1024LL * 1024LL));

        if (valueMB > maxMB)
            maxMB = valueMB;
        if (valueMB < minMB)
            minMB = valueMB;

        this->ui->memorySpinner->setRange(0, MaximumDom0MemoryMB);
        this->ui->memorySpinner->setValue(valueMB);
        this->ui->memorySpinner->setRange(minMB, maxMB);

        this->ui->minimumValueLabel->setText(tr("Minimum: %1 MB").arg(minMB));
        this->ui->maximumValueLabel->setText(tr("Maximum: %1 MB").arg(maxMB));
    }

    this->m_originalMemoryMB = this->ui->memorySpinner->value();
    this->updateMaintenanceWarning();
}

void ControlDomainMemoryDialog::updateMaintenanceWarning()
{
    if (!this->m_host)
        return;

    QSharedPointer<HostMetrics> metrics = this->m_host->GetMetrics();
    const bool maintenanceMode = !this->m_host->IsEnabled() || (metrics && !metrics->IsLive());

    this->ui->maintenanceWarningLabel->setVisible(!maintenanceMode);
    this->ui->enterMaintenanceModeButton->setVisible(!maintenanceMode);
    this->ui->rebootWarningLabel->setVisible(maintenanceMode);
    this->ui->memorySpinner->setEnabled(maintenanceMode);
}

bool ControlDomainMemoryDialog::hasChanged() const
{
    return this->ui->memorySpinner->value() != this->m_originalMemoryMB;
}

bool ControlDomainMemoryDialog::saveChanges()
{
    if (!this->hasChanged() || !this->m_host)
        return false;

    if (WarningDialog::ShowYesNo(tr("Changing control domain memory requires a host reboot. Continue?"),
                                 tr("Confirm Control Domain Memory Change"),
                                 this) != WarningDialog::Result::Yes)
    {
        return false;
    }

    const qint64 memory = static_cast<qint64>(this->ui->memorySpinner->value()) * 1024LL * 1024LL;

    auto ntolPrompt = [](QSharedPointer<Pool> pool, qint64 current, qint64 target) {
        const QString poolName = pool ? pool->GetName() : QString();
        const QString poolLabel = poolName.isEmpty() ? "pool" : QString("pool '%1'").arg(poolName);
        const QString text = QString("HA is enabled for %1.\n\n"
                                     "To reboot this host, the pool's host failures to tolerate must be "
                                     "reduced from %2 to %3.\n\n"
                                     "Do you want to continue?")
                                 .arg(poolLabel)
                                 .arg(current)
                                 .arg(target);

        return QMessageBox::question(MainWindow::instance(),
                                     "Adjust HA Failures to Tolerate",
                                     text,
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No) != QMessageBox::Yes;
    };

    QList<AsyncOperation*> actions;
    actions.append(new ChangeControlDomainMemoryAction(this->m_host, memory, false, nullptr));
    actions.append(new RebootHostAction(this->m_host, ntolPrompt, nullptr));

    MultipleActionLauncher launcher(actions,
                                    tr("Changing control domain memory for %1").arg(this->m_host->GetName()),
                                    tr("Changing control domain memory..."),
                                    tr("Completed"),
                                    false,
                                    nullptr);
    launcher.Run();
    return true;
}
