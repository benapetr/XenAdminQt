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

#ifndef SRIOV_CONFIGURATION_MODE_H
#define SRIOV_CONFIGURATION_MODE_H

#include <QString>

/*!
 * \brief SR-IOV configuration mode enum
 * 
 * Represents the configuration mode for network SR-IOV.
 * First published in XenServer 7.5.
 * 
 * C# equivalent: XenAPI.sriov_configuration_mode
 */
enum class SriovConfigurationMode
{
    /// Configure network sriov by sysfs, do not need reboot
    Sysfs,
    
    /// Configure network sriov by modprobe, need reboot
    Modprobe,
    
    /// Configure network sriov manually
    Manual,
    
    /// Unknown configuration mode
    Unknown
};

/*!
 * \brief Convert SriovConfigurationMode to string
 */
inline QString SriovConfigurationModeToString(SriovConfigurationMode mode)
{
    switch (mode)
    {
        case SriovConfigurationMode::Sysfs:
            return "sysfs";
        case SriovConfigurationMode::Modprobe:
            return "modprobe";
        case SriovConfigurationMode::Manual:
            return "manual";
        default:
            return "unknown";
    }
}

/*!
 * \brief Parse string to SriovConfigurationMode
 */
inline SriovConfigurationMode SriovConfigurationModeFromString(const QString& str)
{
    if (str == "sysfs")
        return SriovConfigurationMode::Sysfs;
    if (str == "modprobe")
        return SriovConfigurationMode::Modprobe;
    if (str == "manual")
        return SriovConfigurationMode::Manual;
    return SriovConfigurationMode::Unknown;
}

#endif // SRIOV_CONFIGURATION_MODE_H
