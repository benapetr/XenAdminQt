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

#include <QTimer>
#include <QDebug>
#include "cdchanger.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vbd.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/actions/vm/changevmisoaction.h"
#include "xenlib/xen/asyncoperation.h"

CDChanger::CDChanger(QWidget* parent) : IsoDropDownBox(parent), changing_(false)
{
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CDChanger::onCurrentIndexChanged);
}

CDChanger::~CDChanger()
{
    this->DeregisterEvents();
}

void CDChanger::SetVM(QSharedPointer<VM> vm)
{
    this->vm_ = vm;
    if (this->vm_ && this->vm_->GetConnection())
    {
        this->SetConnection(this->vm_->GetConnection());
        this->SetVMRef(this->vm_->OpaqueRef());
    }
}

void CDChanger::SetDrive(QSharedPointer<VBD> vbd)
{
    this->disconnectVbdSignals();
    
    this->cdrom_ = vbd;
    
    this->connectVbdSignals();
    
    this->Refresh();
    this->updateSelectedCd();
}

void CDChanger::connectVbdSignals()
{
    if (!this->cdrom_)
        return;

    // Monitor VBD property changes to update UI when CD is changed externally
    connect(this->cdrom_.data(), &XenObject::DataChanged, this, &CDChanger::onVbdPropertyChanged);
}

void CDChanger::disconnectVbdSignals()
{
    if (!this->cdrom_)
        return;
        
    disconnect(this->cdrom_.data(), &XenObject::DataChanged, this, &CDChanger::onVbdPropertyChanged);
}

void CDChanger::updateSelectedCd()
{
    QString vdiRef;
    
    if (this->cdrom_ && !this->cdrom_->Empty())
    {
        vdiRef = this->cdrom_->GetVDIRef();
    }
    
    this->SetSelectedVdiRef(vdiRef);
}

void CDChanger::onVbdPropertyChanged()
{
    if (!this->changing_)
    {
        this->updateSelectedCd();
    }
}

void CDChanger::onCurrentIndexChanged(int index)
{
    Q_UNUSED(index);
    
    // Get selected VDI reference
    QString selectedVdiRef = this->SelectedVdiRef();
    
    if (!this->cdrom_)
        return;
    
    // Don't change if we go from <empty> to <empty>
    QString currentVdiRef = this->cdrom_->Empty() ? QString() : this->cdrom_->GetVDIRef();
    if (selectedVdiRef.isEmpty() && currentVdiRef.isEmpty())
        return;
    
    // Don't change if we leave the same one in
    if (!selectedVdiRef.isEmpty() && selectedVdiRef == currentVdiRef)
        return;
    
    // Trigger the CD change
    this->ChangeCD(selectedVdiRef);
}

void CDChanger::ChangeCD(const QString& vdiRef)
{
    if (!this->cdrom_ || !this->vm_)
        return;
    if (!this->vm_->GetConnection())
        return;

    this->changing_ = true;
    this->setEnabled(false);

    QString vbdRef = this->cdrom_->OpaqueRef();
    
    ChangeVMISOAction* action = new ChangeVMISOAction(
        this->vm_,
        vdiRef,
        vbdRef,
        this
    );
    
    connect(action, &AsyncOperation::completed, this, [this, action]() {
        QTimer::singleShot(0, this, [this]() {
            this->changing_ = false;
            this->updateSelectedCd();
            this->setEnabled(true);
        });
        action->deleteLater();
    });
    
    connect(action, &AsyncOperation::failed, this, [this, action](const QString& error) {
        qWarning() << "CDChanger: Failed to change CD:" << error;
        QTimer::singleShot(0, this, [this]() {
            this->changing_ = false;
            this->updateSelectedCd();
            this->setEnabled(true);
        });
        action->deleteLater();
    });
    
    action->RunAsync();
}

void CDChanger::DeregisterEvents()
{
    this->disconnectVbdSignals();
}
