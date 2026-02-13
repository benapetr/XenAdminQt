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

#include "notificationssubmodeitem.h"
#include <QApplication>

NotificationsSubModeItemDelegate::NotificationsSubModeItemDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
}

void NotificationsSubModeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Get item data
    QVariant data = index.data(NotificationsSubModeRole);
    if (!data.canConvert<NotificationsSubModeItemData>())
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    NotificationsSubModeItemData itemData = data.value<NotificationsSubModeItemData>();

    // Draw background (selected/hovered)
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    // Get icon
    QIcon icon = itemData.getIcon();
    QSize iconSize(16, 16);

    // Calculate positions
    int itemHeight = option.rect.height();
    int imgY = option.rect.top() + (itemHeight - iconSize.height()) / 2;

    // Draw icon
    icon.paint(painter, option.rect.left() + IMG_LEFT_MARGIN, imgY, iconSize.width(), iconSize.height());

    // Get text
    QString text = itemData.getText();
    QString subText = itemData.getSubText();

    // Determine font style (bold for Events with unread entries)
    QFont font = option.font;
    if (itemData.subMode == NavigationPane::Events && itemData.unreadEntries > 0)
        font.setBold(true);

    // Calculate text rect
    int textLeft = option.rect.left() + IMG_LEFT_MARGIN + iconSize.width() + IMG_RIGHT_MARGIN;
    QRect textRect(textLeft, option.rect.top(), option.rect.right() - textLeft, itemHeight);

    // Combine text with subtext if present
    QString fullText = text;
    if (!subText.isEmpty())
        fullText += "\n" + subText;

    // Draw text
    painter->setFont(font);
    painter->setPen(opt.palette.color(opt.state & QStyle::State_Selected
                                          ? QPalette::HighlightedText
                                          : QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, fullText);
}

QSize NotificationsSubModeItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(-1, ITEM_HEIGHT);
}

// NotificationsSubModeItemData implementation

QString NotificationsSubModeItemData::getText() const
{
    // Matches C# Messages.NOTIFICATIONS_SUBMODE_* strings
    switch (subMode)
    {
        case NavigationPane::Alerts:
            if (unreadEntries == 0)
                return QObject::tr("Alerts");
            else
                return QObject::tr("Alerts (%1)").arg(unreadEntries);

        case NavigationPane::Events:
            if (unreadEntries == 0)
                return QObject::tr("Events");
            else if (unreadEntries == 1)
                return QObject::tr("Events (1 error)");
            else
                return QObject::tr("Events (%1 errors)").arg(unreadEntries);

        case NavigationPane::Updates:
            if (unreadEntries == 0)
                return QObject::tr("Updates");
            else
                return QObject::tr("Updates (%1)").arg(unreadEntries);

        default:
            return QString();
    }
}

QString NotificationsSubModeItemData::getSubText() const
{
    // For now, return empty (Updates subtext would show product version info)
    return QString();
}

QIcon NotificationsSubModeItemData::getIcon() const
{
    // TODO: Replace with actual icon paths once icons are available
    switch (subMode)
    {
        case NavigationPane::Alerts:
            return QIcon::fromTheme("dialog-warning");

        case NavigationPane::Events:
            if (unreadEntries == 0)
                return QIcon::fromTheme("view-calendar");
            else
                return QIcon::fromTheme("dialog-error");

        case NavigationPane::Updates:
            return QIcon::fromTheme("system-software-update");

        default:
            return QIcon();
    }
}
