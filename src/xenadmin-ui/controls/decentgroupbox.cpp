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

#include "decentgroupbox.h"
#include <QPainter>
#include <QStyleOption>
#include <QStyle>

DecentGroupBox::DecentGroupBox(QWidget* parent) : QGroupBox(parent)
{
    // Set default styling to match C# DecentGroupBox
    // Using stylesheet for consistent cross-platform appearance
    this->setStyleSheet(
        "DecentGroupBox {"
        "    border: 1px solid #A0A0A0;"
        "    border-radius: 2px;"
        "    margin-top: 0.5em;"
        "    padding-top: 0.5em;"
        "}"
        "DecentGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    left: 10px;"
        "    padding: 0 3px;"
        "    background-color: palette(window);"
        "}"
    );
}

DecentGroupBox::DecentGroupBox(const QString& title, QWidget* parent) : DecentGroupBox(parent)
{
    this->setTitle(title);
}

DecentGroupBox::~DecentGroupBox()
{
}

void DecentGroupBox::paintEvent(QPaintEvent* event)
{
    // Use default QGroupBox painting with our stylesheet
    QGroupBox::paintEvent(event);
}
