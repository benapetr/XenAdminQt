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

#include "tooltipcontainer.h"
#include <QVBoxLayout>
#include <QChildEvent>
#include <QEvent>
#include <QToolTip>
#include <QHelpEvent>
#include <QCursor>
#include <QApplication>

ToolTipContainer::ToolTipContainer(QWidget* parent)
    : QWidget(parent),
      SuppressTooltip(false),
      m_childControl(nullptr),
      m_overlayPanel(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Create transparent overlay panel
    this->m_overlayPanel = new QWidget(this);
    this->m_overlayPanel->setObjectName("overlayPanel");
    this->m_overlayPanel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    this->m_overlayPanel->setStyleSheet("background: transparent;");
    this->m_overlayPanel->installEventFilter(this);
    this->m_overlayPanel->hide();
    
    layout->addWidget(this->m_overlayPanel);
}

ToolTipContainer::~ToolTipContainer()
{
}

void ToolTipContainer::SetToolTip(const QString& text)
{
    this->m_tooltipText = text;
    if (this->m_overlayPanel)
    {
        this->m_overlayPanel->setToolTip(text);
    }
}

void ToolTipContainer::RemoveAll()
{
    this->m_tooltipText.clear();
    if (this->m_overlayPanel)
    {
        this->m_overlayPanel->setToolTip(QString());
    }
}

void ToolTipContainer::childEvent(QChildEvent* event)
{
    QWidget::childEvent(event);

    if (!this->m_overlayPanel)
        return;
    
    if (event->type() == QEvent::ChildAdded)
    {
        QObject* child = event->child();
        QWidget* childWidget = qobject_cast<QWidget*>(child);
        
        if (childWidget && childWidget != this->m_overlayPanel)
        {
            // Store reference to child control
            this->m_childControl = childWidget;
            
            // Install event filter to detect enable/disable changes
            childWidget->installEventFilter(this);
            
            // Ensure child fills container
            if (this->layout())
            {
                this->layout()->removeWidget(this->m_overlayPanel);
                this->layout()->addWidget(childWidget);
                this->layout()->addWidget(this->m_overlayPanel);
            }
            
            // Raise overlay to front
            this->m_overlayPanel->raise();
            
            // Update overlay visibility based on child enabled state
            this->updateOverlay();
        }
    }
}

bool ToolTipContainer::eventFilter(QObject* watched, QEvent* event)
{
    // Handle enable/disable changes on child control
    if (watched == this->m_childControl)
    {
        if (event->type() == QEvent::EnabledChange)
        {
            this->updateOverlay();
        }
    }
    
    // Handle tooltip events on overlay panel
    if (watched == this->m_overlayPanel && event->type() == QEvent::ToolTip)
    {
        if (this->SuppressTooltip)
        {
            return true; // Suppress tooltip
        }
        
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        
        // Only show tooltip if mouse is actually over the overlay area
        QPoint localPos = this->m_overlayPanel->mapFromGlobal(QCursor::pos());
        if (this->m_overlayPanel->rect().contains(localPos))
        {
            if (!this->m_tooltipText.isEmpty())
            {
                QToolTip::showText(helpEvent->globalPos(), this->m_tooltipText, this->m_overlayPanel);
            }
            return true;
        } else
        {
            return true; // Cancel tooltip if mouse not over area
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

void ToolTipContainer::updateOverlay()
{
    if (!this->m_childControl || !this->m_overlayPanel)
        return;
    
    // Show overlay when child is disabled, hide when enabled
    bool childDisabled = !this->m_childControl->isEnabled();
    
    if (childDisabled)
    {
        // Position overlay over the child control
        this->m_overlayPanel->setGeometry(this->m_childControl->geometry());
        this->m_overlayPanel->show();
        this->m_overlayPanel->raise();
    } else
    {
        this->m_overlayPanel->hide();
    }
}
