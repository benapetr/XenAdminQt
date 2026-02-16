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

#include <QDebug>
#include "networkoptionseditpage.h"
#include "ui_networkoptionseditpage.h"
#include "xencache.h"
#include "xenlib/xen/actions/pool/setpoolpropertyaction.h"
#include "xenlib/xen/pool.h"

NetworkOptionsEditPage::NetworkOptionsEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::NetworkOptionsEditPage)
{
    this->ui->setupUi(this);
}

NetworkOptionsEditPage::~NetworkOptionsEditPage()
{
    delete this->ui;
}

QString NetworkOptionsEditPage::GetText() const
{
    return tr("Network Options");
}

QString NetworkOptionsEditPage::GetSubText() const
{
    return this->ui->radioButtonEnable->isChecked() 
        ? tr("IGMP snooping enabled") 
        : tr("IGMP snooping disabled");
}

QIcon NetworkOptionsEditPage::GetImage() const
{
    return QIcon(":/icons/network_16.png");
}

void NetworkOptionsEditPage::SetXenObject(QSharedPointer<XenObject> object,
                                          const QVariantMap& objectDataBefore,
                                          const QVariantMap& objectDataCopy)
{
    this->m_object = object;
    this->m_poolRef_.clear();
    this->m_objectDataBefore_.clear();
    this->m_objectDataCopy_.clear();

    if (!object.isNull() && object->GetObjectType() == XenObjectType::Pool)
    {
        this->m_poolRef_ = object->OpaqueRef();
        this->m_objectDataBefore_ = objectDataBefore;
        this->m_objectDataCopy_ = objectDataCopy;
    }
    else if (!object.isNull() && object->GetCache())
    {
        QSharedPointer<Pool> pool = object->GetCache()->GetPoolOfOne();
        if (!pool.isNull())
        {
            this->m_poolRef_ = pool->OpaqueRef();
            QVariantMap poolData = pool->GetData();
            this->m_objectDataBefore_ = poolData;
            this->m_objectDataCopy_ = poolData;
        }
    }

    if (this->m_poolRef_.isEmpty())
    {
        return;
    }

    // Read igmp_snooping_enabled property from pool
    bool enabled = this->m_objectDataCopy_.value("igmp_snooping_enabled", false).toBool();
    
    if (enabled)
    {
        this->ui->radioButtonEnable->setChecked(true);
    } else
    {
        this->ui->radioButtonDisable->setChecked(true);
    }
}

AsyncOperation* NetworkOptionsEditPage::SaveSettings()
{
    bool enableValue = this->ui->radioButtonEnable->isChecked();
    
    QString title = enableValue 
        ? tr("Enabling IGMP snooping") 
        : tr("Disabling IGMP snooping");
    
    QSharedPointer<Pool> pool = this->connection()->GetCache()->ResolveObject<Pool>(this->m_poolRef_);
    if (!pool || !pool->IsValid())
    {
        qWarning() << "NetworkOptionsEditPage::SaveSettings: Invalid pool" << this->m_poolRef_;
        return nullptr;
    }
    
    return new SetPoolPropertyAction(
        pool,
        "igmp_snooping_enabled",
        enableValue,
        title,
        this
    );
}

bool NetworkOptionsEditPage::HasChanged() const
{
    bool originalEnabled = this->m_objectDataBefore_.value("igmp_snooping_enabled", false).toBool();
    bool currentEnabled = this->ui->radioButtonEnable->isChecked();
    return originalEnabled != currentEnabled;
}

bool NetworkOptionsEditPage::IsValidToSave() const
{
    return true;
}

void NetworkOptionsEditPage::ShowLocalValidationMessages()
{
    // No validation needed
}

void NetworkOptionsEditPage::HideLocalValidationMessages()
{
    // No validation needed
}

void NetworkOptionsEditPage::Cleanup()
{
    // Nothing to clean up
}
