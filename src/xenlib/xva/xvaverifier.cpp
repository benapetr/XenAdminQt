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

#include "xvaverifier.h"
#include <QFile>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QDebug>
#include <QtEndian>
#include <stdexcept>
#include <cstring>

// ---------------------------------------------------------------------------
// Internal TAR constants (POSIX ustar header layout)
// ---------------------------------------------------------------------------
static const int TAR_BLOCK_SIZE   = 512;
static const int TAR_NAME_OFF     = 0;
static const int TAR_NAME_LEN     = 100;
static const int TAR_SIZE_OFF     = 124;
static const int TAR_SIZE_LEN     = 12;
static const int TAR_CHKSUM_OFF   = 148;
static const int TAR_CHKSUM_LEN   = 8;

// ---------------------------------------------------------------------------
// Internal helper: parse a null/space-trimmed octal field from a TAR header
// ---------------------------------------------------------------------------
static quint32 parseOctalField(const char* buf, int len)
{
    quint32 result = 0;
    const char* p   = buf;
    const char* end = buf + len;

    // Skip leading whitespace / nulls
    while (p < end && (*p == ' ' || *p == '\0'))
        ++p;

    while (p < end && *p >= '0' && *p <= '7')
    {
        result = result * 8 + static_cast<quint32>(*p - '0');
        ++p;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Internal helper: check whether a 512-byte block is all zeroes
// ---------------------------------------------------------------------------
static bool isAllZeroes(const QByteArray& block)
{
    for (char c : block)
    {
        if (c != '\0')
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Internal helper: compute the TAR header checksum
// The checksum field (bytes 148-155) is treated as spaces (' ') during sum.
// ---------------------------------------------------------------------------
static quint32 computeHeaderChecksum(const QByteArray& block)
{
    quint32 total = 0;
    for (int i = 0; i < TAR_BLOCK_SIZE; ++i)
    {
        if (i >= TAR_CHKSUM_OFF && i < TAR_CHKSUM_OFF + TAR_CHKSUM_LEN)
            total += 32u; // treat checksum field as spaces
        else
            total += static_cast<quint8>(block[i]);
    }
    return total;
}

// ---------------------------------------------------------------------------
// Internal: trim trailing null bytes and spaces from a QString
// (matches C# trim_trailing_stuff)
// ---------------------------------------------------------------------------
static QString trimTailing(QString s)
{
    int end = s.size();
    while (end > 0 && (s[end - 1] == '\0' || s[end - 1] == ' '))
        --end;
    return s.left(end);
}

// ---------------------------------------------------------------------------
// Internal: standalone XXHash64 implementation (public domain algorithm)
// Produces the same numeric result as YYProject.XXHash64.ComputeHash().
// Output is formatted as a big-endian 16-character uppercase hex string
// to match the C# CommandLib checksum_xxhash() output.
// ---------------------------------------------------------------------------
static quint64 xxhash64Compute(const uchar* data, qint64 len, quint64 seed = 0)
{
    static const quint64 P1 = Q_UINT64_C(11400714785074694791);
    static const quint64 P2 = Q_UINT64_C(14029467366897019727);
    static const quint64 P3 = Q_UINT64_C(1609587929392839161);
    static const quint64 P4 = Q_UINT64_C(9650029242287828579);
    static const quint64 P5 = Q_UINT64_C(2870177450012600261);

    auto rotl64 = [](quint64 v, int r) -> quint64 {
        return (v << r) | (v >> (64 - r));
    };

    auto round = [&](quint64 acc, quint64 input) -> quint64 {
        acc += input * P2;
        acc = rotl64(acc, 31);
        acc *= P1;
        return acc;
    };

    auto mergeAcc = [&](quint64 acc, quint64 a) -> quint64 {
        a    = round(0, a);
        acc ^= a;
        acc  = acc * P1 + P4;
        return acc;
    };

    // Read little-endian values from the byte stream
    auto read64 = [](const uchar* p) -> quint64 {
        quint64 v;
        std::memcpy(&v, p, 8);
        return qFromLittleEndian(v);
    };
    auto read32 = [](const uchar* p) -> quint32 {
        quint32 v;
        std::memcpy(&v, p, 4);
        return qFromLittleEndian(v);
    };

    const uchar* p   = data;
    const uchar* end = data + len;
    quint64 h64;

    if (len >= 32)
    {
        quint64 v1 = seed + P1 + P2;
        quint64 v2 = seed + P2;
        quint64 v3 = seed;
        quint64 v4 = seed - P1;

        const uchar* limit = end - 32;
        do
        {
            v1 = round(v1, read64(p)); p += 8;
            v2 = round(v2, read64(p)); p += 8;
            v3 = round(v3, read64(p)); p += 8;
            v4 = round(v4, read64(p)); p += 8;
        } while (p <= limit);

        h64  = rotl64(v1,  1) + rotl64(v2,  7) +
               rotl64(v3, 12) + rotl64(v4, 18);
        h64  = mergeAcc(h64, v1);
        h64  = mergeAcc(h64, v2);
        h64  = mergeAcc(h64, v3);
        h64  = mergeAcc(h64, v4);
    }
    else
    {
        h64 = seed + P5;
    }

    h64 += static_cast<quint64>(len);

    while (p + 8 <= end)
    {
        quint64 k1 = round(0, read64(p));
        h64 ^= k1;
        h64  = rotl64(h64, 27) * P1 + P4;
        p   += 8;
    }

    if (p + 4 <= end)
    {
        h64 ^= static_cast<quint64>(read32(p)) * P1;
        h64  = rotl64(h64, 23) * P2 + P3;
        p   += 4;
    }

    while (p < end)
    {
        h64 ^= static_cast<quint64>(*p) * P5;
        h64  = rotl64(h64, 11) * P1;
        ++p;
    }

    // Finalization avalanche
    h64 ^= h64 >> 33;
    h64 *= P2;
    h64 ^= h64 >> 29;
    h64 *= P3;
    h64 ^= h64 >> 32;

    return h64;
}

// ===========================================================================
// XvaVerifier — static method implementations
// ===========================================================================

XvaVerifier::TarHeader XvaVerifier::parseTarHeader(const QByteArray& block)
{
    if (block.size() < TAR_BLOCK_SIZE)
        throw std::runtime_error("Short TAR block (< 512 bytes)");

    if (isAllZeroes(block))
        return { QString(), 0, true };

    // Verify the header checksum (matches C# Header constructor validation)
    const quint32 stored     = parseOctalField(block.constData() + TAR_CHKSUM_OFF, TAR_CHKSUM_LEN);
    const quint32 recomputed = computeHeaderChecksum(block);
    if (stored != recomputed)
    {
        throw std::runtime_error(
            QString("TAR header checksum failed: stored=%1 recomputed=%2")
                .arg(stored).arg(recomputed).toStdString());
    }

    // Filename field (bytes 0-99, UTF-8, null/space trimmed)
    const QString filename = trimTailing(
        QString::fromUtf8(block.constData() + TAR_NAME_OFF, TAR_NAME_LEN));

    // File-size field (bytes 124-135, octal ASCII)
    const quint32 fileSize = parseOctalField(block.constData() + TAR_SIZE_OFF, TAR_SIZE_LEN);

    return { filename, fileSize, false };
}

quint32 XvaVerifier::paddingLength(quint32 fileSize)
{
    // Round up to the next 512-byte boundary and return the gap
    const quint32 nextBlock = (fileSize + TAR_BLOCK_SIZE - 1)
                              / TAR_BLOCK_SIZE * TAR_BLOCK_SIZE;
    return nextBlock - fileSize;
}

QString XvaVerifier::sha1Hex(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().toLower();
}

QString XvaVerifier::xxhash64Hex(const QByteArray& data)
{
    const quint64 hash = xxhash64Compute(
        reinterpret_cast<const uchar*>(data.constData()),
        data.size());

    // Format as 16-char uppercase hex (big-endian numeric value),
    // matching the C# hex() function applied to ComputeHash() bytes.
    return QString::number(hash, 16).rightJustified(16, '0').toUpper();
}

QHash<QString, QString> XvaVerifier::parseChecksumXml(const QByteArray& xml)
{
    QHash<QString, QString> table;

    QDomDocument doc;
    QString errorMsg;
    if (!doc.setContent(xml, &errorMsg))
    {
        qWarning() << "XvaVerifier: failed to parse checksum.xml:" << errorMsg;
        return table;
    }

    // Structure: <root> <member> <name>…</name> <value>…</value> </member> …
    const QDomNodeList members = doc.elementsByTagName("member");
    for (int i = 0; i < members.count(); ++i)
    {
        const QDomNode member = members.at(i);
        QString name;
        QString value;

        const QDomNodeList children = member.childNodes();
        for (int j = 0; j < children.count(); ++j)
        {
            const QDomElement child = children.at(j).toElement();
            if (child.tagName() == "name")
                name = child.text();
            else if (child.tagName() == "value")
                value = child.text();
        }

        if (!name.isEmpty())
            table.insert(name, value);
    }

    return table;
}

// ---------------------------------------------------------------------------
// Main verification entry point — ports Export.verify() from C# export.cs
// ---------------------------------------------------------------------------
XvaVerifyResult XvaVerifier::Verify(
    const QString& filename,
    std::function<bool()> cancelCheck,
    std::function<void(qint64)> progressCallback)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        return { false, XvaVerifyResult::ErrorType::IOError,
                 QString("Cannot open file: %1").arg(filename) };
    }

    QHash<QString, QString> recomputedChecksums;
    QHash<QString, QString> originalChecksums;
    qint64 bytesProcessed = 0;
    int    zeroBlockCount = 0;

    auto readBlock = [&](quint32 size, QByteArray& out) -> bool {
        out = file.read(static_cast<qint64>(size));
        if (out.size() != static_cast<int>(size))
            return false;
        bytesProcessed += size;
        if (progressCallback)
            progressCallback(bytesProcessed);
        return true;
    };

    while (true)
    {
        if (cancelCheck && cancelCheck())
        {
            return { false, XvaVerifyResult::ErrorType::Cancelled,
                     "Verification cancelled by user" };
        }

        // Read next 512-byte TAR header
        QByteArray headerBlock;
        if (!readBlock(TAR_BLOCK_SIZE, headerBlock))
        {
            return { false, XvaVerifyResult::ErrorType::IOError,
                     "Unexpected end of file while reading TAR header" };
        }

        // End-of-archive: two consecutive zero blocks
        if (isAllZeroes(headerBlock))
        {
            ++zeroBlockCount;
            if (zeroBlockCount >= 2)
                break;   // End of archive reached
            continue;
        }
        zeroBlockCount = 0;

        // Parse the header
        TarHeader header;
        try
        {
            header = parseTarHeader(headerBlock);
        }
        catch (const std::runtime_error& e)
        {
            return { false, XvaVerifyResult::ErrorType::HeaderChecksum,
                     QString::fromStdString(e.what()) };
        }

        if (header.isEndOfArchive)
            break;

        // Read the file data
        QByteArray fileData;
        if (header.fileSize > 0 && !readBlock(header.fileSize, fileData))
        {
            return { false, XvaVerifyResult::ErrorType::IOError,
                     QString("Unexpected end of file reading entry '%1'")
                         .arg(header.filename) };
        }

        // Read and discard the padding bytes
        if (quint32 pad = paddingLength(header.fileSize); pad > 0)
        {
            QByteArray padding;
            if (!readBlock(pad, padding))
            {
                return { false, XvaVerifyResult::ErrorType::IOError,
                         QString("Unexpected end of file reading padding for '%1'")
                             .arg(header.filename) };
            }
        }

        // ---- Dispatch on entry type ----------------------------------------

        // ova.xml — skip (matches C# "if ova.xml: Debug; continue")
        if (header.filename == "ova.xml")
        {
            qDebug() << "XvaVerifier: skipping ova.xml";
            continue;
        }

        // checksum.xml — parse as global checksum table
        if (header.filename.endsWith("checksum.xml"))
        {
            qDebug() << "XvaVerifier: parsing" << header.filename;
            originalChecksums = parseChecksumXml(fileData);
            continue;
        }

        // Data file — must be followed immediately by a companion checksum entry
        // Read companion header
        QByteArray csumHeaderBlock;
        if (!readBlock(TAR_BLOCK_SIZE, csumHeaderBlock))
        {
            return { false, XvaVerifyResult::ErrorType::IOError,
                     QString("Unexpected end of file reading checksum header for '%1'")
                         .arg(header.filename) };
        }

        TarHeader csumHeader;
        try
        {
            csumHeader = parseTarHeader(csumHeaderBlock);
        }
        catch (const std::runtime_error& e)
        {
            return { false, XvaVerifyResult::ErrorType::HeaderChecksum,
                     QString::fromStdString(e.what()) };
        }

        // Read companion checksum data
        QByteArray csumData;
        if (csumHeader.fileSize > 0 && !readBlock(csumHeader.fileSize, csumData))
        {
            return { false, XvaVerifyResult::ErrorType::IOError,
                     QString("Unexpected end of file reading checksum data for '%1'")
                         .arg(header.filename) };
        }

        // Read and discard companion padding
        if (quint32 csumPad = paddingLength(csumHeader.fileSize); csumPad > 0)
        {
            QByteArray padding;
            if (!readBlock(csumPad, padding))
            {
                return { false, XvaVerifyResult::ErrorType::IOError,
                         QString("Unexpected end of file reading checksum padding for '%1'")
                             .arg(header.filename) };
            }
        }

        // Compute and compare hash
        const QString csumCompare = QString::fromUtf8(csumData).trimmed();
        const bool useXxhash      = csumHeader.filename.endsWith(".xxhash");
        const QString computed    = useXxhash ? xxhash64Hex(fileData) : sha1Hex(fileData);

        if (computed.compare(csumCompare, Qt::CaseInsensitive) != 0)
        {
            return { false, XvaVerifyResult::ErrorType::BlockChecksum,
                     QString("Block checksum failed for '%1': computed=%2 expected=%3")
                         .arg(header.filename).arg(computed).arg(csumCompare) };
        }

        recomputedChecksums.insert(header.filename, computed);
        qDebug() << "XvaVerifier:" << header.filename << "checksum OK";
    }

    // Final cross-check against checksum.xml (if present)
    if (!originalChecksums.isEmpty())
    {
        for (auto it = recomputedChecksums.constBegin(); it != recomputedChecksums.constEnd(); ++it)
        {
            const QString& name = it.key();
            if (!originalChecksums.contains(name))
                continue; // Entry not in global table — tolerated

            if (originalChecksums[name].compare(it.value(), Qt::CaseInsensitive) != 0)
            {
                return { false, XvaVerifyResult::ErrorType::BlockChecksum,
                         QString("Global checksum mismatch for '%1': computed=%2 expected=%3")
                             .arg(name).arg(it.value()).arg(originalChecksums[name]) };
            }
        }
    }

    return { true, XvaVerifyResult::ErrorType::None, QString() };
}
