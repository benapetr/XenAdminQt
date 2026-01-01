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

#ifndef APIVERSION_H
#define APIVERSION_H

#include <QString>

/**
 * @brief XenServer API version enumeration
 *
 * Represents the various XenServer/XCP-ng API versions.
 * Used for feature detection and compatibility checks.
 */
enum class APIVersion
{
    API_1_1 = 1,   // XenServer 4.0 (rio)
    API_1_2 = 2,   // XenServer 4.1 (miami)
    API_1_3 = 3,   // XenServer 5.0 (orlando)
    API_1_6 = 6,   // XenServer 5.5 (george)
    API_1_9 = 9,   // XenServer 6.0 (boston) - event.from introduced
    API_2_0 = 11,  // XenServer 6.2 (clearwater)
    API_2_3 = 14,  // XenServer 6.5 (creedence)
    API_2_5 = 16,  // XenServer 7.0 (dundee)
    API_2_6 = 17,  // XenServer 7.1 (ely)
    API_2_8 = 19,  // XenServer 7.3 (inverness)
    API_2_11 = 22, // XenServer 7.6 (lima)
    API_2_12 = 23, // Citrix Hypervisor 8.0 (naples)
    API_2_14 = 25, // Citrix Hypervisor 8.1 (quebec)
    API_2_15 = 26, // Citrix Hypervisor 8.2 (stockholm)
    API_2_16 = 27, // XCP-ng 8.2
    API_2_20 = 28, // XCP-ng 8.3
    API_2_21 = 29, // XCP-ng 8.3
    LATEST = 29,
    UNKNOWN = 99
};

/**
 * @brief Helper functions for API version handling
 */
class APIVersionHelper
{
    public:
        /**
         * @brief Convert API version to string (e.g., "2.21")
         */
        static QString versionToString(APIVersion version);

        /**
         * @brief Parse API version from major/minor numbers
         */
        static APIVersion fromMajorMinor(long major, long minor);

        /**
         * @brief Parse API version from string (e.g., "2.21")
         */
        static APIVersion fromString(const QString& version);

        /**
         * @brief Check if version meets minimum requirement
         */
        static bool versionMeets(APIVersion current, APIVersion required);

        /**
         * @brief Compare two API versions
         * @return <0 if v1 < v2, 0 if equal, >0 if v1 > v2
         */
        static int versionCompare(APIVersion v1, APIVersion v2);
};

#endif // APIVERSION_H
