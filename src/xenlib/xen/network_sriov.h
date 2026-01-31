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

#ifndef NETWORK_SRIOV_H
#define NETWORK_SRIOV_H

#include "xenobject.h"
#include "sriov_configuration_mode.h"
#include <QString>
#include <QSharedPointer>

class PIF;

/*!
 * \brief Network SR-IOV configuration wrapper class
 * 
 * Represents a network-sriov object which connects logical PIF and physical PIF.
 * First published in XenServer 7.5.
 * 
 * C# equivalent: XenAPI.Network_sriov
 * 
 * Key properties:
 * - physical_PIF: The PIF that has SR-IOV enabled
 * - logical_PIF: The logical PIF to connect to the SR-IOV network after enabling SR-IOV on the physical PIF
 * - requires_reboot: Indicates whether the host needs to be rebooted before SR-IOV is enabled on the physical PIF
 * - configuration_mode: The mode for configuring network sriov
 */
class XENLIB_EXPORT NetworkSriov : public XenObject
{
    Q_OBJECT
    
    public:
        explicit NetworkSriov(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        XenObjectType GetObjectType() const override { return XenObjectType::NetworkSriov; }

        /*!
         * \brief Get the PIF reference that has SR-IOV enabled
         * \return Physical PIF opaque reference
         */
        QString GetPhysicalPIFRef() const;

        /*!
         * \brief Get the physical PIF that has SR-IOV enabled
         * \return Shared pointer to the physical PIF object
         */
        QSharedPointer<PIF> GetPhysicalPIF();

        /*!
         * \brief Get the logical PIF reference
         * 
         * The logical PIF to connect to the SR-IOV network after enabling SR-IOV on the physical PIF.
         * \return Logical PIF opaque reference
         */
        QString GetLogicalPIFRef() const;

        /*!
         * \brief Get the logical PIF object
         * \return Shared pointer to the logical PIF object
         */
        QSharedPointer<PIF> GetLogicalPIF();

        /*!
         * \brief Check if host reboot is required
         * 
         * Indicates whether the host needs to be rebooted before SR-IOV is enabled on the physical PIF.
         * \return true if reboot is required, false otherwise
         */
        bool RequiresReboot() const;

        /*!
         * \brief Get the SR-IOV configuration mode
         * \return Configuration mode enum value
         */
        SriovConfigurationMode GetConfigurationMode() const;

        /*!
         * \brief Get the SR-IOV configuration mode as a string
         * \return Configuration mode string ("sysfs", "modprobe", "manual", "unknown")
         */
        QString GetConfigurationModeString() const;
};

#endif // NETWORK_SRIOV_H
