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

#include "FullScreenForm.h"
#include "../settingsmanager.h"
#include <QCloseEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

// ========== ConnectionBar ==========

ConnectionBar::ConnectionBar(QWidget* parent) : QWidget(parent)
{
    this->setAttribute(Qt::WA_StyledBackground, false);
    this->setMinimumSize(298, 24);
    this->resize(400, 26);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 1, 2, 2);
    layout->setSpacing(2);

    this->m_label = new QLabel(this);
    this->m_label->setStyleSheet("color: white;");
    this->m_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(this->m_label, 1);

    this->m_pinButton = new QToolButton(this);
    this->m_pinButton->setIcon(QIcon(":/icons/pin_16.png"));
    this->m_pinButton->setCheckable(true);
    this->m_pinButton->setAutoRaise(true);
    this->m_pinButton->setToolTip(tr("Always show the connection bar"));
    this->m_pinButton->setChecked(SettingsManager::instance().GetConsolePinConnectionBar());
    layout->addWidget(this->m_pinButton);

    this->m_exitButton = new QToolButton(this);
    this->m_exitButton->setIcon(QIcon(":/icons/window_view_16.png"));
    this->m_exitButton->setAutoRaise(true);
    this->m_exitButton->setToolTip(tr("Exit full screen mode"));
    layout->addWidget(this->m_exitButton);

    this->m_animation = new QPropertyAnimation(this, "pos", this);
    this->m_animation->setDuration(250);
    this->m_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(this->m_exitButton, &QToolButton::clicked, this, &ConnectionBar::exitFullScreenRequested);
    connect(this->m_pinButton, &QToolButton::toggled, this, [this](bool checked)
    {
        // C# buttonPin_CheckedChanged: remembered in the settings
        SettingsManager& settings = SettingsManager::instance();
        if (settings.GetConsolePinConnectionBar() != checked)
            settings.SetConsolePinConnectionBar(checked);
        if (checked)
            this->ShowPinned();
    });
}

void ConnectionBar::SetConnectionName(const QString& name)
{
    this->m_connectionName = name;
    this->updateNameLabel();
}

void ConnectionBar::updateNameLabel()
{
    // C# DisplayConnectionName: ellipsise a long name and show it whole in the tooltip
    const QFontMetrics metrics(this->m_label->font());
    const int width = qMax(0, this->m_label->width());
    const QString elided = metrics.elidedText(this->m_connectionName, Qt::ElideRight, width);
    this->m_label->setText(elided);
    this->m_label->setToolTip(elided == this->m_connectionName ? QString() : this->m_connectionName);
}

void ConnectionBar::Reposition()
{
    if (!this->parentWidget())
        return;
    const int x = (this->parentWidget()->width() - this->width()) / 2;
    const int y = this->m_open ? 0 : -(this->height() - HiddenHeight);
    this->m_animation->stop();
    this->move(x, y);
    this->raise();
}

void ConnectionBar::ShowPinned()
{
    if (!this->m_pinButton->isChecked())
        this->m_pinButton->setChecked(true);
    this->m_open = true;
    this->Reposition();
}

void ConnectionBar::HideAnimated()
{
    if (SettingsManager::instance().GetConsolePinConnectionBar() || !this->m_open)
        return;
    this->m_open = false;
    this->animateTo(-(this->height() - HiddenHeight));
}

void ConnectionBar::showAnimated()
{
    if (this->m_open)
        return;
    if (SettingsManager::instance().GetConsolePinConnectionBar())
    {
        this->ShowPinned();
        return;
    }
    this->m_open = true;
    this->animateTo(0);
}

void ConnectionBar::animateTo(int y)
{
    this->m_animation->stop();
    this->m_animation->setStartValue(this->pos());
    this->m_animation->setEndValue(QPoint(this->x(), y));
    this->m_animation->start();
    this->raise();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ConnectionBar::enterEvent(QEnterEvent* event)
#else
void ConnectionBar::enterEvent(QEvent* event)
#endif
{
    QWidget::enterEvent(event);
    this->showAnimated();
}

void ConnectionBar::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    // Moving onto a child button doesn't leave the bar in Qt, so this is a real exit
    if (!this->rect().contains(this->mapFromGlobal(QCursor::pos())))
        this->HideAnimated();
}

void ConnectionBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->updateNameLabel();
}

void ConnectionBar::paintEvent(QPaintEvent*)
{
    // C# CustomToolStripSystemRenderer: dark grey gradient with a black border
    QPainter painter(this);
    QLinearGradient gradient(0, 0, 0, this->height());
    gradient.setColorAt(0, QColor(64, 64, 64));
    gradient.setColorAt(1, QColor(128, 128, 128));
    painter.fillRect(this->rect(), gradient);
    painter.setPen(Qt::black);
    painter.drawRect(this->rect().adjusted(0, 0, -1, -1));
}

// ========== FullScreenHint ==========

FullScreenHint::FullScreenHint(QWidget* parent) : QWidget(parent)
{
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setAttribute(Qt::WA_TranslucentBackground);

    QFont font = this->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    this->setFont(font);
    this->m_text = FullScreenHint::fullScreenMessage();
    const QSize text = QFontMetrics(font).size(Qt::TextSingleLine, this->m_text);
    this->resize(text.width() + 48, text.height() + 32);

    // C# starts at 70% opacity and fades by 1% every 100 ms, then closes
    this->m_animation = new QPropertyAnimation(this, "fade", this);
    this->m_animation->setStartValue(0.7);
    this->m_animation->setEndValue(0.0);
    this->m_animation->setDuration(7000);
    connect(this->m_animation, &QPropertyAnimation::finished, this, &QWidget::hide);
}

void FullScreenHint::SetFade(qreal fade)
{
    this->m_fade = fade;
    this->update();
}

QString FullScreenHint::fullScreenMessage()
{
    switch (SettingsManager::instance().GetConsoleFullScreenShortcutKey())
    {
        case 0:
            return tr("Press Ctrl+Alt to exit full screen mode");
        case 1:
            return tr("Press Ctrl+Alt+F to exit full screen mode");
        case 2:
            return tr("Press F12 to exit full screen mode");
        default:
            return tr("Press Ctrl+Enter to exit full screen mode");
    }
}

void FullScreenHint::ShowAndFade()
{
    if (QWidget* parent = this->parentWidget())
        this->move((parent->width() - this->width()) / 2, (parent->height() - this->height()) / 2);
    this->show();
    this->raise();
    this->m_animation->stop();
    this->m_animation->start();
}

void FullScreenHint::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(this->m_fade);
    QPainterPath path;
    path.addRoundedRect(QRectF(this->rect()), 10, 10);
    painter.fillPath(path, Qt::black);
    painter.setPen(Qt::white);
    painter.drawText(this->rect(), Qt::AlignCenter, this->m_text);
}

// ========== FullScreenForm ==========

FullScreenForm::FullScreenForm(QWidget* referenceWidget) : QWidget(nullptr), m_referenceWidget(referenceWidget)
{
    this->setAttribute(Qt::WA_DeleteOnClose, false);
    // Like the undocked console, it must not keep the application running on its own
    this->setAttribute(Qt::WA_QuitOnClose, false);
    if (referenceWidget)
    {
        this->setWindowTitle(referenceWidget->window()->windowTitle());
        this->setWindowIcon(referenceWidget->window()->windowIcon());
    }

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    this->setPalette(palette);
    this->setAutoFillBackground(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->m_contentPanel = new QWidget(this);
    this->m_contentLayout = new QVBoxLayout(this->m_contentPanel);
    this->m_contentLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(this->m_contentPanel);

    // Overlays on top of the console, outside the layout
    this->m_connectionBar = new ConnectionBar(this);
    connect(this->m_connectionBar, &ConnectionBar::exitFullScreenRequested, this, &FullScreenForm::closeRequested);
    this->m_hint = new FullScreenHint(this);
    this->m_hint->hide();
}

void FullScreenForm::AttachConsole(QWidget* console, const QString& connectionName)
{
    this->m_contentLayout->addWidget(console);
    console->show();
    this->SetConnectionName(connectionName);
}

void FullScreenForm::DetachConsole(QWidget* console)
{
    this->m_contentLayout->removeWidget(console);
}

void FullScreenForm::SetConnectionName(const QString& name)
{
    this->m_connectionBar->SetConnectionName(name);
}

QSize FullScreenForm::GetContentSize() const
{
    return this->m_contentPanel->size();
}

void FullScreenForm::ShowFullScreen()
{
    // C# OnLoad: cover the screen the console is on
    QScreen* screen = nullptr;
    if (this->m_referenceWidget && this->m_referenceWidget->window()->windowHandle())
        screen = this->m_referenceWidget->window()->windowHandle()->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen)
    {
        this->setGeometry(screen->geometry());
        if (this->windowHandle())
            this->windowHandle()->setScreen(screen);
    }
    this->showFullScreen();
    this->raise();
    this->activateWindow();

    // C# OnShown: pinned bar stays, otherwise it slides away
    if (SettingsManager::instance().GetConsolePinConnectionBar())
        this->m_connectionBar->ShowPinned();
    else
        this->m_connectionBar->HideAnimated();

    this->m_hint->ShowAndFade();
}

void FullScreenForm::closeEvent(QCloseEvent* event)
{
    // C# FormClosing -> toggleFullscreen(): the owner takes the console back and disposes of
    // the window. The close itself is accepted: Qt closes every window when the application
    // quits, and a window that refuses would cancel the quit.
    event->accept();
    emit closeRequested();
}

void FullScreenForm::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    this->m_connectionBar->Reposition();
    if (this->m_hint->isVisible())
        this->m_hint->move((this->width() - this->m_hint->width()) / 2, (this->height() - this->m_hint->height()) / 2);
}
