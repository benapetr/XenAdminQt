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

#include "debugwindow.h"
#include "ui_debugwindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QScrollBar>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QTextEdit>

// Static member definitions
DebugWindow* DebugWindow::s_instance = nullptr;
QtMessageHandler DebugWindow::s_originalHandler = nullptr;
QMutex DebugWindow::s_mutex;

DebugWindow::DebugWindow(QWidget* parent) : QDialog(parent), ui(new Ui::DebugWindow)
{
    this->ui->setupUi(this);

    // Configure dialog properties
    this->setModal(false);                            // Non-modal so it can stay open while using the main app
    this->setAttribute(Qt::WA_DeleteOnClose, false);  // Don't auto-delete when closed
    this->setWindowFlags(this->windowFlags() | Qt::Window); // Make it a proper independent window

    // Set window icon (optional)
    this->setWindowIcon(QIcon(":/icons/debug.png"));

    // Set font for log text edit
    QFont consoleFont("Consolas", 9);
    if (!consoleFont.exactMatch())
    {
        consoleFont.setFamily("Courier New");
    }
    this->ui->logTextEdit->setFont(consoleFont);

    // Connect the signal to append messages (for thread safety)
    connect(this, &DebugWindow::messageReceived, this, &DebugWindow::appendMessage, Qt::QueuedConnection);

    // Set as singleton instance
    DebugWindow::s_instance = this;
}

DebugWindow::~DebugWindow()
{
    DebugWindow::s_instance = nullptr;
    delete this->ui;
}

void DebugWindow::installDebugHandler()
{
    QMutexLocker locker(&DebugWindow::s_mutex);
    if (!DebugWindow::s_originalHandler)
    {
        DebugWindow::s_originalHandler = qInstallMessageHandler(DebugWindow::messageHandler);
    }
}

void DebugWindow::uninstallDebugHandler()
{
    QMutexLocker locker(&DebugWindow::s_mutex);
    if (DebugWindow::s_originalHandler)
    {
        qInstallMessageHandler(DebugWindow::s_originalHandler);
        DebugWindow::s_originalHandler = nullptr;
    }
}

DebugWindow* DebugWindow::GetInstance()
{
    return DebugWindow::s_instance;
}

void DebugWindow::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    // Call original handler first (to preserve console output)
    if (DebugWindow::s_originalHandler)
    {
        DebugWindow::s_originalHandler(type, context, msg);
    }

    // Send to debug window if it exists
    if (DebugWindow::s_instance)
    {
        QString formattedMessage = DebugWindow::s_instance->formatMessage(type, context, msg);

        // Check log level filter
        int messageLevel = 0; // Debug
        switch (type)
        {
            case QtInfoMsg:
                messageLevel = 1;
                break;
            case QtWarningMsg:
                messageLevel = 2;
                break;
            case QtCriticalMsg:
            case QtFatalMsg:
                messageLevel = 3;
                break;
            default:
                messageLevel = 0;
                break;
        }

        if (messageLevel >= DebugWindow::s_instance->m_currentLogLevel)
        {
            emit DebugWindow::s_instance->messageReceived(formattedMessage);
        }
    }
}

QString DebugWindow::formatMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString typeStr;
    QString color;

    switch (type)
    {
        case QtDebugMsg:
            typeStr = "DEBUG";
            color = "gray";
            break;
        case QtInfoMsg:
            typeStr = "INFO ";
            color = "blue";
            break;
        case QtWarningMsg:
            typeStr = "WARN ";
            color = "orange";
            break;
        case QtCriticalMsg:
            typeStr = "ERROR";
            color = "red";
            break;
        case QtFatalMsg:
            typeStr = "FATAL";
            color = "darkred";
            break;
    }

    QString location;
    if (context.file && context.line > 0)
    {
        QString fileName = QFileInfo(context.file).baseName();
        location = QString(" [%1:%2]").arg(fileName).arg(context.line);
    }

    return QString("<span style=\"color: %1;\">[%2] %3%4: %5</span>").arg(color, timestamp, typeStr, location, msg.toHtmlEscaped());
}

void DebugWindow::appendMessage(const QString& message)
{
    if (this->ui->logTextEdit)
    {
        this->ui->logTextEdit->append(message);
        this->m_messageCount++;
        this->ui->messageCountLabel->setText(QString("Messages: %1").arg(this->m_messageCount));

        if (this->m_autoScroll)
        {
            QScrollBar* scrollBar = this->ui->logTextEdit->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }
    }
}

void DebugWindow::clearLog()
{
    if (this->ui->logTextEdit)
    {
        this->ui->logTextEdit->clear();
        this->m_messageCount = 0;
        this->ui->messageCountLabel->setText("Messages: 0");
    }
}

void DebugWindow::saveLog()
{
    if (!this->ui->logTextEdit)
    {
        return;
    }

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString defaultFileName = QString("xenadmin_debug_%1.txt").arg(timestamp);

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Debug Log",
        QDir(defaultPath).filePath(defaultFileName),
        "Text files (*.txt);;All files (*)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&file);

        // Convert HTML to plain text for saving
        QString plainText = this->ui->logTextEdit->toPlainText();
        stream << plainText;

        QMessageBox::information(this, "Log Saved",
                                 QString("Debug log saved to:\n%1").arg(fileName));
    } else
    {
        QMessageBox::warning(this, "Save Failed",
                             QString("Failed to save log to:\n%1\n\nError: %2")
                                 .arg(fileName, file.errorString()));
    }
}

void DebugWindow::toggleAutoScroll(bool enabled)
{
    this->m_autoScroll = enabled;
}

void DebugWindow::setLogLevel(int level)
{
    this->m_currentLogLevel = level;
}
