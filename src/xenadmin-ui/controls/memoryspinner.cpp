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

#include "memoryspinner.h"
#include "globals.h"
#include <QtMath>
#include <QSignalBlocker>

MemorySpinner::MemorySpinner(QWidget* parent) : QDoubleSpinBox(parent)
{
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MemorySpinner::onDisplayValueChanged);
    this->applyPresentation();
}

MemorySpinner::Unit MemorySpinner::GetUnit() const
{
    return this->m_unit;
}

void MemorySpinner::SetUnit(Unit unit)
{
    if (this->m_unit == unit)
        return;

    this->m_unit = unit;
    this->applyPresentation();
}

qint64 MemorySpinner::GetValueInBytes() const
{
    return this->m_valueBytes;
}

void MemorySpinner::SetValueInBytes(qint64 bytes)
{
    if (this->m_maxBytes < bytes)
        this->m_maxBytes = bytes;
    if (this->m_minBytes > bytes)
        this->m_minBytes = bytes;

    this->m_valueBytes = bytes;

    const double minDisplay = bytesToDisplay(this->m_minBytes, this->m_unit);
    const double maxDisplay = bytesToDisplay(this->m_maxBytes, this->m_unit);
    const double displayValue = bytesToDisplay(bytes, this->m_unit);
    QSignalBlocker blocker(this);
    this->m_syncing = true;
    this->setRange(minDisplay, maxDisplay);
    this->setValue(displayValue);
    this->m_syncing = false;
}

void MemorySpinner::SetRangeInBytes(qint64 minBytes, qint64 maxBytes)
{
    this->m_minBytes = minBytes;
    this->m_maxBytes = maxBytes;
    this->applyPresentation();
}

void MemorySpinner::SetSingleStepBytes(qint64 bytes)
{
    this->m_stepBytes = bytes;
    this->applyPresentation();
}

void MemorySpinner::onDisplayValueChanged(double value)
{
    if (this->m_syncing)
        return;

    this->m_valueBytes = displayToBytes(value, this->m_unit);
}

double MemorySpinner::bytesToDisplay(qint64 bytes, Unit unit)
{
    if (unit == Unit::GB)
        return static_cast<double>(bytes) / static_cast<double>(BINARY_GIGA);

    return static_cast<double>(bytes) / static_cast<double>(BINARY_MEGA);
}

qint64 MemorySpinner::displayToBytes(double value, Unit unit)
{
    if (unit == Unit::GB)
        return static_cast<qint64>(qRound64(value * static_cast<double>(BINARY_GIGA)));

    return static_cast<qint64>(qRound64(value * static_cast<double>(BINARY_MEGA)));
}

void MemorySpinner::applyPresentation()
{
    const int decimals = this->m_unit == Unit::GB ? 1 : 0;
    const QString suffix = this->m_unit == Unit::GB ? tr(" GB") : tr(" MB");

    const double minDisplay = bytesToDisplay(this->m_minBytes, this->m_unit);
    const double maxDisplay = bytesToDisplay(this->m_maxBytes, this->m_unit);
    const double stepDisplay = this->m_stepBytes > 0 ? bytesToDisplay(this->m_stepBytes, this->m_unit) : this->singleStep();
    const double valueDisplay = bytesToDisplay(this->m_valueBytes, this->m_unit);

    QSignalBlocker blocker(this);
    this->m_syncing = true;
    this->setDecimals(decimals);
    this->setSuffix(suffix);
    this->setRange(minDisplay, maxDisplay);
    this->setSingleStep(stepDisplay);
    this->setValue(valueDisplay);
    this->m_syncing = false;
}
