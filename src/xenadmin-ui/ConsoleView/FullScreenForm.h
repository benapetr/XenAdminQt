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

#ifndef FULLSCREENFORM_H
#define FULLSCREENFORM_H

#include <QWidget>

class QLabel;
class QToolButton;
class QPropertyAnimation;
class QVBoxLayout;

/**
 * @brief Bar at the top of the full screen console (C# Controls/ConsoleTab/ConnectionBar)
 *
 * Shows the console name, a pin button and an exit button. Unless pinned it slides up
 * until only a 1 px strip is left, and slides back down when the mouse touches it.
 */
class ConnectionBar : public QWidget
{
    Q_OBJECT

    public:
        explicit ConnectionBar(QWidget* parent);

        void SetConnectionName(const QString& name);
        //! Place the bar at the top centre of the parent, in its current open/closed state
        void Reposition();
        void ShowPinned();
        void HideAnimated();

    signals:
        void exitFullScreenRequested();

    protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        void enterEvent(QEnterEvent* event) override;
#else
        void enterEvent(QEvent* event) override;
#endif
        void leaveEvent(QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void paintEvent(QPaintEvent* event) override;

    private:
        static constexpr int HiddenHeight = 1;

        void showAnimated();
        void animateTo(int y);
        void updateNameLabel();

        QLabel* m_label = nullptr;
        QToolButton* m_pinButton = nullptr;
        QToolButton* m_exitButton = nullptr;
        QPropertyAnimation* m_animation = nullptr;
        QString m_connectionName;
        bool m_open = true;
};

/**
 * @brief "Press ... to exit full screen mode" note that fades out (C# FullScreenHint)
 */
class FullScreenHint : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal fade READ GetFade WRITE SetFade)

    public:
        explicit FullScreenHint(QWidget* parent);
        //! Show centred in the parent, then fade out over a few seconds
        void ShowAndFade();

        qreal GetFade() const { return this->m_fade; }
        void SetFade(qreal fade);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        static QString fullScreenMessage();

        QString m_text;
        qreal m_fade = 0.0;
        QPropertyAnimation* m_animation = nullptr;
};

/**
 * @brief Full screen window holding the console (C# Controls/ConsoleTab/FullScreenForm)
 *
 * The console widget is moved into it by AttachConsole() and taken back with
 * DetachConsole(). Closing the window asks the owner to leave full screen.
 */
class FullScreenForm : public QWidget
{
    Q_OBJECT

    public:
        //! @param referenceWidget Widget whose screen the window covers (C# Screen.FromControl)
        explicit FullScreenForm(QWidget* referenceWidget);

        void AttachConsole(QWidget* console, const QString& connectionName);
        void DetachConsole(QWidget* console);
        void SetConnectionName(const QString& name);
        void ShowFullScreen();
        QSize GetContentSize() const;

    signals:
        //! The window is being closed (by the user, or on quit) or the exit button was pressed
        void closeRequested();

    protected:
        void closeEvent(QCloseEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        QWidget* m_referenceWidget = nullptr;
        QWidget* m_contentPanel = nullptr;
        QVBoxLayout* m_contentLayout = nullptr;
        ConnectionBar* m_connectionBar = nullptr;
        FullScreenHint* m_hint = nullptr;
};

#endif // FULLSCREENFORM_H
