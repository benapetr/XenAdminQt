/* Copyright (c) 2025 Petr Bena
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef PROGRESSBARDELEGATE_H
#define PROGRESSBARDELEGATE_H

#include <QStyledItemDelegate>

// C# Equivalent: XenAdmin.XenSearch.BarGraphColumn + Images.GetImageForPercentage()
// C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 249-300
//               xenadmin/XenAdmin/Images.cs lines 244-276
//
// Purpose: Custom item delegate for rendering CPU/Memory usage as progress bars
// in the SearchTabPage table (Overview panel).
//
// C# Implementation:
// - Uses pre-rendered PNG images (usagebar_0.png through usagebar_10.png, 70x8 pixels)
// - 11 images for 0-9%, 10-18%, 19-27%, ... 90-100% ranges
// - GridVerticalArrayItem stacks image above text (centered)
//
// Qt Implementation:
// - Draws progress bar programmatically (no image files needed)
// - Matches C# visual style: blue gradient fill, gray border, white text overlay
// - Uses QPainter for custom rendering in paint()
//
// Usage:
//   m_tableWidget->setItemDelegateForColumn(COL_CPU, new ProgressBarDelegate(this));
//   m_tableWidget->setItemDelegateForColumn(COL_MEMORY, new ProgressBarDelegate(this));

class ProgressBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    public:
        explicit ProgressBarDelegate(QObject* parent = nullptr);
        ~ProgressBarDelegate() override = default;

        // QStyledItemDelegate interface
        void paint(QPainter* painter, const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

        QSize sizeHint(const QStyleOptionViewItem& option,
                       const QModelIndex& index) const override;

    private:
        // Helper: Get percentage value from model (stored in Qt::UserRole)
        int getPercentage(const QModelIndex& index) const;

        // Helper: Get display text from model (e.g., "22% of 8 CPUs")
        QString getText(const QModelIndex& index) const;

        // Helper: Draw the progress bar background and fill
        void drawProgressBar(QPainter* painter, const QRect& barRect, int percent) const;

        // Helper: Draw the text overlay centered on the bar
        void drawText(QPainter* painter, const QRect& textRect, const QString& text,
                      const QColor& textColor) const;

        // Visual constants matching C# usagebar images (70x8 pixels)
        static constexpr int BAR_HEIGHT = 8;
        static constexpr int BAR_WIDTH = 70;
        static constexpr int VERTICAL_MARGIN = 2; // Space above/below bar
};

#endif // PROGRESSBARDELEGATE_H
