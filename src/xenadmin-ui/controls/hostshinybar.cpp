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

#include "hostshinybar.h"
#include "xenlib/xen/hostmetrics.h"
#include "xenlib/xen/vmmetrics.h"
#include "xenlib/utils/misc.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QToolTip>
#include <cmath>

// Color definitions matching C# XenAdmin
const QColor HostShinyBar::COLOR_XEN = QColor(120, 120, 120);
const QColor HostShinyBar::COLOR_CONTROL_DOMAIN = QColor(40, 60, 110);
const QColor HostShinyBar::COLOR_VM1 = QColor(111, 164, 216);
const QColor HostShinyBar::COLOR_VM2 = QColor(153, 198, 241);

HostShinyBar::HostShinyBar(QWidget* parent)
    : ShinyBar(parent)
    , xenMemory_(0)
    , dom0Memory_(0)
{
    this->setMinimumHeight(BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

void HostShinyBar::Initialize(QSharedPointer<Host> host, qint64 xenMemory, qint64 dom0Memory)
{
    this->host_ = host;
    this->xenMemory_ = xenMemory;
    this->dom0Memory_ = dom0Memory;
    
    // Get resident VMs for this host
    this->vms_.clear();
    if (host && !host->IsEvicted())
    {
        this->vms_ = host->GetResidentVMs();
    }
    
    this->update();
}

QSize HostShinyBar::sizeHint() const
{
    return QSize(400, BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

QSize HostShinyBar::minimumSizeHint() const
{
    return QSize(200, BAR_HEIGHT + ShinyBar::RULER_HEIGHT + 8);
}

QRect HostShinyBar::BarRect() const
{
    QRect fullArea = this->rect().adjusted(PAD, PAD, -PAD, -PAD);
    int barTop = fullArea.top() + ShinyBar::RULER_HEIGHT + 4;
    return QRect(fullArea.left(), barTop, fullArea.width(), BAR_HEIGHT);
}

void HostShinyBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    if (!this->host_ || this->host_->IsEvicted())
    {
        return;
    }
    
    // Get host metrics
    QSharedPointer<HostMetrics> metrics = this->host_->GetMetrics();
    if (!metrics || metrics->IsEvicted())
    {
        return;
    }
    
    QRect barArea = this->BarRect();
    qint64 totalMemory = metrics->GetMemoryTotal();
    
    if (totalMemory <= 0)
    {
        return;
    }
    
    this->segments_.clear();
    // Use base class DrawRuler
    ShinyBar::DrawRuler(painter, barArea, totalMemory, static_cast<double>(totalMemory) / barArea.width());

    double bytesPerPixel = static_cast<double>(totalMemory) / barArea.width();

    // Draw segments from left to right
    double left = barArea.left();
    
    // 1. Xen hypervisor memory
    this->DrawHostSegment(painter, this->xenMemory_, bytesPerPixel, tr("Xen"), COLOR_XEN, left);
    
    // 2. Control domain (Dom0) memory
    this->DrawHostSegment(painter, this->dom0Memory_, bytesPerPixel, tr("Control domain"), COLOR_CONTROL_DOMAIN, left);
    
    // 3. VM memory usage
    bool alternate = false;
    for (const QSharedPointer<VM>& vm : this->vms_)
    {
        if (!vm || vm->IsEvicted())
        {
            continue;
        }
        if (vm->IsControlDomain())
        {
            continue;
        }
        
        // Get VM metrics for memory actual
        QSharedPointer<VMMetrics> vmMetrics = vm->GetMetrics();
        if (!vmMetrics || vmMetrics->IsEvicted())
        {
            continue;
        }
        
        qint64 memoryActual = vmMetrics->GetMemoryActual();
        if (memoryActual <= 0)
        {
            continue;
        }
        
        QColor vmColor = alternate ? COLOR_VM2 : COLOR_VM1;
        alternate = !alternate;
        
        this->DrawHostSegment(painter, memoryActual, bytesPerPixel, vm->GetName(), 
                              vmColor, left);
    }
    
    // 4. Free memory (remaining space)
    if (left < barArea.right())
    {
        double freePixels = barArea.right() - left;
        qint64 freeMemory = static_cast<qint64>(freePixels * bytesPerPixel);
        this->DrawHostSegment(painter, freeMemory, bytesPerPixel, tr("Free"),
                              COLOR_UNUSED, left);
    }
}

void HostShinyBar::DrawHostSegment(QPainter& painter, qint64 mem, double bytesPerPixel,
                                    const QString& name, const QColor& color, double& left)
{
    if (mem <= 0)
    {
        return;
    }
    
    QRect barArea = this->BarRect();
    double width = mem / bytesPerPixel;
    
    if (width < 1.0)
    {
        return;  // Too small to display
    }
    
    int segmentLeft = static_cast<int>(std::round(left));
    int segmentRight = static_cast<int>(std::round(left + width));
    
    // Clamp to bar bounds
    if (segmentRight > barArea.right())
    {
        segmentRight = barArea.right();
    }
    
    QRect segmentBounds(segmentLeft, barArea.top(), 
                        segmentRight - segmentLeft, barArea.height());
    
    if (segmentBounds.width() <= 0)
    {
        return;
    }
    
    // Format memory size
    QString memText = Misc::FormatMemorySize(mem);
    QString displayText = QString("%1 %2").arg(name, memText);
    
    // Draw the segment using base class method
    ShinyBar::DrawSegmentFill(painter, barArea, segmentBounds, color, displayText);

    QString tooltip = name.isEmpty()
        ? Misc::FormatMemorySize(mem)
        : QString("%1\n%2").arg(name, Misc::FormatMemorySize(mem));
    SegmentInfo info;
    info.rect = segmentBounds;
    info.tooltip = tooltip;
    this->segments_.append(info);
    
    left += width;
}

void HostShinyBar::mouseMoveEvent(QMouseEvent* event)
{
    for (const SegmentInfo& info : this->segments_)
    {
        if (info.rect.contains(event->pos()))
        {
            if (!info.tooltip.isEmpty())
            {
                QToolTip::showText(event->globalPos(), info.tooltip, this);
                return;
            }
        }
    }

    QToolTip::hideText();
    QWidget::mouseMoveEvent(event);
}

bool HostShinyBar::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        return true;
    }

    return QWidget::event(event);
}
