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

#include "vmenlightenmenteditpage.h"
#include "ui_vmenlightenmenteditpage.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/actions/vm/vmenlightenmentaction.h"

VMEnlightenmentEditPage::VMEnlightenmentEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::VMEnlightenmentEditPage), m_originalEnlightened(false)
{
    this->ui->setupUi(this);
}

VMEnlightenmentEditPage::~VMEnlightenmentEditPage()
{
    delete ui;
}

QString VMEnlightenmentEditPage::GetText() const
{
    return tr("Enlightenment");
}

QString VMEnlightenmentEditPage::GetSubText() const
{
    return this->ui->checkBoxEnlightenment->isChecked() ? tr("Enabled") : tr("Disabled");
}

QIcon VMEnlightenmentEditPage::GetImage() const
{
    return QIcon(":/icons/dc_16.png");
}

void VMEnlightenmentEditPage::SetXenObject(QSharedPointer<XenObject> object,
                                           const QVariantMap& objectDataBefore,
                                           const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    this->m_object = object;
    this->m_vmRef.clear();
    this->m_vm.clear();

    if (!object.isNull() && object->GetObjectType() == XenObjectType::VM)
        this->m_vm = qSharedPointerDynamicCast<VM>(object);

    if (!this->m_vm.isNull())
        this->m_vmRef = this->m_vm->OpaqueRef();

    this->m_originalEnlightened = this->m_vm && this->m_vm->IsEnlightened();
    this->ui->checkBoxEnlightenment->setChecked(this->m_originalEnlightened);
}

AsyncOperation* VMEnlightenmentEditPage::SaveSettings()
{
    if (!HasChanged())
    {
        return nullptr;
    }

    if (!this->m_vm)
        return nullptr;

    if (this->ui->checkBoxEnlightenment->isChecked())
        return new EnableVMEnlightenmentAction(this->m_vm, true, this);

    return new DisableVMEnlightenmentAction(this->m_vm, true, this);
}

bool VMEnlightenmentEditPage::IsValidToSave() const
{
    return true;
}

void VMEnlightenmentEditPage::ShowLocalValidationMessages()
{
    // No validation needed
}

void VMEnlightenmentEditPage::HideLocalValidationMessages()
{
    // No validation messages
}

void VMEnlightenmentEditPage::Cleanup()
{
    // Nothing to clean up
}

bool VMEnlightenmentEditPage::HasChanged() const
{
    return this->ui->checkBoxEnlightenment->isChecked() != this->m_originalEnlightened;
}
