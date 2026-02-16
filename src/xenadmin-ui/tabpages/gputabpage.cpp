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

#include "gputabpage.h"
#include "ui_gputabpage.h"

#include "../controls/gpuplacementpolicypanel.h"
#include "../controls/gpurow.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/gpugroup.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pgpu.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vgputype.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
    struct GpuSettingsKey
    {
        QString gpuName;
        QStringList enabledTypeRefs;
        QStringList supportedTypeRefs;

        bool operator==(const GpuSettingsKey& other) const
        {
            return this->gpuName == other.gpuName
                   && this->enabledTypeRefs == other.enabledTypeRefs
                   && this->supportedTypeRefs == other.supportedTypeRefs;
        }
    };
}

GpuTabPage::GpuTabPage(QWidget* parent) : BaseTabPage(parent), ui(new Ui::GpuTabPage)
{
    this->ui->setupUi(this);
    this->ui->pageLayout->addStretch();
}

GpuTabPage::~GpuTabPage()
{
    this->removeObject();
    delete this->ui;
}

bool GpuTabPage::IsApplicableForObjectType(const QString& objectType) const
{
    return objectType == QLatin1String("host") || objectType == QLatin1String("pool");
}

void GpuTabPage::OnPageShown()
{
    BaseTabPage::OnPageShown();
    this->refreshContent();
}

void GpuTabPage::OnPageHidden()
{
    BaseTabPage::OnPageHidden();
}

void GpuTabPage::removeObject()
{
    if (this->m_connection && this->m_connection->GetCache())
    {
        XenCache* cache = this->m_connection->GetCache();
        disconnect(cache, &XenCache::objectChanged, this, &GpuTabPage::onCacheObjectChanged);
        disconnect(cache, &XenCache::objectRemoved, this, &GpuTabPage::onCacheObjectRemoved);
        disconnect(cache, &XenCache::bulkUpdateComplete, this, &GpuTabPage::onCacheBulkUpdateComplete);
        disconnect(cache, &XenCache::cacheCleared, this, &GpuTabPage::onCacheCleared);
    }
}

void GpuTabPage::updateObject()
{
    if (!this->m_connection || !this->m_connection->GetCache())
        return;

    XenCache* cache = this->m_connection->GetCache();
    connect(cache, &XenCache::objectChanged, this, &GpuTabPage::onCacheObjectChanged, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &GpuTabPage::onCacheObjectRemoved, Qt::UniqueConnection);
    connect(cache, &XenCache::bulkUpdateComplete, this, &GpuTabPage::onCacheBulkUpdateComplete, Qt::UniqueConnection);
    connect(cache, &XenCache::cacheCleared, this, &GpuTabPage::onCacheCleared, Qt::UniqueConnection);
}

void GpuTabPage::refreshContent()
{
    this->rebuild();
}

bool GpuTabPage::shouldShowPlacementPolicyPanel() const
{
    if (!this->m_object || !this->m_object->GetConnection())
        return false;

    // C# GpuPage shows placement policy panel whenever connection has vGPU capability,
    // for both pool and host scopes.
    return GpuHelpers::VGpuCapability(this->m_object->GetConnection());
}

void GpuTabPage::rebuild()
{
    while (QLayoutItem* item = this->ui->pageLayout->takeAt(0))
    {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    this->m_policyPanel = nullptr;
    this->m_noGpuLabelContainer = nullptr;
    this->m_rowsByPgpuRef.clear();

    if (!this->m_object || !this->m_connection || !this->m_connection->GetCache())
    {
        this->ui->pageLayout->addStretch();
        return;
    }

    XenCache* cache = this->m_connection->GetCache();
    QList<QSharedPointer<PGPU>> pGpus = cache->GetAll<PGPU>(XenObjectType::PGPU);
    const bool isPool = this->m_objectType == XenObjectType::Pool;
    const bool isHost = this->m_objectType == XenObjectType::Host;
    const QString selectedHostRef = isHost ? this->m_objectRef : QString();

    pGpus.erase(std::remove_if(pGpus.begin(), pGpus.end(), [&](const QSharedPointer<PGPU>& pgpu) {
                    if (!pgpu || !pgpu->IsValid())
                        return true;
                    if (pgpu->SupportedVGPUTypeRefs().isEmpty())
                        return true;
                    if (isPool)
                        return false;
                    if (isHost)
                        return pgpu->GetHostRef() != selectedHostRef;
                    return true;
                }),
                pGpus.end());

    std::sort(pGpus.begin(), pGpus.end(), [&](const QSharedPointer<PGPU>& a, const QSharedPointer<PGPU>& b) {
        const QSharedPointer<Host> ah = a ? a->GetHost() : QSharedPointer<Host>();
        const QSharedPointer<Host> bh = b ? b->GetHost() : QSharedPointer<Host>();
        const QString ahn = ah ? ah->GetName() : QString();
        const QString bhn = bh ? bh->GetName() : QString();
        if (ahn != bhn)
            return QString::compare(ahn, bhn, Qt::CaseInsensitive) < 0;
        const QString an = a ? a->GetName() : QString();
        const QString bn = b ? b->GetName() : QString();
        return QString::compare(an, bn, Qt::CaseInsensitive) < 0;
    });

    if (this->shouldShowPlacementPolicyPanel())
    {
        this->m_policyPanel = new GpuPlacementPolicyPanel(this->ui->pageContainer);
        this->m_policyPanel->SetXenObject(this->m_object);
        this->ui->pageLayout->addWidget(this->m_policyPanel);
    }

    QList<GpuSettingsKey> keyOrder;
    QHash<QString, QList<QSharedPointer<PGPU>>> grouped;
    for (const QSharedPointer<PGPU>& pgpu : std::as_const(pGpus))
    {
        QStringList enabled = pgpu->EnabledVGPUTypeRefs();
        QStringList supported = pgpu->SupportedVGPUTypeRefs();
        std::sort(enabled.begin(), enabled.end());
        std::sort(supported.begin(), supported.end());

        GpuSettingsKey key{ pgpu->GetName(), enabled, supported };
        QString hashKey = key.gpuName + QLatin1Char('|') + key.enabledTypeRefs.join(QLatin1Char(',')) + QLatin1Char('|') + key.supportedTypeRefs.join(QLatin1Char(','));
        if (!grouped.contains(hashKey))
            keyOrder.append(key);
        grouped[hashKey].append(pgpu);
    }

    for (const GpuSettingsKey& key : std::as_const(keyOrder))
    {
        QString hashKey = key.gpuName + QLatin1Char('|') + key.enabledTypeRefs.join(QLatin1Char(',')) + QLatin1Char('|') + key.supportedTypeRefs.join(QLatin1Char(','));
        GpuRow* row = new GpuRow(this->m_object, grouped.value(hashKey), this->ui->pageContainer);
        this->ui->pageLayout->addWidget(row);
        for (const QSharedPointer<PGPU>& pgpu : grouped.value(hashKey))
        {
            if (pgpu)
                this->m_rowsByPgpuRef.insert(pgpu->OpaqueRef(), row);
        }
    }

    if (keyOrder.isEmpty())
    {
        this->m_noGpuLabelContainer = new QWidget(this->ui->pageContainer);
        QVBoxLayout* noLayout = new QVBoxLayout(this->m_noGpuLabelContainer);
        noLayout->setContentsMargins(8, 8, 8, 8);
        QFrame* frame = new QFrame(this->m_noGpuLabelContainer);
        frame->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout* frameLayout = new QVBoxLayout(frame);
        QLabel* label = new QLabel(isPool ? tr("No GPUs were detected in this pool.") : tr("No GPUs were detected on this host."), frame);
        label->setWordWrap(true);
        frameLayout->addWidget(label);
        noLayout->addWidget(frame);
        this->ui->pageLayout->addWidget(this->m_noGpuLabelContainer);
    }

    this->ui->pageLayout->addStretch();
}

void GpuTabPage::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->isVisible() || connection != this->m_connection)
        return;

    if (type == QLatin1String("pgpu"))
    {
        GpuRow* row = this->m_rowsByPgpuRef.value(ref, nullptr);
        if (row)
        {
            QSharedPointer<PGPU> pgpu = this->m_connection->GetCache()->ResolveObject<PGPU>(ref);
            row->RefreshGpu(pgpu);
            return;
        }
        this->rebuild();
        return;
    }

    if (type == QLatin1String("gpu_group")
        || type == QLatin1String("vgpu")
        || type == QLatin1String("vgpu_type")
        || type == QLatin1String("host")
        || type == QLatin1String("pool"))
    {
        this->rebuild();
    }
}

void GpuTabPage::onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    this->onCacheObjectChanged(connection, type, QString());
}

void GpuTabPage::onCacheBulkUpdateComplete(const QString& type, int count)
{
    Q_UNUSED(count);
    if (!this->isVisible())
        return;
    if (type == QLatin1String("pgpu")
        || type == QLatin1String("gpu_group")
        || type == QLatin1String("vgpu")
        || type == QLatin1String("vgpu_type")
        || type == QLatin1String("host")
        || type == QLatin1String("pool"))
    {
        this->rebuild();
    }
}

void GpuTabPage::onCacheCleared()
{
    if (this->isVisible())
        this->rebuild();
}
