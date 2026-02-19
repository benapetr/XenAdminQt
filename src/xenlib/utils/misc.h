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

#ifndef MISC_H
#define MISC_H

#include <QVariant>
#include <QString>
#include <QDateTime>

class Misc
{
    public:
        // Needed to suppress compiler warnings
        static bool QVariantIsMap(const QVariant& v);
        
        /**
         * @brief Natural string comparison (matches C# StringUtility.NaturalCompare)
         *
         * Compares strings in a way that handles embedded numbers naturally.
         * E.g., "VM2" < "VM10" (unlike alphabetical where "VM10" < "VM2")
         *
         * @param s1 First string to compare
         * @param s2 Second string to compare
         * @return Negative if s1 < s2, 0 if equal, positive if s1 > s2
         */
        static int NaturalCompare(const QString& s1, const QString& s2);

        static QString FormatSize(qint64 bytes);

        static QString FormatUptime(qint64 seconds);

        /**
         * @brief Parse XenAPI/C# compatible date-time strings into UTC
         *
         * Supports the same core formats as C# Marshalling.ParseDateTime:
         * - yyyyMMddTHH:mm:ssZ
         * - yyyy-MM-ddTHH:mm:ssZ
         * - yyyy-MM-dd
         * - yyyy.MMdd
         *
         * Also accepts Qt ISODate/ISODateWithMs compatible strings.
         *
         * @param dateStr Input date-time string
         * @return Parsed UTC datetime or invalid datetime when parsing fails
         */
        static QDateTime ParseXenDateTime(const QString& dateStr);
};

#endif // MISC_H
