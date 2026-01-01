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

FolderNavigator::FolderNavigator(QWidget* parent)
    : QWidget(parent),
      folder_(""),
      pathComponents_(),
      componentRects_(),
      hoveredComponent_(-1)
{
    // C# equivalent: FolderNavigator() constructor (FolderNavigator.cs line 39)
    // C# code:
    //   public FolderNavigator()
    //   {
    //       InitializeComponent();
    //   }
    
    this->setMouseTracking(true);  // Enable mouse move events
    this->setMinimumHeight(20);
    this->setMaximumHeight(30);
}

FolderNavigator::~FolderNavigator()
{
    // Qt manages child widget cleanup automatically
}

void FolderNavigator::SetFolder(const QString& folder)
{
    // C# equivalent: Folder property setter (FolderNavigator.cs lines 44-57)
    // C# code:
    //   public string Folder
    //   {
    //       set
    //       {
    //           if (string.IsNullOrEmpty(value))
    //               this.Visible = false;
    //           else
    //           {
    //               folderListItem = new FolderListItem(value, FolderListItem.AllowSearch.AllButLast, false);
    //               folderListItem.Parent = this;
    //               folderListItem.MaxWidth = this.Parent.Width - this.Left;
    //               this.Width = folderListItem.PreferredSize.Width;
    //               this.Visible = true;
    //               this.Invalidate();
    //           }
    //       }
    //   }
    
    this->folder_ = folder;
    
    if (folder.isEmpty())
    {
        this->setVisible(false);
    }
    else
    {
        this->pathComponents_ = this->parsePath(folder);
        this->calculateLayout();
        this->setVisible(true);
        this->update();
    }
}

QStringList FolderNavigator::parsePath(const QString& path) const
{
    // C# equivalent: Folders.PointToPath() (FolderListItem.cs line 116)
    // Simplified implementation - just split by '/'
    // In C# this involves more complex folder reference resolution
    
    if (path.isEmpty())
        return QStringList();
    
    // Remove "OpaqueRef:" prefix if present
    QString cleanPath = path;
    if (cleanPath.startsWith("OpaqueRef:"))
        cleanPath = cleanPath.mid(10);
    
    return cleanPath.split('/', Qt::SkipEmptyParts);
}

QString FolderNavigator::buildPath(const QStringList& components, int index) const
{
    // C# equivalent: Folders.PathToPoint() (FolderListItem.cs line 153)
    // Build path from components up to (and including) index
    
    if (index < 0 || index >= components.size())
        return QString();
    
    QStringList subList = components.mid(0, index + 1);
    return "OpaqueRef:" + subList.join('/');
}

void FolderNavigator::calculateLayout()
{
    // C# equivalent: CalcSizeAndTrunc() and DrawSelf() in FolderListItem.cs
    // Calculate positions of each breadcrumb component
    // TODO: Implement advanced truncation logic for long paths (ellipsization)
    // TODO: Add "Change" button support for folder selection dialog
    
    this->componentRects_.clear();
    
    if (this->pathComponents_.isEmpty())
        return;
    
    QFontMetrics fm(this->font());
    int x = 3;  // Left margin
    int y = 2;  // Top margin
    
    for (int i = 0; i < this->pathComponents_.size(); ++i)
    {
        QString component = this->pathComponents_[i];
        
        // Add separator before component (except first)
        if (i > 0)
        {
            x += 8 + INNER_PADDING;  // Separator width + padding
        }
        
        // Measure component text
        QRect textRect = fm.boundingRect(component);
        int width = textRect.width();
        int height = textRect.height();
        
        // Store clickable rect for this component
        // All components except the last are clickable
        if (i < this->pathComponents_.size() - 1)
        {
            this->componentRects_.append(QRect(x, y, width, height));
        }
        else
        {
            // Last component is not clickable, but still need rect for layout
            this->componentRects_.append(QRect(x, y, width, height));
        }
        
        x += width + INNER_PADDING;
    }
    
    // Set widget width to fit all components
    this->setMinimumWidth(x + 10);
}

int FolderNavigator::componentAt(const QPoint& pos) const
{
    // Find which component the mouse is over
    for (int i = 0; i < this->componentRects_.size() - 1; ++i)  // Exclude last (non-clickable)
    {
        if (this->componentRects_[i].contains(pos))
            return i;
    }
    return -1;
}

void FolderNavigator::paintEvent(QPaintEvent* event)
{
    // C# equivalent: OnPaint() override (FolderNavigator.cs lines 59-64)
    // C# code:
    //   protected override void OnPaint(PaintEventArgs e)
    //   {
    //       base.OnPaint(e);
    //       if (folderListItem != null)
    //           folderListItem.DrawSelf(e.Graphics, new Rectangle(0, 2, Width, Height), false);
    //   }
    
    QWidget::paintEvent(event);
    
    if (this->pathComponents_.isEmpty())
        return;
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QFontMetrics fm(this->font());
    
    for (int i = 0; i < this->pathComponents_.size(); ++i)
    {
        const QRect& rect = this->componentRects_[i];
        QString component = this->pathComponents_[i];
        
        // Draw separator before component (except first)
        if (i > 0)
        {
            int sepX = rect.x() - INNER_PADDING - 4;
            int sepY = rect.y() + IMAGE_OFFSET;
            painter.drawText(sepX, sepY + fm.ascent(), ">");
        }
        
        // Determine text color
        // All components except last are clickable and shown in blue
        // Last component is shown in default color
        QColor textColor;
        if (i < this->pathComponents_.size() - 1)
        {
            // Clickable component
            if (i == this->hoveredComponent_)
            {
                textColor = QColor(0, 0, 200);  // Darker blue when hovered
                QFont underlineFont = this->font();
                underlineFont.setUnderline(true);
                painter.setFont(underlineFont);
            }
            else
            {
                textColor = QColor(0, 0, 255);  // Blue for clickable
            }
        }
        else
        {
            // Last component (current location, not clickable)
            textColor = this->palette().color(QPalette::WindowText);
        }
        
        painter.setPen(textColor);
        painter.drawText(rect.x(), rect.y() + fm.ascent(), component);
        
        // Reset font if we set underline
        if (painter.font().underline())
        {
            painter.setFont(this->font());
        }
    }
}

void FolderNavigator::mouseMoveEvent(QMouseEvent* event)
{
    // C# equivalent: OnMouseMove() override (FolderNavigator.cs lines 66-71)
    // C# code:
    //   protected override void OnMouseMove(MouseEventArgs e)
    //   {
    //       base.OnMouseMove(e);
    //       if (folderListItem != null)
    //           folderListItem.OnMouseMove(e.Location);
    //   }
    
    QWidget::mouseMoveEvent(event);
    
    int component = this->componentAt(event->pos());
    
    if (component != this->hoveredComponent_)
    {
        this->hoveredComponent_ = component;
        this->update();
        
        // Change cursor to hand when over clickable component
        if (component >= 0)
            this->setCursor(Qt::PointingHandCursor);
        else
            this->setCursor(Qt::ArrowCursor);
    }
}

void FolderNavigator::leaveEvent(QEvent* event)
{
    // C# equivalent: OnMouseLeave() override (FolderNavigator.cs lines 73-78)
    // C# code:
    //   protected override void OnMouseLeave(EventArgs e)
    //   {
    //       base.OnMouseLeave(e);
    //       if (folderListItem != null)
    //           folderListItem.OnMouseLeave();
    //   }
    
    QWidget::leaveEvent(event);
    
    if (this->hoveredComponent_ >= 0)
    {
        this->hoveredComponent_ = -1;
        this->update();
    }
    
    this->setCursor(Qt::ArrowCursor);
}

void FolderNavigator::mousePressEvent(QMouseEvent* event)
{
    // C# equivalent: OnMouseClick() override (FolderNavigator.cs lines 80-85)
    // C# code:
    //   protected override void OnMouseClick(MouseEventArgs e)
    //   {
    //       base.OnMouseClick(e);
    //       if (folderListItem != null)
    //           folderListItem.OnMouseClick(e, e.Location);
    //   }
    // 
    // Which calls Program.MainWindow.SearchForFolder(control.tag)
    // (FolderListItem.cs line 317)
    
    QWidget::mousePressEvent(event);
    
    if (event->button() != Qt::LeftButton)
        return;
    
    int component = this->componentAt(event->pos());
    if (component >= 0)
    {
        QString folderPath = this->buildPath(this->pathComponents_, component);
        emit folderClicked(folderPath);
    }
}

QSize FolderNavigator::sizeHint() const
{
    // C# equivalent: PreferredSize property (FolderListItem.cs lines 178-184)
    
    if (this->componentRects_.isEmpty())
        return QSize(100, 30);
    
    // Calculate total width needed
    int width = 0;
    if (!this->componentRects_.isEmpty())
    {
        const QRect& lastRect = this->componentRects_.last();
        width = lastRect.right() + 10;  // Add right margin
    }
    
    return QSize(width, 30);
}
