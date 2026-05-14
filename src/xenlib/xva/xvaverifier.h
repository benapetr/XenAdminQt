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

#ifndef XVAVERIFIER_H
#define XVAVERIFIER_H

#include <QString>
#include <QHash>
#include <functional>

/**
 * @brief Result of an XVA archive verification pass.
 *
 * Qt equivalent of the HeaderChecksumFailed / BlockChecksumFailed exception
 * types in C# CommandLib/export.cs.
 */
struct XvaVerifyResult
{
    bool success = false;

    enum class ErrorType
    {
        None,
        HeaderChecksum,  ///< TAR header checksum mismatch
        BlockChecksum,   ///< Per-block SHA1/xxhash mismatch
        IOError,         ///< File I/O failure
        Cancelled        ///< User cancelled
    } errorType = ErrorType::None;

    QString errorMessage;
};

/**
 * @brief Verifies an XVA archive file (TAR-based format used by XenServer).
 *
 * Ports the Export.verify() logic from C# CommandLib/export.cs.
 *
 * Algorithm:
 * - Read the XVA (POSIX TAR) sequentially.
 * - "ova.xml" entries are skipped.
 * - "*.checksum.xml" entries are parsed as global checksum tables.
 * - All other data entries are followed by a companion .checksum (SHA1)
 *   or .xxhash (XXHash64) file; hashes are verified immediately.
 * - At end-of-archive, the recomputed table is compared with the global
 *   checksum.xml table (if present).
 */
class XvaVerifier
{
    public:
        /**
         * @brief Verify an XVA file on disk.
         * @param filename         Path to the XVA file.
         * @param cancelCheck      Called to detect cancellation; return true to abort.
         * @param progressCallback Called periodically with bytes consumed so far.
         * @return Verification result.
         */
        static XvaVerifyResult Verify(
            const QString& filename,
            std::function<bool()> cancelCheck = nullptr,
            std::function<void(qint64)> progressCallback = nullptr);

    private:
        XvaVerifier() = delete;

        struct TarHeader
        {
            QString filename;
            quint32 fileSize = 0;
            bool    isEndOfArchive = false;
        };

        /**
         * @brief Parse a 512-byte TAR header block.
         * @throws std::runtime_error on header checksum failure.
         */
        static TarHeader parseTarHeader(const QByteArray& block);

        /** @brief Padding bytes after data to reach the next 512-byte boundary. */
        static quint32 paddingLength(quint32 fileSize);

        /** @brief Compute SHA1 of data, returning lowercase hex string (40 chars). */
        static QString sha1Hex(const QByteArray& data);

        /**
         * @brief Compute XXHash64 of data, returning uppercase hex string (16 chars).
         *
         * Matches the output of YYProject.XXHash64.ComputeHash() as used in
         * the C# CommandLib.
         */
        static QString xxhash64Hex(const QByteArray& data);

        /**
         * @brief Parse a checksum.xml body into a filename → checksum map.
         *
         * The XML uses an XML-RPC struct layout:
         * @code
         *   <someroot>
         *     <member>
         *       <name>filename</name>
         *       <value>sha1hexstring</value>
         *     </member>
         *   </someroot>
         * @endcode
         */
        static QHash<QString, QString> parseChecksumXml(const QByteArray& xml);
};

#endif // XVAVERIFIER_H
