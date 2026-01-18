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

#include "vmmemoryrow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

VMMemoryRow::VMMemoryRow(const QList<QSharedPointer<VM>>& vms, bool expanded, QWidget* parent)
    : QWidget(parent)
    , vms_(vms)
    , expanded_(expanded)
    , panelLabel_(nullptr)
    , panelControls_(nullptr)
    , vmMemoryControls_(nullptr)
{
    this->SetupUi();
}

VMMemoryRow::~VMMemoryRow()
{
    this->UnregisterHandlers();
}

void VMMemoryRow::SetupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create label panel (top section with VM name)
    this->panelLabel_ = new QFrame(this);
    this->panelLabel_->setFrameShape(QFrame::StyledPanel);
    this->panelLabel_->setFrameShadow(QFrame::Raised);
    this->panelLabel_->setLineWidth(1);
    this->panelLabel_->setStyleSheet("background-color: silver;");
    
    QHBoxLayout* labelLayout = new QHBoxLayout(this->panelLabel_);
    labelLayout->setContentsMargins(10, 5, 10, 5);
    labelLayout->setSpacing(10);
    
    // VM label (show first VM name or count if multiple)
    QLabel* vmLabel = new QLabel(this->panelLabel_);
    if (!this->vms_.isEmpty())
    {
        if (this->vms_.size() == 1)
        {
            QSharedPointer<VM> vm = this->vms_.first();
            if (vm && !vm->IsEvicted())
            {
                vmLabel->setText(vm->GetName());
            }
        }
        else
        {
            vmLabel->setText(tr("%1 VMs").arg(this->vms_.size()));
        }
    }
    vmLabel->setStyleSheet("font-weight: bold;");
    labelLayout->addWidget(vmLabel);
    labelLayout->addStretch();
    
    mainLayout->addWidget(this->panelLabel_);
    
    // Create controls panel (bottom section with memory info)
    this->panelControls_ = new QFrame(this);
    this->panelControls_->setFrameShape(QFrame::StyledPanel);
    this->panelControls_->setFrameShadow(QFrame::Raised);
    this->panelControls_->setLineWidth(1);
    
    QHBoxLayout* controlsLayout = new QHBoxLayout(this->panelControls_);
    controlsLayout->setContentsMargins(10, 10, 10, 10);
    controlsLayout->setSpacing(10);
    
    // Memory controls (right side)
    this->vmMemoryControls_ = new VMMemoryControls(this->panelControls_);
    this->vmMemoryControls_->SetVMs(this->vms_);
    controlsLayout->addWidget(this->vmMemoryControls_);
    
    mainLayout->addWidget(this->panelControls_);
}

void VMMemoryRow::UnregisterHandlers()
{
    if (this->vmMemoryControls_)
    {
        this->vmMemoryControls_->UnregisterHandlers();
    }
}
