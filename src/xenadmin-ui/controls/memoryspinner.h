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

#ifndef MEMORYSPINNER_H
#define MEMORYSPINNER_H

#include <QDoubleSpinBox>

class MemorySpinner : public QDoubleSpinBox
{
    Q_OBJECT

    public:
        enum class Unit
        {
            MB,
            GB
        };

        explicit MemorySpinner(QWidget* parent = nullptr);

        Unit GetUnit() const;
        void SetUnit(Unit unit);

        qint64 GetValueInBytes() const;
        void SetValueInBytes(qint64 bytes);

        void SetRangeInBytes(qint64 minBytes, qint64 maxBytes);
        void SetSingleStepBytes(qint64 bytes);

    private slots:
        void onDisplayValueChanged(double value);

    private:
        Unit m_unit = Unit::MB;
        qint64 m_valueBytes = 0;
        qint64 m_minBytes = 0;
        qint64 m_maxBytes = 0;
        qint64 m_stepBytes = 0;
        bool m_syncing = false;

        static double bytesToDisplay(qint64 bytes, Unit unit);
        static qint64 displayToBytes(double value, Unit unit);
        void applyPresentation();
};

#endif // MEMORYSPINNER_H
