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

#include "decompressgzaction.h"

#include <zlib.h>
#include <QFile>
#include <QFileInfo>

// Read buffer: 64 KB is a good balance of throughput vs. cancellation latency
static const int GZ_BUF_SIZE = 64 * 1024;

DecompressGzAction::DecompressGzAction(const QString& sourceGzPath,
                                       const QString& destPath,
                                       QObject* parent)
    : AsyncOperation(tr("Decompress"),
                     tr("Decompressing %1...").arg(QFileInfo(sourceGzPath).fileName()),
                     parent)
    , m_sourceGzPath(sourceGzPath)
    , m_destPath(destPath)
{
}

void DecompressGzAction::run()
{
    this->SetCanCancel(true);
    this->setDescriptionSafe(tr("Decompressing %1...").arg(QFileInfo(this->m_sourceGzPath).fileName()));

    const qint64 compressedSize = QFileInfo(this->m_sourceGzPath).size();

    // Open gzip input. gzopen handles the gzip header transparently.
    gzFile gz = gzopen(this->m_sourceGzPath.toLocal8Bit().constData(), "rb");
    if (!gz)
    {
        this->setError(tr("Cannot open compressed file: %1").arg(this->m_sourceGzPath));
        this->setState(Failed);
        return;
    }

    QFile out(this->m_destPath);
    if (!out.open(QIODevice::WriteOnly))
    {
        gzclose(gz);
        this->setError(tr("Cannot create output file %1: %2")
                         .arg(this->m_destPath, out.errorString()));
        this->setState(Failed);
        return;
    }

    QByteArray buf(GZ_BUF_SIZE, '\0');

    while (true)
    {
        if (this->IsCancelled())
        {
            gzclose(gz);
            out.close();
            QFile::remove(this->m_destPath);
            this->setState(Cancelled);
            return;
        }

        const int bytesRead = gzread(gz, buf.data(), GZ_BUF_SIZE);
        if (bytesRead < 0)
        {
            int errnum = 0;
            const char* msg = gzerror(gz, &errnum);
            gzclose(gz);
            out.close();
            QFile::remove(this->m_destPath);
            this->setError(tr("Decompression error: %1").arg(QString::fromLatin1(msg)));
            this->setState(Failed);
            return;
        }

        if (bytesRead == 0)
            break;  // EOF

        out.write(buf.constData(), bytesRead);

        // Report progress based on how much of the compressed stream has been consumed.
        // gzoffset() (zlib >= 1.2.4) returns the byte position in the compressed file.
        if (compressedSize > 0)
        {
            const z_off_t pos = gzoffset(gz);
            const int pct = static_cast<int>(99.0 * pos / compressedSize);
            this->setPercentCompleteSafe(qBound(0, pct, 99));
        }
    }

    gzclose(gz);
    out.close();

    this->setPercentCompleteSafe(100);
    this->setState(Completed);
}
