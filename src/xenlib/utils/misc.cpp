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

#include "misc.h"
#include <QtCore>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QMetaType>
#endif

bool Misc::QVariantIsMap(const QVariant &v)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: QVariant::type() is deprecated
    return v.metaType() == QMetaType::fromType<QVariantMap>();
#else
    // Qt5
    return v.type() == QVariant::Map;
#endif
}

int Misc::NaturalCompare(const QString& s1, const QString& s2)
{
    if (s1.compare(s2, Qt::CaseInsensitive) == 0)
        return 0;

    if (s1.isEmpty())
        return -1;
    if (s2.isEmpty())
        return 1;

    int i = 0;
    int len1 = s1.length();
    int len2 = s2.length();
    int minLen = qMin(len1, len2);

    while (i < minLen)
    {
        QChar c1 = s1[i];
        QChar c2 = s2[i];

        bool c1IsDigit = c1.isDigit();
        bool c2IsDigit = c2.isDigit();

        if (!c1IsDigit && !c2IsDigit)
        {
            // Two non-digits: alphabetical comparison
            int cmp = s1.mid(i, 1).compare(s2.mid(i, 1), Qt::CaseInsensitive);
            if (cmp != 0)
                return cmp;
            i++;
        } else if (c1IsDigit && c2IsDigit)
        {
            // Both are digits: compare as numbers
            int j = i + 1;
            while (j < len1 && s1[j].isDigit())
                j++;
            int k = i + 1;
            while (k < len2 && s2[k].isDigit())
                k++;

            int numLen1 = j - i;
            int numLen2 = k - i;

            // Shorter number (in digits) is smaller
            if (numLen1 != numLen2)
                return numLen1 - numLen2;

            // Same length: compare digit by digit
            QString num1 = s1.mid(i, numLen1);
            QString num2 = s2.mid(i, numLen2);
            int cmp = num1.compare(num2);
            if (cmp != 0)
                return cmp;

            i = j;
        } else
        {
            // One is digit, one is not: digits come after letters
            return c1IsDigit ? 1 : -1;
        }
    }

    // Strings are equal up to minLen, shorter one is smaller
    return len1 - len2;
}

QString Misc::FormatMemorySize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;

    if (bytes >= TB)
    {
        double tb = static_cast<double>(bytes) / TB;
        return QString::number(tb, 'f', 2) + " TB";
    }
    else if (bytes >= GB)
    {
        double gb = static_cast<double>(bytes) / GB;
        return QString::number(gb, 'f', 2) + " GB";
    }
    else if (bytes >= MB)
    {
        double mb = static_cast<double>(bytes) / MB;
        return QString::number(mb, 'f', 1) + " MB";
    }
    else if (bytes >= KB)
    {
        double kb = static_cast<double>(bytes) / KB;
        return QString::number(kb, 'f', 1) + " KB";
    }

    return QString::number(bytes) + " B";
}
