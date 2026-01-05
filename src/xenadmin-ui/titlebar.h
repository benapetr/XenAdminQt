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

#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QString>
#include <QIcon>

QT_BEGIN_NAMESPACE
class QLabel;
class QHBoxLayout;
QT_END_NAMESPACE

/**
 * @brief TitleBar widget that displays object icon and name/location
 *
 * This widget shows the currently selected object with an icon and title text.
 * It mimics the "shiny gradient bar" from the C# XenAdmin that shows object
 * information and user login status.
 */
class TitleBar : public QWidget
{
    Q_OBJECT

    public:
        explicit TitleBar(QWidget* parent = nullptr);

        // Set the title information
        void SetTitle(const QString& text, const QIcon& icon = QIcon());
        void SetIcon(const QIcon& icon);
        void SetText(const QString& text);

        // Clear the title
        void Clear();

        // Get current values
        QString GetText() const;
        QIcon GetIcon() const;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        QLabel* m_iconLabel;
        QLabel* m_textLabel;
        QHBoxLayout* m_layout;

        QString m_text;
        QIcon m_icon;
};

#endif // TITLEBAR_H
