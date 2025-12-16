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

#include "progressbardelegate.h"
#include <QPainter>
#include <QApplication>

// C# Equivalent: XenAdmin.XenSearch.BarGraphColumn rendering
// C# Reference: xenadmin/XenAdmin/XenSearch/Columns.cs lines 249-300
//
// C# uses pre-rendered usagebar PNG images (70x8 pixels) with gradient fills:
// - usagebar_0.png:  0-8% filled
// - usagebar_1.png:  9-17% filled
// - usagebar_5.png:  45-53% filled (50% fill)
// - usagebar_10.png: 90-100% filled (100% fill)
//
// Visual style (analyzed from C# images):
// - Background: Light gray (#E0E0E0)
// - Border: Dark gray (#A0A0A0)
// - Fill: Blue gradient (#3A7CA8 top to #5BA3D0 bottom)
// - Text: White when over filled area, black when over empty area

ProgressBarDelegate::ProgressBarDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ProgressBarDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    // Get data from model
    int percent = getPercentage(index);
    QString text = getText(index);

    // If no valid percentage data, fall back to default rendering
    if (percent < 0)
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    painter->save();

    // Draw selection background if selected
    if (option.state & QStyle::State_Selected)
    {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    // Calculate bar position (centered vertically in cell)
    int barX = option.rect.x() + (option.rect.width() - BAR_WIDTH) / 2;
    int barY = option.rect.y() + VERTICAL_MARGIN;
    QRect barRect(barX, barY, BAR_WIDTH, BAR_HEIGHT);

    // Draw progress bar
    drawProgressBar(painter, barRect, percent);

    // Draw text below the bar (centered)
    QRect textRect(option.rect.x(), barY + BAR_HEIGHT + 2,
                   option.rect.width(), option.rect.height() - BAR_HEIGHT - 4);
    drawText(painter, textRect, text);

    painter->restore();
}

QSize ProgressBarDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    // C# GridVerticalArrayItem stacks 8px bar + text
    // Total height: margins + bar + spacing + text line
    QFontMetrics fm(QApplication::font());
    int textHeight = fm.height();
    int totalHeight = VERTICAL_MARGIN * 2 + BAR_HEIGHT + 2 + textHeight;

    return QSize(BAR_WIDTH + 20, totalHeight); // +20 for horizontal padding
}

int ProgressBarDelegate::getPercentage(const QModelIndex& index) const
{
    // Percentage stored in Qt::UserRole by SearchTabPage
    QVariant data = index.data(Qt::UserRole);
    if (!data.isValid())
        return -1;

    bool ok;
    int percent = data.toInt(&ok);
    if (!ok)
        return -1;

    return qBound(0, percent, 100);
}

QString ProgressBarDelegate::getText(const QModelIndex& index) const
{
    // Display text stored in Qt::DisplayRole (e.g., "22% of 8 CPUs")
    return index.data(Qt::DisplayRole).toString();
}

void ProgressBarDelegate::drawProgressBar(QPainter* painter, const QRect& barRect,
                                          int percent) const
{
    // C# usagebar image analysis:
    // - Background: RGB(224, 224, 224) = #E0E0E0
    // - Border: RGB(160, 160, 160) = #A0A0A0
    // - Fill gradient: RGB(58, 124, 168) to RGB(91, 163, 208) = #3A7CA8 to #5BA3D0

    painter->setRenderHint(QPainter::Antialiasing, false); // Sharp edges like C#

    // Draw background
    painter->fillRect(barRect, QColor(224, 224, 224));

    // Draw border
    painter->setPen(QColor(160, 160, 160));
    painter->drawRect(barRect.adjusted(0, 0, -1, -1)); // -1 to fit inside rect

    // Draw filled portion with gradient
    if (percent > 0)
    {
        int fillWidth = (barRect.width() - 2) * percent / 100; // -2 for border
        QRect fillRect(barRect.x() + 1, barRect.y() + 1,
                       fillWidth, barRect.height() - 2);

        QLinearGradient gradient(fillRect.topLeft(), fillRect.bottomLeft());
        gradient.setColorAt(0.0, QColor(58, 124, 168)); // #3A7CA8 - darker blue
        gradient.setColorAt(1.0, QColor(91, 163, 208)); // #5BA3D0 - lighter blue

        painter->fillRect(fillRect, gradient);
    }
}

void ProgressBarDelegate::drawText(QPainter* painter, const QRect& textRect,
                                   const QString& text) const
{
    // C# uses QueryPanel.TextBrush (black) and Program.DefaultFont
    // Text is centered below the bar

    painter->setPen(Qt::black);
    painter->setFont(QApplication::font());
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
}
