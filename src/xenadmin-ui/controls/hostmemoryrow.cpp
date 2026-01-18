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

#include "hostmemoryrow.h"
#include "ui_hostmemoryrow.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vmmetrics.h"
#include "xenlib/utils/misc.h"
#include <cmath>
#include <QDebug>

HostMemoryRow::HostMemoryRow(QWidget* parent) : QWidget(parent), ui(new Ui::HostMemoryRow)
{
    this->ui->setupUi(this);
}

HostMemoryRow::HostMemoryRow(const QSharedPointer<Host>& host, QWidget* parent) : QWidget(parent), ui(new Ui::HostMemoryRow)
{
    this->ui->setupUi(this);
    this->SetHost(host);
}

HostMemoryRow::~HostMemoryRow()
{
    this->UnregisterHandlers();
    delete this->ui;
}

void HostMemoryRow::SetHost(const QSharedPointer<Host>& host)
{
    // Unregister old handlers
    this->UnregisterHandlers();
    
    this->host_ = host;
    
    if (this->host_.isNull())
        return;
    
    // C# equivalent: HostMemoryControls.host property setter
    // Subscribe to host property changes
    connect(this->host_.data(), &XenObject::dataChanged, this, &HostMemoryRow::onHostDataChanged);
    
    // Subscribe to all resident VM property changes
    QList<QSharedPointer<VM>> residentVMs = this->host_->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull())
            continue;
            
        connect(vm.data(), &XenObject::dataChanged, this, &HostMemoryRow::onVMDataChanged);
        
        // Subscribe to VM metrics changes
        QSharedPointer<VMMetrics> metrics = vm->GetMetrics();
        if (!metrics.isNull())
        {
            connect(metrics.data(), &XenObject::dataChanged, this, &HostMemoryRow::onMetricsDataChanged);
        }
    }
    
    // Initial refresh
    this->Refresh();
}

void HostMemoryRow::UnregisterHandlers()
{
    // C# equivalent: HostMemoryControls.UnregisterHandlers()
    if (this->host_.isNull())
        return;
    
    disconnect(this->host_.data(), nullptr, this, nullptr);
    
    QList<QSharedPointer<VM>> residentVMs = this->host_->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (vm.isNull())
            continue;
            
        disconnect(vm.data(), nullptr, this, nullptr);
        
        QSharedPointer<VMMetrics> metrics = vm->GetMetrics();
        if (!metrics.isNull())
        {
            disconnect(metrics.data(), nullptr, this, nullptr);
        }
    }
}

void HostMemoryRow::Refresh()
{
    // C# equivalent: HostMemoryControls.OnPaint()
    if (this->host_.isNull())
        return;
    
    QSharedPointer<HostMetrics> hostMetrics = this->host_->GetMetrics();
    if (hostMetrics.isNull())
        return;
    
    // Set host name
    this->ui->hostNameLabel->setText(this->host_->GetName());
    
    // Calculate values to display
    qint64 total = hostMetrics->GetMemoryTotal();
    qint64 free = this->host_->MemoryFreeCalc();
    qint64 used = total - free;
    qint64 xenMemory = this->host_->XenMemoryCalc();
    qint64 avail = this->host_->MemoryAvailableCalc();
    qint64 totDynMax = this->host_->TotDynMax() + xenMemory;
    qint64 dom0 = this->host_->Dom0Memory();

    qint64 xenOverhead = xenMemory - dom0;
    if (xenOverhead < 0)
        xenOverhead = 0;
    
    qint64 overcommit = total > 0 
        ? static_cast<qint64>(std::round(static_cast<double>(totDynMax) / static_cast<double>(total) * 100.0))
        : 0;
    
    // Set the text values
    this->ui->valueTotal->setText(Misc::FormatMemorySize(total));
    this->ui->valueUsed->setText(Misc::FormatMemorySize(used));
    this->ui->valueAvailable->setText(Misc::FormatMemorySize(avail));
    this->ui->valueTotDynMax->setText(Misc::FormatMemorySize(totDynMax));
    this->ui->labelOvercommitValue->setText(QString("%1%").arg(overcommit));
    this->ui->valueControlDomain->setText(Misc::FormatMemorySize(dom0));
    
    // Initialize the shiny bar
    this->ui->hostShinyBar->Initialize(this->host_, xenOverhead, dom0);
    this->ui->hostShinyBar->setVisible(true);
}

void HostMemoryRow::onHostDataChanged()
{
    // C# equivalent: host_PropertyChanged handler
    // Refresh when host.memory_overhead changes
    this->Refresh();
}

void HostMemoryRow::onVMDataChanged()
{
    // C# equivalent: vm_PropertyChanged handler
    // Refresh when VM.memory_overhead changes
    this->Refresh();
}

void HostMemoryRow::onMetricsDataChanged()
{
    // C# equivalent: vm_metrics_PropertyChanged handler
    // Refresh when VM_metrics.memory_actual changes
    this->Refresh();
}
