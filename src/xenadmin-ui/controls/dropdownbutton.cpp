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

#include "dropdownbutton.h"
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>

DropDownButton::DropDownButton(const QString& text, QWidget* parent) : QPushButton(text, parent), m_menu(nullptr), m_ignoreNextClick(false)
{
    // C# equivalent: DropDownButton() constructor (DropDownButton.cs line 42)
    // C# code:
    //   public DropDownButton()
    //   {
    //       InitializeComponent();
    //       base.Image = Images.StaticImages.expanded_triangle;
    //       base.ImageAlign = ContentAlignment.MiddleRight;
    //       DoubleBuffered = true;
    //   }
    
    // Add padding on the right for the dropdown triangle
    this->setStyleSheet(this->styleSheet() + " padding-right: 20px;");
}

DropDownButton::DropDownButton(QWidget* parent) : DropDownButton(QString(), parent)
{
}

DropDownButton::~DropDownButton()
{
    // Menu is owned by this button if set via setMenu
    if (this->m_menu && this->m_menu->parent() == this)
    {
        delete this->m_menu;
    }
}

void DropDownButton::SetMenu(QMenu* menu)
{
    // C# equivalent: ContextMenuStrip property setter
    // (implicitly through OnContextMenuStripChanged, DropDownButton.cs lines 52-74)
    
    // Disconnect old menu
    if (this->m_menu)
    {
        disconnect(this->m_menu, &QMenu::aboutToHide, this, &DropDownButton::onMenuAboutToHide);
        
        // Delete old menu if we own it
        if (this->m_menu->parent() == this)
        {
            delete this->m_menu;
        }
    }
    
    this->m_menu = menu;
    
    // Connect new menu
    if (this->m_menu)
    {
        connect(this->m_menu, &QMenu::aboutToHide, this, &DropDownButton::onMenuAboutToHide);
        
        // C# adds a dummy item if menu is empty so it can still open
        // Qt doesn't need this workaround
    }
}

void DropDownButton::mousePressEvent(QMouseEvent* event)
{
    // C# equivalent: OnClick override (DropDownButton.cs lines 95-104)
    // C# code:
    //   protected override void OnClick(EventArgs e)
    //   {
    //       base.OnClick(e);
    //       if (_contextMenuStrip != null && !_ignoreNextClick)
    //       {
    //           _contextMenuStrip.Show(this, new Point(0, Height), ToolStripDropDownDirection.Default);
    //       }
    //       _ignoreNextClick = false;
    //   }
    
    QPushButton::mousePressEvent(event);
    
    if (this->m_menu && !this->m_ignoreNextClick && event->button() == Qt::LeftButton)
    {
        // Show menu below the button
        QPoint menuPos = this->mapToGlobal(QPoint(0, this->height()));
        this->m_menu->exec(menuPos);
    }
    
    this->m_ignoreNextClick = false;
}

void DropDownButton::onMenuAboutToHide()
{
    // C# equivalent: _contextMenuStrip_Closed event handler
    // (DropDownButton.cs lines 84-90)
    // C# code:
    //   private void _contextMenuStrip_Closed(object sender, ToolStripDropDownClosedEventArgs e)
    //   {
    //       // ensure that if the user clicks on the button when the menu is open, then the menu is closed,
    //       // (not opened again.)
    //       _ignoreNextClick = ClientRectangle.Contains(PointToClient(Cursor.Position));
    //   }
    
    // Check if cursor is still over the button
    QPoint cursorPos = this->mapFromGlobal(QCursor::pos());
    this->m_ignoreNextClick = this->rect().contains(cursorPos);
}

void DropDownButton::paintEvent(QPaintEvent* event)
{
    // First draw the button normally
    QPushButton::paintEvent(event);
    
    // C# equivalent: Image property (shows expanded_triangle)
    // We draw the dropdown triangle manually
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate triangle position (right side of button)
    int triangleSize = 8;
    int rightMargin = 6;
    int x = this->width() - rightMargin - triangleSize;
    int y = (this->height() - triangleSize) / 2;
    
    // Draw down-pointing triangle
    QPolygon triangle;
    triangle << QPoint(x, y)                                    // Top left
             << QPoint(x + triangleSize, y)                     // Top right
             << QPoint(x + triangleSize / 2, y + triangleSize); // Bottom center
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(this->palette().color(QPalette::ButtonText));
    painter.drawPolygon(triangle);
}
