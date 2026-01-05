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

#include "apiversion.h"
#include <QStringList>

QString APIVersionHelper::versionToString(APIVersion version)
{
    switch (version)
    {
    case APIVersion::API_1_1:
        return "1.1";
    case APIVersion::API_1_2:
        return "1.2";
    case APIVersion::API_1_3:
        return "1.3";
    case APIVersion::API_1_4:
        return "1.4";
    case APIVersion::API_1_5:
        return "1.5";
    case APIVersion::API_1_6:
        return "1.6";
    case APIVersion::API_1_7:
        return "1.7";
    case APIVersion::API_1_8:
        return "1.8";
    case APIVersion::API_1_9:
        return "1.9";
    case APIVersion::API_1_10:
        return "1.10";
    case APIVersion::API_2_0:
        return "2.0";
    case APIVersion::API_2_1:
        return "2.1";
    case APIVersion::API_2_2:
        return "2.2";
    case APIVersion::API_2_3:
        return "2.3";
    case APIVersion::API_2_4:
        return "2.4";
    case APIVersion::API_2_5:
        return "2.5";
    case APIVersion::API_2_6:
        return "2.6";
    case APIVersion::API_2_7:
        return "2.7";
    case APIVersion::API_2_8:
        return "2.8";
    case APIVersion::API_2_9:
        return "2.9";
    case APIVersion::API_2_10:
        return "2.10";
    case APIVersion::API_2_11:
        return "2.11";
    case APIVersion::API_2_12:
        return "2.12";
    case APIVersion::API_2_13:
        return "2.13";
    case APIVersion::API_2_14:
        return "2.14";
    case APIVersion::API_2_15:
        return "2.15";
    case APIVersion::API_2_16:
        return "2.16";
    case APIVersion::API_2_20:
        return "2.20";
    case APIVersion::API_2_21:
        return "2.21";
    default:
        return "Unknown";
    }
}

APIVersion APIVersionHelper::fromMajorMinor(long major, long minor)
{
    // Convert major.minor to enum value
    QString versionStr = QString("%1_%2").arg(major).arg(minor);

    // Try to match against known versions
    if (major == 1 && minor == 1)
        return APIVersion::API_1_1;
    if (major == 1 && minor == 2)
        return APIVersion::API_1_2;
    if (major == 1 && minor == 3)
        return APIVersion::API_1_3;
    if (major == 1 && minor == 4)
        return APIVersion::API_1_4;
    if (major == 1 && minor == 5)
        return APIVersion::API_1_5;
    if (major == 1 && minor == 6)
        return APIVersion::API_1_6;
    if (major == 1 && minor == 7)
        return APIVersion::API_1_7;
    if (major == 1 && minor == 8)
        return APIVersion::API_1_8;
    if (major == 1 && minor == 9)
        return APIVersion::API_1_9;
    if (major == 1 && minor == 10)
        return APIVersion::API_1_10;
    if (major == 2 && minor == 0)
        return APIVersion::API_2_0;
    if (major == 2 && minor == 1)
        return APIVersion::API_2_1;
    if (major == 2 && minor == 2)
        return APIVersion::API_2_2;
    if (major == 2 && minor == 3)
        return APIVersion::API_2_3;
    if (major == 2 && minor == 4)
        return APIVersion::API_2_4;
    if (major == 2 && minor == 5)
        return APIVersion::API_2_5;
    if (major == 2 && minor == 6)
        return APIVersion::API_2_6;
    if (major == 2 && minor == 7)
        return APIVersion::API_2_7;
    if (major == 2 && minor == 8)
        return APIVersion::API_2_8;
    if (major == 2 && minor == 9)
        return APIVersion::API_2_9;
    if (major == 2 && minor == 10)
        return APIVersion::API_2_10;
    if (major == 2 && minor == 11)
        return APIVersion::API_2_11;
    if (major == 2 && minor == 12)
        return APIVersion::API_2_12;
    if (major == 2 && minor == 13)
        return APIVersion::API_2_13;
    if (major == 2 && minor == 14)
        return APIVersion::API_2_14;
    if (major == 2 && minor == 15)
        return APIVersion::API_2_15;
    if (major == 2 && minor == 16)
        return APIVersion::API_2_16;
    if (major == 2 && minor == 20)
        return APIVersion::API_2_20;
    if (major == 2 && minor == 21)
        return APIVersion::API_2_21;

    return APIVersion::UNKNOWN;
}

APIVersion APIVersionHelper::fromString(const QString& version)
{
    if (version.isEmpty())
        return APIVersion::UNKNOWN;

    // Split on '.'
    QStringList parts = version.split('.');
    if (parts.size() != 2)
        return APIVersion::UNKNOWN;

    bool ok1, ok2;
    long major = parts[0].toLong(&ok1);
    long minor = parts[1].toLong(&ok2);

    if (!ok1 || !ok2)
        return APIVersion::UNKNOWN;

    return fromMajorMinor(major, minor);
}

bool APIVersionHelper::versionMeets(APIVersion current, APIVersion required)
{
    return versionCompare(current, required) >= 0;
}

int APIVersionHelper::versionCompare(APIVersion v1, APIVersion v2)
{
    return static_cast<int>(v1) - static_cast<int>(v2);
}
