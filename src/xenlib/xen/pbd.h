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

#ifndef PBD_H
#define PBD_H

#include "xenobject.h"

class Host;
class SR;

/**
 * @brief PBD - Physical Block Device
 *
 * Qt equivalent of C# XenAPI.PBD class.
 * Represents the physical block devices through which hosts access SRs.
 *
 * Key properties (from C# PBD class):
 * - uuid - Unique identifier
 * - host - Physical machine on which the PBD is available
 * - SR - The storage repository that the PBD realises
 * - device_config - Config string map provided to the host's SR-backend-driver
 * - currently_attached - Is the SR currently attached on this host?
 * - other_config - Additional configuration
 *
 * First published in XenServer 4.0.
 */
class XENLIB_EXPORT PBD : public XenObject
{
    Q_OBJECT

    public:
        explicit PBD(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~PBD() override = default;

        // XenObject interface
        QString GetObjectType() const override;

        //! @brief Get reference to the host this PBD is on
        //! @return Host opaque reference
        QString GetHostRef() const;

        QSharedPointer<Host> GetHost();

        //! @brief Get reference to the SR this PBD provides access to
        //! @return SR opaque reference
        QString GetSRRef() const;

        QSharedPointer<SR> GetSR();

        //! @brief Get device configuration map
        //! @return Device config dictionary (string->string)
        QVariantMap DeviceConfig() const;

        //! @brief Check if SR is currently attached via this PBD
        //! @return true if currently attached
        bool IsCurrentlyAttached() const;

        //! @brief Get a specific device config value
        //! @param key Config key
        //! @return Config value (empty if not found)
        QString GetDeviceConfigValue(const QString& key) const;

        //! @brief Get a specific other config value
        //! @param key Config key
        //! @return Config value (empty if not found)
        QString GetOtherConfigValue(const QString& key) const;

        //! @brief Check if device config contains a key
        //! @param key Config key
        //! @return true if key exists
        bool HasDeviceConfigKey(const QString& key) const;

        //! @brief Check if other config contains a key
        //! @param key Config key
        //! @return true if key exists
        bool HasOtherConfigKey(const QString& key) const;
};

#endif // PBD_H
