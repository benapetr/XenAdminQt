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
#include "pooladvancededitpage.h"
#include "ui_pooladvancededitpage.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/pool/setpoolpropertyaction.h"
#include "xenlib/xen/pool.h"

PoolAdvancedEditPage::PoolAdvancedEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::PoolAdvancedEditPage)
{
    this->ui->setupUi(this);
}

PoolAdvancedEditPage::~PoolAdvancedEditPage()
{
    delete this->ui;
}

QString PoolAdvancedEditPage::GetText() const
{
    return tr("Advanced Options");
}

QString PoolAdvancedEditPage::GetSubText() const
{
    return this->ui->checkBoxCompression->isChecked() 
        ? tr("Migration compression enabled") 
        : tr("Migration compression disabled");
}

QIcon PoolAdvancedEditPage::GetImage() const
{
    return QIcon(":/icons/configure_16.png");
}

void PoolAdvancedEditPage::SetXenObjects(const QString& objectRef,
                                         const QString& objectType,
                                         const QVariantMap& objectDataBefore,
                                         const QVariantMap& objectDataCopy)
{
    this->m_poolRef_.clear();
    this->m_objectDataBefore_.clear();
    this->m_objectDataCopy_.clear();

    if (objectType == "pool")
    {
        this->m_poolRef_ = objectRef;
        this->m_objectDataBefore_ = objectDataBefore;
        this->m_objectDataCopy_ = objectDataCopy;
    } else
    {
        XenConnection* conn = this->connection();
        XenCache* cache = conn ? conn->GetCache() : nullptr;
        if (cache)
        {
            QList<QSharedPointer<Pool>> pools = cache->GetAll<Pool>();
            if (!pools.isEmpty() && pools.first())
            {
                QSharedPointer<Pool> pool = pools.first();
                this->m_poolRef_ = pool->OpaqueRef();
                QVariantMap poolData = pool->GetData();
                this->m_objectDataBefore_ = poolData;
                this->m_objectDataCopy_ = poolData;
            }
        }
    }

    if (this->m_poolRef_.isEmpty())
    {
        return;
    }

    // Read migration_compression property from pool
    bool compressionEnabled = this->m_objectDataCopy_.value("migration_compression", false).toBool();
    this->ui->checkBoxCompression->setChecked(compressionEnabled);
}

AsyncOperation* PoolAdvancedEditPage::SaveSettings()
{
    bool newValue = this->ui->checkBoxCompression->isChecked();
    
    QSharedPointer<Pool> pool = this->connection()->GetCache()->ResolveObject<Pool>(this->m_poolRef_);
    if (!pool || !pool->IsValid())
    {
        qWarning() << "PoolAdvancedEditPage::SaveSettings: Invalid pool" << this->m_poolRef_;
        return nullptr;
    }
    
    // Use SetPoolPropertyAction to set migration_compression
    return new SetPoolPropertyAction(
        pool,
        "migration_compression",
        newValue,
        tr("Updating migration compression"),
        this
    );
}

bool PoolAdvancedEditPage::HasChanged() const
{
    bool original = this->m_objectDataBefore_.value("migration_compression", false).toBool();
    bool current = this->ui->checkBoxCompression->isChecked();
    return original != current;
}

bool PoolAdvancedEditPage::IsValidToSave() const
{
    return true;
}

void PoolAdvancedEditPage::ShowLocalValidationMessages()
{
    // No validation needed
}

void PoolAdvancedEditPage::HideLocalValidationMessages()
{
    // No validation needed
}

void PoolAdvancedEditPage::Cleanup()
{
    // Nothing to clean up
}
