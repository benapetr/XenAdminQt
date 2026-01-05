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

#ifndef DEBUGWINDOW_H
#define DEBUGWINDOW_H

#include <QDialog>
#include <QDateTime>
#include <QMutex>
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class DebugWindow;
}
class QTextEdit;
QT_END_NAMESPACE

class DebugWindow : public QDialog
{
    Q_OBJECT

    public:
        explicit DebugWindow(QWidget* parent = nullptr);
        ~DebugWindow();

        // Static method to install the custom message handler
        static void installDebugHandler();
        static void uninstallDebugHandler();

        // Static method to get the singleton instance
        static DebugWindow* GetInstance();

    public slots:
        void appendMessage(const QString& message);
        void clearLog();
        void saveLog();
        void toggleAutoScroll(bool enabled);
        void setLogLevel(int level);

    signals:
        void messageReceived(const QString& message);

    private:
        static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
        static DebugWindow* s_instance;
        static QtMessageHandler s_originalHandler;
        static QMutex s_mutex;

        QString formatMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg);

        Ui::DebugWindow* ui;

        bool m_autoScroll;
        int m_messageCount;
        int m_currentLogLevel; // 0=Debug, 1=Info, 2=Warning, 3=Critical
};

#endif // DEBUGWINDOW_H
