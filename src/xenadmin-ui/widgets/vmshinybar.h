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

#ifndef VMSHINYBAR_H
#define VMSHINYBAR_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include "xen/vm.h"

/**
 * @brief A memory visualization widget with draggable sliders for dynamic min/max memory.
 * 
 * This widget displays VM memory usage as a bar graph with interactive sliders
 * for adjusting dynamic memory ranges. It supports memory ballooning visualization.
 * 
 * Based on C# XenAdmin VMShinyBar control.
 */
class VMShinyBar : public QWidget
{
    Q_OBJECT

    public:
        explicit VMShinyBar(QWidget* parent = nullptr);

        /**
         * @brief Populate the widget with VM data
         * @param vms List of VMs to display (typically single VM)
         * @param allowMemEdit Whether memory editing is allowed
         */
        void Populate(const QList<QSharedPointer<VM>>& vms, bool allowMemEdit);

        /**
         * @brief Set valid ranges for dynamic memory sliders
         * @param dynamicMinLowLimit Minimum allowed value for dynamic_min slider
         * @param dynamicMinHighLimit Maximum allowed value for dynamic_min slider
         * @param dynamicMaxLowLimit Minimum allowed value for dynamic_max slider
         * @param dynamicMaxHighLimit Maximum allowed value for dynamic_max slider
         * @param units "MB" or "GB" for rounding behavior
         */
        void SetRanges(double dynamicMinLowLimit, double dynamicMinHighLimit,
                       double dynamicMaxLowLimit, double dynamicMaxHighLimit,
                       const QString& units);

        /**
         * @brief Update memory settings (called when spinners change)
         * @param staticMin Static minimum memory
         * @param dynamicMin Dynamic minimum memory
         * @param dynamicMax Dynamic maximum memory
         * @param staticMax Static maximum memory
         */
        void ChangeSettings(double staticMin, double dynamicMin, double dynamicMax, double staticMax);

        /**
         * @brief Get the increment in which sliders can move (in bytes)
         */
        double Increment() const { return this->increment_; }
        void SetIncrement(double increment) { this->increment_ = increment; }

        /**
         * @brief Get current dynamic minimum memory setting
         */
        double DynamicMin() const { return this->dynamicMin_; }

        /**
         * @brief Get current dynamic maximum memory setting
         */
        double DynamicMax() const { return this->dynamicMax_; }

        /**
         * @brief Check if dynamic min equals dynamic max
         */
        bool Equal() const { return this->dynamicMin_ == this->dynamicMax_; }

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    signals:
        /**
         * @brief Emitted when a slider is dragged by the user
         */
        void sliderDragged();

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    private:
        enum class Slider
        {
            NONE,
            MIN,
            MAX
        };

        // VM data
        qint64 memoryUsed_;
        double staticMin_, staticMax_;
        double dynamicMin_, dynamicMax_;
        double dynamicMinOrig_, dynamicMaxOrig_;
        bool hasBallooning_;
        bool allowEdit_;
        bool multiple_;

        // Slider ranges
        double dynamicMinLowLimit_, dynamicMinHighLimit_;
        double dynamicMaxLowLimit_, dynamicMaxHighLimit_;
        double increment_;

        // Mouse interaction state
        QPoint mouseLocation_;
        QRect minSliderRect_, maxSliderRect_;
        Slider activeSlider_;
        bool mouseIsDown_;
        double bytesPerPixel_;

        // Slider images
        QPixmap sliderMinImage_;
        QPixmap sliderMaxImage_;
        QPixmap sliderMinImageLight_;
        QPixmap sliderMaxImageLight_;
        QPixmap sliderMinImageDark_;
        QPixmap sliderMaxImageDark_;
        QPixmap sliderMinImageNoEdit_;
        QPixmap sliderMaxImageNoEdit_;

        // Helper methods
        static qint64 CalcMemoryUsed(const QList<QSharedPointer<VM>>& vms);
        void SetMemory(Slider slider, double bytes);
        double SliderMinLimit() const;
        double SliderMaxLimit() const;
        void DrawSliderRanges(QPainter& painter);
        void DrawSliders(QPainter& painter, double min, double max);
        void DrawGrid(QPainter& painter, const QRect& barArea, double bytesPerPixel, double max);
        void DrawToTarget(QPainter& painter, const QRect& barArea, const QRect& segmentBounds,
                          const QColor& color, const QString& text = QString(),
                          const QColor& textColor = QColor(), Qt::Alignment alignment = Qt::AlignLeft,
                          const QString& toolTipText = QString());
        QRect BarRect() const;

        // Constants
        static constexpr int RADIUS = 5;
        static constexpr int PAD = 2;
        static constexpr int TEXT_PAD = 3;
        static constexpr int TEXT_FADE = 8;
        static constexpr int BAR_HEIGHT = 20;
        static constexpr int SLIDER_RANGE_HEIGHT = 10;

        // Colors
        static const QColor COLOR_USED;
        static const QColor COLOR_UNUSED;
        static const QColor COLOR_TEXT;
        static const QColor COLOR_GRID;
        static const QColor COLOR_SLIDER_LIMITS;
};

#endif // VMSHINYBAR_H
