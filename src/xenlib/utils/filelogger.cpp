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

#include "filelogger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QThread>

QtMessageHandler FileLogger::s_previousHandler = nullptr;
FileLogger* FileLogger::s_installed = nullptr;

namespace
{
    // Set while a thread is inside the logger, so a message emitted by Qt during a file
    // operation doesn't re-enter it (and deadlock on the mutex)
    thread_local bool t_inLogger = false;

    const char* levelName(QtMsgType type)
    {
        switch (type)
        {
            case QtDebugMsg:
                return "DEBUG";
            case QtInfoMsg:
                return "INFO ";
            case QtWarningMsg:
                return "WARN ";
            case QtCriticalMsg:
                return "ERROR";
            case QtFatalMsg:
                return "FATAL";
        }
        return "?    ";
    }
}

FileLogger* FileLogger::instance()
{
    static FileLogger* logger = new FileLogger();
    return logger;
}

FileLogger::FileLogger(QObject* parent) : QObject(parent)
{
    this->m_flushTimer = new QTimer(this);
    this->m_flushTimer->setInterval(FileLogger::FlushIntervalMs);
    connect(this->m_flushTimer, &QTimer::timeout, this, &FileLogger::Flush);
}

FileLogger::~FileLogger()
{
    if (FileLogger::s_installed == this)
        this->Uninstall();
    QMutexLocker locker(&this->m_mutex);
    this->flushLocked();
    this->closeLocked();
}

void FileLogger::Configure(bool enabled, bool verbose, qint64 maxTotalBytes, const QString& directory, const QString& baseName)
{
    {
        QMutexLocker locker(&this->m_mutex);
        // Write what was logged under the old settings before switching files or turning off
        this->flushLocked();

        const QString newDirectory = QDir::cleanPath(directory);
        const QString newBaseName = baseName.isEmpty() ? this->m_baseName : baseName;
        if (newDirectory != this->m_directory || newBaseName != this->m_baseName || !enabled)
            this->closeLocked();

        this->m_directory = newDirectory;
        this->m_baseName = newBaseName;
        this->m_maxTotalBytes = qMax(maxTotalBytes, FileLogger::MinimumTotalBytes);
        this->m_openFailed = false;
        this->m_verbose.store(verbose);
        this->m_enabled.store(enabled);

        // A lower cap applies right away rather than after the current file grows further
        if (this->m_file.isOpen() && this->m_fileSize > this->GetMaxFileBytes())
            this->rotateLocked();
    }

    if (QThread::currentThread() == this->thread())
    {
        if (enabled)
            this->m_flushTimer->start();
        else
            this->m_flushTimer->stop();
    }
}

void FileLogger::Install()
{
    if (FileLogger::s_installed)
        return;
    FileLogger::s_installed = this;
    FileLogger::s_previousHandler = qInstallMessageHandler(&FileLogger::messageHandler);
}

void FileLogger::Uninstall()
{
    if (FileLogger::s_installed != this)
        return;
    qInstallMessageHandler(FileLogger::s_previousHandler);
    FileLogger::s_previousHandler = nullptr;
    FileLogger::s_installed = nullptr;
    this->Flush();
}

void FileLogger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // Ours first: the default handler aborts on qFatal, and the log should have the reason
    if (FileLogger::s_installed)
        FileLogger::s_installed->Write(type, context, message);

    if (FileLogger::s_previousHandler)
        FileLogger::s_previousHandler(type, context, message);
}

void FileLogger::Write(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (!this->m_enabled.load() || t_inLogger)
        return;
    if (type == QtDebugMsg && !this->m_verbose.load())
        return;

    t_inLogger = true;

    QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss,zzz"));
    line += QLatin1Char(' ');
    line += QLatin1String(levelName(type));
    if (context.category && qstrcmp(context.category, "default") != 0)
    {
        line += QLatin1Char(' ');
        line += QLatin1String(context.category);
    }
    line += QStringLiteral(" [%1] - ").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    line += FileLogger::Sanitize(message);
    line += QLatin1Char('\n');
    const QByteArray bytes = line.toUtf8();

    {
        QMutexLocker locker(&this->m_mutex);
        this->m_buffer.append(bytes);
        if (this->m_buffer.size() >= FileLogger::FlushBufferBytes || type == QtCriticalMsg || type == QtFatalMsg)
            this->flushLocked();
    }

    t_inLogger = false;
}

void FileLogger::Flush()
{
    if (t_inLogger)
        return;
    t_inLogger = true;
    {
        QMutexLocker locker(&this->m_mutex);
        this->flushLocked();
    }
    t_inLogger = false;
}

QString FileLogger::GetDirectory() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_directory;
}

QString FileLogger::GetLogFilePath() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->pathLocked(0);
}

QStringList FileLogger::GetLogFilePaths() const
{
    QMutexLocker locker(&this->m_mutex);
    QStringList paths;
    for (int i = 0; i < FileLogger::FileCount; ++i)
        paths.append(this->pathLocked(i));
    return paths;
}

qint64 FileLogger::GetMaxFileBytes() const
{
    return this->m_maxTotalBytes / FileLogger::FileCount;
}

QString FileLogger::Sanitize(const QString& message)
{
    // HTTP requests carry the session reference as a cookie or query item; with it anybody
    // reading the log could act as the logged-in user until the session ends
    if (!message.contains(QLatin1String("session_id"), Qt::CaseInsensitive))
        return message;

    static const QRegularExpression sessionPattern(QStringLiteral("(session_id=)[^\\s;&\"',]+"),
                                                   QRegularExpression::CaseInsensitiveOption);
    QString sanitized = message;
    sanitized.replace(sessionPattern, QStringLiteral("\\1<hidden>"));
    return sanitized;
}

void FileLogger::flushLocked()
{
    if (this->m_buffer.isEmpty())
        return;

    if (!this->m_enabled.load() || !this->openLocked())
    {
        this->m_buffer.clear();
        return;
    }

    if (this->m_fileSize > 0 && this->m_fileSize + this->m_buffer.size() > this->GetMaxFileBytes())
    {
        this->rotateLocked();
        if (!this->openLocked())
        {
            this->m_buffer.clear();
            return;
        }
    }

    const qint64 written = this->m_file.write(this->m_buffer);
    this->m_file.flush();
    if (written > 0)
        this->m_fileSize += written;
    this->m_buffer.clear();
}

bool FileLogger::openLocked()
{
    if (this->m_file.isOpen())
        return true;
    // Don't retry a failing directory on every flush; Configure() resets this
    if (this->m_openFailed || this->m_directory.isEmpty())
        return false;

    if (!QDir().mkpath(this->m_directory))
    {
        this->m_openFailed = true;
        return false;
    }

    this->m_file.setFileName(this->pathLocked(0));
    // Append like C# (appendToFile=true), so a restart doesn't throw away the last session
    if (!this->m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        this->m_openFailed = true;
        return false;
    }
    this->m_fileSize = this->m_file.size();
    return true;
}

void FileLogger::rotateLocked()
{
    this->closeLocked();

    // <name>.log -> <name>.log.1 -> ... -> <name>.log.<FileCount-1>, the oldest one is dropped
    QFile::remove(this->pathLocked(FileLogger::FileCount - 1));
    for (int i = FileLogger::FileCount - 2; i >= 0; --i)
    {
        const QString from = this->pathLocked(i);
        if (QFile::exists(from))
            QFile::rename(from, this->pathLocked(i + 1));
    }
}

void FileLogger::closeLocked()
{
    if (this->m_file.isOpen())
        this->m_file.close();
    this->m_fileSize = 0;
}

QString FileLogger::pathLocked(int index) const
{
    if (this->m_directory.isEmpty())
        return QString();
    const QString path = QDir(this->m_directory).filePath(this->m_baseName + QStringLiteral(".log"));
    return index == 0 ? path : path + QLatin1Char('.') + QString::number(index);
}
