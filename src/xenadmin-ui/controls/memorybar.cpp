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

#include "memorybar.h"
#include "xenlib/utils/misc.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QToolTip>
#include <QMouseEvent>
#include <QDebug>

MemoryBar::MemoryBar(QWidget* parent) : QWidget(parent), m_totalMemory(0)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MemoryBar::SetTotalMemory(qint64 totalBytes)
{
    m_totalMemory = totalBytes;
    update();
}

void MemoryBar::ClearSegments()
{
    m_segments.clear();
    update();
}

void MemoryBar::AddSegment(const QString& name, qint64 bytes, const QColor& color, const QString& tooltip)
{
    m_segments.append(Segment(name, bytes, color, tooltip));
    update();
}

QSize MemoryBar::sizeHint() const
{
    return QSize(600, BAR_HEIGHT + RULER_HEIGHT + 8);
}

QSize MemoryBar::minimumSizeHint() const
{
    return QSize(200, BAR_HEIGHT + RULER_HEIGHT + 8);
}

void MemoryBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (m_totalMemory <= 0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect fullArea = rect().adjusted(2, 2, -2, -2);
    int barTop = fullArea.top() + RULER_HEIGHT + 4;
    QRect barArea(fullArea.left(), barTop, fullArea.width(), BAR_HEIGHT);

    drawRuler(painter, barArea);

    // Calculate bytes per pixel
    double bytesPerPixel = static_cast<double>(m_totalMemory) / static_cast<double>(barArea.width());

    // Draw segments
    double left = barArea.left();
    for (const Segment& segment : m_segments)
    {
        if (segment.bytes <= 0)
            continue;

        double width = segment.bytes / bytesPerPixel;
        QRect segmentRect(static_cast<int>(left), barArea.top(),
                          static_cast<int>(left + width) - static_cast<int>(left),
                          barArea.height());

        QString text = segment.name + "\n" + Misc::FormatSize(segment.bytes);
        drawSegment(painter, barArea, segmentRect, segment.color, text);

        left += width;
    }

    // Draw free space
    if (left < barArea.right())
    {
        QRect freeRect(static_cast<int>(left), barArea.top(),
                       barArea.right() - static_cast<int>(left), barArea.height());
        drawSegment(painter, barArea, freeRect, QColor(0, 0, 0), "");
    }
}

void MemoryBar::drawSegment(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                            const QColor& color, const QString& text)
{
    if (segmentRect.width() <= 0)
        return;

    // Save state
    painter.save();

    // Set clipping region
    painter.setClipRect(segmentRect);

    // Create rounded rectangle path
    QPainterPath path;
    path.addRoundedRect(barArea, RADIUS, RADIUS);

    // Draw gradient background
    QLinearGradient gradient(barArea.topLeft(), barArea.bottomLeft());
    gradient.setColorAt(0, color);
    gradient.setColorAt(1, color.lighter(120));
    painter.fillPath(path, gradient);

    // Draw text if there's enough space
    if (!text.isEmpty() && segmentRect.width() > 40)
    {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        QRect textRect = segmentRect.adjusted(TEXT_PAD, 0, -TEXT_PAD, 0);
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    // Draw subtle highlight on top half
    QRect highlightRect = barArea;
    highlightRect.setHeight(barArea.height() / 2);
    QPainterPath highlightPath;
    highlightPath.addRoundedRect(highlightRect, RADIUS, RADIUS);
    QLinearGradient highlightGradient(highlightRect.topLeft(), highlightRect.bottomLeft());
    highlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
    highlightGradient.setColorAt(1, QColor(255, 255, 255, 15));
    painter.fillPath(highlightPath, highlightGradient);

    // Restore state
    painter.restore();
}

void MemoryBar::drawGrid(QPainter& painter, const QRect& barArea)
{
    // Draw grid lines at GB intervals if the bar is wide enough
    if (m_totalMemory <= 0 || barArea.width() < 100)
        return;

    const qint64 GB = 1024LL * 1024LL * 1024LL;
    double bytesPerPixel = static_cast<double>(m_totalMemory) / static_cast<double>(barArea.width());

    painter.save();
    painter.setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));

    for (qint64 i = GB; i < m_totalMemory; i += GB)
    {
        int x = barArea.left() + static_cast<int>(i / bytesPerPixel);
        if (x > barArea.left() && x < barArea.right())
        {
            painter.drawLine(x, barArea.top(), x, barArea.bottom());
        }
    }

    painter.restore();
}

void MemoryBar::drawRuler(QPainter& painter, const QRect& barArea)
{
    if (m_totalMemory <= 0 || barArea.width() < 100)
        return;

    const int MIN_GAP = 40;
    const qint64 BINARY_MEGA = 1024LL * 1024LL;
    const qint64 BINARY_GIGA = 1024LL * 1024LL * 1024LL;

    painter.save();
    painter.setPen(QPen(QColor(120, 120, 120), 1));

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    QFontMetrics fm(font);

    QString maxLabel = Misc::FormatSize(m_totalMemory);
    int longest = fm.horizontalAdvance(maxLabel);

    double bytesPerPixel = static_cast<double>(m_totalMemory) / static_cast<double>(barArea.width());
    double incr = BINARY_MEGA / 2.0;
    while (incr / bytesPerPixel * 2 < MIN_GAP + longest)
        incr *= 2;

    int rulerBottom = barArea.top() - 4;
    int tickTop = rulerBottom - RULER_TICK_HEIGHT;
    int textBottom = tickTop - 2;
    int textTop = textBottom - fm.height();

    bool withLabel = true;
    for (double x = 0.0; x <= m_totalMemory; x += incr)
    {
        int pos = barArea.left() + static_cast<int>(x / bytesPerPixel);
        painter.drawLine(pos, tickTop, pos, rulerBottom);

        if (withLabel)
        {
            if (m_totalMemory <= BINARY_GIGA || static_cast<qint64>(x) % (BINARY_GIGA / 2) == 0)
            {
                QString label = Misc::FormatSize(static_cast<qint64>(x));
                QSize size = fm.size(Qt::TextSingleLine, label);
                QRect textRect(QPoint(pos - size.width() / 2, textTop), size);
                painter.drawText(textRect, Qt::AlignCenter, label);
            }
        }
        withLabel = !withLabel;
    }

    painter.restore();
}

void MemoryBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_totalMemory <= 0)
        return;

    QRect fullArea = rect().adjusted(2, 2, -2, -2);
    int barTop = fullArea.top() + RULER_HEIGHT + 4;
    QRect barArea(fullArea.left(), barTop, fullArea.width(), BAR_HEIGHT);
    double bytesPerPixel = static_cast<double>(m_totalMemory) / static_cast<double>(barArea.width());

    double left = barArea.left();
    for (const Segment& segment : m_segments)
    {
        if (segment.bytes <= 0)
            continue;

        double width = segment.bytes / bytesPerPixel;
        QRect segmentRect(static_cast<int>(left), barArea.top(),
                          static_cast<int>(left + width) - static_cast<int>(left),
                          barArea.height());

        if (segmentRect.contains(event->pos()))
        {
            QString tooltip = segment.tooltip.isEmpty()
                                  ? (segment.name + "\n" + Misc::FormatSize(segment.bytes))
                                  : segment.tooltip;
            // Qt5 compatibility: use globalPos() instead of globalPosition()
            QToolTip::showText(event->globalPos(), tooltip, this);
            return;
        }

        left += width;
    }

    QToolTip::hideText();
    QWidget::mouseMoveEvent(event);
}

bool MemoryBar::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        return true; // We handle tooltips ourselves
    }
    return QWidget::event(event);
}

