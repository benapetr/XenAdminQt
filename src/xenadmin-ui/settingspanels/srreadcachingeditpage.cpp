/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "srreadcachingeditpage.h"
#include "ui_srreadcachingeditpage.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/actions/delegatedasyncoperation.h"
#include "xenlib/xen/xenapi/xenapi_SR.h"

SrReadCachingEditPage::SrReadCachingEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::SrReadCachingEditPage)
{
    this->ui->setupUi(this);
}

SrReadCachingEditPage::~SrReadCachingEditPage()
{
    delete this->ui;
}

QString SrReadCachingEditPage::GetText() const
{
    return tr("Read Caching");
}

QString SrReadCachingEditPage::GetSubText() const
{
    return this->ui->checkBoxEnableReadCaching->isChecked() ? tr("Enabled") : tr("Disabled");
}

QIcon SrReadCachingEditPage::GetImage() const
{
    return QIcon(":/icons/storage.png");
}

void SrReadCachingEditPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    this->m_sr = qSharedPointerDynamicCast<SR>(object);
    if (!this->m_sr)
        return;

    this->m_originalEnabled = this->m_sr->GetReadCachingEnabled();
    this->ui->checkBoxEnableReadCaching->setChecked(this->m_originalEnabled);
}

AsyncOperation* SrReadCachingEditPage::SaveSettings()
{
    if (!this->m_sr || !this->HasChanged())
        return nullptr;

    // C# SR.SetReadCachingEnabled: enabling removes o_direct, disabling sets o_direct=true
    const bool enable = this->ui->checkBoxEnableReadCaching->isChecked();
    auto* op = new DelegatedAsyncOperation(this->m_sr->GetConnection(),
        enable ? tr("Enable read caching") : tr("Disable read caching"),
        enable ? tr("Enabling read caching...") : tr("Disabling read caching..."),
        [srRef = this->m_sr->OpaqueRef(), enable](DelegatedAsyncOperation* self)
        {
            XenAPI::Session* session = self->GetSession();
            XenAPI::SR::remove_from_other_config(session, srRef, "o_direct");
            if (!enable)
                XenAPI::SR::add_to_other_config(session, srRef, "o_direct", "true");
        });
    op->AddApiMethodToRoleCheck("SR.remove_from_other_config");
    if (!enable)
        op->AddApiMethodToRoleCheck("SR.add_to_other_config");
    return op;
}

bool SrReadCachingEditPage::IsValidToSave() const
{
    return true;
}

void SrReadCachingEditPage::ShowLocalValidationMessages()
{
}

void SrReadCachingEditPage::HideLocalValidationMessages()
{
}

void SrReadCachingEditPage::Cleanup()
{
}

bool SrReadCachingEditPage::HasChanged() const
{
    return this->m_sr && this->ui->checkBoxEnableReadCaching->isChecked() != this->m_originalEnabled;
}
