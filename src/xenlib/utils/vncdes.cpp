/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "vncdes.h"

// Plain DES (FIPS 46-3). Only encryption of single blocks is needed for VNC.
// Tables use the standard 1-based bit numbering, bit 1 being the most significant.

namespace
{
    const int IP[64] = {
        58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
        62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
        57, 49, 41, 33, 25, 17, 9,  1, 59, 51, 43, 35, 27, 19, 11, 3,
        61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7};

    const int FP[64] = {
        40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
        38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
        36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
        34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9,  49, 17, 57, 25};

    const int E[48] = {
        32, 1,  2,  3,  4,  5,  4,  5,  6,  7,  8,  9,
        8,  9,  10, 11, 12, 13, 12, 13, 14, 15, 16, 17,
        16, 17, 18, 19, 20, 21, 20, 21, 22, 23, 24, 25,
        24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1};

    const int P[32] = {
        16, 7, 20, 21, 29, 12, 28, 17, 1,  15, 23, 26, 5,  18, 31, 10,
        2,  8, 24, 14, 32, 27, 3,  9,  19, 13, 30, 6,  22, 11, 4,  25};

    const int PC1[56] = {
        57, 49, 41, 33, 25, 17, 9,  1,  58, 50, 42, 34, 26, 18,
        10, 2,  59, 51, 43, 35, 27, 19, 11, 3,  60, 52, 44, 36,
        63, 55, 47, 39, 31, 23, 15, 7,  62, 54, 46, 38, 30, 22,
        14, 6,  61, 53, 45, 37, 29, 21, 13, 5,  28, 20, 12, 4};

    const int PC2[48] = {
        14, 17, 11, 24, 1,  5,  3,  28, 15, 6,  21, 10,
        23, 19, 12, 4,  26, 8,  16, 7,  27, 20, 13, 2,
        41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
        44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32};

    const int SHIFTS[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};

    const int S[8][64] = {
        {14, 4,  13, 1,  2,  15, 11, 8,  3,  10, 6,  12, 5,  9,  0,  7,
         0,  15, 7,  4,  14, 2,  13, 1,  10, 6,  12, 11, 9,  5,  3,  8,
         4,  1,  14, 8,  13, 6,  2,  11, 15, 12, 9,  7,  3,  10, 5,  0,
         15, 12, 8,  2,  4,  9,  1,  7,  5,  11, 3,  14, 10, 0,  6,  13},
        {15, 1,  8,  14, 6,  11, 3,  4,  9,  7,  2,  13, 12, 0,  5,  10,
         3,  13, 4,  7,  15, 2,  8,  14, 12, 0,  1,  10, 6,  9,  11, 5,
         0,  14, 7,  11, 10, 4,  13, 1,  5,  8,  12, 6,  9,  3,  2,  15,
         13, 8,  10, 1,  3,  15, 4,  2,  11, 6,  7,  12, 0,  5,  14, 9},
        {10, 0,  9,  14, 6,  3,  15, 5,  1,  13, 12, 7,  11, 4,  2,  8,
         13, 7,  0,  9,  3,  4,  6,  10, 2,  8,  5,  14, 12, 11, 15, 1,
         13, 6,  4,  9,  8,  15, 3,  0,  11, 1,  2,  12, 5,  10, 14, 7,
         1,  10, 13, 0,  6,  9,  8,  7,  4,  15, 14, 3,  11, 5,  2,  12},
        {7,  13, 14, 3,  0,  6,  9,  10, 1,  2,  8,  5,  11, 12, 4,  15,
         13, 8,  11, 5,  6,  15, 0,  3,  4,  7,  2,  12, 1,  10, 14, 9,
         10, 6,  9,  0,  12, 11, 7,  13, 15, 1,  3,  14, 5,  2,  8,  4,
         3,  15, 0,  6,  10, 1,  13, 8,  9,  4,  5,  11, 12, 7,  2,  14},
        {2,  12, 4,  1,  7,  10, 11, 6,  8,  5,  3,  15, 13, 0,  14, 9,
         14, 11, 2,  12, 4,  7,  13, 1,  5,  0,  15, 10, 3,  9,  8,  6,
         4,  2,  1,  11, 10, 13, 7,  8,  15, 9,  12, 5,  6,  3,  0,  14,
         11, 8,  12, 7,  1,  14, 2,  13, 6,  15, 0,  9,  10, 4,  5,  3},
        {12, 1,  10, 15, 9,  2,  6,  8,  0,  13, 3,  4,  14, 7,  5,  11,
         10, 15, 4,  2,  7,  12, 9,  5,  6,  1,  13, 14, 0,  11, 3,  8,
         9,  14, 15, 5,  2,  8,  12, 3,  7,  0,  4,  10, 1,  13, 11, 6,
         4,  3,  2,  12, 9,  5,  15, 10, 11, 14, 1,  7,  6,  0,  8,  13},
        {4,  11, 2,  14, 15, 0,  8,  13, 3,  12, 9,  7,  5,  10, 6,  1,
         13, 0,  11, 7,  4,  9,  1,  10, 14, 3,  5,  12, 2,  15, 8,  6,
         1,  4,  11, 13, 12, 3,  7,  14, 10, 15, 6,  8,  0,  5,  9,  2,
         6,  11, 13, 8,  1,  4,  10, 7,  9,  5,  0,  15, 14, 2,  3,  12},
        {13, 2,  8,  4,  6,  15, 11, 1,  10, 9,  3,  14, 5,  0,  12, 7,
         1,  15, 13, 8,  10, 3,  7,  4,  12, 5,  6,  11, 0,  14, 9,  2,
         7,  11, 4,  1,  9,  12, 14, 2,  0,  6,  10, 13, 15, 3,  5,  8,
         2,  1,  14, 7,  4,  10, 8,  13, 15, 12, 9,  0,  3,  5,  6,  11}};

    //! Picks bits of an inWidth-bit value into a new value, table entries 1-based from the top
    quint64 permute(quint64 in, int inWidth, const int* table, int outWidth)
    {
        quint64 out = 0;
        for (int i = 0; i < outWidth; ++i)
            out = (out << 1) | ((in >> (inWidth - table[i])) & 1);
        return out;
    }

    quint32 rotateLeft28(quint32 v, int n)
    {
        return ((v << n) | (v >> (28 - n))) & 0x0FFFFFFF;
    }

    quint32 feistel(quint32 right, quint64 subkey)
    {
        const quint64 expanded = permute(right, 32, E, 48) ^ subkey;
        quint32 out = 0;
        for (int box = 0; box < 8; ++box)
        {
            const int six = int((expanded >> (42 - 6 * box)) & 0x3F);
            const int row = ((six & 0x20) >> 4) | (six & 1);
            const int column = (six >> 1) & 0xF;
            out = (out << 4) | quint32(S[box][row * 16 + column]);
        }
        return quint32(permute(out, 32, P, 32));
    }

    quint64 fromBytes(const QByteArray& bytes)
    {
        quint64 v = 0;
        for (int i = 0; i < 8; ++i)
            v = (v << 8) | quint8(i < bytes.size() ? bytes.at(i) : 0);
        return v;
    }

    QByteArray toBytes(quint64 v)
    {
        QByteArray bytes(8, '\0');
        for (int i = 7; i >= 0; --i)
        {
            bytes[i] = char(v & 0xFF);
            v >>= 8;
        }
        return bytes;
    }

    quint8 reverseBits(quint8 v)
    {
        quint8 r = 0;
        for (int i = 0; i < 8; ++i)
            if (v & (1 << i))
                r |= quint8(0x80 >> i);
        return r;
    }
}

QByteArray VncDes::EncryptBlock(const QByteArray& key, const QByteArray& block)
{
    // Key schedule
    const quint64 permutedKey = permute(fromBytes(key), 64, PC1, 56);
    quint32 c = quint32(permutedKey >> 28) & 0x0FFFFFFF;
    quint32 d = quint32(permutedKey) & 0x0FFFFFFF;
    quint64 subkeys[16];
    for (int round = 0; round < 16; ++round)
    {
        c = rotateLeft28(c, SHIFTS[round]);
        d = rotateLeft28(d, SHIFTS[round]);
        subkeys[round] = permute((quint64(c) << 28) | d, 56, PC2, 48);
    }

    // Sixteen Feistel rounds between the initial and final permutations
    const quint64 permuted = permute(fromBytes(block), 64, IP, 64);
    quint32 left = quint32(permuted >> 32);
    quint32 right = quint32(permuted);
    for (int round = 0; round < 16; ++round)
    {
        const quint32 next = left ^ feistel(right, subkeys[round]);
        left = right;
        right = next;
    }

    return toBytes(permute((quint64(right) << 32) | left, 64, FP, 64));
}

QByteArray VncDes::ChallengeResponse(const QByteArray& challenge, const QString& password)
{
    // C#: keyBytes[i] = Reverse((byte)password[i]) for the first 8 characters
    // ((byte) truncates each UTF-16 character to its low byte)
    QByteArray key(8, '\0');
    for (int i = 0; i < 8 && i < password.size(); ++i)
        key[i] = char(reverseBits(quint8(password.at(i).unicode() & 0xFF)));

    return EncryptBlock(key, challenge.left(8)) + EncryptBlock(key, challenge.mid(8, 8));
}
