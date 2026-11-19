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

#ifndef FILELOGGER_H
#define FILELOGGER_H

#include "../xenlib_global.h"
#include <QObject>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <atomic>

/**
 * @brief Application log file (C# log4net RollingFileAppender in app.config).
 *
 * Qt messages are formatted like C#'s "%date %-5level %logger [%thread] - %message" and
 * written to <directory>/<baseName>.log. Two things keep the disk usage and the number
 * of writes small:
 *
 *  - Size cap: the log rotates through a fixed number of files (current + backups), each
 *    at most maxTotalBytes / FileCount, so the files never take more than maxTotalBytes.
 *  - Batching: messages are kept in memory and written in one go every few seconds, when
 *    the buffer fills up, on a critical message, or on Flush(). A crash can lose at most
 *    the last few seconds of debug/info/warning output.
 *
 * Debug messages are only written in verbose mode, since this client prints a lot of them.
 * Messages from any thread are accepted; the periodic flush runs on the logger's thread.
 */
class XENLIB_EXPORT FileLogger : public QObject
{
    Q_OBJECT

    public:
        static constexpr int FileCount = 3;
        static constexpr qint64 MinimumTotalBytes = 1024 * 1024;
        static constexpr int FlushBufferBytes = 64 * 1024;
        static constexpr int FlushIntervalMs = 5000;

        static FileLogger* instance();

        explicit FileLogger(QObject* parent = nullptr);
        ~FileLogger() override;

        /**
         * @brief Apply the logging settings.
         * @param enabled Whether anything is written to disk
         * @param verbose Include debug messages (otherwise info, warnings and errors)
         * @param maxTotalBytes Upper bound for all log files together
         * @param directory Folder of the log files, created on the first write
         * @param baseName File name without extension (C# BrandConsole)
         */
        void Configure(bool enabled, bool verbose, qint64 maxTotalBytes, const QString& directory, const QString& baseName);

        //! Install the Qt message handler that feeds this logger (chains to the previous one).
        void Install();
        //! Restore the previous message handler and write out anything pending.
        void Uninstall();

        //! Queue one message; normally called by the installed message handler.
        void Write(QtMsgType type, const QMessageLogContext& context, const QString& message);
        //! Write everything queued so far to disk.
        void Flush();

        bool IsEnabled() const { return this->m_enabled.load(); }
        bool IsVerbose() const { return this->m_verbose.load(); }
        QString GetDirectory() const;
        //! Path of the current log file (it may not exist yet).
        QString GetLogFilePath() const;
        //! Paths of the current file and its backups, newest first.
        QStringList GetLogFilePaths() const;
        qint64 GetMaxFileBytes() const;

        //! Mask session references that would otherwise end up in the log file.
        static QString Sanitize(const QString& message);

    private:
        static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message);

        void flushLocked();
        bool openLocked();
        void rotateLocked();
        void closeLocked();
        QString pathLocked(int index) const;

        mutable QMutex m_mutex;
        std::atomic<bool> m_enabled { false };
        std::atomic<bool> m_verbose { false };
        qint64 m_maxTotalBytes = 10 * 1024 * 1024;
        QString m_directory;
        QString m_baseName = QStringLiteral("XenAdmin");
        QByteArray m_buffer;
        QFile m_file;
        qint64 m_fileSize = 0;
        bool m_openFailed = false;
        QTimer* m_flushTimer = nullptr;

        static QtMessageHandler s_previousHandler;
        static FileLogger* s_installed;
};

#endif // FILELOGGER_H
