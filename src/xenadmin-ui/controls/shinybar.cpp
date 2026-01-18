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

#include "shinybar.h"
#include <QPainterPath>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QPen>
#include <cmath>

// Color definitions
const QColor ShinyBar::COLOR_UNUSED = QColor(0, 0, 0);      // Black
const QColor ShinyBar::COLOR_GRID = QColor(169, 169, 169);  // DarkGray

ShinyBar::ShinyBar(QWidget* parent)
    : QWidget(parent)
{
    this->setMouseTracking(true);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ShinyBar::DrawSegmentFill(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                                const QColor& color, const QString& text)
{
    if (segmentRect.width() <= 0)
        return;

    painter.save();
    painter.setClipRect(segmentRect);

    // Create rounded rectangle path for the entire bar
    QPainterPath path;
    path.addRoundedRect(barArea, RADIUS, RADIUS);

    // Fill with gradient
    QLinearGradient gradient(barArea.topLeft(), barArea.bottomLeft());
    gradient.setColorAt(0, color);
    gradient.setColorAt(1, color.lighter(120));
    painter.fillPath(path, gradient);

    // Draw text if provided and there's enough space
    if (!text.isEmpty() && segmentRect.width() > MIN_GAP)
    {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        QRect textRect = segmentRect.adjusted(TEXT_PAD, 0, -TEXT_PAD, 0);
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    // Add highlight effect on upper half
    QRect highlightRect = barArea;
    highlightRect.setHeight(barArea.height() / 2);
    QPainterPath highlightPath;
    highlightPath.addRoundedRect(highlightRect, RADIUS, RADIUS);
    QLinearGradient highlightGradient(highlightRect.topLeft(), highlightRect.bottomLeft());
    highlightGradient.setColorAt(0, QColor(255, 255, 255, 60));
    highlightGradient.setColorAt(1, QColor(255, 255, 255, 15));
    painter.fillPath(highlightPath, highlightGradient);

    painter.restore();

    // Draw segment border (subtle)
    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1));
    int borderX = segmentRect.right();
    if (borderX > barArea.left() && borderX < barArea.right())
        painter.drawLine(borderX, barArea.top() + 2, borderX, barArea.bottom() - 2);
    painter.restore();
}

void ShinyBar::DrawSegment(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                            const QColor& color, const QString& text, const QColor& textColor,
                            Qt::Alignment alignment)
{
    if (segmentRect.width() <= 0)
        return;

    painter.save();
    painter.setClipRect(segmentRect);

    // Create rounded rectangle path
    QPainterPath outerPath;
    outerPath.addRoundedRect(barArea, RADIUS, RADIUS);

    // Outer rounded rectangle with gradient
    QLinearGradient outerBrush(barArea.topLeft(), barArea.bottomLeft());
    outerBrush.setColorAt(0, color);
    outerBrush.setColorAt(1, color.lighter(120));
    painter.fillPath(outerPath, outerBrush);

    // Render text if provided
    if (!text.isEmpty() && textColor.isValid())
    {
        painter.setPen(textColor);
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);

        QFontMetrics fm(font);
        QSize textSize = fm.size(Qt::TextSingleLine, text);

        float horizPos;
        if (alignment & Qt::AlignRight)
            horizPos = segmentRect.right() - textSize.width() - TEXT_PAD;
        else if (alignment & Qt::AlignHCenter)
            horizPos = segmentRect.left() + (segmentRect.width() - textSize.width()) / 2;
        else
            horizPos = segmentRect.left() + TEXT_PAD;

        QRectF textRect(horizPos,
                        segmentRect.top() + (segmentRect.height() - textSize.height() * 0.9f) / 2,
                        textSize.width(),
                        textSize.height());

        if (textRect.x() < segmentRect.x() + TEXT_PAD)
            textRect.moveLeft(segmentRect.x() + TEXT_PAD);

        painter.drawText(textRect, Qt::AlignLeft, text);
    }

    // Inner rounded rectangle for highlight effect
    QRectF innerRect(barArea.x() + PAD, barArea.y() + PAD,
                     barArea.width() - (2.0 * PAD), barArea.height() * 0.49);
    QPainterPath innerPath;
    innerPath.addRoundedRect(innerRect, RADIUS - PAD, RADIUS - PAD);

    QLinearGradient lighterBrush(innerRect.topLeft(), innerRect.bottomLeft());
    lighterBrush.setColorAt(0, QColor(255, 255, 255, 120));
    lighterBrush.setColorAt(1, QColor(255, 255, 255, 30));
    painter.fillPath(innerPath, lighterBrush);

    painter.restore();
}

void ShinyBar::DrawRuler(QPainter& painter, const QRect& barArea, qint64 totalValue, double bytesPerPixel)
{
    if (totalValue <= 0 || barArea.width() < 100)
        return;

    const int MIN_LABEL_GAP = MIN_GAP;
    const qint64 BINARY_MEGA = 1024LL * 1024LL;

    painter.save();
    painter.setPen(QPen(COLOR_GRID, 1));

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    QFontMetrics fm(font);

    // Find the size of the longest label
    QString maxLabel = this->FormatMemorySize(totalValue);
    int longest = fm.horizontalAdvance(maxLabel);

    // Calculate a suitable increment
    double incr = BINARY_MEGA / 2.0;
    while (incr / bytesPerPixel * 2 < MIN_LABEL_GAP + longest)
        incr *= 2;

    int rulerBottom = barArea.top() - 4;
    int tickTop = rulerBottom - RULER_TICK_HEIGHT;
    int textBottom = tickTop - 2;
    int textTop = textBottom - fm.height();

    bool withLabel = true;
    for (double x = 0.0; x <= totalValue; x += incr)
    {
        int px = barArea.left() + static_cast<int>(x / bytesPerPixel);
        if (px < barArea.left() || px > barArea.right())
            continue;

        // Draw tick mark
        int tickHeight = withLabel ? RULER_TICK_HEIGHT : RULER_TICK_HEIGHT / 2;
        int tickStart = rulerBottom - tickHeight;
        painter.drawLine(px, tickStart, px, rulerBottom);

        // Draw label
        if (withLabel)
        {
            QString label = this->FormatMemorySize(static_cast<qint64>(x));
            int textWidth = fm.horizontalAdvance(label);
            int textLeft = px - textWidth / 2;
            QRect textRect(textLeft, textTop, textWidth, fm.height());
            painter.drawText(textRect, Qt::AlignCenter, label);
        }

        withLabel = !withLabel;
    }

    painter.restore();
}

QString ShinyBar::FormatMemorySize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;

    if (bytes >= TB)
    {
        double tb = static_cast<double>(bytes) / TB;
        return QString::number(tb, 'f', 1) + " TB";
    }
    else if (bytes >= GB)
    {
        double gb = static_cast<double>(bytes) / GB;
        return QString::number(gb, 'f', 1) + " GB";
    }
    else if (bytes >= MB)
    {
        double mb = static_cast<double>(bytes) / MB;
        return QString::number(mb, 'f', 1) + " MB";
    }
    else if (bytes >= KB)
    {
        double kb = static_cast<double>(bytes) / KB;
        return QString::number(kb, 'f', 1) + " KB";
    }

    return QString::number(bytes) + " B";
}
