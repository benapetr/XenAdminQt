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


#ifndef GPUSHINYBAR_H
#define GPUSHINYBAR_H

#include "shinybar.h"
#include <QMap>
#include <QSharedPointer>

class PGPU;
class VGPU;
class VM;

class GpuShinyBar : public ShinyBar
{
    Q_OBJECT

    public:
        explicit GpuShinyBar(QWidget* parent = nullptr);

        void Initialize(const QSharedPointer<PGPU>& pgpu);
        QSharedPointer<PGPU> GetPGPU() const
        {
            return this->m_pgpu;
        }

        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;
        QRect BarRect() const override;
        int GetBarHeight() const override
        {
            return BAR_HEIGHT;
        }

    private:
        void DrawGrid(QPainter& painter, const QRect& barArea) const;

        static constexpr int BAR_HEIGHT = 40;
        static const QColor COLOR_VM1;
        static const QColor COLOR_VM2;
        static const QColor COLOR_TEXT;

        QSharedPointer<PGPU> m_pgpu;
        QList<QSharedPointer<VGPU>> m_vgpus;
        QMap<QString, QSharedPointer<VM>> m_vmsByVgpuRef;
        qint64 m_capacity = 1;
        qint64 m_maxCapacity = 1;
};

#endif // GPUSHINYBAR_H
