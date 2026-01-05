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

#include "titlebar.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    // Set minimum and fixed height
    this->setMinimumHeight(40);
    this->setMaximumHeight(40);

    // Create layout
    this->m_layout = new QHBoxLayout(this);
    this->m_layout->setContentsMargins(10, 0, 10, 0);
    this->m_layout->setSpacing(8);

    // Create icon label
    this->m_iconLabel = new QLabel(this);
    this->m_iconLabel->setFixedSize(24, 24);
    this->m_iconLabel->setScaledContents(true);
    this->m_layout->addWidget(this->m_iconLabel);

    // Create text label
    this->m_textLabel = new QLabel(this);
    QFont font = this->m_textLabel->font();
    font.setPointSize(font.pointSize() + 2);
    font.setBold(true);
    this->m_textLabel->setFont(font);
    this->m_textLabel->setStyleSheet("color: white;");
    this->m_layout->addWidget(this->m_textLabel, 1); // Stretch factor of 1

    // Add stretch to push everything to the left
    this->m_layout->addStretch();

    this->setLayout(this->m_layout);
}

void TitleBar::SetTitle(const QString& text, const QIcon& icon)
{
    this->m_text = text;
    this->m_icon = icon;

    this->m_textLabel->setText(text);

    if (!icon.isNull())
    {
        this->m_iconLabel->setPixmap(icon.pixmap(24, 24));
        this->m_iconLabel->show();
    } else
    {
        this->m_iconLabel->hide();
    }

    this->update();
}

void TitleBar::SetIcon(const QIcon& icon)
{
    this->m_icon = icon;

    if (!icon.isNull())
    {
        this->m_iconLabel->setPixmap(icon.pixmap(24, 24));
        this->m_iconLabel->show();
    } else
    {
        this->m_iconLabel->hide();
    }
}

void TitleBar::SetText(const QString& text)
{
    this->m_text = text;
    this->m_textLabel->setText(text);
}

void TitleBar::Clear()
{
    this->m_text.clear();
    this->m_icon = QIcon();
    this->m_textLabel->clear();
    this->m_iconLabel->clear();
    this->m_iconLabel->hide();
    this->update();
}

QString TitleBar::GetText() const
{
    return this->m_text;
}

QIcon TitleBar::GetIcon() const
{
    return this->m_icon;
}

void TitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Create gradient background (blue-ish gradient similar to C# version)
    QLinearGradient gradient(0, 0, 0, this->height());
    gradient.setColorAt(0.0, QColor(63, 125, 186)); // Lighter blue at top
    gradient.setColorAt(1.0, QColor(41, 84, 124));  // Darker blue at bottom

    painter.fillRect(this->rect(), gradient);

    // Draw a subtle border at the bottom
    painter.setPen(QColor(30, 60, 90));
    painter.drawLine(0, this->height() - 1, this->width(), this->height() - 1);
}
