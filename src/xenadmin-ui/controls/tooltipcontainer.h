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

#ifndef TOOLTIPCONTAINER_H
#define TOOLTIPCONTAINER_H

#include <QWidget>

/**
 * @brief Container that shows tooltip when child control is disabled
 * 
 * Qt port of C# XenAdmin.Controls.ToolTipContainer
 * 
 * This widget wraps a single child control and displays a tooltip
 * when the child is disabled and the mouse hovers over it.
 * 
 * C# location: XenAdmin/Controls/ToolTipContainer.cs
 */
class ToolTipContainer : public QWidget
{
    Q_OBJECT

    public:
        explicit ToolTipContainer(QWidget* parent = nullptr);
        ~ToolTipContainer() override;

        /**
         * @brief Set the tooltip text to display when child is disabled
         * @param text Tooltip text
         */
        void SetToolTip(const QString& text);

        /**
         * @brief Remove all tooltips
         */
        void RemoveAll();

        /**
         * @brief If true, prevents the tooltip from appearing
         */
        bool SuppressTooltip;

    protected:
        void childEvent(QChildEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void updateOverlay();

        QWidget* m_childControl;
        QString m_tooltipText;
        QWidget* m_overlayPanel;  // Transparent overlay that captures events when child is disabled
};

#endif // TOOLTIPCONTAINER_H
