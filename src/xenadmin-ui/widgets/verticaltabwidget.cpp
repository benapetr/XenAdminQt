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

// verticaltabwidget.cpp - Custom QListWidget for vertical tabs with icons and text
#include "verticaltabwidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <QStyleOptionViewItem>
#include <QFontMetrics>
#include <QWidget>

// VerticalTabDelegate implementation
VerticalTabDelegate::VerticalTabDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
}

void VerticalTabDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect rect = option.rect;
    bool isSelected = (option.state & QStyle::State_Selected);
    const QPalette& palette = option.palette;

    // Draw background
    if (isSelected)
    {
        // Selected item - theme-derived gradient background
        QColor highlight = palette.highlight().color();
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0, highlight.lighter(115));
        gradient.setColorAt(1, highlight.darker(115));
        painter->fillRect(rect, gradient);
    } else
    {
        // Not selected - use palette base color
        painter->fillRect(rect, palette.base());
    }

    // Get data from model
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QString text = index.data(Qt::DisplayRole).toString();
    QString subText = index.data(Qt::UserRole + 1).toString();

    // Draw icon (32x32, centered in left 32px)
    if (!icon.isNull())
    {
        QPixmap pixmap = icon.pixmap(32, 32);
        int iconX = rect.x() + (32 - pixmap.width()) / 2;
        int iconY = rect.y() + (rect.height() - pixmap.height()) / 2;
        painter->drawPixmap(iconX, iconY, pixmap);
    }

    // Text colors
    QColor textColor = isSelected ? palette.highlightedText().color() : palette.text().color();
    QColor subTextColor = isSelected ? palette.highlightedText().color() : palette.placeholderText().color();
    if (!subTextColor.isValid())
    {
        subTextColor = textColor;
        subTextColor.setAlphaF(0.6);
    }

    // Draw main text (top half)
    QRect topRect(rect.x() + 36, rect.y() + 2, rect.width() - 38, rect.height() / 2 - 2);
    painter->setPen(textColor);
    QFont mainFont = option.font;
    mainFont.setPointSize(9);
    mainFont.setBold(false);
    painter->setFont(mainFont);
    painter->drawText(topRect, Qt::AlignLeft | Qt::AlignBottom,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, topRect.width()));

    // Draw subtext (bottom half)
    QRect bottomRect(rect.x() + 36, rect.y() + rect.height() / 2, rect.width() - 38, rect.height() / 2 - 2);
    painter->setPen(subTextColor);
    QFont smallerFont = option.font;
    smallerFont.setPointSize(8);
    painter->setFont(smallerFont);
    painter->drawText(bottomRect, Qt::AlignLeft | Qt::AlignTop,
                      painter->fontMetrics().elidedText(subText, Qt::ElideRight, bottomRect.width()));

    painter->restore();
}

QSize VerticalTabDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(180, 40); // Match C# ItemHeight
}

// VerticalTabWidget implementation
VerticalTabWidget::VerticalTabWidget(QWidget* parent) : QListWidget(parent)
{
    // Set delegate for custom painting
    this->setItemDelegate(new VerticalTabDelegate(this));

    // Matches C# VerticalTabs properties
    this->setFixedWidth(180); // Match C# vertical tab width
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->setSpacing(0);
    this->setUniformItemSizes(true);

    // Match C# item height (40 pixels)
    this->setIconSize(QSize(32, 32));

    // Style to remove default selection appearance (delegate handles it)
    this->setStyleSheet(R"(
        QListWidget {
            border: none;
            outline: none;
            show-decoration-selected: 0;
        }
        QListWidget::item {
            border: none;
            padding: 0px;
        }
        QListWidget::item:selected {
            background: transparent;
        }
        QListWidget::item:hover {
            background: transparent;
        }
    )");
}

void VerticalTabWidget::AddTab(const QIcon& icon, const QString& text, const QString& subText, QWidget* page)
{
    TabData data;
    data.icon = icon;
    data.text = text;
    data.subText = subText;
    data.page = page;
    this->m_tabs.append(data);

    // Add item to list
    QListWidgetItem* item = new QListWidgetItem();
    item->setSizeHint(QSize(180, 40));              // Match C# ItemHeight
    item->setData(Qt::UserRole, this->m_tabs.size() - 1); // Store index for page lookup
    item->setData(Qt::DisplayRole, text);           // Main text
    item->setData(Qt::UserRole + 1, subText);       // Subtext
    item->setData(Qt::DecorationRole, icon);        // Icon

    this->addItem(item);

    // Hide page initially
    if (page)
    {
        page->hide();
    }
}

void VerticalTabWidget::UpdateTabText(QWidget* page, const QString& text)
{
    if (!page)
        return;

    for (int i = 0; i < this->m_tabs.size(); ++i)
    {
        if (this->m_tabs[i].page != page)
            continue;

        if (this->m_tabs[i].text == text)
            return;

        this->m_tabs[i].text = text;
        QListWidgetItem* item = this->item(i);
        if (item)
            item->setData(Qt::DisplayRole, text);

        this->viewport()->update();
        return;
    }
}

void VerticalTabWidget::UpdateTabSubText(QWidget* page, const QString& subText)
{
    if (!page)
        return;

    for (int i = 0; i < this->m_tabs.size(); ++i)
    {
        if (this->m_tabs[i].page != page)
            continue;

        if (this->m_tabs[i].subText == subText)
            return;

        this->m_tabs[i].subText = subText;
        QListWidgetItem* item = this->item(i);
        if (item)
            item->setData(Qt::UserRole + 1, subText);

        this->viewport()->update();
        return;
    }
}

void VerticalTabWidget::ClearTabs()
{
    this->m_tabs.clear();
    this->clear();
}

QWidget* VerticalTabWidget::GetCurrentPage() const
{
    int index = this->currentRow();
    if (index >= 0 && index < this->m_tabs.size())
    {
        return this->m_tabs[index].page;
    }
    return nullptr;
}

QWidget* VerticalTabWidget::PageAt(int index) const
{
    if (index >= 0 && index < this->m_tabs.size())
    {
        return this->m_tabs[index].page;
    }
    return nullptr;
}

void VerticalTabWidget::SetCurrentPage(QWidget* page)
{
    for (int i = 0; i < this->m_tabs.size(); ++i)
    {
        if (this->m_tabs[i].page == page)
        {
            this->setCurrentRow(i);
            break;
        }
    }
}
