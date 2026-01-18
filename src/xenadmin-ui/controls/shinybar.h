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

#ifndef SHINYBAR_H
#define SHINYBAR_H

#include <QWidget>
#include <QPainter>
#include <QRect>

/**
 * @brief Base class for shiny bar visualization widgets
 * 
 * This abstract base class provides common functionality for memory visualization
 * widgets that display segmented bars with gradients, rulers, and tooltips.
 * 
 * Derived classes implement specific visualization logic for VMs, hosts, etc.
 */
class ShinyBar : public QWidget
{
    Q_OBJECT

    public:
        explicit ShinyBar(QWidget* parent = nullptr);
        virtual ~ShinyBar() = default;

    protected:
        /**
         * @brief Calculate the rectangle where the bar should be drawn
         * @return QRect representing the bar area
         */
        virtual QRect BarRect() const = 0;

        /**
         * @brief Get the height of the bar
         * @return Bar height in pixels
         */
        virtual int GetBarHeight() const = 0;

        /**
         * @brief Draw a segment of the bar with gradient fill
         * @param painter The QPainter to draw with
         * @param barArea The full bar area (for clipping context)
         * @param segmentRect The specific segment rectangle to draw
         * @param color The base color for the segment
         * @param text Optional text to display in the segment
         */
        void DrawSegmentFill(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                             const QColor& color, const QString& text = QString());

        /**
         * @brief Draw a ruler/grid above the bar
         * @param painter The QPainter to draw with
         * @param barArea The bar area
         * @param totalValue The maximum value represented by the bar
         * @param bytesPerPixel Conversion factor from bytes to pixels
         */
        void DrawRuler(QPainter& painter, const QRect& barArea, qint64 totalValue, double bytesPerPixel);

        /**
         * @brief Format a memory size in bytes to human-readable string
         * @param bytes Memory size in bytes
         * @return Formatted string (e.g., "1.5 GB", "512 MB")
         */
        QString FormatMemorySize(qint64 bytes) const;

        /**
         * @brief Draw rounded rectangle segment with gradient and highlight
         * @param painter The QPainter to draw with
         * @param barArea The full bar area (for rounded corners context)
         * @param segmentRect The segment to draw
         * @param color Base color for the segment
         * @param text Optional text to render
         * @param textColor Color for the text
         * @param alignment Text alignment within segment
         */
        void DrawSegment(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                         const QColor& color, const QString& text = QString(),
                         const QColor& textColor = Qt::white,
                         Qt::Alignment alignment = Qt::AlignCenter);

        // Constants
        static constexpr int RADIUS = 5;           // Rounded corner radius
        static constexpr int PAD = 2;              // Inner padding for highlight effect
        static constexpr int TEXT_PAD = 3;         // Padding around text
        static constexpr int RULER_HEIGHT = 18;    // Height of ruler above bar
        static constexpr int RULER_TICK_HEIGHT = 6; // Height of ruler tick marks
        static constexpr int MIN_GAP = 40;         // Minimum gap between labels

        // Common colors
        static const QColor COLOR_UNUSED;          // Color for unused/free memory
        static const QColor COLOR_GRID;            // Color for grid lines
};

#endif // SHINYBAR_H
