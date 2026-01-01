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

#include "navigationbuttons.h"
#include <QPainter>
#include <QStyleOptionButton>

// ============================================================================
// NavigationButtonBig
// ============================================================================

NavigationButtonBig::NavigationButtonBig(QWidget* parent)
    : QToolButton(parent), m_pairedItem(nullptr)
{
    this->setCheckable(true);
    this->setAutoExclusive(true); // Radio button behavior
    this->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    this->setMinimumHeight(40);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    connect(this, &QToolButton::toggled, this, [this](bool checked) {
        if (checked && this->m_pairedItem)
        {
            // Sync paired item without triggering signals
            this->m_pairedItem->setChecked(true);
        }
        if (checked)
        {
            emit this->navigationViewChanged();
        }
    });
}

void NavigationButtonBig::setChecked(bool checked)
{
    QToolButton::setChecked(checked);
}

// ============================================================================
// NavigationButtonSmall
// ============================================================================

NavigationButtonSmall::NavigationButtonSmall(QWidget* parent)
    : QToolButton(parent), m_pairedItem(nullptr)
{
    this->setCheckable(true);
    this->setAutoExclusive(true); // Radio button behavior
    this->setToolButtonStyle(Qt::ToolButtonIconOnly);
    this->setIconSize(QSize(16, 16));
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(this, &QToolButton::toggled, this, [this](bool checked) {
        if (checked && this->m_pairedItem)
        {
            // Sync paired item without triggering signals
            this->m_pairedItem->setChecked(true);
        }
        if (checked)
        {
            emit this->navigationViewChanged();
        }
    });
}

void NavigationButtonSmall::setChecked(bool checked)
{
    QToolButton::setChecked(checked);
}

// ============================================================================
// NavigationDropDownButtonBig
// ============================================================================

NavigationDropDownButtonBig::NavigationDropDownButtonBig(QWidget* parent)
    : NavigationButtonBig(parent), m_menu(nullptr)
{
    this->m_menu = new QMenu(this);
    this->setMenu(this->m_menu);
    this->setPopupMode(QToolButton::MenuButtonPopup); // Split button with dropdown arrow
}

void NavigationDropDownButtonBig::setItemList(const QList<QAction*>& items)
{
    this->m_menu->clear();
    for (QAction* action : items)
    {
        this->m_menu->addAction(action);
    }
} 

// ============================================================================
// NavigationDropDownButtonSmall
// ============================================================================

NavigationDropDownButtonSmall::NavigationDropDownButtonSmall(QWidget* parent)
    : NavigationButtonSmall(parent), m_menu(nullptr)
{
    this->m_menu = new QMenu(this);
    this->setMenu(this->m_menu);
    this->setPopupMode(QToolButton::InstantPopup); // Show menu immediately on click
}

void NavigationDropDownButtonSmall::setItemList(const QList<QAction*>& items)
{
    this->m_menu->clear();
    for (QAction* action : items)
    {
        this->m_menu->addAction(action);
    }
}

// ============================================================================
// NotificationButtonBig
// ============================================================================

NotificationButtonBig::NotificationButtonBig(QWidget* parent)
    : NavigationButtonBig(parent), m_unreadCount(0)
{
}

void NotificationButtonBig::setUnreadEntries(int count)
{
    if (this->m_unreadCount != count)
    {
        this->m_unreadCount = count;
        this->update(); // Repaint to show badge
    }
}

void NotificationButtonBig::paintEvent(QPaintEvent* event)
{
    // Call base class paint
    NavigationButtonBig::paintEvent(event);

    // Draw unread count badge if > 0
    if (this->m_unreadCount > 0)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw red badge in top-right corner
        int badgeSize = 18;
        int badgeX = this->width() - badgeSize - 4;
        int badgeY = 4;

        painter.setBrush(QColor(200, 0, 0));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(badgeX, badgeY, badgeSize, badgeSize);

        // Draw count text
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(8);
        font.setBold(true);
        painter.setFont(font);

        QString countText = this->m_unreadCount > 99 ? "99+" : QString::number(this->m_unreadCount);
        painter.drawText(QRect(badgeX, badgeY, badgeSize, badgeSize),
                         Qt::AlignCenter, countText);
    }
}

// ============================================================================
// NotificationButtonSmall
// ============================================================================

NotificationButtonSmall::NotificationButtonSmall(QWidget* parent)
    : NavigationButtonSmall(parent), m_unreadCount(0)
{
}

void NotificationButtonSmall::setUnreadEntries(int count)
{
    if (this->m_unreadCount != count)
    {
        this->m_unreadCount = count;
        this->update(); // Repaint to show badge
    }
}

void NotificationButtonSmall::paintEvent(QPaintEvent* event)
{
    // Call base class paint
    NavigationButtonSmall::paintEvent(event);

    // Draw unread count badge if > 0
    if (this->m_unreadCount > 0)
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw red badge in top-right corner (smaller for small button)
        int badgeSize = 12;
        int badgeX = this->width() - badgeSize - 2;
        int badgeY = 2;

        painter.setBrush(QColor(200, 0, 0));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(badgeX, badgeY, badgeSize, badgeSize);

        // Draw count text
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSize(7);
        font.setBold(true);
        painter.setFont(font);

        QString countText = this->m_unreadCount > 99 ? "99+" : QString::number(this->m_unreadCount);
        painter.drawText(QRect(badgeX, badgeY, badgeSize, badgeSize),
                         Qt::AlignCenter, countText);
    }
}
