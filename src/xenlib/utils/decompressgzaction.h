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

#ifndef DECOMPRESSGZACTION_H
#define DECOMPRESSGZACTION_H

#include "../xen/asyncoperation.h"

/**
 * @brief Decompresses a gzip-compressed file to a given destination path.
 *
 * Client-side file preprocessing — no XenServer connection required.
 * Used by ImportWizard to decompress .xva.gz and .ova.gz files before
 * the import action begins.
 *
 * The output file is created at the path given to the constructor.
 * On cancellation or failure the partial output file is removed.
 *
 * C# equivalent: ImportSourcePage.Uncompress() / _unzipWorker_DoWork()
 */
class XENLIB_EXPORT DecompressGzAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Construct with source and destination paths.
         * @param sourceGzPath  Absolute path to the .gz input file.
         * @param destPath      Absolute path for the decompressed output file.
         * @param parent        Optional QObject parent.
         */
        explicit DecompressGzAction(const QString& sourceGzPath,
                                    const QString& destPath,
                                    QObject* parent = nullptr);

        /** @brief Destination path supplied to the constructor. */
        QString OutputPath() const { return this->m_destPath; }

    protected:
        void run() override;

    private:
        QString m_sourceGzPath;
        QString m_destPath;
};

#endif // DECOMPRESSGZACTION_H
