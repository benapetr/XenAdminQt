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

#ifndef XENAPI_SR_H
#define XENAPI_SR_H

#include "../session.h"
#include <QString>
#include <QVariantMap>
#include <QVariantList>

namespace XenAPI
{

    /**
     * @brief SR - XenAPI Storage Repository bindings
     *
     * Static-only class providing XenAPI SR method bindings.
     * Matches C# XenAPI.SR class from XenModel/XenAPI/SR.cs
     */
    class SR
    {
    private:
        SR() = delete; // Static-only class

    public:
        /**
         * @brief Get full SR record
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @return SR record as QVariantMap
         */
        static QVariantMap get_record(XenSession* session, const QString& sr);

        /**
         * @brief Get SR name label
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @return SR name label
         */
        static QString get_name_label(XenSession* session, const QString& sr);

        /**
         * @brief Get SR by UUID
         * @param session Active XenSession
         * @param uuid SR UUID
         * @return SR opaque reference
         */
        static QString get_by_uuid(XenSession* session, const QString& uuid);

        /**
         * @brief Get list of PBDs for SR
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @return List of PBD references
         */
        static QVariantList get_PBDs(XenSession* session, const QString& sr);

        /**
         * @brief Create a new SR
         * @param session Active XenSession
         * @param host Host to create SR on
         * @param deviceConfig Device configuration map
         * @param physicalSize Physical size (0 for auto-detect)
         * @param nameLabel SR name
         * @param nameDescription SR description
         * @param type SR type (nfs, lvmoiscsi, etc.)
         * @param contentType Content type (user, iso, etc.)
         * @param shared Whether SR is shared across pool
         * @param smConfig SM-specific config
         * @return SR opaque reference
         */
        static QString create(XenSession* session,
                              const QString& host,
                              const QVariantMap& deviceConfig,
                              qint64 physicalSize,
                              const QString& nameLabel,
                              const QString& nameDescription,
                              const QString& type,
                              const QString& contentType,
                              bool shared,
                              const QVariantMap& smConfig);

        /**
         * @brief Introduce an existing SR (async)
         * @param session Active XenSession
         * @param uuid SR UUID
         * @param nameLabel SR name
         * @param nameDescription SR description
         * @param type SR type
         * @param contentType Content type
         * @param shared Whether SR is shared
         * @param smConfig SM config
         * @return Task reference
         */
        static QString async_introduce(XenSession* session,
                                       const QString& uuid,
                                       const QString& nameLabel,
                                       const QString& nameDescription,
                                       const QString& type,
                                       const QString& contentType,
                                       bool shared,
                                       const QVariantMap& smConfig);

        /**
         * @brief Forget SR (async)
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @return Task reference
         */
        static QString async_forget(XenSession* session, const QString& sr);

        /**
         * @brief Forget SR (sync)
         * @param session Active XenSession
         * @param sr SR opaque reference
         */
        static void forget(XenSession* session, const QString& sr);

        /**
         * @brief Destroy SR (async)
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @return Task reference
         */
        static QString async_destroy(XenSession* session, const QString& sr);

        /**
         * @brief Set SR name label
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @param value New name label
         */
        static void set_name_label(XenSession* session, const QString& sr, const QString& value);

        /**
         * @brief Set SR name description
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @param value New description
         */
        static void set_name_description(XenSession* session, const QString& sr, const QString& value);

        /**
         * @brief Set SR other_config
         * @param session Active XenSession
         * @param sr SR opaque reference
         * @param value other_config map
         */
        static void set_other_config(XenSession* session, const QString& sr, const QVariantMap& value);

        /**
         * @brief Scan SR for new/changed VDIs
         * @param session Active XenSession
         * @param sr SR opaque reference
         *
         * C# equivalent: SR.scan(Session, sr)
         * Scans the SR to detect new, changed, or removed VDIs
         */
        static void scan(XenSession* session, const QString& sr);

        /**
         * @brief Probe for existing SRs (async version)
         * @param session Active XenSession
         * @param host Host to probe from
         * @param device_config Device configuration (target, server, etc.)
         * @param type SR type (e.g., "nfs", "lvmoiscsi")
         * @param sm_config SM configuration
         * @return Task reference
         *
         * C# equivalent: SR.async_probe()
         * Returns XML describing available SRs
         */
        static QString async_probe(XenSession* session, const QString& host,
                                   const QVariantMap& device_config,
                                   const QString& type,
                                   const QVariantMap& sm_config);

        /**
         * @brief Probe for existing SRs (extended version)
         * @param session Active XenSession
         * @param host Host to probe from
         * @param device_config Device configuration
         * @param type SR type
         * @param sm_config SM configuration
         * @return List of SR info records
         *
         * C# equivalent: SR.probe_ext()
         * Returns structured data instead of XML
         */
        static QVariantList probe_ext(XenSession* session, const QString& host,
                                      const QVariantMap& device_config,
                                      const QString& type,
                                      const QVariantMap& sm_config);

        /**
         * @brief Create new SR (async)
         * @param session Active XenSession
         * @param host Host to create SR on
         * @param device_config Device configuration
         * @param physical_size Physical size in bytes
         * @param name_label SR name
         * @param name_description SR description
         * @param type SR type
         * @param content_type Content type
         * @param shared Whether SR is shared
         * @param sm_config SM configuration
         * @return Task reference
         *
         * C# equivalent: SR.async_create()
         * Creates new storage repository
         */
        static QString async_create(XenSession* session,
                                    const QString& host,
                                    const QVariantMap& device_config,
                                    qint64 physical_size,
                                    const QString& name_label,
                                    const QString& name_description,
                                    const QString& type,
                                    const QString& content_type,
                                    bool shared,
                                    const QVariantMap& sm_config);
    };

} // namespace XenAPI

#endif // XENAPI_SR_H
