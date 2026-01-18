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

#include "vmmemorycontrols.h"
#include "xenlib/xen/vmmetrics.h"
#include "xenlib/utils/misc.h"
#include <QVBoxLayout>

VMMemoryControls::VMMemoryControls(QWidget* parent) : QWidget(parent)
{
    this->SetupUi();
}

void VMMemoryControls::SetupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);
    
    // Add shiny bar at top
    this->vmShinyBar_ = new VMShinyBar(this);
    mainLayout->addWidget(this->vmShinyBar_);
    
    // Statistics grid
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(10);
    gridLayout->setVerticalSpacing(5);
    
    int row = 0;
    
    // Dynamic Minimum
    this->labelDynMin_ = new QLabel(tr("Dynamic Minimum:"), this);
    this->valueDynMin_ = new QLabel("", this);
    gridLayout->addWidget(this->labelDynMin_, row, 0, Qt::AlignRight);
    gridLayout->addWidget(this->valueDynMin_, row, 1);
    row++;
    
    // Dynamic Maximum
    this->labelDynMax_ = new QLabel(tr("Dynamic Maximum:"), this);
    this->valueDynMax_ = new QLabel("", this);
    gridLayout->addWidget(this->labelDynMax_, row, 0, Qt::AlignRight);
    gridLayout->addWidget(this->valueDynMax_, row, 1);
    row++;
    
    // Static Maximum (may be hidden if same as dynamic max)
    this->labelStatMax_ = new QLabel(tr("Static Maximum:"), this);
    this->valueStatMax_ = new QLabel("", this);
    gridLayout->addWidget(this->labelStatMax_, row, 0, Qt::AlignRight);
    gridLayout->addWidget(this->valueStatMax_, row, 1);
    
    mainLayout->addLayout(gridLayout);
    mainLayout->addStretch();
}

void VMMemoryControls::SetVMs(const QList<QSharedPointer<VM>>& vms)
{
    this->UnregisterHandlers();
    
    this->vms_ = vms;
    
    if (this->vms_.isEmpty())
    {
        return;
    }
    
    // Subscribe to VM property changes
    for (const QSharedPointer<VM>& vm : this->vms_)
    {
        if (vm && !vm->IsEvicted())
        {
            connect(vm.data(), &XenObject::dataChanged, this, &VMMemoryControls::onVMDataChanged);
            
            // Subscribe to metrics changes
            QSharedPointer<VMMetrics> metrics = vm->GetMetrics();
            if (metrics && !metrics->IsEvicted())
            {
                connect(metrics.data(), &XenObject::dataChanged, this, &VMMemoryControls::onVMMetricsChanged);
            }
        }
    }
    
    this->Refresh();
}

void VMMemoryControls::UnregisterHandlers()
{
    for (const QSharedPointer<VM>& vm : this->vms_)
    {
        if (vm && !vm->IsEvicted())
        {
            disconnect(vm.data(), nullptr, this, nullptr);
            
            QSharedPointer<VMMetrics> metrics = vm->GetMetrics();
            if (metrics && !metrics->IsEvicted())
            {
                disconnect(metrics.data(), nullptr, this, nullptr);
            }
        }
    }
}

void VMMemoryControls::Refresh()
{
    if (this->vms_.isEmpty())
    {
        // Clear all values
        this->valueDynMin_->setText("");
        this->valueDynMax_->setText("");
        this->valueStatMax_->setText("");
        return;
    }
    
    // Use first VM for display (C# does the same)
    QSharedPointer<VM> vm0 = this->vms_.first();
    if (!vm0 || vm0->IsEvicted())
    {
        return;
    }
    
    // Update VMShinyBar
    this->vmShinyBar_->Populate(this->vms_, false);
    
    // Check if VM supports ballooning
    bool supportsBallooning = vm0->SupportsBallooning();
    
    if (supportsBallooning)
    {
        // Show dynamic memory settings
        qint64 dynMin = vm0->GetMemoryDynamicMin();
        qint64 dynMax = vm0->GetMemoryDynamicMax();
        qint64 statMax = vm0->GetMemoryStaticMax();
        
        this->valueDynMin_->setText(Misc::FormatMemorySize(dynMin));
        this->valueDynMax_->setText(Misc::FormatMemorySize(dynMax));
        
        // Hide static max if it's the same as dynamic max
        if (dynMax == statMax)
        {
            this->labelStatMax_->setVisible(false);
            this->valueStatMax_->setVisible(false);
        }
        else
        {
            this->labelStatMax_->setVisible(true);
            this->valueStatMax_->setVisible(true);
            this->valueStatMax_->setText(Misc::FormatMemorySize(statMax));
        }
        
        this->labelDynMin_->setText(tr("Dynamic Minimum:"));
        this->labelDynMax_->setVisible(true);
        this->valueDynMax_->setVisible(true);
    }
    else
    {
        // For VMs without ballooning, just show static memory as "Memory"
        qint64 statMax = vm0->GetMemoryStaticMax();
        this->valueDynMin_->setText(Misc::FormatMemorySize(statMax));
        this->labelDynMin_->setText(tr("Memory:"));
        
        // Hide dynamic max and static max rows
        this->labelDynMax_->setVisible(false);
        this->valueDynMax_->setVisible(false);
        this->labelStatMax_->setVisible(false);
        this->valueStatMax_->setVisible(false);
    }
}

void VMMemoryControls::onVMDataChanged()
{
    // Refresh when VM properties change (power_state, name, memory settings)
    this->Refresh();
}

void VMMemoryControls::onVMMetricsChanged()
{
    // Refresh when VM metrics change (memory_actual)
    this->Refresh();
}

