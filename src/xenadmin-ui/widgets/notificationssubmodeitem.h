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

#ifndef NOTIFICATIONSSUBMODEITEM_H
#define NOTIFICATIONSSUBMODEITEM_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QListWidget>
#include "../widgets/navigationpane.h"

/**
 * @brief Custom delegate for painting notification sub-mode items
 *
 * Matches C# NotificationsSubModeItem + OnDrawItem painting
 * File: xenadmin/XenAdmin/Controls/MainWindowControls/NotificationsView.cs
 */
class NotificationsSubModeItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit NotificationsSubModeItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    static constexpr int IMG_LEFT_MARGIN = 5;
    static constexpr int IMG_RIGHT_MARGIN = 5;
    static constexpr int ITEM_HEIGHT = 40;
};

/**
 * @brief Data structure for notification sub-mode items
 *
 * Matches C# NotificationsSubModeItem class
 */
struct NotificationsSubModeItemData
{
    NavigationPane::NotificationsSubMode subMode;
    int unreadEntries;

    QString getText() const;
    QString getSubText() const;
    QIcon getIcon() const;
};

// Custom role for storing NotificationsSubModeItemData
constexpr int NotificationsSubModeRole = Qt::UserRole + 1;

Q_DECLARE_METATYPE(NotificationsSubModeItemData)

#endif // NOTIFICATIONSSUBMODEITEM_H
