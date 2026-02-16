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


#include "gpuplacementpolicypanel.h"

#include "../dialogs/hostpropertiesdialog.h"
#include "../dialogs/poolpropertiesdialog.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/gpugroup.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/xenapi/xenapi_GPU_group.h"
#include "xenlib/xen/xenobject.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
    XenAPI::AllocationAlgorithm algorithmFromText(const QString& text)
    {
        if (text == QLatin1String("depth_first"))
            return XenAPI::AllocationAlgorithm::DepthFirst;
        if (text == QLatin1String("breadth_first"))
            return XenAPI::AllocationAlgorithm::BreadthFirst;
        return XenAPI::AllocationAlgorithm::Unknown;
    }
}

GpuPlacementPolicyPanel::GpuPlacementPolicyPanel(QWidget* parent) : QWidget(parent)
{
    QFrame* frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);

    QHBoxLayout* row = new QHBoxLayout(frame);
    row->setContentsMargins(8, 8, 8, 8);

    this->m_policyLabel = new QLabel(frame);
    this->m_policyLabel->setWordWrap(true);
    this->m_policyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    this->m_editButton = new QPushButton(tr("Edit"), frame);
    connect(this->m_editButton, &QPushButton::clicked, this, &GpuPlacementPolicyPanel::onEditClicked);

    row->addWidget(this->m_policyLabel, 1);
    row->addWidget(this->m_editButton);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(frame);
}

void GpuPlacementPolicyPanel::SetXenObject(const QSharedPointer<XenObject>& object)
{
    if (this->m_object == object)
        return;

    this->UnregisterHandlers();
    this->m_object = object;
    this->RegisterHandlers();
    this->PopulatePage();
}

void GpuPlacementPolicyPanel::RegisterHandlers()
{
    if (!this->m_object || !this->m_object->GetConnection() || !this->m_object->GetConnection()->GetCache())
        return;

    XenCache* cache = this->m_object->GetConnection()->GetCache();
    connect(cache, &XenCache::objectChanged, this, &GpuPlacementPolicyPanel::onCacheObjectChanged, Qt::UniqueConnection);
    connect(cache, &XenCache::objectRemoved, this, &GpuPlacementPolicyPanel::onCacheObjectRemoved, Qt::UniqueConnection);
    connect(cache, &XenCache::cacheCleared, this, &GpuPlacementPolicyPanel::onCacheCleared, Qt::UniqueConnection);
}

void GpuPlacementPolicyPanel::UnregisterHandlers()
{
    if (!this->m_object || !this->m_object->GetConnection() || !this->m_object->GetConnection()->GetCache())
        return;

    XenCache* cache = this->m_object->GetConnection()->GetCache();
    disconnect(cache, &XenCache::objectChanged, this, &GpuPlacementPolicyPanel::onCacheObjectChanged);
    disconnect(cache, &XenCache::objectRemoved, this, &GpuPlacementPolicyPanel::onCacheObjectRemoved);
    disconnect(cache, &XenCache::cacheCleared, this, &GpuPlacementPolicyPanel::onCacheCleared);
}

QString GpuPlacementPolicyPanel::AllocationAlgorithmText(XenAPI::AllocationAlgorithm algorithm)
{
    switch (algorithm)
    {
        case XenAPI::AllocationAlgorithm::BreadthFirst:
            return QObject::tr("Max Performance");
        case XenAPI::AllocationAlgorithm::DepthFirst:
            return QObject::tr("Max Density");
        case XenAPI::AllocationAlgorithm::Unknown:
        default:
            return QObject::tr("Mixed");
    }
}

void GpuPlacementPolicyPanel::PopulatePage()
{
    if (!this->m_object || !this->m_object->GetConnection() || !this->m_object->GetConnection()->GetCache())
    {
        this->m_policyLabel->setText(tr("GPU placement policy unavailable."));
        return;
    }

    XenAPI::AllocationAlgorithm algorithm = XenAPI::AllocationAlgorithm::Unknown;
    bool hasGroups = false;
    XenCache* cache = this->m_object->GetConnection()->GetCache();
    const QList<QSharedPointer<GPUGroup>> groups = cache->GetAll<GPUGroup>(XenObjectType::GPUGroup);
    for (const QSharedPointer<GPUGroup>& group : groups)
    {
        if (!group || !group->IsValid())
            continue;
        const XenAPI::AllocationAlgorithm next = algorithmFromText(group->AllocationAlgorithm());
        if (!hasGroups)
        {
            algorithm = next;
            hasGroups = true;
            continue;
        }
        if (algorithm != next)
        {
            algorithm = XenAPI::AllocationAlgorithm::Unknown;
            break;
        }
    }

    this->m_policyLabel->setText(tr("GPU placement policy: %1").arg(AllocationAlgorithmText(algorithm)));
}

void GpuPlacementPolicyPanel::onEditClicked()
{
    if (!this->m_object)
        return;

    if (this->m_object->GetObjectType() == XenObjectType::Pool)
    {
        QSharedPointer<Pool> pool = qSharedPointerDynamicCast<Pool>(this->m_object);
        if (!pool)
            return;
        PoolPropertiesDialog dlg(pool, this);
        dlg.SelectPoolGpuEditPage();
        dlg.exec();
        return;
    }

    if (this->m_object->GetObjectType() == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->m_object);
        if (!host)
            return;
        HostPropertiesDialog dlg(host, this);
        dlg.SelectPoolGpuEditPage();
        dlg.exec();
    }
}

void GpuPlacementPolicyPanel::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString&)
{
    if (!this->m_object || connection != this->m_object->GetConnection())
        return;
    if (type == QLatin1String("gpu_group") || type == QLatin1String("host") || type == QLatin1String("pool"))
        this->PopulatePage();
}

void GpuPlacementPolicyPanel::onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString&)
{
    this->onCacheObjectChanged(connection, type, QString());
}

void GpuPlacementPolicyPanel::onCacheCleared()
{
    this->PopulatePage();
}
