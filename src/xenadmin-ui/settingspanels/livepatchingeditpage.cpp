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

#include "livepatchingeditpage.h"
#include "ui_livepatchingeditpage.h"
#include "xencache.h"
#include "xenlib/xen/actions/pool/setpoolpropertyaction.h"
#include "xenlib/xen/pool.h"

LivePatchingEditPage::LivePatchingEditPage(QWidget* parent)
    : IEditPage(parent)
    , ui(new Ui::LivePatchingEditPage)
{
    this->ui->setupUi(this);
}

LivePatchingEditPage::~LivePatchingEditPage()
{
    delete this->ui;
}

QString LivePatchingEditPage::text() const
{
    return tr("Live Patching");
}

QString LivePatchingEditPage::subText() const
{
    return this->ui->radioButtonEnable->isChecked() ? tr("Enabled") : tr("Disabled");
}

QIcon LivePatchingEditPage::image() const
{
    return QIcon(":/icons/patch_16.png");
}

void LivePatchingEditPage::setXenObjects(const QString& objectRef,
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
            QList<QSharedPointer<Pool>> pools = cache->GetAll<Pool>("pool");
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

    // Read live_patching_disabled property from pool
    // Note: The server flag is "disabled", so we invert for the UI
    bool disabled = this->m_objectDataCopy_.value("live_patching_disabled", false).toBool();
    
    if (disabled)
    {
        this->ui->radioButtonDisable->setChecked(true);
    } else
    {
        this->ui->radioButtonEnable->setChecked(true);
    }
}

AsyncOperation* LivePatchingEditPage::saveSettings()
{
    // Server expects "disabled" flag, so invert the enable button state
    bool disableValue = this->ui->radioButtonDisable->isChecked();
    
    QString title = disableValue 
        ? tr("Disabling live patching") 
        : tr("Enabling live patching");
    
    return new SetPoolPropertyAction(
        this->connection(),
        this->m_poolRef_,
        "live_patching_disabled",
        disableValue,
        title,
        this
    );
}

bool LivePatchingEditPage::hasChanged() const
{
    bool originalDisabled = this->m_objectDataBefore_.value("live_patching_disabled", false).toBool();
    bool currentDisabled = this->ui->radioButtonDisable->isChecked();
    return originalDisabled != currentDisabled;
}

bool LivePatchingEditPage::isValidToSave() const
{
    return true;
}

void LivePatchingEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void LivePatchingEditPage::hideLocalValidationMessages()
{
    // No validation needed
}

void LivePatchingEditPage::cleanup()
{
    // Nothing to clean up
}
