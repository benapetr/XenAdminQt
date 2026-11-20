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

#include "foldernavigator.h"
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include "xenlib/folders/foldersmanager.h"

FolderNavigator::FolderNavigator(QWidget* parent) : QWidget(parent), separator_(":/icons/folder_separator.png")
{
    this->setMouseTracking(true);
    this->setMinimumHeight(20);
    this->setMaximumHeight(30);
}

FolderNavigator::~FolderNavigator()
{
}

// C# FolderNavigator.Folder setter
void FolderNavigator::SetFolder(const QString& folder)
{
    this->folder_ = folder;
    this->pathComponents_ = FoldersManager::PointToPath(folder);
    this->hoveredPart_ = -1;
    this->setVisible(!folder.isEmpty());
    this->calculateLayout();
    this->updateGeometry();
    this->update();
}

int FolderNavigator::fullWidth() const
{
    const QFontMetrics fm(this->font());
    int width = LEFT_MARGIN;
    for (int i = 0; i < this->pathComponents_.size(); ++i)
    {
        if (i > 0)
            width += this->separator_.width() + INNER_PADDING;
        width += fm.horizontalAdvance(this->pathComponents_.at(i)) + INNER_PADDING;
    }
    return width - INNER_PADDING;
}

void FolderNavigator::calculateLayout()
{
    this->parts_.clear();
    const int count = this->pathComponents_.size();
    if (count == 0)
        return;

    const QFontMetrics fm(this->font());
    const QString ellipsis(QChar(0x2026));
    const int available = this->width() > 0 ? this->width() : INT_MAX;

    // C# CalcSizeAndTrunc. trunc1: leading components replaced by the ellipsis.
    int width = this->fullWidth();
    int trunc1 = 0;
    for (; width > available && trunc1 < count - 1; ++trunc1)
    {
        if (trunc1 == 0)
            width += fm.horizontalAdvance(ellipsis);
        else
            width -= this->separator_.width() + 2 * INNER_PADDING;
        width -= fm.horizontalAdvance(this->pathComponents_.at(trunc1));
    }

    // Still too wide: the last remaining component loses characters from its end
    QString last = this->pathComponents_.at(count - 1);
    if (width > available)
    {
        const int lastWidth = fm.horizontalAdvance(last);
        last = fm.elidedText(last, Qt::ElideRight, qMax(0, lastWidth - (width - available)));
    }

    // C# DrawSelf
    int x = LEFT_MARGIN;
    const int y = 2;
    const int height = fm.height();
    if (trunc1 > 0)
    {
        Part part;
        part.text = ellipsis;
        part.rect = QRect(x, y, fm.horizontalAdvance(ellipsis), height);
        this->parts_.append(part);
        x += part.rect.width() + INNER_PADDING;
    }
    for (int i = trunc1; i < count; ++i)
    {
        Part part;
        part.separatorBefore = i > 0;
        if (part.separatorBefore)
            x += this->separator_.width() + INNER_PADDING;

        part.text = (i == count - 1) ? last : this->pathComponents_.at(i);
        part.rect = QRect(x, y, fm.horizontalAdvance(part.text), height);
        // C# AllowSearch.AllButLast: each parent opens a search for that folder
        if (i != count - 1)
            part.searchPath = FoldersManager::PathToPoint(this->pathComponents_, i + 1);
        this->parts_.append(part);
        x += part.rect.width() + INNER_PADDING;
    }
}

int FolderNavigator::partAt(const QPoint& pos) const
{
    for (int i = 0; i < this->parts_.size(); ++i)
    {
        if (!this->parts_.at(i).searchPath.isEmpty() && this->parts_.at(i).rect.contains(pos))
            return i;
    }
    return -1;
}

void FolderNavigator::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->calculateLayout();
}

void FolderNavigator::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    const QFontMetrics fm(this->font());
    const QColor linkColor = this->palette().color(QPalette::Link);

    for (int i = 0; i < this->parts_.size(); ++i)
    {
        const Part& part = this->parts_.at(i);
        if (part.separatorBefore)
        {
            const int separatorX = part.rect.x() - INNER_PADDING - this->separator_.width();
            painter.drawPixmap(separatorX, part.rect.y() + IMAGE_OFFSET, this->separator_);
        }

        const bool clickable = !part.searchPath.isEmpty();
        QFont font = this->font();
        font.setUnderline(clickable && i == this->hoveredPart_);
        painter.setFont(font);
        painter.setPen(clickable ? linkColor : this->palette().color(QPalette::WindowText));
        painter.drawText(part.rect.x(), part.rect.y() + fm.ascent(), part.text);
    }
}

void FolderNavigator::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);

    const int part = this->partAt(event->pos());
    if (part == this->hoveredPart_)
        return;

    this->hoveredPart_ = part;
    this->setCursor(part >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    this->update();
}

void FolderNavigator::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);

    if (this->hoveredPart_ >= 0)
    {
        this->hoveredPart_ = -1;
        this->update();
    }
    this->setCursor(Qt::ArrowCursor);
}

// C# FolderListItem.OnMouseClick: Program.MainWindow.SearchForFolder(path)
void FolderNavigator::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);

    if (event->button() != Qt::LeftButton)
        return;

    const int part = this->partAt(event->pos());
    if (part >= 0)
        emit folderClicked(this->parts_.at(part).searchPath);
}

// C# FolderListItem.PreferredSize: the whole path
QSize FolderNavigator::sizeHint() const
{
    return QSize(this->fullWidth(), 24);
}

// Narrower widths truncate the path
QSize FolderNavigator::minimumSizeHint() const
{
    return QSize(0, 20);
}
