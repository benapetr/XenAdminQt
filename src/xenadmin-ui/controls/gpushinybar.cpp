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


#include "gpushinybar.h"

#include "xenlib/xen/actions/gpu/gpuhelpers.h"
#include "xenlib/xen/pgpu.h"
#include "xenlib/xen/vgpu.h"
#include "xenlib/xen/vgputype.h"
#include "xenlib/xen/vm.h"

#include <QPainter>
#include <algorithm>

const QColor GpuShinyBar::COLOR_VM1 = QColor(111, 164, 216);
const QColor GpuShinyBar::COLOR_VM2 = QColor(153, 198, 241);
const QColor GpuShinyBar::COLOR_TEXT = QColor(255, 255, 255);

GpuShinyBar::GpuShinyBar(QWidget* parent) : ShinyBar(parent)
{
    this->setMinimumHeight(BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

void GpuShinyBar::Initialize(const QSharedPointer<PGPU>& pgpu)
{
    this->m_pgpu = pgpu;
    this->m_vgpus.clear();
    this->m_vmsByVgpuRef.clear();
    this->m_capacity = 1;
    this->m_maxCapacity = 1;

    if (!pgpu || !pgpu->IsValid())
        return;

    this->m_vgpus = pgpu->GetResidentVGPUs();
    for (const QSharedPointer<VGPU>& vgpu : std::as_const(this->m_vgpus))
    {
        if (!vgpu || !vgpu->IsValid())
            continue;
        this->m_vmsByVgpuRef.insert(vgpu->OpaqueRef(), vgpu->GetVM());
    }

    const bool vgpuCapability = !GpuHelpers::FeatureForbidden(pgpu->GetConnection(), &Host::RestrictVgpu) && pgpu->HasVGpu();
    if (vgpuCapability)
    {
        const QVariantMap caps = pgpu->SupportedVGPUMaxCapacities();
        for (auto it = caps.constBegin(); it != caps.constEnd(); ++it)
        {
            const qint64 value = it.value().toLongLong();
            if (value > this->m_maxCapacity)
                this->m_maxCapacity = value;
        }
    }

    if (!this->m_vgpus.isEmpty())
    {
        const QSharedPointer<VGPU>& first = this->m_vgpus.first();
        if (first && first->IsValid())
        {
            const QVariantMap caps = pgpu->SupportedVGPUMaxCapacities();
            const QString key = first->TypeRef();
            if (caps.contains(key))
                this->m_capacity = std::max<qint64>(1, caps.value(key).toLongLong());
            else
                this->m_capacity = this->m_maxCapacity;
        }
    }

    if (this->m_capacity <= 0)
        this->m_capacity = std::max<qint64>(1, this->m_maxCapacity);
}

QSize GpuShinyBar::sizeHint() const
{
    return QSize(420, BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

QSize GpuShinyBar::minimumSizeHint() const
{
    return QSize(220, BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

QRect GpuShinyBar::BarRect() const
{
    const QRect full = this->rect();
    const int top = full.top() + ShinyBar::RULER_HEIGHT + 4;
    return QRect(full.left() + 10, top, std::max(10, full.width() - 20), BAR_HEIGHT);
}

void GpuShinyBar::DrawGrid(QPainter& painter, const QRect& barArea) const
{
    if (this->m_capacity <= 1 || barArea.width() <= 0)
        return;

    const double segment = static_cast<double>(barArea.width()) / static_cast<double>(this->m_capacity);
    painter.save();
    painter.setPen(QPen(ShinyBar::COLOR_GRID, 1));
    for (qint64 i = 0; i <= this->m_capacity; ++i)
    {
        const int x = barArea.left() + static_cast<int>(i * segment);
        painter.drawLine(x, barArea.top(), x, barArea.bottom());
    }
    painter.restore();
}

void GpuShinyBar::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    if (!this->m_pgpu || !this->m_pgpu->IsValid())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect barArea = this->BarRect();
    if (!barArea.isValid())
        return;

    const qint64 total = std::max<qint64>(1, this->m_capacity);
    const double bytesPerPixel = static_cast<double>(total) / static_cast<double>(std::max(1, barArea.width()));
    this->DrawRuler(painter, barArea, total, bytesPerPixel);
    if (this->m_maxCapacity > 1)
        this->DrawGrid(painter, barArea);

    QList<QSharedPointer<VGPU>> ordered = this->m_vgpus;
    std::sort(ordered.begin(), ordered.end(), [this](const QSharedPointer<VGPU>& a, const QSharedPointer<VGPU>& b) {
        const QSharedPointer<VM> avm = a ? this->m_vmsByVgpuRef.value(a->OpaqueRef()) : QSharedPointer<VM>();
        const QSharedPointer<VM> bvm = b ? this->m_vmsByVgpuRef.value(b->OpaqueRef()) : QSharedPointer<VM>();
        const QString an = avm ? avm->GetName() : QString();
        const QString bn = bvm ? bvm->GetName() : QString();
        return QString::compare(an, bn, Qt::CaseInsensitive) < 0;
    });

    double left = barArea.left();
    const double segmentLength = static_cast<double>(barArea.width()) / static_cast<double>(total);
    int index = 0;
    for (const QSharedPointer<VGPU>& vgpu : std::as_const(ordered))
    {
        if (!vgpu || !vgpu->IsValid())
            continue;

        const QSharedPointer<VM> vm = this->m_vmsByVgpuRef.value(vgpu->OpaqueRef());
        QSharedPointer<VGPUType> type = vgpu->GetConnection() && vgpu->GetConnection()->GetCache()
                                            ? vgpu->GetConnection()->GetCache()->ResolveObject<VGPUType>(vgpu->TypeRef())
                                            : QSharedPointer<VGPUType>();

        const QRect segmentRect(static_cast<int>(left),
                                barArea.top(),
                                std::max(1, static_cast<int>(left + segmentLength) - static_cast<int>(left)),
                                barArea.height());

        const QString line1 = vm ? vm->GetName() : QStringLiteral("VM");
        const QString line2 = type ? type->ModelName() : QString();
        const QString text = line2.isEmpty() ? line1 : QStringLiteral("%1\n%2").arg(line1, line2);
        const QColor color = (index++ % 2 == 0) ? COLOR_VM1 : COLOR_VM2;
        this->DrawSegment(painter, barArea, segmentRect, color, text, COLOR_TEXT, Qt::AlignCenter);
        left += segmentLength;
    }

    if (left < barArea.right())
    {
        const QRect freeRect(static_cast<int>(left), barArea.top(), barArea.right() - static_cast<int>(left), barArea.height());
        this->DrawSegment(painter, barArea, freeRect, ShinyBar::COLOR_UNUSED);
    }
}
