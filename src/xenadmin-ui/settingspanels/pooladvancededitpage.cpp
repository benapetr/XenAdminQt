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
#include "xenlib/operations/multipleaction.h"
#include "xenlib/xen/actions/pool/setpoolpropertyaction.h"
#include "xenlib/xen/network.h"
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

void PoolAdvancedEditPage::SetXenObject(QSharedPointer<XenObject> object,
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

    // Read migration_compression property from pool
    bool compressionEnabled = this->m_objectDataCopy_.value("migration_compression", false).toBool();
    this->ui->checkBoxCompression->setChecked(compressionEnabled);

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->ResolveObject<Pool>(this->m_poolRef_) : QSharedPointer<Pool>();
    this->populateMigrationNetworkCombo(pool);
}

AsyncOperation* PoolAdvancedEditPage::SaveSettings()
{
    bool newCompressionValue = this->ui->checkBoxCompression->isChecked();
    const QString newMigrationNetworkRef = this->currentMigrationNetworkRef();

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    QSharedPointer<Pool> pool = cache ? cache->ResolveObject<Pool>(this->m_poolRef_) : QSharedPointer<Pool>();
    if (!pool || !pool->IsValid())
    {
        qWarning() << "PoolAdvancedEditPage::SaveSettings: Invalid pool" << this->m_poolRef_;
        return nullptr;
    }

    QList<AsyncOperation*> operations;

    if (this->m_objectDataBefore_.value("migration_compression", false).toBool() != newCompressionValue)
    {
        operations.append(new SetPoolPropertyAction(
            pool,
            "migration_compression",
            newCompressionValue,
            tr("Updating migration compression"),
            this));
    }

    if (this->originalMigrationNetworkRef() != newMigrationNetworkRef)
    {
        operations.append(new SetPoolPropertyAction(
            pool,
            "xo_migration_network",
            newMigrationNetworkRef,
            tr("Updating migration network override"),
            this));
    }

    if (operations.isEmpty())
        return nullptr;

    if (operations.size() == 1)
        return operations.first();

    return new MultipleAction(
        pool->GetConnection(),
        tr("Updating pool advanced options"),
        tr("Updating pool advanced options..."),
        tr("Pool advanced options updated"),
        operations,
        false,
        false,
        true,
        this);
}

bool PoolAdvancedEditPage::HasChanged() const
{
    bool original = this->m_objectDataBefore_.value("migration_compression", false).toBool();
    bool current = this->ui->checkBoxCompression->isChecked();
    return original != current || this->originalMigrationNetworkRef() != this->currentMigrationNetworkRef();
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

void PoolAdvancedEditPage::populateMigrationNetworkCombo(const QSharedPointer<Pool>& pool)
{
    this->ui->comboMigrationNetwork->clear();
    this->ui->comboMigrationNetwork->addItem(tr("<default>"), QString());

    if (!pool || !pool->GetCache())
        return;

    const QString selectedRef = this->originalMigrationNetworkRef();
    bool selectedFound = selectedRef.isEmpty();

    QList<QSharedPointer<Network>> networks = pool->GetCache()->GetAll<Network>(XenObjectType::Network);
    for (const QSharedPointer<Network>& network : networks)
    {
        if (!network || !network->IsValid())
            continue;

        this->ui->comboMigrationNetwork->addItem(network->GetName(), network->OpaqueRef());
        if (network->OpaqueRef() == selectedRef)
            selectedFound = true;
    }

    if (!selectedRef.isEmpty() && !selectedFound)
        this->ui->comboMigrationNetwork->addItem(selectedRef, selectedRef);

    int selectedIndex = this->ui->comboMigrationNetwork->findData(selectedRef);
    if (selectedIndex < 0)
        selectedIndex = 0;
    this->ui->comboMigrationNetwork->setCurrentIndex(selectedIndex);
}

QString PoolAdvancedEditPage::currentMigrationNetworkRef() const
{
    return this->ui->comboMigrationNetwork->currentData().toString();
}

QString PoolAdvancedEditPage::originalMigrationNetworkRef() const
{
    const QVariantMap otherConfig = this->m_objectDataBefore_.value("other_config").toMap();
    return otherConfig.value("xo:migrationNetwork").toString();
}
