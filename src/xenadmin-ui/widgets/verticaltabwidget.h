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

// verticaltabwidget.h - Custom QListWidget for vertical tabs with icons and text
// Matches C# XenAdmin.Controls.VerticalTabs control
#ifndef VERTICALTABWIDGET_H
#define VERTICALTABWIDGET_H

#include <QListWidget>
#include <QIcon>
#include <QString>
#include <QPainter>
#include <QLinearGradient>
#include <QStyledItemDelegate>

// Custom delegate for painting tab items
class VerticalTabDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
        explicit VerticalTabDelegate(QObject* parent = nullptr);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class VerticalTabWidget : public QListWidget
{
    Q_OBJECT

    public:
        explicit VerticalTabWidget(QWidget* parent = nullptr);

        // Add a tab with icon, main text, and subtext
        void addTab(const QIcon& icon, const QString& text, const QString& subText, QWidget* page);

        // Get the widget associated with the selected tab
        QWidget* currentPage() const;

        // Get the widget at the specified index
        QWidget* pageAt(int index) const;

        // Set current page by widget
        void setCurrentPage(QWidget* page);

    private:
        struct TabData
        {
            QIcon icon;
            QString text;
            QString subText;
            QWidget* page;
        };

        QList<TabData> m_tabs;
};

#endif // VERTICALTABWIDGET_H
