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

#ifndef MEMORYBAR_H
#define MEMORYBAR_H

#include <QWidget>
#include <QPainter>
#include <QList>
#include <QString>
#include <QColor>

/**
 * A custom widget that displays memory usage as a segmented bar.
 * Similar to the ShinyBar control in the C# XenAdmin.
 */
class MemoryBar : public QWidget
{
    Q_OBJECT

    public:
        struct Segment
        {
            QString name;
            qint64 bytes;
            QColor color;
            QString tooltip;

            Segment(const QString& n, qint64 b, const QColor& c, const QString& tt = QString())
                : name(n), bytes(b), color(c), tooltip(tt)
            {
            }
        };

        explicit MemoryBar(QWidget* parent = nullptr);

        void SetTotalMemory(qint64 totalBytes);
        void ClearSegments();
        void AddSegment(const QString& name, qint64 bytes, const QColor& color, const QString& tooltip = QString());

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        bool event(QEvent* event) override;

    private:
        qint64 m_totalMemory;
        QList<Segment> m_segments;

        static constexpr int RADIUS = 5;
        static constexpr int BAR_HEIGHT = 40;
        static constexpr int TEXT_PAD = 3;
        static constexpr int RULER_HEIGHT = 18;
        static constexpr int RULER_TICK_HEIGHT = 6;

        void drawSegment(QPainter& painter, const QRect& barArea, const QRect& segmentRect,
                         const QColor& color, const QString& text);
        void drawGrid(QPainter& painter, const QRect& barArea);
        void drawRuler(QPainter& painter, const QRect& barArea);
};

#endif // MEMORYBAR_H
