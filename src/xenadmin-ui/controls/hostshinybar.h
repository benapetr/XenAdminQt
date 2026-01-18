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

#ifndef HOSTSHINYBAR_H
#define HOSTSHINYBAR_H

#include "shinybar.h"
#include <QSharedPointer>
#include <QVector>
#include <QRect>
#include "xen/host.h"
#include "xen/vm.h"

/**
 * @brief A shiny bar visualization for host memory usage
 *
 * Shows host memory breakdown with segments for:
 * - Xen hypervisor memory
 * - Dom0 (control domain) memory  
 * - Resident VM memory usage
 * - Free memory
 *
 * Qt port of C# XenAdmin HostShinyBar control.
 */
class HostShinyBar : public ShinyBar
{
    Q_OBJECT

    public:
        explicit HostShinyBar(QWidget* parent = nullptr);

        /**
         * @brief Initialize the bar with host data
         * @param host The host to visualize
         * @param xenMemory Xen hypervisor memory in bytes
         * @param dom0Memory Control domain memory in bytes
         */
        void Initialize(QSharedPointer<Host> host, qint64 xenMemory, qint64 dom0Memory);

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        bool event(QEvent* event) override;

        // ShinyBar interface implementation
        QRect BarRect() const override;
        int GetBarHeight() const override { return BAR_HEIGHT; }

    private:
        struct SegmentInfo
        {
            QRect rect;
            QString tooltip;
        };

        QSharedPointer<Host> host_;
        QList<QSharedPointer<VM>> vms_;
        qint64 xenMemory_;
        qint64 dom0Memory_;
        QVector<SegmentInfo> segments_;

        void DrawHostSegment(QPainter& painter, qint64 mem, double bytesPerPixel,
                             const QString& name, const QColor& color, double& left);

        // Constants
        static constexpr int BAR_HEIGHT = 40;

        // Colors
        static const QColor COLOR_XEN;
        static const QColor COLOR_CONTROL_DOMAIN;
        static const QColor COLOR_VM1;
        static const QColor COLOR_VM2;
};

#endif // HOSTSHINYBAR_H
