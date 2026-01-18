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
const QColor HostShinyBar::COLOR_UNUSED = QColor(0, 0, 0);

HostShinyBar::HostShinyBar(QWidget* parent)
    : QWidget(parent)
    , xenMemory_(0)
    , dom0Memory_(0)
{
    this->setMinimumHeight(BAR_HEIGHT + RULER_HEIGHT + 8);
    this->setMouseTracking(true);
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
    return QSize(400, BAR_HEIGHT + RULER_HEIGHT + 8);
}

QSize HostShinyBar::minimumSizeHint() const
{
    return QSize(200, BAR_HEIGHT + RULER_HEIGHT + 8);
}

QRect HostShinyBar::BarRect() const
{
    QRect fullArea = this->rect().adjusted(PAD, PAD, -PAD, -PAD);
    int barTop = fullArea.top() + RULER_HEIGHT + 4;
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
    this->DrawRuler(painter, barArea, totalMemory);

    double bytesPerPixel = static_cast<double>(totalMemory) / barArea.width();

    // Draw segments from left to right
    double left = barArea.left();
    
    // 1. Xen hypervisor memory
    this->DrawSegment(painter, this->xenMemory_, bytesPerPixel, tr("Xen"), COLOR_XEN, left);
    
    // 2. Control domain (Dom0) memory
    this->DrawSegment(painter, this->dom0Memory_, bytesPerPixel, tr("Control domain"), COLOR_CONTROL_DOMAIN, left);
    
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
        
        this->DrawSegment(painter, memoryActual, bytesPerPixel, vm->GetName(), 
                          vmColor, left);
    }
    
    // 4. Free memory (remaining space)
    if (left < barArea.right())
    {
        double freePixels = barArea.right() - left;
        qint64 freeMemory = static_cast<qint64>(freePixels * bytesPerPixel);
        this->DrawSegment(painter, freeMemory, bytesPerPixel, tr("Free"),
                          COLOR_UNUSED, left);
    }
}

void HostShinyBar::DrawSegment(QPainter& painter, qint64 mem, double bytesPerPixel,
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
    
    // Draw the segment
    this->DrawSegmentFill(painter, barArea, segmentBounds, color, displayText);

    QString tooltip = name.isEmpty()
        ? Misc::FormatMemorySize(mem)
        : QString("%1\n%2").arg(name, Misc::FormatMemorySize(mem));
    SegmentInfo info;
    info.rect = segmentBounds;
    info.tooltip = tooltip;
    this->segments_.append(info);
    
    left += width;
}

void HostShinyBar::DrawRuler(QPainter& painter, const QRect& barArea, qint64 totalMemory)
{
    if (totalMemory <= 0 || barArea.width() < 100)
        return;

    const int MIN_LABEL_GAP = 40;
    const qint64 BINARY_MEGA = 1024LL * 1024LL;

    painter.save();
    painter.setPen(QPen(QColor(120, 120, 120), 1));

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    QFontMetrics fm(font);

    QString maxLabel = Misc::FormatMemorySize(totalMemory);
    int longest = fm.horizontalAdvance(maxLabel);

    double bytesPerPixel = static_cast<double>(totalMemory) / static_cast<double>(barArea.width());
    double incr = BINARY_MEGA / 2.0;
    while (incr / bytesPerPixel * 2 < MIN_LABEL_GAP + longest)
        incr *= 2;

    int rulerBottom = barArea.top() - 4;
    int tickTop = rulerBottom - RULER_TICK_HEIGHT;
    int textBottom = tickTop - 2;
    int textTop = textBottom - fm.height();

    bool withLabel = true;
    for (double x = 0.0; x <= totalMemory; x += incr)
    {
        int px = barArea.left() + static_cast<int>(x / bytesPerPixel);
        if (px < barArea.left() || px > barArea.right())
            continue;

        int tickHeight = withLabel ? RULER_TICK_HEIGHT : RULER_TICK_HEIGHT / 2;
        int tickStart = rulerBottom - tickHeight;
        painter.drawLine(px, tickStart, px, rulerBottom);

        if (withLabel)
        {
            QString label = Misc::FormatMemorySize(static_cast<qint64>(x));
            int textWidth = fm.horizontalAdvance(label);
            int textLeft = px - textWidth / 2;
            QRect textRect(textLeft, textTop, textWidth, fm.height());
            painter.drawText(textRect, Qt::AlignCenter, label);
        }

        withLabel = !withLabel;
    }

    painter.restore();
}

void HostShinyBar::DrawSegmentFill(QPainter& painter, const QRect& barArea,
                                    const QRect& segmentRect, const QColor& color,
                                    const QString& text)
{
    if (segmentRect.width() <= 0)
        return;

    painter.save();
    painter.setClipRect(segmentRect);

    QPainterPath path;
    path.addRoundedRect(barArea, RADIUS, RADIUS);

    QLinearGradient gradient(barArea.topLeft(), barArea.bottomLeft());
    gradient.setColorAt(0, color);
    gradient.setColorAt(1, color.lighter(120));
    painter.fillPath(path, gradient);

    if (!text.isEmpty() && segmentRect.width() > MIN_GAP)
    {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        QRect textRect = segmentRect.adjusted(TEXT_PAD, 0, -TEXT_PAD, 0);
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    QRect highlightRect = barArea;
    highlightRect.setHeight(barArea.height() / 2);
    QPainterPath highlightPath;
    highlightPath.addRoundedRect(highlightRect, RADIUS, RADIUS);
    QLinearGradient highlightGradient(highlightRect.topLeft(), highlightRect.bottomLeft());
    highlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
    highlightGradient.setColorAt(1, QColor(255, 255, 255, 15));
    painter.fillPath(highlightPath, highlightGradient);

    painter.restore();

    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1));
    int borderX = segmentRect.right();
    if (borderX > barArea.left() && borderX < barArea.right())
        painter.drawLine(borderX, barArea.top() + 2, borderX, barArea.bottom() - 2);
    painter.restore();
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
